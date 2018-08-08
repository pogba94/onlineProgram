#include "UserConfig.h"
#include "program.h"
#include "cJSON.h"
#include "lib_crc16.h"
#include "UUencode.h"
#include "SDFileSystem.h"
#include "indicator.h"
#if ENABLE_LCD12832
	#include "C12832.h"
#endif
#include "mbed.h"

/* Macro Definition */
#define  FW_DESCRIPTION           "Online Program\r\nVersion:%s\r\nTarget platform:%s\r\n"
#define  FW_VERSION               "V1.0.0"
#define  TARGET_PLATFORM          "LPC1125"
#define  SPLIT_LINE               "--------------------------------------------------------------"
#define  SD_DISK_NAME             "/sd"

#define  DEBUG_UART_BAUD_RATE     (115200)
#define  ISP_UART_BAUD_RATE       (115200)

typedef enum DEV_STATUS{
	DEV_STATUS_BIN_NOT_FOUND=0,
	DEV_STATUS_DISCONNECTED,
	DEV_STATUS_CONNECTED,
	DEV_STATUS_PROGRAMING,
	DEV_STATUS_PROGRAM_SUCCESS,
	DEV_STATUS_PROGRAM_FAIL,
}DEV_STATUS_T;

typedef struct EVENT_FLAGS{
	bool devStatusChangeFlag;
	bool binFileExsitFlag;
	bool boardConnectedFlag;
	bool startProgramFlag;
	bool resetDevStatusFlag;
}EVENT_FLAGS_T;

/* Variable Definition */
Serial pc(USBTX,USBRX);               //Debug UART
Serial isp(PTE24,PTE25);              // ISP UART 
DigitalOut blue(LED_BLUE);
DigitalOut green(LED_GREEN);
DigitalOut red(LED_RED);
InterruptIn sw2(SW2);
SDFileSystem sd(PTE3, PTE1, PTE2, PTE4, "sd"); // MOSI, MISO, SCK, CS
FILE *fp;
volatile int devStatus = DEV_STATUS_BIN_NOT_FOUND;
volatile EVENT_FLAGS_T eventFlags = {false,false,false,false};

char buffer[512];
char binFilePath[36];
#if ENABLE_LCD12832
	C12832 lcd(D11,D13,D12,D7,D10);
#endif
/*sw2 irq handler */
void sw2_release()
{
	if(devStatus == DEV_STATUS_CONNECTED){
		eventFlags.startProgramFlag = true;
	}else if(devStatus == DEV_STATUS_PROGRAM_FAIL || devStatus == DEV_STATUS_PROGRAM_SUCCESS){
		eventFlags.resetDevStatusFlag = true;
	}else{
	}
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
	if(d == NULL)
		return -2;
	struct dirent *fp;
	char *p;
	while((fp = readdir(d)) != NULL){
		p = strstr(fp->d_name,".bin");
		if(p != NULL){
			if(strcmp(path,fp->d_name)){
				path[0] = '\0';
				strcat(path,SD_DISK_NAME);
				strcat(path,"/");
				strcat(path,fp->d_name);
				closedir(d);
			}		
			return 0;
		}
	}
	closedir(d);
	return -3;
}

int detectConnectStatus(void)
{
	return ISP_syncBaudRate();
}

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
	ret = ISP_erase(0,SECTOR_NUM-1);
	if(ret != RET_CODE_SUCCESS){
		pc.printf("Erase sectors failed!ret=%d\r\n",ret);
		return -4;
	}
	pc.printf("Erase sectors successfully!\r\n");
	pc.printf("Programing...\r\n");
	
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
	#if SHOW_PROGRAM_SCHEDULE
	int n = 60;
	for(int i=0;i<n;i++)
		pc.putc('=');
	pc.puts("\r\n");
	#endif
	ISP_unlock();
	for(int i=0;i<num;i++){
		Indicator_programing();
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
		#if SHOW_PROGRAM_SCHEDULE
		n = (i==num-1)?(60-(60/num)*(num-1)):(60/num);
		for(int k=0;k<n;k++)
			pc.putc('*');
		#endif
	}
	pc.printf("\r\nprogram finished\r\nverifying...\r\n");
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
	pc.printf("verify OK!\r\n");
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
	uint32_t programAddr;
	int sectorIndex = addr/SECTOR_SIZE;
	uint32_t checksumR=0,checksumW = 0;
	int ret;
	programAddr = (2*(addr/512)+(addr%512+size>512))*256;
	
	memset(buffer,0xff,512);
	for(i=0;i<size;i++)
		buffer[offset+i] = id[i];
	
	/* Erase sector */
	ISP_unlock();
	if(ISP_erase(sectorIndex,sectorIndex) != RET_CODE_SUCCESS){
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
	}
//  pc.printf("checksumR:%d\r\n",checksumR);	
	if(checksumW != checksumR){
		return -7;
	}
	return 0;
}

int main()
{
	ledInit();
	pc.baud(DEBUG_UART_BAUD_RATE);
	isp.baud(ISP_UART_BAUD_RATE);
	sw2.rise(&sw2_release);
	pc.printf("%s\r\n",SPLIT_LINE);
	pc.printf(FW_DESCRIPTION,FW_VERSION,TARGET_PLATFORM);
	pc.printf("%s\r\n",SPLIT_LINE);

	#if ENABLE_LCD12832
		lcd.cls();
		lcd.locate(0,0);
		lcd.printf("Online Program for LPC1125");
	#endif
	
	pc.printf("SD card Initializing,wait for a moment please!\r\n");
	wait(2);
	pc.printf("Start to work!\r\n");
	
	/*
	if(!detectBinFile(SD_DISK_NAME,binFilePath)){
		pc.printf("Dectect bin file in sd card\r\nbin file:%s\r\n",binFilePath);
		if(ISP_syncBaudRate() == RET_CODE_SUCCESS){
			int ret = programBinFile(binFilePath);
			if(!ret){
				pc.printf("program  successfully!\r\n");
			}else{
				pc.printf("program failed!ret=%d\r\n",ret);
			}
			char uid[] = "abcdefghijklmnopqrstyvw";
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
	*/
	while(1){
		/* detect if bin file exsit */
		if(!detectBinFile(SD_DISK_NAME,binFilePath)){
			eventFlags.binFileExsitFlag = true;
		}else{
			eventFlags.binFileExsitFlag = false;
		}
		/* detect if connected to target board */
		if(!detectConnectStatus()){
			eventFlags.boardConnectedFlag = true;
		}else{
			eventFlags.boardConnectedFlag = false;
		}
		/* status machine handle */
		switch(devStatus){
			case DEV_STATUS_BIN_NOT_FOUND:
				Indicator_noBin();
				if(eventFlags.binFileExsitFlag == true){
					devStatus = DEV_STATUS_DISCONNECTED;
				}
				break;
			case DEV_STATUS_CONNECTED:
				Indicator_connected();
				if(eventFlags.boardConnectedFlag == false){
					devStatus = DEV_STATUS_DISCONNECTED;
					eventFlags.startProgramFlag = false;
				}
				if(eventFlags.startProgramFlag == true){
					devStatus = DEV_STATUS_PROGRAMING;
					eventFlags.startProgramFlag = false;
				}
				if(eventFlags.binFileExsitFlag == false){
					devStatus = DEV_STATUS_BIN_NOT_FOUND;
					eventFlags.startProgramFlag = false;
				}
				break;
			case DEV_STATUS_DISCONNECTED:
				Indicator_disconnected();
				if(eventFlags.boardConnectedFlag == true){
					devStatus = DEV_STATUS_CONNECTED;
				}
				if(eventFlags.binFileExsitFlag == false){
					devStatus = DEV_STATUS_BIN_NOT_FOUND;
				}
				break;
			case DEV_STATUS_PROGRAMING:
				pc.printf("bin:%s\r\n",binFilePath);
				if(!programBinFile(binFilePath)){
					pc.printf("program successfully!\r\n");
					devStatus = DEV_STATUS_PROGRAM_SUCCESS;
				}else{
					pc.printf("program failed!\r\n");
					devStatus = DEV_STATUS_PROGRAM_FAIL;
				}
				break;
			case DEV_STATUS_PROGRAM_SUCCESS:
				Indicator_programDone();
				if(eventFlags.resetDevStatusFlag == true){
					devStatus = DEV_STATUS_CONNECTED;
					eventFlags.resetDevStatusFlag = false;
				}
				break;
			case DEV_STATUS_PROGRAM_FAIL:
				Indicator_programFail();
				if(eventFlags.resetDevStatusFlag == true){
					devStatus = DEV_STATUS_CONNECTED;
					eventFlags.resetDevStatusFlag = false;
				}
				break;
			default:
				break;
		}
		wait_ms(500);
	}
}
