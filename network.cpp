#include "network.h"
#include "cJSON.h"
#include "OnlineProgram.h"

/* global variable */
EthernetInterface eth;
TCPSocketConnection tcpSock;
SOCKET_INFO_T socketInfo;
NETWORK_EVENT_T networkEvent;
MSG_HANDLE_T msgHandle = {MSG_ID_INVALID,MSG_ID_INVALID,RESP_CODE_DEFAULT,RESP_CODE_DEFAULT};
char DevMAC[16];
extern Serial pc;
extern char customerCode[];
extern char uid[36];
static uint32_t sendMsgTime = 0;
extern uint32_t systemTimer;
extern volatile EVENT_FLAGS_T eventFlags;

void initNetwork(void)
{
	uint8_t macAddr[6];
	#if SERVER_LOCAL
	eth.init(DEFAULT_IP_ADDR,DEFAULT_NETMASK,DEFAULT_GATEWAY);
	pc.printf("use static ip\r\n");
	#else
	eth.init();
	pc.printf("use dhcp!\r\n");
	#endif
	eth.connect();
	if(strcmp(eth.getIPAddress(),NULL) == NULL){
		pc.printf("RJ45 Error,system will be reset now!");
		NVIC_SystemReset();
	}
	sscanf(eth.getMACAddress(),"%02x:%02x:%02x:%02x:%02x:%02x:",
		&macAddr[0],&macAddr[1],&macAddr[2],&macAddr[3],&macAddr[4],&macAddr[5]);
	sprintf(DevMAC,"%02x%02x%02x%02x%02x%02x",macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
	pc.printf("ip:%s,mac:%s\r\n",eth.getIPAddress(),DevMAC);
	tcpSock.set_blocking(false,40);
}

void initNetworkEvent(void)
{
	networkEvent.authorizeReqFlag = false;
	networkEvent.heartbeatFlag = false;
	networkEvent.notifyResultFlag = false;
	networkEvent.offlineFlag = true;
	networkEvent.uidReqFlag = false;
	networkEvent.firstConnectFlag = true;
	networkEvent.authorizePassFlag = false;
}

void checkNetwork(void)
{
	static char heartbeat[32];
	if(networkEvent.offlineFlag == true){
		tcpSock.close();
		if(tcpSock.connect(DEFAULT_SERVER_IP,DEFAULT_SERVER_PORT) < 0){
			pc.printf("cannot connect to server\r\n");
		}else{
			if(networkEvent.firstConnectFlag == true){
				networkEvent.firstConnectFlag = false;
			}
			networkEvent.offlineFlag = false;
			networkEvent.authorizeReqFlag = true;
			pc.printf("connected to server\r\n");
		}
	}else{
		if(networkEvent.heartbeatFlag == true){
			networkEvent.heartbeatFlag = false;
			sprintf(heartbeat,REQ_HEARTBEAT,API_ID_HEARTBEAT);
			if(tcpSock.send(heartbeat,strlen(heartbeat))<0){
				pc.printf("send heartbeat failed!\r\n");
				networkEvent.offlineFlag = true;
				networkEvent.authorizePassFlag = false;
			}else{
				pc.printf("send heartbeat successfully!\r\n");
			}
		}
	}
}

static void sendMsgHandle(MSG_ID_T msgId)
{
	int len;
	switch((int)msgId){
		case MSG_ID_AUTH:
			sprintf(socketInfo.outBuffer,REQ_AUTH,API_ID_AUTH,DevMAC,!networkEvent.firstConnectFlag);
			break;
		case MSG_ID_GET_UID:
			sprintf(socketInfo.outBuffer,REQ_GET_UID,API_ID_GET_UID,customerCode);
			break;
		case MSG_ID_NOTIFY:
			sprintf(socketInfo.outBuffer,REQ_NOTIFY_RESULT,API_ID_NOTIFY_PROGRAM_UID_RESULT,uid,eventFlags.UIDProgramSuccessFlag);
			break;
		default:
			pc.printf("msgid illegal\r\n");
		  return;
	}
	len = strlen(socketInfo.outBuffer);
	if(tcpSock.send(socketInfo.outBuffer,len)>=0){
		pc.printf("send %d bytes,%s\r\n",len,socketInfo.outBuffer);
		msgHandle.msgSendId = msgId;
		sendMsgTime = systemTimer;
	}
}

static void parseRecvMsgInfo(char *text)
{
	cJSON *json;
	json = cJSON_Parse(text);
	if(!json){
		pc.printf("not json string!");
		return;
	}else{
		if(cJSON_GetObjectItem(json,"respCode") != NULL && cJSON_GetObjectItem(json,"apiId") != NULL){
			msgHandle.recvRespCode = (RESP_CODE_T)cJSON_GetObjectItem(json,"respCode")->valueint;
			int apiId = (API_ID_T)cJSON_GetObjectItem(json,"apiId")->valueint;
			switch(apiId){
				case API_ID_AUTH:
					msgHandle.msgRecvId = MSG_ID_AUTH;
					if(msgHandle.recvRespCode == RESP_CODE_SUCCESS){
						networkEvent.authorizePassFlag = true;
						pc.printf("authorize success\r\n");
					}else{
						pc.printf("authorize fail,respCode:%d\r\n",msgHandle.recvRespCode);
					}
					break;
				case API_ID_GET_UID:
					msgHandle.msgRecvId = MSG_ID_GET_UID;
					if(msgHandle.recvRespCode == RESP_CODE_SUCCESS && cJSON_GetObjectItem(json,"UID") != NULL){
						sprintf(uid,"%s",cJSON_GetObjectItem(json,"UID")->valuestring);
						eventFlags.UIDavailableFlag = true;//start to program uid to target board
						pc.printf("get uid successfully!UID:%s\r\n",uid);
					}else{
						pc.printf("get uid fail!\r\n");
					}
					break;
				case API_ID_NOTIFY_PROGRAM_UID_RESULT:
					msgHandle.msgRecvId = MSG_ID_NOTIFY;
					if(msgHandle.recvRespCode == RESP_CODE_SUCCESS){
						eventFlags.notifyDoneFlag = true;
						pc.printf("notify result successfully\r\n");
					}else{
						pc.printf("notify result failed\r\n");
					}
					break;
				default:;
			}
			if(msgHandle.msgSendId == msgHandle.msgRecvId && msgHandle.recvRespCode == RESP_CODE_SUCCESS){
				msgHandle.msgSendId = msgHandle.msgRecvId = MSG_ID_INVALID;
				msgHandle.recvRespCode = RESP_CODE_DEFAULT;
				pc.printf("send msg %d successfully\r\n",apiId);
			}
		}
	}
}

static void msgRecvHandle(void)
{
	int len;
	len = sizeof(socketInfo.inBuffer);
	memset(socketInfo.inBuffer,0x0,len);
	int n = tcpSock.receive(socketInfo.inBuffer,len);
	if(n>0){
		socketInfo.inBuffer[n] = '\0';
		pc.printf("recieve %d bytes,%s\r\n",n,socketInfo.inBuffer);
		parseRecvMsgInfo(socketInfo.inBuffer);
	}	
}

void msgTransceiverHandle(void)
{
	if(networkEvent.offlineFlag == false){
		if(msgHandle.msgSendId != MSG_ID_INVALID){
			if(systemTimer - sendMsgTime >= MSG_RESEND_PERIOD_S)
				sendMsgHandle(msgHandle.msgSendId);
		}else{
			if(networkEvent.authorizeReqFlag == true){
				msgHandle.msgSendId = MSG_ID_AUTH;
				networkEvent.authorizeReqFlag = false;
				sendMsgHandle(MSG_ID_AUTH);
				pc.printf("request authorization\r\n");
			}else if(networkEvent.uidReqFlag == true){
				msgHandle.msgSendId = MSG_ID_GET_UID;
				networkEvent.uidReqFlag = false;
				sendMsgHandle(MSG_ID_GET_UID);
				pc.printf("request uid\r\n");
			}else if(networkEvent.notifyResultFlag == true){
				msgHandle.msgSendId = MSG_ID_NOTIFY;
				networkEvent.notifyResultFlag = false;
				sendMsgHandle(MSG_ID_NOTIFY);
				pc.printf("notify result of programing uid\r\n");
			}
		}
		msgRecvHandle();
	}
}
