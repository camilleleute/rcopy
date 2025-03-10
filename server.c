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
#include <signal.h>


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

void processClient(int socketNum);
int filenamePacketCheck(int messageLen, uint8_t buff[], char filename[], FILE **from_filename);
int checkArgs(int argc, char *argv[]);
FILE * check_filename(char * filename);
void createPDU(uint8_t sendBuf[], uint32_t seq_num, uint8_t flag, uint8_t buffer[], uint16_t bufSize);
void sendingData(int socketNum, struct sockaddr_in6 *client, FILE * from_filename, int count);
void check_error_rate(char * rate);
void handleEndOfFile(int socketNum, struct sockaddr_in6 * client, int count);
int checkRRSandSREJs(int socketNum, struct sockaddr_in6 * client, int count);

// global variable 
SenderWindow * senderBuffer = NULL;
uint32_t seqNum = 0;


int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				
	int portNumber = 0;
	signal(SIGCHLD, SIG_IGN);

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
		setupPollSet();
		addToPollSet(socketNum);
		int clientSocket = pollCall(-1);
		if ((messageLen = recvfrom(clientSocket, recvBuff, MAXBUF, 0, (struct sockaddr *)&client, &addrLen)) < 0)
		{
			// no message, recv again
			continue;
		}

		// check filename packet validity and the from-filename
		char filename[101];
		FILE * from_filename = NULL;
		int valid = filenamePacketCheck(messageLen, recvBuff, filename, &from_filename);
		if (valid == 1) {
			printf("Invalid filename packet.\n");
			continue;
		} else if (valid == 2) {
			// filename doesnt exist.
			printf("filename doesn't exist\n");
			uint8_t smallBuf[1]; 
			uint8_t sendBuff[MAXBUF];
			memset(smallBuf, 0, 1);
			createPDU(sendBuff, 1, 33, smallBuf, 1);
			int sent = sendtoErr(clientSocket, sendBuff, 8, 0, (struct sockaddr *)&client, sizeof(client));
			if (sent <= 0)
			{
				perror("send call");
				exit(-1);
			}
			continue;

		} else {
			// good filename packet
			pid_t pid = fork();
			if (pid < 0) {
                perror("fork failed");
                exit(-1);
			} else if (pid == 0) {
				// child
				printf("I forked\n");
				close(socketNum);
				int newSocket = udpServerSetup(0);  // OS picks le port number
                if (newSocket < 0) {
                    perror("Failed to create new socket");
                    exit(-1);
                }
				char messageBuf[256]; 
				int size = snprintf(messageBuf, sizeof(messageBuf), "file OK"); 
				uint8_t sendBuf[MAXBUF];
				createPDU(sendBuf, 1, 9, (uint8_t *)messageBuf, size +1);
				int sent = sendtoErr(clientSocket, sendBuf, size + 8, 0, (struct sockaddr *)&client, sizeof(client));
				if (sent <= 0)
				{
					perror("send call");
					exit(-1);
				}
				// Handle file transfer with the client
				int count = 0;
				sendingData(newSocket, &client, from_filename, count);
				handleEndOfFile(newSocket, &client, count);
				close(newSocket);
				fclose(from_filename);
				printf("I died\n");
				exit(0);

			} else {
				// parent
				continue;
			}
		}
	}

}

void sendingData(int socketNum, struct sockaddr_in6 *client, FILE * from_filename, int count) {
	
	setupPollSet();
	addToPollSet(socketNum);

	while (1) {
		while (windowOpen(senderBuffer)) {
			uint8_t dataBuffer[senderBuffer->buffer_size];
			size_t bytesRead = fread(dataBuffer, 1, senderBuffer->buffer_size, from_filename);
			if ((bytesRead <= 0)) {
				printf("End of file\n");
				return;
			}
			// Create and send the data packet
			uint8_t sendBuf[bytesRead+7];
			createPDU(sendBuf, seqNum, DPACK, dataBuffer, bytesRead);
			// store PDU in window
			add_packet_to_window(senderBuffer, seqNum, (const char *)sendBuf, bytesRead+7);
			int sent = sendtoErr(socketNum, sendBuf, bytesRead + 7, 0, (struct sockaddr *)client, sizeof(*client));
			if (sent <= 0) {
				perror("send call");
				exit(-1);
			}
			seqNum++;
			while (pollCall(0) != -1) {
				count = 0;
				checkRRSandSREJs(socketNum, client, count);
			}
		}
		while (!windowOpen(senderBuffer)){
			if (pollCall(1000) != -1) {
				count = 0;
				checkRRSandSREJs(socketNum, client, count);
			} else {
				// send lowest packet
				if (count == 9){
					return;
				}
				int data_size;
				Packet *packet = get_packet(senderBuffer, senderBuffer->lower, &data_size);
				if (packet == NULL ){
					printf("%d doesnt exist!\n",  senderBuffer->lower);
				} else {
					int sent = sendtoErr(socketNum, packet, data_size, 0, (struct sockaddr *)client, sizeof(*client));
					if (sent <= 0) {
						perror("send call");
						exit(-1);
					}
					count++;
				}
			}
		}
	}
}

void handleEndOfFile(int socketNum, struct sockaddr_in6 * client, int count) {

	// Create and send the EOF packet
	printf("I'm sending EOF\n");
	uint8_t sendBuf[8];
	uint8_t blankBuf = 0;
	createPDU(sendBuf, seqNum, EOFF, (uint8_t *)&blankBuf, 1);
	int sent = sendtoErr(socketNum, sendBuf, 8, 0, (struct sockaddr *)client, sizeof(*client));
	if (sent <= 0) {
		perror("send call");
		exit(-1);
	}
	// store PDU in window
	add_packet_to_window(senderBuffer, seqNum, (const char *)sendBuf, 8);
	// handle remaining RRs and SREJs
	while (windowOpen(senderBuffer)){
		while (pollCall(0) != -1) {
			count = 0;
			int check = checkRRSandSREJs(socketNum, client, count);
			if (check == 1){
				// got EOF ack
				return;
			}
		}
	}
	while (!(windowOpen(senderBuffer))){
		if (pollCall(1000) != -1) {
			count = 0;
			int check = checkRRSandSREJs(socketNum, client, count);
			if (check == 1){
				// got EOF ack
				return;
			}
		} else {
			// send lowest packet
			int data_size;
			Packet *packet = get_packet(senderBuffer, senderBuffer->lower, &data_size);
			if (packet == NULL ){
				printf("%d doesnt exist!\n",  senderBuffer->lower);
			} else {
				if (count == 9){
					return;
				}
				int sent = sendtoErr(socketNum, packet, data_size, 0, (struct sockaddr *)client, sizeof(*client));
				if (sent <= 0) {
					perror("send call");
					exit(-1);
				}
				count++;
			}
		}
	}
}
	


int checkRRSandSREJs(int socketNum, struct sockaddr_in6 * client, int count){
	int messageLen = 0;
	socklen_t clientLen = sizeof(*client);  
	uint32_t recvBuff[senderBuffer->buffer_size];
	memset(recvBuff, 0, sizeof(recvBuff));  

	if ((messageLen = recvfrom(socketNum, recvBuff, MAXBUF, 0, (struct sockaddr *)client, &clientLen)) < 0) {
		printf("Empty message\n");
		return 0;  // Return without processing
	}


	uint16_t calculatedChecksum = in_cksum((unsigned short *)recvBuff, messageLen);

	if (calculatedChecksum) {
		printf("Checksum mismatch. Discarding packet.\n");
		count++;
		return 0;
	}

	uint8_t recv_flag;
	uint32_t recv_seq_num;
	memcpy(&recv_flag, recvBuff+6, 1);
	memcpy(&recv_seq_num, recvBuff+7, 4);
	recv_seq_num = ntohl(recv_seq_num);
	switch (recv_flag) {
		case RR:
			acknowledge_packet(senderBuffer, recv_seq_num);
			break;
		case SREJ: 
			{int data_size;
			Packet *packet = get_packet(senderBuffer, recv_seq_num, &data_size);
			if (packet == NULL ){
				printf("%d doesnt exist!\n", recv_seq_num);
			} else {
				int sent = sendtoErr(socketNum, packet, data_size, 0, (struct sockaddr *)client, sizeof(*client));
				if (sent <= 0) {
					perror("send call");
					exit(-1);
				}
			}
			break;}
		case EOFF:
			printf("got an eof\n");
			return 1;
		default:
			break;
	}
	return 0;
}

int filenamePacketCheck(int messageLen, uint8_t buff[], char filename[], FILE **from_filename) {
    uint16_t checksum = in_cksum((unsigned short *)buff, messageLen);
    uint8_t flag;
    memcpy(&flag, buff+6, 1);
    if ((flag != 8) || (checksum != 0)) {
        return 1;
    } else {
        strcpy(filename, (const char *)(buff + 13));
        *from_filename = check_filename(filename);  // Assign to the caller's pointer

        if (*from_filename == NULL) {
            return 2;
        }
        uint32_t window_size = 0;
        uint16_t buffer_size = 0;
        memcpy(&(window_size), buff+7, 4);
        memcpy(&(buffer_size), buff+11, 2);
        senderBuffer = create_sender_window(window_size, buffer_size);
        return 0;
    }
}
	
FILE * check_filename(char * filename) {
	FILE* file_pointer = fopen(filename, "rb");
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

	check_error_rate(argv[1]);
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}
	
	return portNumber;
}

void check_error_rate(char * rate) {
	uint8_t error_rate = atof(rate);
	if ((error_rate < 0) || (error_rate > 1)){
		printf("error-rate is out of range. please input a rate between 0 and 1.\n");
		exit(1);
	}
	sendtoErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
}

