#include "../include/client.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static struct sockaddr_in server_addr;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

int main(void)
{
    int sockfd;
    //    char           buffer[BUF_SIZE];
    int            sequence_number = 0;
    CircularBuffer bufferQueue;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family      = AF_INET;    // IPv4
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    initializeBuffer(&bufferQueue);

    while(1)
    {
        int            max_fd;
        fd_set         read_fds;
        int            ready_fds;
        struct timeval timeout;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &read_fds);

        max_fd = MAX(STDIN_FILENO, sockfd);

        timeout.tv_sec  = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        ready_fds = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if(ready_fds < 0)
        {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }
        else if(ready_fds == 0)
        {
            printf("Timeout occurred. No input received.\n");
            continue;
        }

        if(FD_ISSET(STDIN_FILENO, &read_fds))
        {
            handleStdinInput(sockfd, &bufferQueue, &sequence_number);
        }

        if(FD_ISSET(sockfd, &read_fds))
        {
            handleServerResponse(sockfd, &bufferQueue);
        }
    }

    // Close socket
    // close(sockfd);
    //
    // return 0;
}

void initializeBuffer(CircularBuffer *buffer)
{
    buffer->front = -1;
    buffer->rear  = -1;
    memset(buffer->last_sent_times, 0, sizeof(buffer->last_sent_times));
}

int isEmpty(const CircularBuffer *buffer)
{
    return buffer->front == -1;
}

int isFull(const CircularBuffer *buffer)
{
    return (buffer->rear + 1) % MAX_SEQ_NUM == buffer->front;
}

void enqueue(CircularBuffer *buffer, const char *packet, int sequence_number)
{
    if(isFull(buffer))
    {
        buffer->front = (buffer->front + 1) % MAX_SEQ_NUM;
    }
    buffer->rear = (buffer->rear + 1) % MAX_SEQ_NUM;
    strcpy(buffer->packets[buffer->rear], packet);
    buffer->sequence_numbers[buffer->rear] = sequence_number;
    gettimeofday(&(buffer->last_sent_times[buffer->rear]), NULL);
    if(buffer->front == -1)
    {
        buffer->front = buffer->rear;
    }
}

void dequeue(CircularBuffer *buffer, char *packet, int *sequence_number)
{
    if(!isEmpty(buffer))
    {
        strcpy(packet, buffer->packets[buffer->front]);
        *sequence_number = buffer->sequence_numbers[buffer->front];
        memset(buffer->packets[buffer->front], 0, sizeof(buffer->packets[buffer->front]));
        buffer->sequence_numbers[buffer->front] = 0;

        if(buffer->front == buffer->rear)
        {
            buffer->front = -1;
            buffer->rear  = -1;
        }
        else
        {
            buffer->front = (buffer->front + 1) % MAX_SEQ_NUM;
        }
    }
    else
    {
        *sequence_number = -1;
    }
}

int isTimeoutExpired(struct timeval *last_sent_time)
{
    struct timeval now;
    struct timeval diff;
    gettimeofday(&now, NULL);
    timersub(&now, last_sent_time, &diff);
    return diff.tv_sec >= TIMEOUT_SEC;
}

void handleStdinInput(int sockfd, CircularBuffer *bufferQueue, int *sequence_number)
{
    char            buffer[BUF_SIZE];
    struct timespec delay;
    delay.tv_nsec = DELAY_NANOSECONDS;

    printf("Enter message: ");
    fgets(buffer, BUF_SIZE, stdin);

    while(isFull(bufferQueue))
    {
        nanosleep(&delay, NULL);
        printf("Sleeping, circ buffer full\n");
    }

    sprintf(buffer + strlen(buffer), ":%d", *sequence_number);
    sendto(sockfd, (const char *)buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));

    enqueue(bufferQueue, buffer, *sequence_number);

    // Increment sequence number for the next packet
    *sequence_number = (*sequence_number + 1) % MAX_SEQ_NUM;
    printf("Sending number: %i\n", *sequence_number);
}

void handleServerResponse(int sockfd, CircularBuffer *bufferQueue)
{
    char        buffer[BUF_SIZE];
    long        len;
    const char *sequence_str;
    len         = recvfrom(sockfd, (char *)buffer, BUF_SIZE, MSG_WAITALL, NULL, NULL);
    buffer[len] = '\0';
    printf("Server reply: %s\n", buffer);

    sequence_str = strchr(buffer, ':');
    if(sequence_str != NULL)
    {
        long seq = strtol(sequence_str + 1, NULL, MAX_SEQ_NUM);

        if(seq >= 0 && seq < MAX_SEQ_NUM)
        {
            char packet[BUF_SIZE];
            int  seq_to_release;
            dequeue(bufferQueue, packet, &seq_to_release);
            if(seq_to_release != seq)
            {
                printf("Error: Received sequence number does not match expected sequence number\n");
            }
            else
            {
                printf("Released packet with sequence number: %ld\n", seq);
            }

            if(strcmp(buffer, "Out-of-order") == 0)
            {
                printf("Resending packet with sequence number: %d\n", seq_to_release);
                sendto(sockfd, (const char *)packet, strlen(packet), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
            }
        }
        else
        {
            printf("Error: Invalid sequence number received from server\n");
        }
    }
    else
    {
        printf("Error: Malformed server reply. Delimiter ':' not found.\n");
    }
}
