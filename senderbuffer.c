#include "buffer.h"
#include <stdlib.h>
#include <string.h>

SenderWindow* create_sender_window(int window_size, int buffer_size) {
    SenderWindow *window = malloc(sizeof(SenderWindow));
    if (!window) return NULL;

    window->buffer = malloc(window_size * sizeof(Packet*));
    if (!window->buffer) { free(window); return NULL; }
    int i = 0;
    for ( i = 0; i < window_size; i++)
        window->buffer[i] = NULL;

    window->window_size = window_size;
    window->buffer_size = buffer_size;
    window->lower = 0;
    window->upper = window_size - 1;
    window->current = 0;
    return window;
}

void add_packet_to_window(SenderWindow *window, int sequence_number, const char *data, int data_size) {
    int index = sequence_number % window->window_size;
    if (window->buffer[index]) free(window->buffer[index]);

    window->buffer[index] = malloc(sizeof(Packet) + data_size);
    if (!window->buffer[index]) return;

    window->buffer[index]->sequence_number = sequence_number;
    window->buffer[index]->data_size = data_size;
    window->buffer[index]->valid = 0;
    memcpy(window->buffer[index]->data, data, data_size);
    window->current++;
}

void acknowledge_packet(SenderWindow *window, int sequence_number) {
    int i = 0;

    for ( i = window->lower; i <= sequence_number; i++) {
        int index = i % window->window_size;
        if (window->buffer[index] && window->buffer[index]->sequence_number <= sequence_number) {
            free(window->buffer[index]);
            window->buffer[index] = NULL;
        }
    }
    slide_window(window, sequence_number + 1);
}

void slide_window(SenderWindow *window, int new_lower) {
    window->lower = new_lower;
    window->upper = new_lower + window->window_size - 1;
}

Packet* get_packet(SenderWindow *window, int sequence_number, int *data_size) {
    int index = sequence_number % window->window_size;
    if (window->buffer[index] && window->buffer[index]->sequence_number == sequence_number) {
        *data_size = window->buffer[index]->data_size;
        return window->buffer[index];
    }
    return NULL;
}

int windowOpen(SenderWindow *window) {
    return (window->current < (window->lower + window->window_size));
}

void free_sender_window(SenderWindow *window) {
    if (!window) return;
    int i = 0;
    for (i = 0; i < window->window_size; i++)
        if (window->buffer[i]) free(window->buffer[i]);
    free(window->buffer);
    free(window);
}