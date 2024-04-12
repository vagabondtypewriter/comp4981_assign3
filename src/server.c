#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUF_SIZE 1024
#define REPLY "Message received"
#define MAX_SEQ_NUM 10
#define MAX_CLIENTS 2

typedef struct
{
    int                sockfd;
    struct sockaddr_in address;
    int                active;
    int                expected_sequence_number;
} ClientInfo;

//typedef struct
//{
//     int border_x;
//     int border_y;
//     int player_pos[MAX_CLIENTS][2];
//} GameSettings;


int main(void)
{
    int                sockfd;
    struct sockaddr_in server_addr;
    ClientInfo         clients[MAX_CLIENTS];
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;    // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if(bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].sockfd                   = -1;
        clients[i].active                   = 0;
        clients[i].expected_sequence_number = 0;
    }

    while(1)
    {
        ssize_t            len;
        char               buffer[BUF_SIZE];
        struct sockaddr_in client_addr;
        socklen_t          client_len = sizeof(client_addr);
        int                client_index;
        len         = recvfrom(sockfd, (char *)buffer, BUF_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &client_len);
        buffer[len] = '\0';

        client_index = -1;

        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            if(clients[i].active && memcmp(&clients[i].address, &client_addr, sizeof(client_addr)) == 0)
            {
                client_index = i;
                break;
            }
        }

        if(client_index == -1)
        {
            for(int i = 0; i < MAX_CLIENTS; i++)
            {
                if(!clients[i].active)
                {
                    client_index       = i;
                    clients[i].sockfd  = sockfd;
                    clients[i].address = client_addr;
                    clients[i].active  = 1;
                    break;
                }
            }
        }

        if(client_index != -1)
        {
            const char *sequence_str    = strrchr(buffer, ':');
            long        sequence_number = strtol(sequence_str + 1, NULL, MAX_SEQ_NUM) % MAX_SEQ_NUM;

            if(sequence_number == clients[client_index].expected_sequence_number)
            {
                sprintf(buffer, "%s:%ld", REPLY, sequence_number);
                sendto(sockfd, (const char *)buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&client_addr, client_len);
                printf("(Client%d) Reply sent for sequence number: %ld\n", client_index, sequence_number);

                clients[client_index].expected_sequence_number++;
                clients[client_index].expected_sequence_number %= MAX_SEQ_NUM;
            }
            else
            {
                sprintf(buffer, "%s:%ld:expected:%d", "Out-of-order", sequence_number, clients[client_index].expected_sequence_number);
                sendto(sockfd, (const char *)buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&client_addr, client_len);
                printf("Out-of-order packet received. Notification sent.\n");
            }
        }
        else
        {
            printf("No available slot for new client.\n");
        }
    }

    //    close(sockfd);

    //    return 0;
}
