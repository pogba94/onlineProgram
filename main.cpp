#include "mbed.h"
#include "program.h"
#include "LPC1125_FlashMap.h"
#include "cJSON.h"
//#include "C12832.h"
#include "lib_crc16.h"
#include "UUencode.h"
#include "SDFileSystem.h"
#include "UserConfig.h"

/* Variable Definition */
Serial pc(USBTX,USBRX);               //Debug UART
Serial isp(PTE24,PTE25);              // ISP UART 
DigitalOut blue(LED_BLUE);
DigitalOut green(LED_GREEN);
DigitalOut red(LED_RED);
InterruptIn sw2(SW2);
SDFileSystem sd(PTE3, PTE1, PTE2, PTE4, "sd"); // MOSI, MISO, SCK, CS
FILE *fp;

char buffer[512];
char binFilePath[36];
//C12832 lcd(D11,D13,D12,D7,D10);

/* Macro Definition */
#define  FW_DESCRIPTION           "Online Program\r\nVersion:%s\r\nTarget platform:%s\r\n"
#define  FW_VERSION               "V1.0.0"
#define  TARGET_PLATFORM          "LPC1125"
#define  SD_DISK_NAME             "/sd"

#define  DEBUG_UART_BAUD_RATE     (115200)
#define  ISP_UART_BAUD_RATE       (115200)

/*sw2 irq handler */
void sw2_release()
{
	blue = !blue;
}

void ledInit(void)
{
	red =1;
	green = 1;
	blue = 1;
}

int detectBinFile(const char *dir,char *path)
{
	if(dir == NULL && path == NULL)
		return -1;
	DIR *d = opendir(dir);
	struct dirent *fp;
	char *p;
	while((fp = readdir(d)) != NULL){
		p = strstr(fp->d_name,".bin");
		if(p != NULL){
			strcpy(path,SD_DISK_NAME);
			strcat(path,"/");
			strcat(path,fp->d_name);
			return 0;
		}
	}
	return -2;
}

//uint32_t listFiles(const char *dir)
//{
//    DIR *d = opendir(dir);
//    struct dirent *p;
//    uint32_t counter = 0;

//    while ((p = readdir(d)) != NULL) {
//        counter++;
//        printf("%s\r\n", p->d_name);
//    }
//    closedir(d);
//    return counter;
//}

//void do_remove(const char *fsrc)
//{
//    DIR *d = opendir(fsrc);
//    struct dirent *p;
//    char path[30] = {0};
//    while((p = readdir(d)) != NULL) {
//        strcpy(path, fsrc);
//        strcat(path, "/");
//        strcat(path, p->d_name);
//        remove(path);
//    }
//    closedir(d);
//    remove(fsrc);
//}

int programBinFile(const char *path)
{
	if(path == NULL)
		return -1;
	/* open file */
	fp = fopen(path,"rb");
	if(fp == NULL){
		pc.printf("failed to open bin file,please check if bin file exsit!\r\n");
		return -2;
	}
	/* check size of bin file */
	int size;
	fseek(fp,0,SEEK_END);
	size = ftell(fp);
	pc.printf("size of bin file:%d bytes\r\n",size);
	if(size < MIN_BIN_SIZE || size > FLASH_SIZE){
		pc.printf("size is out of range allowed\r\n");
		return -3;
	}
	/* erase sectors */
	int ret;
	ISP_unlock();
	ret = ISP_EraseSector(0,SECTOR_NUM-1);
	if(ret != RET_CODE_SUCCESS){
		pc.printf("Erase sectors failed!\r\n");
		return -4;
	}
	pc.printf("Erase sectors successfully!\r\n");
	
	/* malloc memory */
	char *buffer = (char*)malloc(512);
	if(buffer == NULL){
		return -5;
	}
	/* program */
	int num = size/512 +(size%512!=0);
	int lastPackSize = (size%512==0)?512:(size%512);
	int *checksumW = (int*)malloc(sizeof(int)*num);
	if(checksumW == NULL){
	  free(buffer);
		return -5;
	}
	int *checksumR = (int*)malloc(sizeof(int)*num);
	if(checksumR == NULL){
	  free(buffer);
		free(checksumW);
		return -5;
	}
	ISP_unlock();
	for(int i=0;i<num;i++){
		/* load data */
		fseek(fp,i*512,SEEK_SET);
		if(i==num-1){
			if(fread(buffer,sizeof(char),lastPackSize,fp)!=lastPackSize){
				ret = -6;
				goto Exit;
			}
			for(int j=lastPackSize;j<512;j++)//filled with 0xff
				buffer[j] = 0xff;
		}else{
			if(fread(buffer,sizeof(char),512,fp) != 512){
				ret = -6;
				goto Exit;
			}
		}
		if(i==0){//modify vector at 0x1c
			uint32_t tmp = 0;
			for(int j=0;j<28;j++){
				tmp += (uint32_t)buffer[j]<<((j%4)*8);
			}
			tmp = ~tmp + 1;
			for(int j=0;j<4;j++){
				buffer[28+j] = tmp>>(j*8);
			}
		}
		if(ISP_WriteToRAM(ISP_WRITE_RAM_ADDR,512,buffer)!= RET_CODE_SUCCESS){
			ret = -7;
			goto Exit;
		}
		if(ISP_copyToFlash(i*512,ISP_WRITE_RAM_ADDR,512) != RET_CODE_SUCCESS){
			ret = -8;
			goto Exit;
		}
		/* calculate checksum */
		checksumW[i] = 0;
		for(int j=0;j<512;j++){
			checksumW[i] += buffer[j];
		}
	}
	/* Verify */
	for(int i=0;i<num;i++){
		checksumR[i] = 0;
		if(ISP_readMemory(i*512,512,buffer) != RET_CODE_SUCCESS){
			ret = -9;
			goto Exit;
		}
		for(int j=0;j<512;j++){
			checksumR[i] += buffer[j];
		}
		if(checksumR[i] != checksumW[i]){
			ret = -10;
			goto Exit;
		}
	}
	return 0;
	/* free memory here */
	Exit:
	{
		free(buffer);
		free(checksumR);
		free(checksumW);
		return ret;
	}
}

int programUID(uint32_t addr,const char id[],uint8_t size)
{
	int i;
	if(!CHECK_FLASH_ADDR(addr) || addr%16!=0 || id == NULL || size <= 0)
		return -1;
	if(addr+size > FLASH_END_ADDRESS)
		return -2;	
	
	int offset = addr%512;
	uint32_t programAddr = (addr/512)*512;
	int sectorIndex = addr/SECTOR_SIZE;
	uint32_t checksumR=0,checksumW = 0;
	int ret;
	
	memset(buffer,0xff,512);
	for(i=0;i<size;i++)
		buffer[offset+i] = id[i];
	
	/* Erase sector */
	ISP_unlock();
	if(ISP_EraseSector(sectorIndex,sectorIndex) != RET_CODE_SUCCESS){
		return -3;
	}
	/* program */
	ISP_unlock();
	if(ISP_WriteToRAM(ISP_WRITE_RAM_ADDR,512,buffer) != RET_CODE_SUCCESS){
		return -4;
	}
	if(ISP_copyToFlash(programAddr,ISP_WRITE_RAM_ADDR,512) != RET_CODE_SUCCESS){
		return -5;
	}
	/* Verify */
	for(i=0;i<512;i++)
		checksumW += buffer[i];
//  pc.printf("checksumW:%d\r\n",checksumW);	
	if(ISP_readMemory(programAddr,512,buffer) != RET_CODE_SUCCESS){
		return -6;
	}
	for(i=0;i<512;i++){
		checksumR += buffer[i];
//		if(i%16==0){pc.printf("\r\n");}
//		pc.printf("%x\t",buffer[i]);
	}
//  pc.printf("checksumR:%d\r\n",checksumR);	
	if(checksumW != checksumR){
		return -7;
	}
	return 0;
}


//void SDFileSysTest(void)
//{
//	pc.printf("SD File system test!\r\n");
//	pc.printf("Initializing...\r\n");
//	wait(2);

//	if(!detectBinFile(SD_DISK_NAME,binFilePath)){
//		pc.printf("bin file:%s\r\n",binFilePath);
//	}else{
//		pc.printf("cannot find bin file\r\n");
//	}
//	for(;;);
//	
//	fp = fopen(binFilePath,"rb");
//	if(fp == NULL){
//		pc.printf("fail to open bin file in the sd card\r\n");
//	}else{
//		int checksum = 0;
//		fseek(fp,0,SEEK_END);
//		int size = ftell(fp);
//		pc.printf("Detect bin file,size is:%d bytes\r\n",size);
//		rewind(fp);
//		if(fread(buffer,sizeof(char),size,fp) == size){
//			pc.printf("read data from bin file successfully\r\n");
//			for(int i=size;i<sizeof(buffer);i++) //filled with 0xff
//				buffer[i] = 0xff;
//			/* modify the vector at 0x1c*/
//      uint32_t tmp = 0;
//			for(int i=0;i<28;i++){
//				tmp += (uint32_t)buffer[i]<<((i%4)*8);
//			}
//			tmp = ~tmp + 1;
//			for(int i=0;i<4;i++){
//				buffer[28+i] = tmp>>(i*8);
//			}
//			/*
//			int len = size/512,extra = size%512;
//			if(extra > 0){
//				for(int i=0;i<(512-extra);i++)
//					buffer[size+i] = 0xff;
//				len++;
//			}
//			for(int i=0;i<len;i++){
//				checksum = 0;
//				for(int j=0;j<512;j++)
//					checksum += buffer[i*512+j];
//				pc.printf("sector:%d\tchecksum:%d\r\n",i,checksum);
//			}
//			char encode[64];
//			for(int i =0;i<len;i++){
//				int num = 512/45 + 1,lastSize = 512%45;
//				pc.printf("sector:%d\r\n",i);
//				for(int j=0;j<num;j++){
//					if(j<num-1)
//						UUencodeLine(buffer+i*512+j*45,encode,45);
//					else
//						UUencodeLine(buffer+i*512+j*45,encode,lastSize);
//					pc.printf("encode of sector%d-line%d:  %s\r\n",i,j,encode);
//				}
//				pc.printf("\r\n");
//			}
//			*/
//		}else{
//			pc.printf("read data failed!\r\n");
//		}
//	}
////	char txt1[] = "__\\Q";
////	char txt2[] = "__^M";
////	char decode[4];
////	UUdecodeBase(txt1,decode);
////	pc.printf("decode1:%d,%d,%d\r\n",decode[0],decode[1],decode[2]);
////	UUdecodeBase(txt2,decode);
////	pc.printf("decode2:%d,%d,%d\r\n",decode[0],decode[1],decode[2]);
//}


//void ispTest(void)
//{
//	char partID[12];	
//	ISP_syncBaudRate();
//	int ret;
//	ret = ISP_getPartID(partID);
//	if(ret == RET_CODE_SUCCESS){
//		pc.printf("partId:%s\r\n",partID);
//	}else{
//		pc.printf("read partId failed,ret=%d\r\n",ret);
//	}
//	
//	ret = ISP_unlock();
//	if(ret == RET_CODE_SUCCESS){
//		pc.printf("Unlock successfully!\r\n");
//	}else{
//		pc.printf("unlock failed,ret=%d\r\n",ret);
//	}
//	
////	ret = ISP_readMemory(0,512,buffer);
////	if(ret != RET_CODE_SUCCESS){
////		pc.printf("read menmory failed,ret=%d\r\n",ret);
////	}
////	pc.printf("read from flash:\r\n");
////  for(int i=0;i<512;i++){
////		if(i%16==0)
////			pc.printf("\r\n");
////		pc.printf("%x\t",buffer[i]);
////	}
////	for(;;);
//	
//	ret = ISP_EraseSector(0,SECTOR_NUM-1);
//	int num = sizeof(buffer)/512;
//	if(ret != RET_CODE_SUCCESS){
//		pc.printf("Erase flash failed\r\n");
//	}else{
//		pc.printf("Erase flash successfully\r\n");
//		ISP_unlock();
//		int offset;
//		for(int i=0;i<num;i++){
//			offset = i*512;
//			ret = ISP_WriteToRAM(0x10000300,512,buffer+offset);
//			if(ret == RET_CODE_SUCCESS){
//				ret = ISP_copyToFlash(offset,0x10000300,512);
//				if(ret == RET_CODE_SUCCESS){
//					pc.printf("program to address:%x successfully\r\n",offset);
//				}else{
//					pc.printf("program to address:%x failed\r\n",offset);
//					break;
//				}
//			}
//		}
//	}
//	for(;;);
//	
//	ret = ISP_EraseSector(0,SECTOR_NUM-1);
//	if(ret == RET_CODE_SUCCESS){
//		pc.printf("erase successfully\r\n");
//		ISP_unlock();
//		ret = ISP_WriteToRAM(0x10000300,512,buffer);
//		if(ret == RET_CODE_SUCCESS){
//			pc.printf("write to ram successfully\r\n");
//			ret = ISP_copyToFlash(0,0x10000300,512);
//			if(ret == RET_CODE_SUCCESS){
//				pc.printf("copy successfully\r\n");
//			}else{
//				pc.printf("copy failed,ret=%d\r\n",ret);
//			}
//		}else{
//			pc.printf("write to ram fail\r\n");
//		}
//	}else{
//		pc.printf("erase failed\r\n");
//	}
//}

int main()
{
	ledInit();
	pc.baud(DEBUG_UART_BAUD_RATE);
	isp.baud(ISP_UART_BAUD_RATE);
	sw2.rise(&sw2_release);
	pc.printf("***************************************\r\n");
	pc.printf(FW_DESCRIPTION,FW_VERSION,TARGET_PLATFORM);
	pc.printf("***************************************\r\n\r\n");

//	lcd.cls();
//	lcd.locate(0,0);
//	lcd.printf("Online Program for LPC1125");

//	UUencodeTest();
//	SDFileSysTest();
//	ispTest();
	pc.printf("wait for SD card Initializing...\r\n");
	wait(2);
	
	if(!detectBinFile(SD_DISK_NAME,binFilePath)){
		pc.printf("Dectect bin file in sd card\r\nbin file:%s\r\n",binFilePath);
		if(ISP_syncBaudRate() == RET_CODE_SUCCESS){
//			int ret = programBinFile(binFilePath);
//			if(!ret){
//				pc.printf("program  successfully!\r\n");
//			}else{
//				pc.printf("program failed!ret=%d\r\n",ret);
//			}
			char uid[] = "abcdefghijklmnopqrstyvw";
			int ret;
			ret = programUID(0xE160,uid,strlen(uid));
			if(!ret){
				pc.printf("program uid successfully!\r\n");
			}else{
				pc.printf("program uid failed!ret=%d\r\n",ret);
			}
		}
	}else{
		pc.printf("cannot detect bin file in sd card!please check!\r\n");
	}

	while(1){
	}
}
