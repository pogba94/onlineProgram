#ifndef _INDICATOR_H
#define _INDICATOR_H

#include "mbed.h"

void Indicator_reset(void);
void Indicator_noBin(void);
void Indicator_disconnected(void);
void Indicator_connected(void);
void Indicator_programing(void);
void Indicator_programDone(void);
void Indicator_programFail(void);
void Indicator_offline(void);
void Indicator_modeSwitch(void);
void Indicator_noAuth(void);
#endif