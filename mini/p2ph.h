#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024
#define MAX_CLNT 30

typedef struct
{
    int *clnt_socks;
    int serv_sock;
    int num_accept;
} accept_thread_t;

typedef struct
{
    struct sockaddr_in *addr_info;
    int *serv_socks;
    int num_connect;
} connect_thread_t;

typedef struct
{
    char file_name[BUF_SIZE];
    int total_size;
    int total_seq_num;
} file_pkt_t;

typedef struct
{
    int seq;
    char *content;
    int cur_size;
} data_pkt_t;

void setOption(int argc, char *argv[], char *option_type,
               int *s_peer, int *r_peer, int *max_peer, char *file_name,
               int *seg_size, char *s_peer_ip, char *s_peer_port, char *port);
void showHelp(char *this_name);
int recvPkt(int sock, void *dest, int pkt_size);

void showHelp(char *this_name)
{
    printf("Usage : %s [OPTION] [CONTENTS]\n"
           "-s                            SET SENDING PEER\n"
           "-r                            SET RECEIVING PEER\n"
           "-n [ MAX_NUM ]                NUMBER of receving peer - for sending peer\n"
           "-f [ FILE_NAME ]              FILE NAME to send\n"
           "-g [ SEGMENT_SIZE [KiB] ]      SEGMENT SIZE (KiB)\n"
           "-a [ <IP> <PORT> ]            IP and PORT for sending peer\n"
           "-p [ PORT ]                   PORT for listen\n",
           this_name);
}

void setOption(int argc, char *argv[], char *option_type,
               int *s_peer, int *r_peer, int *max_peer, char *file_name,
               int *seg_size, char *s_peer_ip, char *s_peer_port, char *port)
{

    int opt;

    while ((opt = getopt(argc, argv, option_type)) != -1)
    {
        switch (opt)
        {
        case 'h':
            showHelp(argv[0]);
            break;

        case 's':
            *s_peer = 1;
            printf("This process is sending peer\n");
            break;

        case 'r':
            *r_peer = 1;
            printf("This process is receiving peer\n");
            break;

        case 'n':
            *max_peer = atoi(optarg);
            if (*max_peer > MAX_CLNT)
            {
                printf("MAX_PEER limit is %d", MAX_CLNT);
                exit(1);
            }
            break;

        case 'f':
            strcpy(file_name, optarg);
            if (access(file_name, R_OK) != 0)
            {
                printf("%s does not exist or no permission\n", file_name);
                exit(1);
            }
            break;

        case 'g':
            *seg_size = atoi(optarg);
            break;

        case 'a':
            strcpy(s_peer_ip, optarg);
            if (argv[optind][0] == '-')
            {
                printf("Inaccurate -a option\n");
                exit(1);
            }
            strcpy(s_peer_port, argv[optind]);
            optind++;
            if (argv[optind][0] != '-')
            {
                printf("Inaccurate -a option\n");
                exit(1);
            }
            break;

        case 'p':
            strcpy(port, optarg);
            break;

        case '?':
            printf("unkown command\n");
            showHelp(argv[0]);
            exit(1);
            break;
        }
    }

    // IF TYPE NOT DEFINED
    if ((*s_peer == 1) && (*r_peer == 1))
    {
        printf("TYPE NOT FIXED!! \n");
        showHelp(argv[0]);
        exit(1);
    }

    else if ((*s_peer == 0) && (*r_peer == 0))
    {
        printf("TYPE NOT FIXED!! \n");
        showHelp(argv[0]);
        exit(1);
    }

    // if sending peer
    if (*s_peer == 1)
    {
        if (*max_peer == 0) // -n
        {
            printf("[SENDING PEER] -n option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (strcmp(file_name, "") == 0) // -f
        {
            printf("[SENDING PEER] -f option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (*seg_size == 0) // -g
        {
            printf("[SENDING PEER] -g option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (strcmp(port, "") == 0) // -p
        {
            printf("[SENDING PEER] -p option needed\n");

            showHelp(argv[0]);
            exit(1);
        }
    }

    // if receiving peer
    else if (*r_peer == 1)
    {
        if (strcmp(s_peer_ip, "") == 0) // -a
        {
            printf("[RECEVING PEER] -a option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (strcmp(s_peer_port, "") == 0) // -a
        {
            printf("[RECEVING PEER] -a option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (strcmp(port, "") == 0) // -p
        {
            printf("[RECEVING PEER] -p option needed\n");

            showHelp(argv[0]);
            exit(1);
        }
    }
}

int recvPkt(int sock, void *dest, int pkt_size)
{
    int recv_size;
    char *temp = malloc(pkt_size);

    while (1)
    {
        recv_size = recv(sock, temp, pkt_size, MSG_PEEK);

        if (recv_size == -1)
            perror("recv(): ");

        if (recv_size == pkt_size)
            break;
    }

    recv_size = recv(sock, temp, pkt_size, 0);
    memcpy(dest, temp, pkt_size);

    free(temp);
    return recv_size;
}
