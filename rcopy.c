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

#define MAXBUF 1407

void talkToServer(int socketNum, struct sockaddr_in6 * server);
int readFromStdin(char * buffer);
int checkArgs(int argc, char * argv[]);


int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
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
	int serverSocket = 0;

	// filename exchange
	uint8_t count = 1
	filenameExchangePacket(argv, &server, socketNum);
	while (count < 10) {
		int serverSocket = pollCall(1000) {
			if (serverSocket == -1) {
				close(socketNum);
				removeFromPollSet(socketNum);
				newSocketNum = setupUdpClientToServer(&server, argv[6], atoi(argv[7]));
				addToPollSet(newSocketNum);
				filenameExchangePacket(argv, &server, newSocketNum);
				count++;
			}
			else {
				break;
			}
		}
	}

	if (count == 10) {
		printf("Failed to send filename. Terminating.\n");
		exit(1);
	}

	uint8_t recvBuffer[MAXBUF];
	int messageLen = 0;
	
	if ((messageLen = recvfrom(serverSocket, recvBuffer, MAXBUF, 0, (struct sockaddr *)server, sizeof(*server))) < 0)
	{
		perror("recv call");
		exit(-1);
	}
	// check the checksum

	uint8_t flag = 0;
	memcpy(&flag, recvBuffer + 6, 1)

	// response to filename packet
	if (flag == 33) {// TODO: update flag = 33 is bad from-filename
		printf("Error: file %s not found.\n", argv[1]);
		exit(1);
	}

	// if filename good
	FILE * to_filename = check_filename(argv[1]);
	printf("Fie OK!\n");
	
	
	
	
	// uint8_t state = 0;
	// switch(state) {
	// 	case ()
	// }
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

	createPDU(sendBuf, 1, 8, filenamePacket, filename_size+7);
	
	int sent = sendtoErr(socketNum, sendBuf, bufSize+7, 0, (struct sockaddr *)server, sizeof(*server));
	if (sent <= 0)
	{
		perror("send call");
		exit(-1);
	}

	


}

void createPDU(uint8_t sendBuf[], uint32_t seq_num, uint8_t flag, uint8_t buffer[], uint16_t bufSize) {
	uint32_t seq_num_NW = htonl(seq_num);
	memcpy(sendBuf, &seq_num_NW, 4);
	memset(sendBuf + 4, 0, 2);
	memcpy(sendBuf+6, &flag, 1);
	memcpy(sendBuf+7, buffer, bufSize);
	uint16_t checksum = in_cksum(sendBuf, bufSize + 7);
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

    int portNumber = 0;
	
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
	
	portNumber = atoi(argv[7]);
		
	return portNumber;
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





