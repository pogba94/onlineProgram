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
	wait(1);
	fp = fopen("/sd/periph_blinky.bin","rb");
	if(fp == NULL){
		pc.printf("cannot found file 'periph_blinky.bin' in the sd card\r\n");
	}else{
		char buffer[4096];
		int checksum = 0;
		fseek(fp,0,SEEK_END);
		int size = ftell(fp);
		pc.printf("size of bin file:%d bytes\r\n",size);
		rewind(fp);
		if(fread(buffer,sizeof(char),size,fp) == size){
			pc.printf("read data from bin file:\r\n");
			int ret;
			ret = ISP_WriteToRAM(0x10000100,512,buffer);
			pc.printf("ret=%d\r\n",ret);
			
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

/*
void SDFileSysTest(void)
{
	  pc.printf("Initializing \r\n");
    wait(2);
    
    do_remove("/sd/test1");
    if (do_list("/sd") == 0) {
        printf("No files/directories on the sd card.");
    }

    printf("\r\nCreating two folders. \r\n");
    mkdir("/sd/test1", 0777);
    mkdir("/sd/test2", 0777);

    fp = fopen("/sd/test1/1.txt", "w");
    if (fp == NULL) {
        pc.printf("Unable to write the file \r\n");
    } else {
        fprintf(fp, "1.txt in test 1");
        fclose(fp);
    }

    fp = fopen("/sd/test2/2.txt", "w");
    if (fp == NULL) {
        pc.printf("Unable to write the file \r\n");
    } else {
        fprintf(fp, "2.txt in test 2");
        fclose(fp);
    }

    printf("\r\nList all directories/files /sd.\r\n");
    do_list("/sd");

    printf("\r\nList all files within /sd/test1.\r\n");
    do_list("/sd/test1");


    printf("\r\nList all files within /sd/test2.\r\n");
    do_list("/sd/test2");

    int status = file_copy("/sd/test2/2.txt", "/sd/test1/2_copy.txt");
    if (status == -1) {
        printf("Error, file was not copied.\r\n");
    }
    printf("Removing test2 folder and 2.txt file inside.");
    remove("/sd/test2/2.txt");
    remove("/sd/test2");

    printf("\r\nList all directories/files /sd.\r\n");
    do_list("/sd");

    printf("\r\nList all files within /sd/test1.\r\n");
    do_list("/sd/test1");

    printf("\r\nEnd of complete Lab 5. \r\n");
}
*/
int main()
{
	char partID[12];
	pc.baud(DEBUG_UART_BAUD_RATE);
	isp.baud(ISP_UART_BAUD_RATE);
	sw2.rise(&sw2_release);
	pc.printf(FW_DESCRIPTION,FW_VERSION,TARGET_PLATFORM);
	
//	lcd.cls();
//	lcd.locate(0,0);
//	lcd.printf("Online Program for LPC1125");
//	UUencodeTest();
//	SDFileSysTest();
	ISP_syncBaudRate();
	int ret;
	ret = ISP_getPartID(partID);
	if(ret == RET_CODE_SUCCESS){
		pc.printf("partId:%s\r\n",partID);
	}
	
	ret = ISP_unlock();
	if(ret == RET_CODE_SUCCESS){
		pc.printf("Unlock successfully!\r\n");
	}else{
		pc.printf("unlock failed,ret=%d\r\n",ret);
	}
	
	ret = ISP_sectorOperation(ISP_CMD_PREPARE_SECTORS,0,SECTOR_NUM-1);
	pc.printf("prepare sectors\r\n");
	if(ret == RET_CODE_SUCCESS){
		pc.printf("prepare sectors successfully!\r\n");
		ret = ISP_sectorOperation(ISP_CMD_ERASE_SECTORS,0,SECTOR_NUM-1);
		if(ret == RET_CODE_SUCCESS){
			pc.printf("Erase sectors successfully!\r\n");
		}else{
			pc.printf("Erase sectors failed!ret=%d\r\n");
		}
	}else{
		pc.printf("prepare sectors failed!ret=%d\r\n",ret);
	}
	
	
	while(1){
	}
}