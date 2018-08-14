#ifndef _ONLINE_PROGRAM_H
#define _ONLINE_PROGRAM_H

typedef enum MODE{
	MODE_PROGRAM_CODE = 1,
	MODE_PROGRAM_UID = 2,
}MODE_T;

typedef enum DEV_STATUS{
	DEV_STATUS_BIN_NOT_FOUND=0,
	DEV_STATUS_DISCONNECTED,
	DEV_STATUS_CONNECTED,
	DEV_STATUS_PROGRAMING,
	DEV_STATUS_PROGRAM_SUCCESS,
	DEV_STATUS_PROGRAM_FAIL,
}DEV_STATUS_T;

typedef struct EVENT_FLAGS{
	bool binFileExsitFlag;
	bool boardConnectedFlag;
	bool startProgramFlag;
	bool resetDevStatusFlag;
	bool modeSwitchFlag;
	bool UIDavailableFlag;
	bool UIDProgramSuccessFlag;
	bool notifyDoneFlag;
}EVENT_FLAGS_T;

#endif