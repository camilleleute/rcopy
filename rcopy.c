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


#define MAXBUF 1407

void talkToServer(int socketNum, struct sockaddr_in6 * server, char * argv[]);
void filenameExchangePacket(char* argv[], struct sockaddr_in6 * server, int socketNum);
void createPDU(uint8_t sendBuf[], uint32_t seq_num, uint8_t flag, uint8_t buffer[], uint16_t bufSize);
int readFromStdin(char * buffer);
int checkArgs(int argc, char * argv[]);
int check_window_size(char * size);
int check_buffer_size(char * size);
int check_filename_length(char * filename, char * fromORto);
int check_error_rate(char * rate);
FILE * check_filename(char * filename);
void printBufferInHex(const uint8_t *buffer, size_t length);


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
	FILE * to_filename = NULL;

	filenameExchange(argv, socketNum, server, servAddrLen, to_filename);

	receiveData();
	
	// now onto use / data receiving
}

void receiveData(int socketNum, struct sockaddr_in6 * server, FILE* to_filename, char*argv[]){
	uint8_t count = 0;
	uint32_t window_size = atoi(argv[3]);
	uint16_t buffer_size + 7 = atoi(argv[4]);
	ReceiverBuffer* myBuffer = create_receiver_buffer(buffer_size);
	uint8_t dataBuffer[buffer_size];
	uint32_t highest = 1;
	uint32_t expected = 1;

	while (count < 10) {
		int serverSocket = pollCall(1000);
		if (serverSocket == -1) {
			// #nodata #sad #:(
			count++;
			continue;
		} else {
			// #data #happy!
			int messageLen = 0;
			if ((messageLen = recvfrom(serverSocket, dataBuffer, MAXBUF, 0, (struct sockaddr *)server, &servAddrLen)) < 0)
			{
				perror("recv call");
				exit(-1);
			}
			uint16_t calculatedChecksum = in_cksum((unsigned short *)dataBuffer, messageLen);
	
			if (calculatedChecksum) {
				printf("Checksum mismatch. Discarding packet.\n");
				count++;
				continue;
			} else {
				uint32_t actual = 1;
				uint8_t writingBuffer[buffer_size-7];
				uint8_t flag = 0;

				memcpy(&flag, dataBuffer + 6, 1);
				memcpy(&actual, dataBuffer, 4);
				memcpy(writingBuffer, dataBuffer+7, messageLen-7);
				int state = 0;

				if (actual == expected) {
					state = 1;
				} else if (actual > expected) {
					state = 2;
				}

				switch (state) {
					case 1:		// inorder
						break;
					case 2:		// buffering
						break;
					case 3:		// flushing
				}

				if (actual == expected) {
					// in order state
					size_t bytesWritten = fwrite(writingBuffer, 1, messageLen-7, to_filename);
					highest = expected;
					expected++;
					// send RR

				} else if (actual > expected) {
					// buffering state
					// send SREJ for expected
					// buffer received PDU
					//highest = whatever I just got

					// stay in buffering til u get what u want. then go to flushing state

				}



				
			}
			break;
		}
	}

	if (count == 10) {
		printf("Data receiving timed out. Terminating.\n");
		exit(1);
	}




	
	if (flag == 16) {
		// Regular data packet
	} else if (flag == 17) {
		// Resent data packet (after sender receiving a SREJ, not a timeout)

	} else if (flag == 18) {
		// Resent data packet after a timeout (so lowest in window resent data packet.)

	} else if (flag == 10) {
		// Packet is your EOF indication or is the last data packet (sender to receiver)
	} else {
		// random flag. ignore this packet
	}
}



void filenameExchange(char* argv[], int socketNum, struct sockaddr_in6 * server, socklen_t servAddrLen, FILE * to_filename) {
		// filename exchange
		uint8_t count = 1;
		uint8_t recvBuffer[MAXBUF];
		int serverSocket = 0;

		filenameExchangePacket(argv, server, socketNum);
		while (count < 10) {
			serverSocket = pollCall(1000); 
				if (serverSocket == -1) {
					close(socketNum);
					removeFromPollSet(socketNum);
					int newSocketNum = setupUdpClientToServer(server, argv[6], atoi(argv[7]));
					addToPollSet(newSocketNum);
					filenameExchangePacket(argv, server, newSocketNum);
					count++;
				}
				else {
					int messageLen = 0;
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
			
		}
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
		}
	
		// if filename good
		to_filename = check_filename(argv[1]);
		printf("File OK!\n");
		return;
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

	createPDU(sendBuf, 1, 8, filenamePacket, filename_size);
	
	int sent = sendtoErr(socketNum, sendBuf, filename_size+7, 0, (struct sockaddr *)server, sizeof(*server));
	if (sent <= 0)
	{
		perror("send call");
		exit(-1);
	}
}

void printBufferInHex(const uint8_t *buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", buffer[i]); // Print each byte as a 2-digit hexadecimal number
        if ((i + 1) % 16 == 0) {   // Print a newline every 16 bytes for readability
            printf("\n");
        }
    }
    printf("\n"); // Ensure the output ends with a newline
}

void createPDU(uint8_t sendBuf[], uint32_t seq_num, uint8_t flag, uint8_t buffer[], uint16_t bufSize) {
	uint32_t seq_num_NW = htonl(seq_num);
	memcpy(sendBuf, &seq_num_NW, 4);
	memset(sendBuf + 4, 0, 2);
	sendBuf[7] = flag;
	memcpy(sendBuf+7, buffer, bufSize);
	uint16_t checksum = in_cksum((unsigned short *)sendBuf, bufSize + 7);
	memcpy(sendBuf + 4, &checksum, 2);
}

FILE * check_filename(char * filename) {
	FILE* file_pointer = fopen(filename, "w");
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





