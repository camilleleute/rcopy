#include <stdlib.h>
#include <string.h>
#include "buffer.h"

// Initialize the sender's window
SenderWindow* create_sender_window(int window_size, int buffer_size) {
    SenderWindow *window = (SenderWindow*) malloc(sizeof(SenderWindow));
    int i;  // Declare loop variable outside the loop for C89 compatibility

    if (!window) {
        perror("Failed to allocate memory for sender buffer");
        return NULL;
    }

    // Allocate memory for the buffer (array of Packet pointers)
    window->buffer = (Packet**) malloc(window_size * sizeof(Packet*));
    if (!window->buffer) {
        perror("Failed to allocate memory for buffer");
        free(window);
        return NULL;
    }

    // Initialize the buffer slots to NULL
    for (i = 0; i < window_size; i++) {
        window->buffer[i] = NULL;
    }

    window->window_size = window_size;
    window->buffer_size = buffer_size;
    window->lower = 0;
    window->upper = window_size - 1;
    window->current = 0;

    return window;
}

// Add a packet to the sender's window
void add_packet_to_window(SenderWindow *window, int sequence_number, const char *data, int data_size) {
    int index = sequence_number % window->window_size;

    // Free the existing packet if it exists
    if (window->buffer[index]) {
        free(window->buffer[index]);
        window->buffer[index] = NULL;  // Set to NULL after freeing
    }

    // Allocate memory for the new packet
    window->buffer[index] = (Packet*) malloc(sizeof(Packet) + window->buffer_size * sizeof(uint8_t));
    if (!window->buffer[index]) {
        perror("Failed to allocate memory for packet");
        return;
    }

    // Initialize packet
    window->buffer[index]->sequence_number = sequence_number;
    window->buffer[index]->data_size = data_size;
    window->buffer[index]->valid = 0;  // Mark as unacknowledged
    memcpy(window->buffer[index]->data, data, data_size);
}

// Free packet if receive RR
void acknowledge_packet(SenderWindow *window, int sequence_number) {
    // Iterate through all packets in the window
    int i;
    for (i = window->lower; i <= sequence_number; i++) {
        int index = i % window->window_size;

        // Free the packet if it exists
        if (window->buffer[index] && window->buffer[index]->sequence_number <= sequence_number) {
            free(window->buffer[index]);
            window->buffer[index] = NULL;  // Set to NULL after freeing
        }
    }

    // Slide the window forward to the next unacknowledged packet
    slide_window(window, sequence_number + 1);
}

Packet* get_packet(SenderWindow *window, int sequence_number, int * data_size) {
    // Calculate the index in the circular buffer
    int index = sequence_number % window->window_size;

    // Check if the packet exists and matches the sequence number
    if (window->buffer[index] && window->buffer[index]->sequence_number == sequence_number) {
        *data_size = window->buffer[index]->data_size;
        return window->buffer[index];
    }

    // Return NULL if the packet is not found or has been acknowledged
    return NULL;
}

// Slide the window forward
void slide_window(SenderWindow *window, int new_lower) {
    window->lower = new_lower;
    window->upper = (new_lower + window->window_size - 1) % window->window_size;
}

// Check if the window is open
int windowOpen(SenderWindow *window) {
    if (window->current >= window->upper) {
        return 0;  // Window is closed
    } else {
        return 1;  // Window is open
    }
}