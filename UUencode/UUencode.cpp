#include "UUencode.h"
#include "mbed.h"

#define  UU_ENCODE_BYTE(a)      (((a)==0)?0x60:((a)+0x20))
#define  UU_DECODE_BYTE(a)      (((a)==0)?0x60:((a)-0x20))
#define  UU_ENCODE_LEN_BYTE(a)  ((a)+0x20)
#define  UU_DECODE_LEN_BYTE(a)  ((a)-0x20)

extern Serial pc;

/* UU-code encode and decode functions */

/**
*  @brief convert 3 assic string to 4 uu-encode string 
*/
static void UUencodeBase(const char in[],char out[])
{
	out[0] = UU_ENCODE_BYTE((in[0]&0xFC)>>2);
	out[1] = UU_ENCODE_BYTE(((in[0]&0x03)<<4) + ((in[1]&0xF0)>>4));
	out[2] = UU_ENCODE_BYTE(((in[1]&0x0F)<<2) + ((in[2]&0xC0)>>6));
	out[3] = UU_ENCODE_BYTE((in[2]&0x3F));
}
/**
*  @brief convert 4 uu-encode string to 3 assic string 
*/
static void UUdecodeBase(const char in[],char out[])
{
	out[0] = (UU_DECODE_BYTE(in[0])<<2) + ((UU_DECODE_BYTE(in[1])&0x30)>>4);
	out[1] = ((UU_DECODE_BYTE(in[1])&0x0F)<<4) + ((UU_DECODE_BYTE(in[2])&0x3C)>>2);
	out[2] = ((UU_DECODE_BYTE(in[2])&0x03)<<6) + ((UU_DECODE_BYTE(in[3])&0x3F));
}

/**
	@brief convert assic string top at 45 bytes to uu-encode 
				string top at 61 bytes include the first byte recording length
*/
int UUencodeLine(const char* input,char* output,uint8_t len)
{
	if(input == NULL || output == NULL || len == 0 || len >45)
		return -1;
	
	output[0] = UU_ENCODE_LEN_BYTE(len);
	int i,num;
	num = len/3 + ((len%3==0)?0:1);
	for(i=0;i<num;i++){
		if(i <= num-1){
			UUencodeBase(input+i*3,output+i*4+1);
		}else{
			int j = len%3;
			char str[3];
			if(j==0){
				strncpy(str,input+i*3,3);
			}else if(j == 1){
				str[0] = *(input+i*3);
				str[1] = str[2] = '0';
			}else{
				str[0] = *(input+3*i);
				str[1] = *(input+3*i+1);
				str[2] = '0';
			}
			UUencodeBase(str,output+i*4+1);
		}
	}
	output[num*4+1] = '\r';
	output[num*4+2] = '\n';
	output[num*4+3] = '\0';
	return 1;
}

/**
	@brief convert uu-encode string top at 61 bytes to assic 
				string top at 45 bytes
*/
int UUdecodeLine(const char* input,char* output)
{
	if(input == NULL || output == NULL)
		return -1;
	int len = input[0] - 32;
	if(len <= 0 || len > 45)//invalid encode
		return -2;
	int num = len/3 + ((len%3==0)?0:1);
	for(int i=0;i<num;i++){
		UUdecodeBase(input+4*i+1,output+3*i);
	}
	output[len] = '\0';
}

#define UU_TXT_SIZE     (44)

/**
	@brief test function for uu-encode
*/
void UUencodeTest(void)
{
	pc.printf("UUencode test!\r\n");
	char txt[64];
	char encode[64];
	char decode[64];
	for(int i=0;i<UU_TXT_SIZE;i++)
		txt[i] = i+32;
	txt[UU_TXT_SIZE] = '\0';
	UUencodeLine(txt,encode,UU_TXT_SIZE);
	pc.printf("before uu-encode:   %s\r\nafter uu-encode:   %s\r\n",txt,encode);
	UUdecodeLine(encode,decode);
	pc.printf("before uu-decode:   %s\r\nafter uu-decode:   %s\r\n",encode,decode);
}