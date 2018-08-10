#ifndef _NETWORK_H
#define _NETWORK_H

#include "mbed.h"
#include "EthernetInterface.h"
#include "UserConfig.h"

#if SERVER_LOCAL
#define  DEFAULT_IP_ADDR          "192.168.1.100"
#define  DEFAULT_NETMASK          "255.255.255.0"
#define  DEFAULT_GATEWAY          "192.168.1.1"

#define  DEFAULT_SERVER_IP        "192.168.1.100"
#define  DEFAULT_SERVER_PORT      11111
#else
#define  DEFAULT_SERVER_IP        "119.23.18.135"
#define  DEFAULT_SERVER_PORT      11111
#endif

#define  SOCKET_IN_BUFFER_SIZE    (512)
#define  SOCKET_OUT_BUFFER_SIZE   (512)

#define  REQ_AUTH                "{\"apiId\":%d,\"mac\":\"%s\",\"reconnect\":%d}"
#define  REQ_GET_UID             "{\"apiId\":%d,\"customerCode\":\"%s\"}"
#define  REQ_NOTIFY_RESULT       "{\"apiId\":%d,\"UID\":\"%s\",\"result\":%d}"
#define  REQ_HEARTBEAT           "{\"apiId\":%d}"

#define  MSG_RESEND_PERIOD_S       (5)

typedef enum MSG_ID{
	MSG_ID_INVALID = 0,
	MSG_ID_AUTH,
	MSG_ID_GET_UID,
	MSG_ID_NOTIFY,
}MSG_ID_T;

typedef enum API_ID{
	API_ID_AUTH = 0,
	API_ID_HEARTBEAT,
	API_ID_GET_UID,
	API_ID_NOTIFY_PROGRAM_UID_RESULT,
}API_ID_T;

typedef enum RESP_CODE{
	RESP_CODE_DEFAULT =0,
	RESP_CODE_ERROR = 4,
	RESP_CODE_SUCCESS = 100,
}RESP_CODE_T;

typedef struct NETWORT_EVENT{
	bool authorizeReqFlag;
	bool heartbeatFlag;
	bool uidReqFlag;
	bool notifyResultFlag;
	bool offlineFlag;
	bool firstConnectFlag;
	bool uidProgramSuccessFlag;
	bool authorizePassFlag;
}NETWORK_EVENT_T;

typedef struct SOCKET_INFO{
	char inBuffer[SOCKET_IN_BUFFER_SIZE];
	char outBuffer[SOCKET_OUT_BUFFER_SIZE];
}SOCKET_INFO_T;

typedef struct MSG_HANDLE{
	MSG_ID_T msgSendId;
	MSG_ID_T msgRecvId;
	RESP_CODE_T sendRespCode;
	RESP_CODE_T recvRespCode;
}MSG_HANDLE_T;

#endif