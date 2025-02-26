#include <stdlib.h>
#include <string.h>

// Initialize the sender's window
SenderWindow* create_sender_window(int window_size) {
    SenderWindow *window = (SenderWindow*) malloc(sizeof(SenderWindow));
    window->buffer = (Packet*) malloc(window_size * sizeof(Packet));
    window->window_size = window_size;
    window->lower = 0;
    window->upper = window_size - 1;
    window->current = 0;
    return window;
}

// Add a packet to the sender's window
void add_packet_to_window(SenderWindow *window, int sequence_number, const char *data, int data_size) {
    int index = sequence_number % window->window_size;
    window->buffer[index].seq_num = sequence_number;
    memcpy(window->buffer[index].data, data, data_size);
    window->buffer[index].ACK = 0; // Mark as unacknowledged
}

// Check if a packet has been acknowledged
int is_packet_acknowledged(SenderWindow *window, int sequence_number) {
    int index = sequence_number % window->window_size;
    return window->buffer[index].ACK;
}

// Slide the window forward
void slide_window(SenderWindow *window, int new_lower) {
    window->lower = new_lower;
    window->upper = (new_lower + window->window_size - 1) % window->window_size;
}