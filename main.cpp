#include "mbed.h"
#include "program.h"
#include "LPC1125_FlashMap.h"
#include "cJSON.h"
//#include "C12832.h"
#include "lib_crc16.h"

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
//C12832 lcd(D11,D13,D12,D7,D10);

/*sw2 irq handler */
void sw2_release()
{
	led = !led;
}

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
	
	ISP_syncBaudRate();
	if(ISP_getPartID(partID) > 0){
		pc.printf("partId:%s\r\n",partID);
	}
	
	while(1){
	}
}