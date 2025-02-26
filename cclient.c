/******************************************************************************
* myClient.c
*
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

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
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "send.h"
#include "pollLib.h"
#include "shared.h"


#define MAXBUF 1402
#define DEBUG_FLAG 1

void sendToServer(int, char *);
void parseAndPrint(char * , int);
void processMsgFromServer(int);
void clientControl(int, char *);
void receiveFromServer(int );
int readFromStdin(uint8_t *);
void checkArgs(int argc, char * argv[]);
char* handle_parser(int , uint8_t [], uint8_t [], int *);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], 0);
	// client polling manager
	clientControl(socketNum, argv[1]);
	
	return 0;
}

void clientControl(int clientSocket, char * myHandle) {
	setupPollSet();
	addToPollSet(clientSocket);
	sendHandle(clientSocket, myHandle, 1);

	while(1){
		int curr_socket = pollCall(-1);
		if (curr_socket == clientSocket) {
			processMsgFromServer(curr_socket);

		} else {
			sendToServer(clientSocket, myHandle);
		}
		printf("$: ");
		fflush(stdout);
	}

}



void processMsgFromServer(int serverSocket){
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the server_socket
	if ((messageLen = recvPDU(serverSocket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		uint8_t flag = dataBuffer[0];
		char *msg = (char *)&dataBuffer[1];
		//printf("Flag is: %d\n", flag);
		switch(flag){
			case 2:
				addToPollSet(STDIN_FILENO);
				break;
			case 3:
				printf("Handle already in use: %s\n", msg);
				exit(-1);
				break;
			case 4: // %B
				parseAndPrint(msg, flag);
				break;
			case 5: // %M
				parseAndPrint(msg, flag);
				break;
			case 6: // %C
				parseAndPrint(msg, flag);
				break;
			case 7:
				printf("\nClient with handle %s does not exist\n", msg);
				break;
			case 11: {
				uint32_t num_handles = 0;
				memcpy(&num_handles, msg, 4);
				uint32_t num_handles_HOST = ntohl(num_handles);
				printf("\nNumber of clients: %d\n", num_handles_HOST);
				removeFromPollSet(STDIN_FILENO);					// block stdin?
				pollCall(-1);
				processMsgFromServer(serverSocket);
				break;
			}
			case 12: {
				uint8_t handle_len = dataBuffer[1];
				uint8_t handle_name[handle_len+1];
				memcpy(handle_name, &dataBuffer[2], handle_len);
				handle_name[handle_len+1] = '\0';
				printf("\t%s\n", handle_name);
				pollCall(-1);
				processMsgFromServer(serverSocket);
				break;
			}
			case 13:
				addToPollSet(STDIN_FILENO);
				break;
			default:
				break;
		}
		
	}

	else {
		close(serverSocket);
        printf("\nServer has terminated\n");
		removeFromPollSet(serverSocket);
		exit(-1);
	}
}

void parseAndPrint(char * msg, int flag) {
	// 				  msg |>      
	// |-PDU LEN-||-flag-||-src handle len-||-src handle-||-# dest handles-||-dest handle len-||-dest handle-||-text-|
	//   2 bytes     1            1               XX               1                1                  XX        XX
	uint8_t src_handle_len = msg[0];
	char src_handle[src_handle_len+1];
	memcpy(src_handle, msg+1, src_handle_len);
	src_handle[src_handle_len] = '\0';
	int idx = src_handle_len + 1;

	if (flag == 6 || flag == 5) {
		uint8_t num_dest_handles = msg[src_handle_len+1];
		idx++;
		for (int i = 0; i < num_dest_handles; i++){
			uint8_t dest_handle_len = msg[idx++];
			idx += dest_handle_len;
		}
	} 

	msg += idx;
	
	char formatted_message[src_handle_len + 2 + strlen(msg) + 1];
	snprintf(formatted_message, sizeof(formatted_message), "%s: %s", src_handle, msg);
	printf("\n%s\n", formatted_message);

}

void sendToServer(int socketNum, char * myHandle)
{
	uint8_t sendBuf[MAXBUF];   
	uint8_t inputMsg[MAXBUF];
	char * msg = NULL;
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   
	char command[3]; 		   //2 bytes + 1 for null terminator
	int flag = 0;			   //numeric value of command
	int src_handle_len = strlen(myHandle);
	uint8_t num_dest_handles = 0;
	int idx = 0; 				// index into sendBuf
	int start = 0;				// where to start parsing for destination handles
	

	// |-PDU LEN-||-flag-||-src handle len-||-src handle-||-# dest handles-||-dest handle len-||-dest handle-||-text-|
	//   2 bytes     1            1               XX               1                1                  XX        XX
	memset(sendBuf, 0, sizeof(sendBuf));	
	memset(inputMsg, 0, sizeof(inputMsg));	

	int inputLen = readFromStdin(inputMsg); 
	if (inputLen == -1){
		return;
	}

	command[0] = inputMsg[0];
	command[1] = inputMsg[1];
	command[2] = '\0'; // Null terminate

	if (strcmp(command, "%M") == 0 || strcmp(command, "%m") == 0) {
        flag = 5;
		num_dest_handles = 1;
		start = 3;	
    } else if (strcmp(command, "%B") == 0 || strcmp(command, "%b") == 0) {
        flag = 4;
    } else if (strcmp(command, "%C") == 0 || strcmp(command, "%c") == 0) {
        flag = 6;
		start = 5;
		
		char *token = strtok((char *)&inputMsg[3], " ");
    
		if (token == NULL) {
			printf("Invalid command format\n");
			return;
		}
		 num_dest_handles = atoi(token);

		if ((num_dest_handles < 2) || (num_dest_handles > 9)) {
        	printf("Invalid input: number of recipients is out of range\n");
        	return;
    	}
		
    } else if (strcmp(command, "%L") == 0 || strcmp(command, "%l") == 0) {
		flag = 10;
		sendBuf[0] = flag;
		sent = sendPDU(socketNum, sendBuf, 1);
		return;
    } else {
        printf("Invalid command.\n");
		fflush(stdout);
		return;
    }
 
	sendBuf[idx++] = flag; 
	sendBuf[idx++] = src_handle_len;
	memcpy(&sendBuf[idx], myHandle, src_handle_len);	
	idx += src_handle_len;
	
	if (flag == 6 || flag == 5) {
		sendBuf[idx++] = num_dest_handles;
		msg = handle_parser(num_dest_handles, &inputMsg[start], sendBuf, &idx);
		if (msg == NULL){
			return;
		}

	} else {
		msg = (char *)&inputMsg[3];
	}

	// Handle empty messages
    if (msg == NULL) {
        msg = "";
    }

	int msgLen = strlen(msg);
	while (msgLen > 200) {
		uint8_t sendBufDUPE[MAXBUF];
		memcpy(sendBufDUPE, sendBuf, idx);
		memcpy(&sendBufDUPE[idx], msg, 199);
		sendBufDUPE[idx+199] = '\0';
		sent = sendPDU(socketNum, sendBufDUPE, idx+200);
		if (sent < 0)
		{
			perror("send call");
			exit(-1);
		}
		msg += 199;
		msgLen -= 199;
	}

	memcpy(&sendBuf[idx], msg, msgLen); 
	idx += msgLen; 
	sendBuf[idx++] = 0; 
	sendLen = idx;

	sent = sendPDU(socketNum, sendBuf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}
}

char* handle_parser(int num_handles, uint8_t msg[], uint8_t sendBuf[], int * idx) {
    char msg_copy[MAXBUF];
    strncpy(msg_copy, (char *)msg, MAXBUF - 1);
    msg_copy[MAXBUF - 1] = '\0';  
    char *token = strtok(msg_copy, " ");
    // Extract handles
    for (int i = 0; i < num_handles; i++) {
        if (token == NULL) {
            printf("Not enough handles provided\n");
            return NULL;
        }
        uint8_t handle_len = strlen(token);
        sendBuf[(*idx)++] = handle_len;  
        memcpy(&sendBuf[*idx], token, handle_len);
        *idx += handle_len;
        token = strtok(NULL, " ");
    }

    // After extracting handles, check if there's a message
    if (token == NULL) {
        return ""; 
    }
    return (char *)(msg + (token - msg_copy));
}


int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	//printf("\n$: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}

	if (inputLen >= MAXBUF - 1 && aChar != '\n') {
        printf("Error: Input exceeds the maximum of 1400 characters. Command ignored.\n");

        while (getchar() != '\n');
        return -1;
    }
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}

	if ((strlen(argv[1])) >= 101) {
		printf("Invalid handle, handle longer than 100 characters: %s\n", argv[1]);
		exit(1);
	}
}
