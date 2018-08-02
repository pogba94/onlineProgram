#include "mbed.h"
#include "program.h"
#include "LPC1125_FlashMap.h"
#include "cJSON.h"
//#include "C12832.h"
#include "lib_crc16.h"
#include "UUencode.h"
#include "SDFileSystem.h"

/* Macro Definition */
#define  FW_DESCRIPTION           "Online Program\r\nVersion:%s\r\nTarget platform:%s\r\n"
#define  FW_VERSION               "V1.0.0"
#define  TARGET_PLATFORM          "LPC1125"

#define  DEBUG_UART_BAUD_RATE      (115200)
#define  ISP_UART_BAUD_RATE        (115200)

/* Variable Definition */
Serial pc(USBTX,USBRX);               //Debug UART
Serial isp(PTE24,PTE25);              // ISP UART 
DigitalOut led(LED_BLUE);
InterruptIn sw2(SW2);

SDFileSystem sd(PTE3, PTE1, PTE2, PTE4, "sd"); // MOSI, MISO, SCK, CS
FILE *fp;
char buffer[4096];

//C12832 lcd(D11,D13,D12,D7,D10);

/*sw2 irq handler */
void sw2_release()
{
	led = !led;
}

int file_copy(const char *src, const char *dst)
{
    int retval = 0;
    int ch;
 
    FILE *fpsrc = fopen(src, "r");   // src file
    FILE *fpdst = fopen(dst, "w");   // dest file
    
    while (1) {                  // Copy src to dest
        ch = fgetc(fpsrc);       // until src EOF read.
        if (ch == EOF) break;
        fputc(ch, fpdst);
    }
    fclose(fpsrc);
    fclose(fpdst);
  
    fpdst = fopen(dst, "r");     // Reopen dest to insure
    if (fpdst == NULL) {          // that it was created.
        retval = -1;           // Return error.
    } else {
        fclose(fpdst);
        retval = 0;              // Return success.
    }
    return retval;
}

uint32_t do_list(const char *fsrc)
{
    DIR *d = opendir(fsrc);
    struct dirent *p;
    uint32_t counter = 0;

    while ((p = readdir(d)) != NULL) {
        counter++;
        printf("%s\r\n", p->d_name);
    }
    closedir(d);
    return counter;
}

void do_remove(const char *fsrc)
{
    DIR *d = opendir(fsrc);
    struct dirent *p;
    char path[30] = {0};
    while((p = readdir(d)) != NULL) {
        strcpy(path, fsrc);
        strcat(path, "/");
        strcat(path, p->d_name);
        remove(path);
    }
    closedir(d);
    remove(fsrc);
}

void SDFileSysTest(void)
{
	pc.printf("SD File system test!\r\n");
	pc.printf("Initializing...\r\n");
	wait(2);
	fp = fopen("/sd/periph_blinky.bin","rb");
	if(fp == NULL){
		pc.printf("cannot found file 'periph_blinky.bin' in the sd card\r\n");
	}else{
		int checksum = 0;
		fseek(fp,0,SEEK_END);
		int size = ftell(fp);
		pc.printf("Detect bin file,size is:%d bytes\r\n",size);
		rewind(fp);
		if(fread(buffer,sizeof(char),size,fp) == size){
			pc.printf("read data from bin file successfully\r\n");
			for(int i=size;i<sizeof(buffer);i++) //filled with 0xff
				buffer[i] = 0xff;
			/* modify the vector at 0x1c*/
      uint32_t tmp = 0;
			for(int i=0;i<28;i++){
				tmp += (uint32_t)buffer[i]<<((i%4)*8);
			}
			tmp = ~tmp + 1;
			for(int i=0;i<4;i++){
				buffer[28+i] = tmp>>(i*8);
			}
			/*
			int len = size/512,extra = size%512;
			if(extra > 0){
				for(int i=0;i<(512-extra);i++)
					buffer[size+i] = 0xff;
				len++;
			}
			for(int i=0;i<len;i++){
				checksum = 0;
				for(int j=0;j<512;j++)
					checksum += buffer[i*512+j];
				pc.printf("sector:%d\tchecksum:%d\r\n",i,checksum);
			}
			char encode[64];
			for(int i =0;i<len;i++){
				int num = 512/45 + 1,lastSize = 512%45;
				pc.printf("sector:%d\r\n",i);
				for(int j=0;j<num;j++){
					if(j<num-1)
						UUencodeLine(buffer+i*512+j*45,encode,45);
					else
						UUencodeLine(buffer+i*512+j*45,encode,lastSize);
					pc.printf("encode of sector%d-line%d:  %s\r\n",i,j,encode);
				}
				pc.printf("\r\n");
			}
			*/
		}else{
			pc.printf("read data failed!\r\n");
		}
	}
//	char txt1[] = "__\\Q";
//	char txt2[] = "__^M";
//	char decode[4];
//	UUdecodeBase(txt1,decode);
//	pc.printf("decode1:%d,%d,%d\r\n",decode[0],decode[1],decode[2]);
//	UUdecodeBase(txt2,decode);
//	pc.printf("decode2:%d,%d,%d\r\n",decode[0],decode[1],decode[2]);
}

void ispTest(void)
{
	char partID[12];	
	ISP_syncBaudRate();
	int ret;
	ret = ISP_getPartID(partID);
	if(ret == RET_CODE_SUCCESS){
		pc.printf("partId:%s\r\n",partID);
	}else{
		pc.printf("read partId failed,ret=%d\r\n",ret);
	}
	
	ret = ISP_unlock();
	if(ret == RET_CODE_SUCCESS){
		pc.printf("Unlock successfully!\r\n");
	}else{
		pc.printf("unlock failed,ret=%d\r\n",ret);
	}
	
//	ret = ISP_readMemory(0,512,buffer);
//	if(ret != RET_CODE_SUCCESS){
//		pc.printf("read menmory failed,ret=%d\r\n",ret);
//	}
//	pc.printf("read from flash:\r\n");
//  for(int i=0;i<512;i++){
//		if(i%16==0)
//			pc.printf("\r\n");
//		pc.printf("%x\t",buffer[i]);
//	}
//	for(;;);
	
	ret = ISP_EraseSector(0,SECTOR_NUM-1);
	int num = sizeof(buffer)/512;
	pc.printf("num:%d\r\n",num);
	if(ret != RET_CODE_SUCCESS){
		pc.printf("Erase flash failed\r\n");
	}else{
		pc.printf("Erase flash successfully\r\n");
		ISP_unlock();
		int offset;
		for(int i=0;i<num;i++){
			offset = i*512;
			pc.printf("programing to address:%x\r\n",offset);
			ret = ISP_WriteToRAM(0x10000300,512,buffer+offset);
			if(ret == RET_CODE_SUCCESS){
				ret = ISP_copyToFlash(offset,0x10000300,512);
				if(ret == RET_CODE_SUCCESS){
					pc.printf("program to address:%x successfully\r\n",offset);
				}else{
					pc.printf("program to address:%x failed\r\n",offset);
					break;
				}
			}
		}
	}
	for(;;);
	
	ret = ISP_EraseSector(0,SECTOR_NUM-1);
	if(ret == RET_CODE_SUCCESS){
		pc.printf("erase successfully\r\n");
		ISP_unlock();
		ret = ISP_WriteToRAM(0x10000300,512,buffer);
		if(ret == RET_CODE_SUCCESS){
			pc.printf("write to ram successfully\r\n");
			ret = ISP_copyToFlash(0,0x10000300,512);
			if(ret == RET_CODE_SUCCESS){
				pc.printf("copy successfully\r\n");
			}else{
				pc.printf("copy failed,ret=%d\r\n",ret);
			}
		}else{
			pc.printf("write to ram fail\r\n");
		}
	}else{
		pc.printf("erase failed\r\n");
	}
}

int main()
{
	pc.baud(DEBUG_UART_BAUD_RATE);
	isp.baud(ISP_UART_BAUD_RATE);
	sw2.rise(&sw2_release);
	pc.printf("***************************************\r\n");
	pc.printf(FW_DESCRIPTION,FW_VERSION,TARGET_PLATFORM);
	pc.printf("***************************************\r\n");
//	lcd.cls();
//	lcd.locate(0,0);
//	lcd.printf("Online Program for LPC1125");
//	UUencodeTest();

	SDFileSysTest();
	ispTest();

	while(1){
	}
}