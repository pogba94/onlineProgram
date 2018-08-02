#ifndef _LPC1125_FLASHMAP_H
#define _LPC1125_FLASHMAP_H

#define  SECTOR_NUM                       16  
#define  SECTOR_SIZE                      4096
#define  FLASH_SIZE                       (SECTOR_NUM*SECTOR_SIZE)
#define  FLASH_START_ADDRESS              0x0
#define  FLASH_END_ADDRESS                (FLASH_START_ADDRESS + FLASH_SIZE - 1)
#define  RAM_START_ADDRESS                0x10000000
#define  RAM_SIZE                         8192
#define  RAM_END_ADDRESS                  (RAM_START_ADDRESS + RAM_SIZE - 1)

#define  CHECK_RAM_ADDR(x)        				(x>=RAM_START_ADDRESS&&x<=RAM_END_ADDRESS)
#define  CHECK_FLASH_ADDR(x)              (x>=FLASH_START_ADDRESS&&x<=FLASH_END_ADDRESS)

#endif