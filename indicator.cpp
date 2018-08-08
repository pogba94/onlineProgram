#include "indicator.h"
#include "mbed.h"

extern DigitalOut blue;
extern DigitalOut green;
extern DigitalOut red;

#define  BLUE_LED_ON              (blue=0)
#define  BLUE_LED_OFF             (blue=1)
#define  BLUE_LED_TOGGLE          (blue = !blue)
#define  RED_LED_ON               (red=0)
#define  RED_LED_OFF              (red=1)
#define  RED_LED_TOGGLE           (red = !red)
#define  GREEN_LED_ON             (green=0)
#define  GREEN_LED_OFF            (green=1)
#define  GREEN_LED_TOGGLE         (green = !green)

void Indicator_reset(void)
{
	RED_LED_OFF;
	GREEN_LED_OFF;
	BLUE_LED_OFF;
}

void Indicator_noBin(void)
{
	BLUE_LED_OFF;
	GREEN_LED_OFF;
	RED_LED_TOGGLE;
}

void Indicator_disconnected(void)
{
	GREEN_LED_OFF;
	RED_LED_OFF;
	BLUE_LED_TOGGLE;
}

void Indicator_connected(void)
{
	RED_LED_OFF;
	GREEN_LED_OFF;
	BLUE_LED_ON;
}

void Indicator_programing(void)
{
	BLUE_LED_OFF;
	RED_LED_OFF;
	GREEN_LED_TOGGLE;
}

void Indicator_programDone(void)
{	
	BLUE_LED_OFF;
	RED_LED_OFF;
	GREEN_LED_ON;
}

void Indicator_programFail(void)
{
	BLUE_LED_OFF;
	GREEN_LED_OFF;
	RED_LED_ON;
}
