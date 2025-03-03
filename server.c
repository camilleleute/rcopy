/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include "pollLib.h"


#define MAXBUF 1407

void processClient(int socketNum);
int filenamePacketCheck(int messageLen, uint8_t buff[], char filename[], uint32_t window_size, uint16_t buffer_size);
int checkArgs(int argc, char *argv[]);
FILE * check_filename(char * filename);
void createPDU(uint8_t sendBuf[], uint32_t seq_num, uint8_t flag, uint8_t buffer[], uint16_t bufSize);

int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
		
	socketNum = udpServerSetup(portNumber);

	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

void processClient(int socketNum)
{
	struct sockaddr_in6 client;		// Supports 4 and 6 but requires IPv6 struct
	socklen_t addrLen;
	uint8_t recvBuff[MAXBUF];

	while (1) {
		addrLen = sizeof(client);
		int messageLen = 0;
		if ((messageLen = recvfrom(socketNum, recvBuff, MAXBUF, 0, (struct sockaddr *)&client, &addrLen)) < 0)
		{
			perror("recv call");
			continue;
		}

		// check filename packet validity and the from-filename
		char filename[101];
		uint32_t window_size = 0;
		uint16_t buffer_size = 0;
		int valid = filenamePacketCheck(messageLen, recvBuff, filename, window_size, buffer_size);
		if (valid == 1) {
			printf("Invalid filename packet.\n");
			continue;
		} else if (valid == 2) {
			// filename doesnt exist.
			uint8_t smallBuf[1]; 
			uint8_t sendBuff[MAXBUF];
			memset(smallBuf, 0, 1);
			createPDU(sendBuff, 1, 33, smallBuf, 1);
			int sent = sendtoErr(socketNum, sendBuff, 8, 0, (struct sockaddr *)&client, sizeof(client));
			if (sent <= 0)
			{
				perror("send call");
				exit(-1);
			}
			continue;

		} else {
			// good filename packet
			char messageBuf[256]; 
			int size = snprintf(messageBuf, sizeof(messageBuf), "file OK");
			uint8_t sendBuf[MAXBUF];

			createPDU(sendBuf, 1, 9, (uint8_t *)messageBuf, size +1);
			int sent = sendtoErr(socketNum, sendBuf, size + 8, 0, (struct sockaddr *)&client, sizeof(client));
			if (sent <= 0)
			{
				perror("send call");
				exit(-1);
			}
			//fork
		}


	}

}

int filenamePacketCheck(int messageLen, uint8_t buff[], char filename[], uint32_t window_size, uint16_t buffer_size) {
	uint16_t checksum = in_cksum((unsigned short *)buff, messageLen);
	uint8_t flag;
	memcpy(&flag, buff+6, 1);
	if ((flag != 8) || (checksum != 0)) {
		return 1;
	} else {
		strcpy(filename, (const char *)(buff + 13));
		FILE * to_filename = check_filename(filename);

		if (to_filename == NULL) {
			return 2;
		}

		memcpy(&window_size, buff+7, 4);
		memcpy(&buffer_size, buff+11, 2);
		return 0;
	}
	

}

FILE * check_filename(char * filename) {
	FILE* file_pointer = fopen(filename, "r");
	return file_pointer;
}

void createPDU(uint8_t sendBuf[], uint32_t seq_num, uint8_t flag, uint8_t buffer[], uint16_t bufSize) {
    uint32_t seq_num_NW = htonl(seq_num);
    memcpy(sendBuf, &seq_num_NW, 4);
    memset(sendBuf + 4, 0, 2); 
    memcpy(sendBuf + 6, &flag, 1);
    memcpy(sendBuf + 7, buffer, bufSize);
    uint16_t checksum = in_cksum((unsigned short *)sendBuf, bufSize + 7);
    memcpy(sendBuf + 4, &checksum, 2); 
}


int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if ((argc < 2) || (argc > 3))
	{
		printf("Usage %s error-rate [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}
	
	return portNumber;
}


