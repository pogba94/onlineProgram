#include "program.h"
#include "mbed.h"
#include "string.h"
#include "stdlib.h"
#include "LPC1125_FlashMap.h"
#include "UUencode.h"

#define  ISP_RECV_BUF_SIZE            (1024)
#define  ISP_UART_RECV_TIMEOUT_US     (500)

char isp_cmd_list[] = {'U','B','A','W','R','P','C','G','E','I','J','K','M','N'};

extern Serial isp;
extern Serial pc;
char ispRecvBuf[ISP_RECV_BUF_SIZE]; //uart recieve buffer


int ISP_getResp(char *buf,uint32_t timeout_us)
{
	int count = 0,pStr = 0;
	if(NULL == buf)
		return RET_CODE_PARAM_ILLEGAL;
	while(count < timeout_us){
		if(isp.readable()){
			count = 0;
			buf[pStr++] = isp.getc();
		}else{
			wait_us(1);
			count++;
		}
	}
	return pStr;
}

int ISP_syncBaudRate(void)
{
	BAUDRATE_SYNC_STATUS_T syncStatus = SYNC_STATUS_SETUP_SYNC;
	char *p;
	while(1){
		switch(syncStatus){
			case SYNC_STATUS_SETUP_SYNC:
				memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
				isp.puts(BAUDRATE_SYNC_SEND_STR);
				if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
					syncStatus = SYNC_STATUS_SYNC_TIMEOUT;
				}else{
					pc.printf("recieve data:%s\r\n",ispRecvBuf);
					p = strstr(ispRecvBuf,BAUDRATE_SYNC_RECV_STR);
					if(p != NULL){
						syncStatus = SYNC_STATUS_RESP_SYNC;
					}else{
						p = strstr(ispRecvBuf,BAUDRATE_SYNC_SEND_STR);
						if(p == NULL)
							syncStatus = SYNC_STATUS_SYNC_ERROR;
						else
							syncStatus = SYNC_STATUS_EXIT_SYNC;
					}
				}
			  break;
			case SYNC_STATUS_RESP_SYNC:
				memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
				isp.puts(BAUDRATE_SYNC_RECV_STR);
				isp.puts(ENDCODE_STR);
				if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
					syncStatus = SYNC_STATUS_SYNC_TIMEOUT;
				}else{
					pc.printf("recieve data:%s\r\n",ispRecvBuf);
					p = strstr(ispRecvBuf,OK_STR);
					if(p != NULL){
						syncStatus = SYNC_STATUS_SEND_FREQ;
					}else{
						syncStatus = SYNC_STATUS_SYNC_ERROR;
					}
				}
				break;
			case SYNC_STATUS_SEND_FREQ:
				memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
			  isp.puts("0\r\n");
				if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
					syncStatus = SYNC_STATUS_SYNC_TIMEOUT;
				}else{
					pc.printf("recieve data:%s\r\n",ispRecvBuf);
					p = strstr(ispRecvBuf,OK_STR);
					if(p != NULL){
						syncStatus = SYNC_STATUS_SYNC_DONE;
					}else{
						syncStatus = SYNC_STATUS_SYNC_ERROR;
					}
				}
				break;
			case SYNC_STATUS_EXIT_SYNC:
				memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
				isp.putc(27);
				if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
					syncStatus = SYNC_STATUS_SYNC_TIMEOUT;
				}else{
					if(ispRecvBuf[0] == 27)
						syncStatus = SYNC_STATUS_SYNC_DONE;
					else
						syncStatus = SYNC_STATUS_SYNC_ERROR;
				}
				break;
			case SYNC_STATUS_SYNC_DONE:
				pc.printf("Baudrate synchronize done!\r\n");
				return RET_CODE_SUCCESS;
			case SYNC_STATUS_SYNC_TIMEOUT:
				pc.printf("Baudrate synchronize timeout!\r\n");
				return RET_CODE_RESP_TIMEOUT;
			case SYNC_STATUS_SYNC_ERROR:
				pc.printf("Baudrate synchronize error!\r\n");
				return RET_CODE_ERROR;
			default:
				return 0;
		}
	}
}

int ISP_getPartID(char partId[])
{
	char *p;
	memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
	isp.putc(isp_cmd_list[ISP_CMD_READ_PART_ID]);
	isp.puts(ENDCODE_STR);
  if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
		return RET_CODE_RESP_TIMEOUT;
	}else{
		p = strstr(ispRecvBuf,CMD_SUCCESS_RESP_STR);
	  if(p != NULL){
			int i = 0;
			p += strlen(CMD_SUCCESS_RESP_STR);
			while(*p != '\r' || i > 10){
				partId[i++] = *p++; 
			}
			if(i > 10){
				pc.printf("read partId failed\r\n");
				return RET_CODE_ERROR;
			}
			return RET_CODE_SUCCESS;
		}else{
			return RET_CODE_ERROR;
		}
	}
}

int ISP_unlock(void)
{
	memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
	isp.puts(UNLOCK_CMD_STR);
	isp.puts(ENDCODE_STR);
	if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
		return RET_CODE_RESP_TIMEOUT;
	}else{
		char *p = strstr(ispRecvBuf,CMD_SUCCESS_RESP_STR);
		return ((p != NULL)?RET_CODE_SUCCESS:RET_CODE_ERROR);
	}
}

int ISP_disableEcho(void)
{
	isp.puts(DISABLE_ECHO_CMD_STR);
	if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
		return RET_CODE_RESP_TIMEOUT;
	}else{
		char *p = strstr(ispRecvBuf,CMD_SUCCESS_RESP_STR);
		if(p != NULL)
			return RET_CODE_SUCCESS;
		else
			return RET_CODE_ERROR;
	}	
}

int ISP_sectorOperation(int select,int start,int end)
{
	if(start >=0 && end <= SECTOR_NUM - 1){
		char cmdStr[8];
		int timeout;
		if(select == ISP_CMD_PREPARE_SECTORS){
			sprintf(cmdStr,"%c %d %d\r\n",isp_cmd_list[ISP_CMD_PREPARE_SECTORS],start,end);
			timeout = 500;
	  }else if(select == ISP_CMD_ERASE_SECTORS){
			sprintf(cmdStr,"%c %d %d\r\n",isp_cmd_list[ISP_CMD_ERASE_SECTORS],start,end);
			timeout = 120000; //120 ms
		}else{
			return RET_CODE_PARAM_ILLEGAL;
		}
		memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
		isp.puts(cmdStr);
		if(ISP_getResp(ispRecvBuf,timeout) <= 0){
			return RET_CODE_RESP_TIMEOUT;
		}else{
			char *p = strstr(ispRecvBuf,CMD_SUCCESS_RESP_STR);
			return ((p != NULL)?RET_CODE_SUCCESS:RET_CODE_ERROR);
		}
	}else{
		return RET_CODE_PARAM_ILLEGAL;
	}
}

int ISP_EraseSector(int start,int end)
{
	int ret;
	ret = ISP_sectorOperation(ISP_CMD_PREPARE_SECTORS,start,end);
	if(ret == RET_CODE_SUCCESS){
		ret = ISP_sectorOperation(ISP_CMD_ERASE_SECTORS,start,end);
		return ret;
	}else{
		return ret;
	}
}

int ISP_WriteToRAM(uint32_t start_addr,uint32_t size,char *data)
{
	if(start_addr % 32 ==0 && size % 4 == 0 && size > 0 && size <= ISP_MAX_SIZE_WRITE && data != NULL){ //address must be a word boundary,size must be multiple of 4
		char cmdStr[32];
		sprintf(cmdStr,"%c %d %d\r\n",isp_cmd_list[ISP_CMD_WRITE_TO_RAM],start_addr,size);
		pc.printf("write cmd=%s\r\n",cmdStr);
		memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
		isp.puts(cmdStr);
		if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
			return RET_CODE_RESP_TIMEOUT;
		}else{
			char *p = strstr(ispRecvBuf,CMD_SUCCESS_RESP_STR);
			if(p != NULL){
				int num = size/45,extra = size%45,checksum = 0;
				num += (extra==0?0:1);
				char *encode = (char*)malloc(64);
				char checksumStr[8];
				if(encode == NULL)
						return RET_CODE_MEM_OVERFLOW;
				//send data that was encoded
				for(int i=0;i<num;i++){
					if(i < num-1)
						UUencodeLine(data+45*i,encode,45);
					else
						UUencodeLine(data+45*i,encode,extra);
					isp.puts(encode);
					wait_ms(10);  //may be unnecessary
				}
				/* send checksum that is the sum of data to be sent */
				for(int i=0;i<size;i++)
					checksum += data[i];
				sprintf(checksumStr,"%d\r\n",checksum);
				pc.printf("checksum=%d\r\n",checksum);
				memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
				isp.puts(checksumStr);
				if(ISP_getResp(ispRecvBuf,1000) <= 0){
					return RET_CODE_RESP_TIMEOUT;
				}else{
					char *p = strstr(ispRecvBuf,OK_STR);
					return ((p != NULL)?RET_CODE_SUCCESS:RET_CODE_ERROR);
				}
			}else{
				return RET_CODE_ERROR;
			}
		}
	}else{
		return RET_CODE_PARAM_ILLEGAL;
	}
}




int ISP_copyToFlash(uint32_t dst,uint32_t src,uint16_t size)
{
	if(dst%256 == 0
	&& (size == 256 || size == 512 || size == 1024 || size == 4096)){
		int ret;
		ret = ISP_sectorOperation(ISP_CMD_PREPARE_SECTORS,0,SECTOR_NUM-1);
		if(ret == RET_CODE_SUCCESS){
			char cmdStr[36];
			sprintf(cmdStr,"%c %d %d %d\r\n",isp_cmd_list[ISP_CMD_COPY_RAM_TO_FLASH],dst,src,size);
			pc.printf("cmd str:%s\r\n",cmdStr);
			memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
			isp.puts(cmdStr);
			if(ISP_getResp(ispRecvBuf,10000) <= 0){
				return RET_CODE_RESP_TIMEOUT;
			}else{
				char *p = strstr(ispRecvBuf,CMD_SUCCESS_RESP_STR);
				return ((p!=NULL)?RET_CODE_SUCCESS:RET_CODE_ERROR);
			}		
		}else{
			return ret;
		}
	}else{
		return RET_CODE_PARAM_ILLEGAL;
	}
}