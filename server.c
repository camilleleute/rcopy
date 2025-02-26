/******************************************************************************
* myServer.c
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
#include "dict.h"
#include "shared.h"


#define MAXBUF 1024
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket, Dict *);
int checkArgs(int argc, char *argv[]);
void addNewSocket(int);
void serverControl(int);
void get_dest_handles(uint8_t [], Dict *, int, int);
void broadcastHandling(uint8_t [], Dict * , int , int );
void list_handles(Dict * , int );


int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);
	// server control
	serverControl(mainServerSocket);
	/* close the sockets */
	close(mainServerSocket);
	return 0;
}

void serverControl(int mainServerSocket) {
	setupPollSet();
	addToPollSet(mainServerSocket);
	Dict *handle_table = dctcreate();

	while (1) {
		int socketNum = pollCall(-1);
		if (socketNum == mainServerSocket) {
			addNewSocket(socketNum);
		} else if (socketNum < 0) {
			perror("Failed to poll");
			return;
		} else {
			recvFromClient(socketNum, handle_table);
		}
	}
}

void addNewSocket(int socketNum){
	int newSocket = tcpAccept(socketNum, 0);
	addToPollSet(newSocket);
	return;
}

void recvFromClient(int clientSocket, Dict * handle_table)
{
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0) {
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		uint8_t flag = dataBuffer[0]; 
		
		switch(flag) {
			case 1: { // Incoming handle from client
				uint8_t handle_len = dataBuffer[1];

				// Copy the handle into a buffer and null-terminate it
				char handle[handle_len + 1];
				memcpy(handle, &dataBuffer[2], handle_len); 
				handle[handle_len] = '\0'; 

				// Check if the handle already exists in the dictionary
				if (dctget(handle_table, handle) == NULL) {
					dctinsert(handle_table, handle, (void *)(long)clientSocket);
					sendHandle(clientSocket, handle, 2);
				} else {
					sendHandle(clientSocket, handle, 3);
				}
				break;
			}
			case 4: // broadcast %B
				broadcastHandling(dataBuffer, handle_table, clientSocket, messageLen);
				break;
			case 5: // message %M
				get_dest_handles(dataBuffer, handle_table, clientSocket, messageLen);			
				break;
			case 6: // multicast %C
				get_dest_handles(dataBuffer, handle_table, clientSocket, messageLen);
				break;
			case 10: // list %L
				list_handles(handle_table, clientSocket);
				break;

			default:
				break;
		}
	} else {
		close(clientSocket);
		dctremoveVAL(handle_table, (void *)(intptr_t)clientSocket);
		//printf("Connection closed by other side\n");
		removeFromPollSet(clientSocket);
	}
}

void list_handles(Dict * handle_table, int clientSocket) {
	uint8_t firstBuf[5];
	uint32_t num_handles = htonl(handle_table->size);
	firstBuf[0] = 11;
	memcpy(&firstBuf[1], &num_handles, 4);

	int sent = sendPDU(clientSocket, firstBuf, 5);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	char ** handles = dctkeys(handle_table);
	for (int i = 0; i < handle_table->size; i++) {
		uint8_t handlePackets[MAXBUF];
		uint8_t handle_len = strlen(handles[i]);
		handlePackets[0] = 12;
		handlePackets[1] = handle_len;
		memcpy(&handlePackets[2], handles[i], handle_len);
		int sent = sendPDU(clientSocket, handlePackets, handle_len+2);
		if (sent < 0)
		{
			perror("send call");
			exit(-1);
		}
		memset(handlePackets, 0, sizeof(handlePackets));	
	}

	uint8_t doneBuf[1];
	doneBuf[0] = 13;
	int donesent = sendPDU(clientSocket, doneBuf, 1);
	if (donesent < 0)
	{
		perror("send call");
		exit(-1);
	}
}

// sends message to all clients on server
void broadcastHandling(uint8_t dataBuffer[], Dict * handle_table, int clientSocket, int messageLen) {
	char ** sockets = dctkeys(handle_table);
	if (sockets == NULL) {
		return;
	} else {
		for (int i = 0; i < handle_table->size; i++) {

			void* lookupResult = dctget(handle_table, sockets[i]); 
			if (lookupResult == NULL) { 
				printf("ERRORERROR\n");
			} else {
				int destSocket = (intptr_t)lookupResult;
				if (destSocket == clientSocket) continue;
				int sent = sendPDU(destSocket, dataBuffer, messageLen);
				if (sent < 0)
				{
					perror("send call");
					exit(-1);
				}
			}
		}	
		free(sockets); 
	}
}

// parses through destination handles and passes on message to said handles
void get_dest_handles(uint8_t dataBuffer[], Dict * handle_table, int clientSocket, int messageLen){
	
	int num_dest_handles_idx = 1 + 1 + dataBuffer[1];

	uint8_t num_dest_handles = dataBuffer[num_dest_handles_idx];

	int idx = num_dest_handles_idx + 1;
	for (int i = 0; i < num_dest_handles; i++){
		uint8_t dest_handle_len = dataBuffer[idx++];
		char dest_handle[dest_handle_len+1];
		memcpy(dest_handle, &dataBuffer[idx], dest_handle_len);
		dest_handle[dest_handle_len] = '\0';

		void* lookupResult = dctget(handle_table, dest_handle); 

		if (lookupResult == NULL) { 
			sendHandle(clientSocket, dest_handle, 7); 
		} else {
			int destSocket = (intptr_t)lookupResult;
			int sent = sendPDU(destSocket, dataBuffer, messageLen);
			if (sent < 0)
			{
				perror("send call");
				exit(-1);
			}
		}
		idx += dest_handle_len;
	}
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}
