#include "program.h"
#include "mbed.h"
#include "string.h"
#include "stdlib.h"

#define  ISP_RECV_BUF_SIZE            (1024)
#define  ISP_UART_RECV_TIMEOUT_US     (200)

char isp_cmd_list[] = {'U','B','A','W','R','P','C','G','E','I','J','K','M','N'};

extern Serial isp;
extern Serial pc;
char ispRecvBuf[ISP_RECV_BUF_SIZE];

int ISP_getResp(char *buf,uint32_t timeout_us)
{
	int count = 0,pStr = 0;
	if(NULL == buf)
		return -1;
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
							syncStatus = SYNC_STATUS_SYNCHRONIZED_COMFIRM;
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
						syncStatus = SYNC_STATUS_SYNC_CONFIRM;
					}else{
						syncStatus = SYNC_STATUS_SYNC_ERROR;
					}
				}
				break;
			case SYNC_STATUS_SYNC_CONFIRM:
				memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
			  char s[4];
				sprintf(s,"%d",ISP_CMD_SUCCESS);
				isp.puts(s);
			  isp.puts(ENDCODE_STR);
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
			case SYNC_STATUS_SYNCHRONIZED_COMFIRM:
				memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
				isp.putc('\27');  //ESC key
				isp.puts(ENDCODE_STR);
				if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
					syncStatus = SYNC_STATUS_SYNC_TIMEOUT;
				}else{
					pc.printf("recieve data:%s\r\n",ispRecvBuf);
					p = strstr(ispRecvBuf,"\27");
					if(p != NULL){
						syncStatus = SYNC_STATUS_SYNC_DONE;
					}else{
						syncStatus = SYNC_STATUS_SYNC_ERROR;
					}
				}
				break;
			case SYNC_STATUS_SYNC_DONE:
				pc.printf("Baudrate synchronize done!\r\n");
				return 1;
			case SYNC_STATUS_SYNC_TIMEOUT:
				pc.printf("Baudrate synchronize timeout!\r\n");
				return -1;
			case SYNC_STATUS_SYNC_ERROR:
				pc.printf("Baudrate synchronize error!\r\n");
				return -2;
			default:
				return 0;
		}
	}
}

int ISP_getPartID(char partId[])
{
	char *p;
	isp.putc(isp_cmd_list[ISP_CMD_READ_PART_ID]);
	isp.puts(ENDCODE_STR);
	memset(ispRecvBuf,0x0,sizeof(ispRecvBuf));
	if(ISP_getResp(ispRecvBuf,ISP_UART_RECV_TIMEOUT_US) <= 0){
		return 0;
	}else{
		char expStr[4];
		sprintf(expStr,"%d\r\n",ISP_CMD_SUCCESS);
		p = strstr(ispRecvBuf,expStr);
		if(p != NULL){
			int i = 0;
			p += 3;
			while(*p != '\r' || i > 10){
				partId[i++] = *p++; 
			}
			if(i > 10){
				pc.printf("read partId failed\r\n");
				return -1;
			}
			return 1;
		}
	}
}
