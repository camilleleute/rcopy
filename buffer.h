#ifndef WINDOW_BUFFER_H
#define WINDOW_BUFFER_H
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h> 

typedef struct Packet {
    int sequence_number;
    int data_size;
    int valid;
    uint8_t data[];
} Packet;

typedef struct ReceiverBuffer {
    Packet **buffer;
    int window_size;
    int buffer_size;
    int expected;
    int highest;
} ReceiverBuffer;

typedef struct SenderWindow {
    Packet **buffer;
    int window_size;
    int buffer_size;
    int lower;
    int upper;
    int current;
} SenderWindow;

// Function prototypes
SenderWindow* create_sender_window(int window_size, int buffer_size);
void add_packet_to_window(SenderWindow *window, int sequence_number, const char *data, int data_size);
void acknowledge_packet(SenderWindow *window, int sequence_number);
void slide_window(SenderWindow *window, int new_lower);
Packet* get_packet(SenderWindow *window, int sequence_number, int * data_size);
int windowOpen(SenderWindow *window);
void free_sender_window(SenderWindow *window);

ReceiverBuffer* create_receiver_buffer(int window_size, int buffer_size);
void add_packet_to_buffer(ReceiverBuffer *buffer, int sequence_number, const char *data, int data_size);
const char * fetch_data_from_buffer(ReceiverBuffer *buffer, int * data_size);
int is_expected_packet_received(ReceiverBuffer *buffer);
void free_receiver_buffer(ReceiverBuffer *buffer);

#endif // WINDOW_BUFFER_H