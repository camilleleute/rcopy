// Initialize the receiver's buffer
ReceiverBuffer* create_receiver_buffer(int buffer_size) {
    ReceiverBuffer *buffer = (ReceiverBuffer*) malloc(sizeof(ReceiverBuffer));
    if (!buffer) {
        perror("Failed to allocate memory for receiver buffer");
        return NULL;
    }

    // Allocate memory for the buffer (array of Packet pointers)
    buffer->buffer = (Packet**) malloc(buffer_size * sizeof(Packet*));
    if (!buffer->buffer) {
        perror("Failed to allocate memory for buffer");
        free(buffer);
        return NULL;
    }

    // Initialize the buffer slots to NULL
    for (int i = 0; i < buffer_size; i++) {
        buffer->buffer[i] = NULL;
        buffer->buffer[i].valid = 0;
    }

    buffer->buffer_size = buffer_size;
    buffer->expected = 0;
    buffer->highest = -1;

    return buffer;
}

// Add a packet to the receiver's buffer
void add_packet_to_buffer(ReceiverBuffer *buffer, int sequence_number, const char *data, int data_size) {
    int index = sequence_number % buffer->buffer_size;

    // Free the existing packet if it exists
    if (buffer->buffer[index]) {
        free(buffer->buffer[index]);
    }

    // Allocate memory for the new packet
    buffer->buffer[index] = (Packet*) malloc(sizeof(Packet) + data_size * sizeof(uint8_t));
    if (!buffer->buffer[index]) {
        perror("Failed to allocate memory for packet");
        return;
    }

    // Init packet
    buffer->buffer[index]->sequence_number = sequence_number;
    buffer->buffer[index]->data_size = data_size;
    buffer->buffer[index]->valid = 1; // Mark as valid
    memcpy(buffer->buffer[index]->data, data, data_size);

    // Update highest received sequence number
    if (sequence_number > buffer->highest) {
        buffer->highest = sequence_number;
    }
}

int fetch_data_from_buffer(ReceiverBuffer *buffer) {
    int index = buffer->expected % buffer->buffer_size;

    // Check if the buffer slot is valid and matches the expected sequence number
    if (buffer->buffer[index] && 
        buffer->buffer[index]->valid && 
        buffer->buffer[index]->sequence_number == buffer->expected) {
        
        // Write the data to the file
        fwrite(buffer->buffer[index]->data, 1, buffer->buffer[index]->data_size, outputFile);
        // Invalidate the buffer slot
        buffer->buffer[index]->valid = 0;
        buffer->expected++;

        // Return the size of the data fetched
        return buffer->buffer[index]->data_size;
    }

    // Return 0 if no data was fetched
    return 0;
}
