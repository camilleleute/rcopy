// Client side - UDP Code				    
// By Hugh Smith	4/1/2017		

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include "pollLib.h"
#include "buffer.h"


#define MAXBUF 1407
#define RR 5
#define SREJ 6
#define SFLNM 8
#define RFLNM 9
#define EOFF 10
#define DPACK 16
#define ST_RECVDATA 0
#define ST_FILENAME 1
#define ST_INORDER 2
#define ST_BUFFER 3
#define ST_FLUSH 4
#define ST_EOF 5


void talkToServer(int socketNum, struct sockaddr_in6 * server, char * argv[]);
void filenameExchangePacket(char* argv[], struct sockaddr_in6 * server, int socketNum);
void createPDU(uint8_t sendBuf[], uint8_t flag, uint8_t buffer[], uint16_t bufSize);
int readFromStdin(char * buffer);
int checkArgs(int argc, char * argv[]);
int check_window_size(char * size);
int check_buffer_size(char * size);
int check_filename_length(char * filename, char * fromORto);
int check_error_rate(char * rate);
FILE * check_filename(char * filename);
void printBufferInHex(const uint8_t *buffer, size_t length);
void inOrderData(int socketNum, struct sockaddr_in6 * server, uint8_t * writingBuffer, uint16_t messageLen);
void flushingBuffer(int socketNum, struct sockaddr_in6 *server, uint8_t recvDataBuffer[], int messageLen);
uint32_t inOrderPacketCheck (uint8_t recvDataBuffer[]);
void receivingData(uint8_t recvDataBuffer[], int messageLen, struct sockaddr_in6 *server, socklen_t servAddrLen);
void bufferingData(int socketNum, struct sockaddr_in6 * server, uint8_t recvDataBuffer[], uint16_t messageLen);
uint8_t filenameExchange(char* argv[], int socketNum, struct sockaddr_in6 * server, socklen_t servAddrLen);

// global variables
uint32_t seq_num = 0;
ReceiverBuffer* receiverBuffer = NULL;
FILE * to_filename = NULL;



int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	
	int argFlag = checkArgs(argc, argv);
	if (argFlag) {
		exit(1);
	}
	portNumber = atoi(argv[7]);

	socketNum = setupUdpClientToServer(&server, argv[6], portNumber);
	
	talkToServer(socketNum, &server, argv);
	
	close(socketNum);

	return 0;
}


void talkToServer(int socketNum, struct sockaddr_in6 * server, char* argv[])
{
	sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
	setupPollSet();
	addToPollSet(socketNum);
	socklen_t servAddrLen = sizeof(server);
	uint32_t state = ST_FILENAME;
	uint32_t buffer_size = atoi(argv[4]) +7;
	uint8_t recvDataBuffer[buffer_size];
	int messageLen = 0;


	while (1) {
		switch(state) {
			case ST_RECVDATA: // receiving data
				receivingData(recvDataBuffer, messageLen, server, servAddrLen);
				state = inOrderPacketCheck(recvDataBuffer);
				if (state == 0) {
					state = ST_INORDER;
				} else {
					state = ST_BUFFER;
				}
			break;
			case ST_FILENAME: // filename exchange
				state = filenameExchange(argv, socketNum, server, servAddrLen);
				break;
			case ST_INORDER: // inorder
				inOrderData(socketNum, server, recvDataBuffer, messageLen);
				state = ST_RECVDATA;
				break;
			case ST_BUFFER: // buffering
				bufferingData(socketNum, server, recvDataBuffer, messageLen);
				receivingData(recvDataBuffer, messageLen, server, servAddrLen);
				state = inOrderPacketCheck(recvDataBuffer);
				if (state == 0) {
					state = ST_FLUSH;
				} else {
					state = ST_BUFFER;
				}
				break;
			case ST_FLUSH: // flushing
				flushingBuffer(socketNum, server, recvDataBuffer, messageLen);
				state = ST_RECVDATA;
				break;
			default: // mystery!
				printf("Something broke prolly\n");
				break;
		}
	}
}

void flushingBuffer(int socketNum, struct sockaddr_in6 *server, uint8_t recvDataBuffer[], int messageLen) {
	// Write the received in-order data to disk
    inOrderData(socketNum, server, recvDataBuffer, messageLen);

	while (is_expected_packet_received(receiverBuffer)) {
		int data_size;
    	const char *fetched_data = fetch_data_from_buffer(receiverBuffer, &data_size);

		if (fetched_data == NULL) {
			printf("Oops!\n");
		} else {
			inOrderData(socketNum, server, (uint8_t *)fetched_data, data_size);
		}
	}
	
}

uint32_t inOrderPacketCheck (uint8_t recvDataBuffer[]) {
	uint32_t actualNW = 0;
	memcpy(&actualNW, recvDataBuffer, 4);
	uint32_t actualHOST = ntohl(actualNW);

	if (actualHOST == receiverBuffer->expected) {
		return 0;
	} else {
		return actualHOST;
	}
}

void receivingData(uint8_t recvDataBuffer[], int messageLen, struct sockaddr_in6 *server, socklen_t servAddrLen){
	uint8_t count = 1;
	messageLen = 0;
	//printf("1. looking for segfault\n");
	do {
		int serverSocket = pollCall(1000);
		if (serverSocket == -1) {
			// #nodata #sad #:(
			count++;
			continue;
		} else {
			// #data #happy!
			memset(recvDataBuffer, 0, receiverBuffer->buffer_size);
			if ((messageLen = recvfrom(serverSocket, recvDataBuffer, receiverBuffer->buffer_size, 0, (struct sockaddr *)server, &servAddrLen)) < 0)
			{
				perror("recv call");
				exit(-1);
			}
			uint16_t calculatedChecksum = in_cksum((unsigned short *)recvDataBuffer, messageLen);
	
			if (calculatedChecksum) {
				printf("Checksum mismatch. Discarding packet.\n");
				count++;
				continue;
			} 
			break;
		}
	} while (count < 10);
	//printf("2. looking for segfault\n");

	if (count == 10) {
		printf("Data receiving timed out. Terminating.\n");
		exit(1);
	}
	return;
}

void bufferingData(int socketNum, struct sockaddr_in6 * server, uint8_t recvDataBuffer[], uint16_t messageLen) {
	// send SREJ for expected
	uint32_t net_expected = htonl(receiverBuffer->expected);
	uint8_t sendDataBuffer[receiverBuffer->buffer_size];
	createPDU(sendDataBuffer, SREJ, (uint8_t *)&net_expected, 4);
	int sent = sendtoErr(socketNum, sendDataBuffer, 11, 0, (struct sockaddr *)server, sizeof(*server));
	if (sent == -1) {
		printf("Bad things happened\n");
		exit(1);
	}

	// buffer received PDU
	uint32_t actualNW = 0;
	memcpy(&actualNW, recvDataBuffer, 4);
	uint32_t actualHOST = ntohl(actualNW);

	add_packet_to_buffer(receiverBuffer, actualHOST, (const char *)recvDataBuffer, messageLen);
	return;
}

void inOrderData(int socketNum, struct sockaddr_in6 * server, uint8_t * writingBuffer, uint16_t messageLen){
	// send RR
	uint8_t flag = 0;
	memcpy(&flag, writingBuffer + 6, 1);
	//printBufferInHex(writingBuffer, messageLen);
	//printf("1.looking for segfault\n");
	(receiverBuffer->expected)++;
	uint32_t net_expected = htonl(receiverBuffer->expected);
	uint8_t sendDataBuffer[11];

	if (flag != EOFF) {
		// write to disk
		fwrite((const void *)writingBuffer+7, 1, messageLen-7, to_filename);
		createPDU(sendDataBuffer, RR, (uint8_t *)&net_expected, 4);

	} else {
		createPDU(sendDataBuffer, EOFF, (uint8_t *)&net_expected, 4);
	}

	int sent = sendtoErr(socketNum, sendDataBuffer, 11, 0, (struct sockaddr *)server, sizeof(*server));
	if (sent == -1) {
		printf("Bad things happened\n");
		exit(1);
	}

	if (flag == EOFF) {
		printf("Got EOF, ACKing then dying\n");
		exit(1);
	}

	return;
}


uint8_t filenameExchange(char* argv[], int socketNum, struct sockaddr_in6 * server, socklen_t servAddrLen) {
	// filename exchange
	uint8_t count = 0;
	uint8_t recvBuffer[MAXBUF];
	int serverSocket = 0;
	int messageLen = 0;

	do {
		filenameExchangePacket(argv, server, socketNum);
		serverSocket = pollCall(1000); 

			if (serverSocket == -1) {
				close(socketNum);
				removeFromPollSet(socketNum);
				socketNum = setupUdpClientToServer(server, argv[6], atoi(argv[7]));
				addToPollSet(socketNum);
				count++;

			} else {
				if ((messageLen = recvfrom(serverSocket, recvBuffer, MAXBUF, 0, (struct sockaddr *)server, &servAddrLen)) < 0)
				{
					perror("recv call");
					exit(-1);
				}

				uint16_t calculatedChecksum = in_cksum((unsigned short *)recvBuffer, messageLen);

				if (calculatedChecksum) {
					printf("Checksum mismatch. Discarding packet.\n");
					count++;
					continue;
				}
				break;
			}
	} while (count < 10);

	if (count == 10) {
		printf("Failed to send filename. Terminating.\n");
		exit(1);
	}

	uint8_t flag = 0;
	memcpy(&flag, recvBuffer + 6, 1);

	// response to filename packet
	if (flag == 33) {// TODO: update flag = 33 is bad from-filename
		printf("Error: file: %s not found.\n", argv[1]);
		exit(1);
	} else {
		to_filename = check_filename(argv[1]);
		uint16_t buffer_size = atoi(argv[4]) + 7;
		uint32_t window_size = atoi(argv[3]);
		receiverBuffer = create_receiver_buffer(window_size, buffer_size);
		if (flag == 9) {
			printf("File OK!\n");
			//printf("returning value: %d\n", ST_RECVDATA);
			return ST_RECVDATA;
		} else if (flag == 16) {
			uint8_t inOrder = inOrderPacketCheck(recvBuffer);
			if (inOrder == 0) {
				inOrderData(socketNum, server, recvBuffer, messageLen);
			} else {
				bufferingData(socketNum, server, recvBuffer, messageLen);
			}
			return ST_RECVDATA;
		} else if (flag == EOFF) {
			printf("Got an EOF off the bat\n");
			return ST_RECVDATA;  // Handle EOF appropriately
		} else {
			printf("Unexpected flag: %d\n", flag);
			return ST_RECVDATA;  // Default to ST_RECVDATA
		}
	}
	return ST_RECVDATA; 
}

void filenameExchangePacket(char* argv[], struct sockaddr_in6 * server, int socketNum) {
	uint32_t window_size = atoi(argv[3]);
	uint16_t buffer_size = atoi(argv[4]);
	char from_filename[101];
	strcpy(from_filename, argv[1]);
	uint8_t filename_size = strlen(from_filename);
	uint8_t filenamePacket[107];
	memcpy(filenamePacket, &window_size, 4);
	memcpy(filenamePacket+4, &buffer_size, 2);
	memcpy(filenamePacket+6, from_filename, filename_size + 1);

	uint8_t sendBuf[MAXBUF];
	filename_size += 7;

	createPDU(sendBuf, SFLNM, filenamePacket, filename_size);
	//printBufferInHex(sendBuf, filename_size+7);
	
	int sent = sendtoErr(socketNum, sendBuf, filename_size+7, 0, (struct sockaddr *)server, sizeof(*server));
	if (sent <= 0)
	{
		perror("send call");
		exit(-1);
	}
}

void printBufferInHex(const uint8_t *buffer, size_t length) {
	size_t i = 0;
    for (i = 0; i < length; i++) {
        printf("%02X ", buffer[i]); // Print each byte as a 2-digit hexadecimal number
        if ((i + 1) % 16 == 0) {   // Print a newline every 16 bytes for readability
            printf("\n");
        }
    }
    printf("\n"); // Ensure the output ends with a newline
}

void createPDU(uint8_t sendBuf[], uint8_t flag, uint8_t buffer[], uint16_t bufSize) {
	uint32_t seq_num_NW = htonl(seq_num);
	memcpy(sendBuf, &seq_num_NW, 4);
	memset(sendBuf + 4, 0, 2);
	sendBuf[6] = flag;
	memcpy(sendBuf+7, buffer, bufSize);
	uint16_t checksum = in_cksum((unsigned short *)sendBuf, bufSize + 7);
	memcpy(sendBuf + 4, &checksum, 2);

	checksum = in_cksum((unsigned short *)sendBuf, bufSize + 7);
	if (checksum != 0) {
		printf("Checksum calculated incorrectly\n");
	}


	seq_num++;
}

FILE * check_filename(char * filename) {
	FILE* file_pointer = fopen(filename, "wb");
	if (file_pointer == NULL) {
		perror("Error on open of output file: %s\n");
		exit(1);
	}
	return file_pointer;
	
}



int checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 8)
	{
		printf("Usage: %s from-filename to-filename window-size buffer-size error-rate remote-machine remote-port\n", argv[0]);
		exit(1);
	}

	uint8_t wrong_input = 0;

	wrong_input = check_filename_length(argv[1], "from-filename");
	wrong_input = check_filename_length(argv[2], "to-filename");
	wrong_input = check_window_size(argv[3]);
	wrong_input = check_buffer_size(argv[4]);
	wrong_input = check_error_rate(argv[5]);
		
	return wrong_input;
}

int check_window_size(char * size){
	uint32_t window_size = atoi(size);
	if ((window_size < 1) || (window_size >= 1073741824)) {
		printf("window size is out of range. please input an amount between 1 and 1073741823\n");
		return 1;
	}
	return 0;
}

int check_buffer_size(char * size) {
	uint16_t buffer_size = atoi(size);
	if ((buffer_size < 1) || (buffer_size > 1400)) {
		printf("buffer size is out of range. please input an amount between 1 and 1400.\n");
		return 1;
	}
	return 0;
}


int check_filename_length(char * filename, char * fromORto) {
	if ((strlen(filename) > 100)) {
		printf("%s exceeds maximum filename length of 100 characters\n", fromORto);
		return 1;
	}
	return 0;
}

int check_error_rate(char * rate) {
	uint8_t error_rate = atof(rate);
	if ((error_rate < 0) || (error_rate > 1)){
		printf("error-rate is out of range. please input a rate between 0 and 1.\n");
		return 1;
	}
	return 0;
}





