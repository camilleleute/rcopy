#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ReceiverBuffer* create_receiver_buffer(int window_size, int buffer_size) {
    ReceiverBuffer *buffer = malloc(sizeof(ReceiverBuffer));
    if (!buffer) return NULL;

    buffer->buffer = malloc(window_size * sizeof(Packet*));
    if (!buffer->buffer) { free(buffer); return NULL; }
    int i = 0;
    for (i = 0; i < window_size; i++)
        buffer->buffer[i] = NULL;

    buffer->window_size = window_size;
    buffer->buffer_size = buffer_size;
    buffer->expected = 0;
    buffer->highest = -1;
    return buffer;
}

void add_packet_to_buffer(ReceiverBuffer *buffer, int sequence_number, const char *data, int data_size) {
    int index = sequence_number % buffer->window_size;
    if (buffer->buffer[index]) free(buffer->buffer[index]);

    buffer->buffer[index] = malloc(sizeof(Packet) + data_size);
    if (!buffer->buffer[index]) return;

    buffer->buffer[index]->sequence_number = sequence_number;
    buffer->buffer[index]->data_size = data_size;
    buffer->buffer[index]->valid = 1;
    memcpy(buffer->buffer[index]->data, data, data_size);

    if (sequence_number > buffer->highest)
        buffer->highest = sequence_number;
}

const char* fetch_data_from_buffer(ReceiverBuffer *buffer, int *data_size) {
    int index = buffer->expected % buffer->window_size;
    if (buffer->buffer[index] && buffer->buffer[index]->valid && 
        buffer->buffer[index]->sequence_number == buffer->expected) {
        *data_size = buffer->buffer[index]->data_size;
        buffer->buffer[index]->valid = 0;
        buffer->expected++;
        return (const char*)buffer->buffer[index]->data;
    }
    return NULL;
}

int is_expected_packet_received(ReceiverBuffer *buffer) {
    int index = buffer->expected % buffer->window_size;
    return (buffer->buffer[index] && buffer->buffer[index]->valid &&
            buffer->buffer[index]->sequence_number == buffer->expected);
}

void free_receiver_buffer(ReceiverBuffer *buffer) {
    if (!buffer) return;
    int i = 0;
    for (i = 0; i < buffer->window_size; i++)
        if (buffer->buffer[i]) free(buffer->buffer[i]);
    free(buffer->buffer);
    free(buffer);
}