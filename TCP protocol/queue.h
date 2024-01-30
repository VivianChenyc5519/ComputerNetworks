#ifndef QUEUE_H
#define QUEUE_H

// Define the maximum size of the queue
#define MAX_SIZE 100

// Define the queue structure
struct Queue {
    tcp_packet* items[MAX_SIZE];
    int front;
    int rear;
};

// Initialize the queue
void initQueue(struct Queue* queue);

// Check if the queue is empty
int isEmpty(struct Queue* queue);

int size(struct Queue* queue);

// Add an element to the rear of the queue
void enqueue(struct Queue* queue, tcp_packet* value);

// Remove an element from the front of the queue
tcp_packet* dequeue(struct Queue* queue);

// Peek at the front element of the queue without removing it
tcp_packet* peek(struct Queue* queue);

// Function to print all integers in the queue from the first enqueued to the last
void printQueue(struct Queue* queue);

#endif // QUEUE_H
