//
// Created by kiefer on 4/11/24.
//

#ifndef MAIN_CLIENT_H
#define MAIN_CLIENT_H

#define PORT 8080
#define BUF_SIZE 1024
#define MAX_SEQ_NUM 10
#define TIMEOUT_SEC 5
#define DELAY_NANOSECONDS 100000000
#include <sys/time.h>

typedef struct
{
    char           packets[MAX_SEQ_NUM][BUF_SIZE];
    int            sequence_numbers[MAX_SEQ_NUM];
    int            front;
    int            rear;
    struct timeval last_sent_times[MAX_SEQ_NUM];
} CircularBuffer;

void initializeBuffer(CircularBuffer *buffer);
int  isEmpty(const CircularBuffer *buffer);
int  isFull(const CircularBuffer *buffer);
void enqueue(CircularBuffer *buffer, const char *packet, int sequence_number);
void dequeue(CircularBuffer *buffer, char *packet, int *sequence_number);
int  isTimeoutExpired(struct timeval *last_sent_time);
void handleStdinInput(int sockfd, CircularBuffer *bufferQueue, int *sequence_number);
void handleServerResponse(int sockfd, CircularBuffer *bufferQueue);

#endif    // MAIN_CLIENT_H
