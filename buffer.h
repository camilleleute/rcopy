#ifndef WINDOW_BUFFER_H
#define WINDOW_BUFFER_H
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h> 


// Define the Packet structure
typedef struct Packet {
    int sequence_number;
    int data_size;
    int valid;
    uint8_t data[];  // Flexible array member for packet data
} Packet;

// Define the ReceiverBuffer structure
typedef struct ReceiverBuffer {
    Packet **buffer;  // Array of Packet pointers
    int window_size;
    int buffer_size;
    int expected;
    int highest;
} ReceiverBuffer;


typedef struct SenderWindow {
    Packet **buffer;  // Array of Packet pointers
    int window_size;
    int buffer_size;
    int lower;
    int upper;
    int current;
} SenderWindow;

// Function prototypes for SenderWindow
SenderWindow* create_sender_window(int window_size, int buffer_size);
void add_packet_to_window(SenderWindow *window, int sequence_number, const char *data, int data_size);
void acknowledge_packet(SenderWindow *window, int sequence_number);
void slide_window(SenderWindow *window, int new_lower);
int windowOpen(SenderWindow *window);  // Change return type to int

// Function prototypes
ReceiverBuffer* create_receiver_buffer(int window_size, int buffer_size);
void add_packet_to_buffer(ReceiverBuffer *buffer, int sequence_number, const char *data, int data_size);
int fetch_data_from_buffer(ReceiverBuffer *buffer, FILE *to_filename);

#endif // WINDOW_BUFFER_H