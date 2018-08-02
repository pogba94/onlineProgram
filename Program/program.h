#ifndef _PROGRAM_H
#define _PROGRAM_H

#include "mbed.h"

typedef enum ISP_RESULT_CODE{
	ISP_CMD_SUCCESS = 0,
	ISP_INVALID_COMMAND,
	ISP_SRC_ADDR_ERROR,
	ISP_DST_ADDR_ERROR,
	ISP_SRC_ADDR_NOT_MAPPED,
	ISP_DST_ADDR_NOT_MAPPED,
	ISP_COUNT_ERROR,
	ISP_INVALID_SECTOR,
	ISP_SECTOR_NOT_BLANK,
	ISP_SECTOR_NOT_PREPARED_FOR_WRITE_OPERATION,
	ISP_COMPARE_ERROR,
	ISP_BUSY,
	ISP_PARAM_ERROR,
	ISP_ADDR_ERROR,
	ISP_ADDR_NOT_MAPPED,
	ISP_CMD_LOCKED,
	ISP_INVALID_CODE,
	ISP_INVALID_BAUD_RATE,
	ISP_INVALID_STOP_BIT,
	ISP_CODE_READ_PROTECTION_ENABLED
}ISP_RESULT_CODE_T;


typedef enum BAUDRATE_SYNC_STATUS{
	SYNC_STATUS_SETUP_SYNC = 0,
	SYNC_STATUS_RESP_SYNC,
	SYNC_STATUS_SEND_FREQ,
	SYNC_STATUS_EXIT_SYNC,
	SYNC_STATUS_SYNC_DONE,
	SYNC_STATUS_SYNC_TIMEOUT,
	SYNC_STATUS_SYNC_ERROR
}BAUDRATE_SYNC_STATUS_T;

typedef enum ISP_CMD{
	ISP_CMD_UNLOCK = 0,
	ISP_CMD_SET_BAUDRATE,
	ISP_CMD_ECHO,
	ISP_CMD_WRITE_TO_RAM,
	ISP_CMD_READ_MEMORY,
	ISP_CMD_PREPARE_SECTORS,
	ISP_CMD_COPY_RAM_TO_FLASH,
	ISP_CMD_GO,
	ISP_CMD_ERASE_SECTORS,
	ISP_CMD_BLANK_CHECK_SECTORS,
	ISP_CMD_READ_PART_ID,
	ISP_CMD_READ_BOOT_CODE_VERSION,
	ISP_CMD_COMPARE,
	ISP_CMD_READ_UID
}ISP_CMD_T;

typedef enum RET_CODE
{
	RET_CODE_SUCCESS = 0,
	RET_CODE_ERROR = -1,
	RET_CODE_RESP_TIMEOUT = -2,
	RET_CODE_PARAM_ILLEGAL = -3,
	RET_CODE_MEM_OVERFLOW = -4,
	RET_CODE_WRITE_FAIL = -5
}RET_CODE_T;

#define   BAUDRATE_SYNC_SEND_STR          "?"
#define   BAUDRATE_SYNC_RECV_STR          "Synchronized"
#define   OK_STR                          "OK"
#define   ENDCODE_STR                     "\r\n"
#define   UNLOCK_CMD_STR                  "U 23130"
#define   DISABLE_ECHO_CMD_STR            "A 0\r\n"
#define   CMD_SUCCESS_RESP_STR            "0\r\n"

#define   ISP_MAX_SIZE_WRITE                     (512)
#define   ISP_MAX_SIZE_COPY                      (1024) 

int ISP_syncBaudRate(void);
int ISP_getPartID(char partId[]);
int ISP_unlock(void);
int ISP_disableEcho(void);
int ISP_sectorOperation(int select,int start,int end);
int ISP_EraseSector(int start,int end);
int ISP_WriteToRAM(uint32_t start_addr,uint32_t size,char *data);
int ISP_copyToFlash(uint32_t dst,uint32_t src,uint16_t size);
int ISP_readMemory(uint32_t addr,uint32_t size,char *data);
#endif
