#include "send.h"
#include "networks.h"

int flag_parse(uint8_t * dataBuffer) {
    uint8_t flag = 0;
    memcpy(&flag, dataBuffer, 1);
    return flag;
}

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData) {
    uint8_t PDUbuf[lengthOfData + 2];
    uint16_t PDUlength_HOST = lengthOfData + 2;
    uint16_t PDUlength_NWO = htons(PDUlength_HOST);
    memcpy(PDUbuf, &PDUlength_NWO, 2);
    memcpy(PDUbuf + 2, dataBuffer, lengthOfData);
    int sent = send(clientSocket, PDUbuf, PDUlength_HOST, 0);
    if (sent <= 0)
	{
		perror("send call");
		exit(-1);
	}
    return sent;
}

int recvPDU(int clientSocket, uint8_t * dataBuffer, int bufferSize) {

    uint16_t headerlen = recv(clientSocket, dataBuffer, 2, MSG_WAITALL);
    
    if (headerlen <= 0) {
        return 0;
    }

    uint16_t datalen = ntohs(*(uint16_t *)dataBuffer) - 2;

    if ((datalen > bufferSize)) {
        perror("Packet to big");
		return -1;
    }

    int received_data = recv(clientSocket, dataBuffer, datalen, MSG_WAITALL);
    
    if (received_data <= 0) {
        perror("recv call (error reading data)");
		return -1;
    }

    return received_data;
}
