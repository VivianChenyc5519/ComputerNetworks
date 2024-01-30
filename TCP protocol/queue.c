#include <stdio.h>
#include <stdlib.h>
#include "packet.h"
#include <string.h>
#define MAX_SIZE 100

struct Queue {
    tcp_packet* items[MAX_SIZE];
    int front;
    int rear;
};

// Initialize the queue
void initQueue(struct Queue* queue) {
    queue->front = -1;
    queue->rear = -1;
}

// Check if the queue is empty
int isEmpty(struct Queue* queue) {
    return (queue->front == -1 && queue->rear == -1);
}

// Check if the queue is full
int isFull(struct Queue* queue) {
    return (queue->rear == MAX_SIZE - 1);
}

int size(struct Queue* queue) {
    if (isEmpty(queue)) {
        return 0; // Queue is empty, so size is 0
    }

    // The size is the difference between the rear and front indices, plus 1
    return (queue->rear - queue->front + 1);
}

// Add an element to the rear of the queue
void enqueue(struct Queue* queue, tcp_packet* input) {
    
    tcp_packet* value = make_packet(input->hdr.data_size);
        memcpy(value->data, input->data, input->hdr.data_size);
        value->hdr.seqno = input->hdr.seqno;    
        value->send_time = input->send_time;
       
    if (isFull(queue)) {
        // Queue is full; handle the error or return an error code.
        printf("Queue is full. Cannot enqueue.\n");
        return;
    }
    if (isEmpty(queue)) {
        queue->items[0] = value;
        queue->front = queue->rear = 0;
        return;
    }

    
    int insertPosition = queue->rear;

    // Find the correct position to insert based on seqno
    while (insertPosition >= queue->front && value->hdr.seqno < queue->items[insertPosition]->hdr.seqno) {
        // Shift elements to make room for the new element
        queue->items[insertPosition + 1] = queue->items[insertPosition];
        insertPosition--;
    }

    // Insert the element at the correct position
    queue->items[insertPosition + 1] = value;

    // If the inserted element is at the rear, update the rear index
    if (insertPosition == queue->rear) {
        queue->rear++;
    }

    // Ensure the front index is updated if the queue was empty before insertion
    if (isEmpty(queue)) {
        queue->front = 0;
    }
}

 

// Remove an element from the front of the queue
tcp_packet* dequeue(struct Queue* queue) {


    tcp_packet* removedItem = queue->items[queue->front];
    
    if (queue->front == queue->rear) {
        queue->front = queue->rear = -1;
    } else {
        queue->front++;
    }

    return removedItem;
}

// Peek at the front element of the queue without removing it
tcp_packet* peek(struct Queue* queue) {
    if (isEmpty(queue)){
        return make_packet(0);
    }else{
        return queue->items[queue->front];
    }
    
}

// Function to print all integers in the queue from the first enqueued to the last
void printQueue(struct Queue* queue) {
    if (isEmpty(queue)) {
        printf("Queue is empty.\n");
        return;
    }

    printf("Queue elements from first to last: ");
    for (int i = queue->front; i <= queue->rear; i++) {
        printf("%d ", queue->items[i]->hdr.seqno);
    }
    printf("\n");
}
