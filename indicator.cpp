#include "indicator.h"
#include "mbed.h"

extern DigitalOut blue;
extern DigitalOut green;
extern DigitalOut red;

#define  BLUE_LED_ON              (blue=0)
#define  BLUE_LED_OFF             (blue=1)
#define  RED_LED_ON               (red=0)
#define  RED_LED_OFF              (red=1)
#define  GREEN_LED_ON             (green=0)
#define  GREEN_LED_OFF            (green=1)

void Indicator_reset(void)
{
	RED_LED_OFF;
	GREEN_LED_OFF;
	BLUE_LED_OFF;
}	

void Indicator_binExist(void)
{
	Indicator_reset();
	while(1){
		RED_LED_ON;
		wait(0.5);
		RED_LED_OFF;
		wait(0.5);
	}
}

void Indicator_noBin(void)
{
	Indicator_reset();
	while(1){
		RED_LED_ON;
		wait(1);
	}
}

void Indicator_Connected(void)
{
	Indicator_reset();
	while(1){
		BLUE_LED_ON;
		wait(0.5);
		BLUE_LED_OFF;
		wait(0.5);
	}
}

void Indicator_programing(void)
{
	Indicator_reset();
	while(1){
		BLUE_LED_ON;
		wait_ms(100);
		BLUE_LED_OFF;
		wait_ms(100);
	}
}
