#include "UserConfig.h"
#include "program.h"
#include "cJSON.h"
#include "lib_crc16.h"
#include "UUencode.h"
#include "SDFileSystem.h"
#include "indicator.h"
#include "network.h"
#if ENABLE_LCD12832
	#include "C12832.h"
#endif
#include "mbed.h"
#include "OnlineProgram.h"

/* Macro Definition */
#define  FW_DESCRIPTION           "Online Program\r\nVersion:%s\r\nTarget platform:%s\r\n"
#define  FW_VERSION               "V1.0.0"
#define  TARGET_PLATFORM          "LPC1125"
#define  SPLIT_LINE               "--------------------------------------------------------------"
#define  SD_DISK_NAME             "/sd"

#define  DEFAULT_DEBUG_UART_BAUDRATE     (115200)
#define  DEFAULT_ISP_UART_BAUDRATE       (115200)
#define  CFG_FILE_SIZE_LIMIT             (1024)

/* Variable Definition */
Serial pc(USBTX,USBRX);               //Debug UART
Serial isp(PTE24,PTE25);              // ISP UART 
DigitalOut blue(LED_BLUE);
DigitalOut green(LED_GREEN);
DigitalOut red(LED_RED);
InterruptIn sw2(SW2);
InterruptIn sw3(SW3);
SDFileSystem sd(PTE3, PTE1, PTE2, PTE4, "sd"); // MOSI, MISO, SCK, CS
#if ENABLE_LCD12832
	C12832 lcd(D11,D13,D12,D7,D10);
#endif
FILE *fp;
volatile int devStatus;
volatile EVENT_FLAGS_T eventFlags;
ISP_CONFIG_T ISPcfg;

extern TCPSocketConnection tcpSock;
extern NETWORK_EVENT_T networkEvent;
char buffer[512];
char binFilePath[36];
char cfgFilePath[36];
char customerCode[] = "0000000100000002abcdefgh";
char uid[36];
uint32_t systemTimer = 0;
volatile int deviceMode;

void oneSecondThread(const void *agurement)
{
	while(1){
		Thread::wait(1000);
		pc.printf("--------------------systemTimer=%d----------------------\r\n",systemTimer++);
		if(deviceMode == MODE_PROGRAM_UID && systemTimer%HEARTBEAT_PERIOD_S == 0 && networkEvent.offlineFlag == false)
			networkEvent.heartbeatFlag = true;
	}
}

/* start to program */
void sw2_release()
{
	if(deviceMode == MODE_PROGRAM_CODE){
		if(devStatus == DEV_STATUS_CONNECTED){
			eventFlags.startProgramFlag = true;
		}
	}else{
		if(devStatus == DEV_STATUS_CONNECTED){
			networkEvent.uidReqFlag = true;  //request UID
		}
	}
	if(devStatus == DEV_STATUS_PROGRAM_FAIL || devStatus == DEV_STATUS_PROGRAM_SUCCESS)
			eventFlags.resetDevStatusFlag = true;
}

/* switch deviceMode */
void sw3_release()
{
	eventFlags.modeSwitchFlag = true;
	if(deviceMode == MODE_PROGRAM_CODE){
		deviceMode = MODE_PROGRAM_UID;
	}else{
		deviceMode = MODE_PROGRAM_CODE;
	}
}

void ledInit(void)
{
	red = 1;
	green = 1;
	blue = 1;
}

void resetEventFlags(void)
{
	eventFlags.binFileExsitFlag = false;
	eventFlags.boardConnectedFlag = false;
	eventFlags.resetDevStatusFlag = false;
	eventFlags.startProgramFlag = false;
	eventFlags.modeSwitchFlag = false;
	eventFlags.UIDavailableFlag = false;
	eventFlags.UIDProgramSuccessFlag = false;
	eventFlags.notifyDoneFlag = false;
}

int detectFile(const char *dir,const char* file_type,char *path)
{
	if(dir == NULL || path == NULL || file_type == NULL)
		return -1;
	DIR *d = opendir(dir);
	if(d == NULL)
		return -2;
	struct dirent *fp;
	char *p;
	while((fp = readdir(d)) != NULL){
		p = strstr(fp->d_name,file_type);
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
	fclose(fp);
	return 0;
	/* free memory here */
	Exit:
	{
		free(buffer);
		free(checksumR);
		free(checksumW);
		fclose(fp);
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

int strFlit(const char* in,char *out)
{	
	if(in == NULL || out == NULL)
		return -1;
	int pIn = 0,pOut = 0;
	while(*(in+pIn++) != '{'){}
	pIn--;
	while(*(in+pIn) != '\0'){
		if(*(in+pIn)>32 && *(in+pIn)<126){
			*(out+pOut) = *(in+pIn);
			pOut++;
		}
		pIn++;
	}
	out[pOut] = '\0';
	return 0;
}


int getConfig(const char* path,ISP_CONFIG_T* config)
{
	if(path == NULL || config == NULL)
		return -1;
	fp = fopen(path,"rt");
	if(fp == NULL){
		pc.printf("cannot open file\r\n");
		return -2;
	}
	fseek(fp,0,SEEK_END);
	int size = ftell(fp);
	pc.printf("size of config file:%d bytes\r\n",size);
	if(size > CFG_FILE_SIZE_LIMIT){
		pc.printf("config file is too large,no more than %d bytes\r\n",CFG_FILE_SIZE_LIMIT);
		fclose(fp);
		return -3;
	}
	char *txt = (char*)malloc(CFG_FILE_SIZE_LIMIT+1);
	if(txt == NULL){
		fclose(fp);
		return -4;
	}
	char *txtFlit = (char*)malloc(CFG_FILE_SIZE_LIMIT+1);
	if(txtFlit == NULL){
		fclose(fp);
		return -4;
	}
	int ret;
	rewind(fp);
	fread(txt,sizeof(char),size,fp);
	txt[size] = '\0';
	pc.printf("txt:%s\r\n",txt);
	strFlit(txt,txtFlit);
	pc.printf("txt filter:%s\r\n",txtFlit);
	cJSON *json = cJSON_Parse(txt);
	if(!json){
		pc.printf("no json string!\r\n");
		ret = -5;
		goto Exit;
	}
	if(cJSON_GetObjectItem(json,"platform") == NULL ||
			cJSON_GetObjectItem(json,"map") == NULL ||
			cJSON_GetObjectItem(json,"ISP_setting") == NULL){
				pc.printf("lack of item\r\nplatform:%d,map:%d,isp_setting:%d\r\n",
				cJSON_GetObjectItem(json,"platform"),
				cJSON_GetObjectItem(json,"map"),
				cJSON_GetObjectItem(json,"ISP_setting")
				);
		ret = -6;
		goto Exit;
	}
	sprintf(config->tragetIC,"%s",cJSON_GetObjectItem(json,"platfrom")->valuestring);
	pc.printf("platform:%s\r\n",config->tragetIC);
  cJSON *ISP_seting = cJSON_GetObjectItem(json,"ISP_setting");
  cJSON *memory_map = cJSON_GetObjectItem(json,"map");
  sprintf(config->baudrate,"%s",cJSON_GetObjectItem(ISP_seting,"baudrate")->valuestring);
	pc.printf("baudrate:%d\r\n",config->baudrate);
  pc.printf("check item pass\r\n");	
	return 0;
	Exit:
	{
		fclose(fp);
		free(txt);
		return ret;
	}
}

void test(void)
{
	if(!detectFile(SD_DISK_NAME,".json",cfgFilePath)){
		pc.printf("found configure file:%s\r\n",cfgFilePath);
		getConfig(cfgFilePath,&ISPcfg);
	}else{
		pc.printf("cannot found configure file\r\n");
	}
}

int main()
{
	ledInit();
	resetEventFlags();
	sw2.rise(&sw2_release);
	sw3.rise(&sw3_release);
	/* set baudrate */
	pc.baud(DEFAULT_DEBUG_UART_BAUDRATE);
	isp.baud(DEFAULT_ISP_UART_BAUDRATE);
	/* print setup imformation */
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
	
	initNetworkEvent();
	initNetwork();
	Thread th1(oneSecondThread,NULL,osPriorityNormal,512);
	
  deviceMode = MODE_PROGRAM_UID; //select mode
	pc.printf("Initial device mode:%d\r\n",deviceMode);
	/* device status initialization */
	if(deviceMode == MODE_PROGRAM_UID){
		devStatus = DEV_STATUS_DISCONNECTED;
	}else{
		devStatus = DEV_STATUS_BIN_NOT_FOUND;
	}

	while(1){
		/* Switch Stauts when deviceMode switched */
		if(eventFlags.modeSwitchFlag == true){
			pc.printf("mode change to %d\r\n",deviceMode);
			eventFlags.modeSwitchFlag = false;
			eventFlags.startProgramFlag = false;
			if(deviceMode == MODE_PROGRAM_UID){
				devStatus = DEV_STATUS_DISCONNECTED;
			}else{
				devStatus = DEV_STATUS_BIN_NOT_FOUND;
			}
		}
		
		/* detect if bin file exsit */
		if(deviceMode == MODE_PROGRAM_CODE){ //Detect bin file when in program code deviceMode
			if(!detectFile(SD_DISK_NAME,".bin",binFilePath)){
				eventFlags.binFileExsitFlag = true;
			}else{
				eventFlags.binFileExsitFlag = false;
			}
		}
		/* detect if connected to target board */
		if(!detectConnectStatus()){
			eventFlags.boardConnectedFlag = true;
		}else{
			eventFlags.boardConnectedFlag = false;
		}
		if(deviceMode == MODE_PROGRAM_UID){//network enable when in program UID deviceMode
//			checkNetwork();
//			msgTransceiverHandle();
		}
		if((deviceMode == MODE_PROGRAM_UID && networkEvent.authorizePassFlag == true) || deviceMode == MODE_PROGRAM_CODE){
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
					if(deviceMode == MODE_PROGRAM_CODE){
						if(eventFlags.binFileExsitFlag == false){
							devStatus = DEV_STATUS_BIN_NOT_FOUND;
							eventFlags.startProgramFlag = false;
						}
					}else{
						if(eventFlags.UIDavailableFlag == true){
							devStatus = DEV_STATUS_PROGRAMING;
							eventFlags.UIDavailableFlag = false;
						}
					}
					break;
				case DEV_STATUS_DISCONNECTED:
					Indicator_disconnected();
					if(eventFlags.boardConnectedFlag == true){
						devStatus = DEV_STATUS_CONNECTED;
					}
					if(eventFlags.binFileExsitFlag == false && deviceMode == MODE_PROGRAM_CODE){
							devStatus = DEV_STATUS_BIN_NOT_FOUND;
					}
					break;
				case DEV_STATUS_PROGRAMING:
					if(deviceMode == MODE_PROGRAM_CODE){
						if(!programBinFile(binFilePath)){ //start to program code
							pc.printf("program code successfully!\r\n");
							devStatus = DEV_STATUS_PROGRAM_SUCCESS;
						}else{
							pc.printf("program code failed!\r\n");
							devStatus = DEV_STATUS_PROGRAM_FAIL;
						}
					}else{//start to program uid
						if(!programUID(UID_PROGRAM_ADDRESS,uid,strlen(uid))){
							devStatus = DEV_STATUS_PROGRAM_SUCCESS;
							eventFlags.UIDProgramSuccessFlag = true;
							pc.printf("Program UID successfully\r\n");
						}else{
							devStatus = DEV_STATUS_PROGRAM_FAIL;
							eventFlags.UIDProgramSuccessFlag = false;
							pc.printf("Program UID failed\r\n");
						}
						networkEvent.notifyResultFlag = true; //notify the result to server!
					}
					break;
				case DEV_STATUS_PROGRAM_SUCCESS:
					Indicator_programDone();
					if(eventFlags.resetDevStatusFlag == true){
						if(deviceMode == MODE_PROGRAM_CODE){
							devStatus = DEV_STATUS_CONNECTED;
							eventFlags.resetDevStatusFlag = false;
						}else{
							if(eventFlags.notifyDoneFlag == true){
								devStatus = DEV_STATUS_CONNECTED;
								eventFlags.resetDevStatusFlag = false;
								eventFlags.notifyDoneFlag = false;
							}
						}
					}
					break;
				case DEV_STATUS_PROGRAM_FAIL:
					Indicator_programFail();
					if(eventFlags.resetDevStatusFlag == true){
						if(deviceMode == MODE_PROGRAM_CODE){
							devStatus = DEV_STATUS_CONNECTED;
							eventFlags.resetDevStatusFlag = false;
						}else{
							if(eventFlags.notifyDoneFlag == true){
								devStatus = DEV_STATUS_CONNECTED;
								eventFlags.resetDevStatusFlag = false;
								eventFlags.notifyDoneFlag = false;
							}
						}
					}
					break;
				default:
					break;
			}
			wait_ms(200);
		}else{
			Indicator_offline();
		}
//    pc.printf("devStatus=%d\r\nmode=%d\r\n",devStatus,deviceMode);
	}
}
