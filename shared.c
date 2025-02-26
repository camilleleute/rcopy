#include "shared.h"
#include "send.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAXBUF 1402

void sendHandle(int socketNum, char * handle, int flag) {
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   */
	
	sendLen = strlen(handle);
	sendBuf[0] = flag; 
	sendBuf[1] = sendLen;
	memcpy(&sendBuf[2], handle, sendLen);
	
	sent = sendPDU(socketNum, sendBuf, sendLen + 2);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}	
}
