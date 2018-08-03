#ifndef _UU_ENCODE_H
#define _UU_ENCODE_H

#include "mbed.h"

int UUencodeLine(const char* input,char* output,uint8_t len);
int UUdecodeLine(const char* input,char* output);
void UUencodeTest(void);

#endif
