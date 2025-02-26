// Initialize the receiver's buffer
ReceiverBuffer* create_receiver_buffer(int buffer_size) {
    ReceiverBuffer *buffer = (ReceiverBuffer*) malloc(sizeof(ReceiverBuffer));
    buffer->buffer = (Packet*) malloc(buffer_size * sizeof(Packet));
    buffer->buffer_size = buffer_size;
    buffer->expected_sequence = 0;
    buffer->highest_received = -1;
    return buffer;
}

// Add a packet to the receiver's buffer
void add_packet_to_buffer(ReceiverBuffer *buffer, int sequence_number, const char *data, int data_size) {
    int index = sequence_number % buffer->buffer_size;
    buffer->buffer[index].sequence_number = sequence_number;
    memcpy(buffer->buffer[index].data, data, data_size);

    // Update highest received sequence number
    if (sequence_number > buffer->highest_received) {
        buffer->highest_received = sequence_number;
    }
}

// Check if the expected packet has arrived
int is_expected_packet_received(ReceiverBuffer *buffer) {
    int index = buffer->expected_sequence % buffer->buffer_size;
    return buffer->buffer[index].sequence_number == buffer->expected_sequence;
}

// Process the next expected packet
void process_expected_packet(ReceiverBuffer *buffer) {
    int index = buffer->expected_sequence % buffer->buffer_size;
    if (buffer->buffer[index].sequence_number == buffer->expected_sequence) {
        // Process the packet (e.g., pass it to the application layer)
        buffer->expected_sequence++;
    }
}