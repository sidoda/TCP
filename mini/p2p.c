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

void setOption(int argc, char *argv[], char *option_type,
               int *s_peer, int *r_peer, int *max_peer, char *file_name,
               int *seg_size, char *s_peer_ip, char *s_peer_port, char *port);
void showHelp(char *this_name);
void sendingPeer(int max_peer, char *file_name, int seg_size, char *port);
void receivingPeer(char *s_peer_port, char *s_peer_ip, char *port);
int recvPkt(int sock, void *dest, int pkt_size);
void *acceptThread(void *arg);
void *connectThread(void *arg);

int main(int argc, char *argv[])
{
    char option_type[BUF_SIZE] = "srn:f:g:a:p:h";
    int s_peer = 0, r_peer = 0, max_peer = 0;
    int seg_size = 0;
    char file_name[BUF_SIZE] = "";
    char s_peer_ip[BUF_SIZE] = "";
    char s_peer_port[BUF_SIZE] = "";
    char port[BUF_SIZE] = "";

    setOption(argc, argv, option_type,
              &s_peer, &r_peer, &max_peer, file_name,
              &seg_size, s_peer_ip, s_peer_port, port);

    if (s_peer == 1)
        sendingPeer(max_peer, file_name, seg_size, port);

    else
        receivingPeer(s_peer_port, s_peer_ip, port);

    return 0;
}

void sendingPeer(int max_peer, char *file_name, int seg_size, char *port)
{
    int serv_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;

    int clnt_count = 0;
    int clnt_socks[MAX_CLNT];
    struct sockaddr_in addr_info[MAX_CLNT];

    printf("MAX_PEER is %d\n", max_peer);
    printf("SEGMENT_SIZE is %d\n", seg_size);

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        perror("[Server] sock() error: ");
        exit(1);
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(port));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        perror("[Server] bind() error : ");
        exit(1);
    }
    if (listen(serv_sock, max_peer) == -1)
    {
        perror("[Server] listen() error : ");
    }

    // Connect with receiver and save ip and port info
    while (clnt_count < max_peer)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_socks[clnt_count] = accept(serv_sock, (struct sockaddr *)&clnt_adr, (socklen_t *)&clnt_adr_sz);

        memcpy(&addr_info[clnt_count], &clnt_adr, sizeof(struct sockaddr_in));
        clnt_count++;
    }

    printf("--- Clienct list ---\n");
    for (int i = 0; i < max_peer; i++)
    {
        printf("socket number %d \n", clnt_socks[i]);
        printf("IP : %s PORT : %d \n", inet_ntoa(addr_info[i].sin_addr), ntohs(addr_info[i].sin_port));
    }
    // Send the ip and port of recevier to recevier
    for (int i = 1; i < max_peer + 1; i++)
    {
        int value = max_peer - i;
        write(clnt_socks[i - 1], &value, sizeof(int));
        value = i - 1;
        write(clnt_socks[i - 1], &value, sizeof(int));

        for (int j = i; j < max_peer; j++)
        {
            write(clnt_socks[i - 1], &addr_info[j], sizeof(struct sockaddr_in));
        }
    }

    while (1)
    {
    }

    for (int i = 0; i < max_peer; i++)
        close(clnt_socks[i]);
    close(serv_sock);
}

void receivingPeer(char *s_peer_port, char *s_peer_ip, char *port)
{
    int sock, serv_sock;
    int num_connect, num_accept;
    struct sockaddr_in serv_adr, serv_addr;
    // int clnt_adr_sz;

    // int clnt_count = 0;
    int clnt_socks[MAX_CLNT];
    int serv_socks[MAX_CLNT];
    struct sockaddr_in addr_info[MAX_CLNT];

    accept_thread_t accept_thread_arg;
    connect_thread_t connect_thread_arg;
    pthread_t accept_thread;
    pthread_t connect_thread;

    // sock to connect with server
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket() error: ");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(s_peer_ip);
    serv_addr.sin_port = htons(atoi(s_peer_port));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("connect() error: ");
        exit(1);
    }

    printf("Connected with server \n");

    // sock to connect with other recevier
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        perror("[Server] sock() error: ");
        exit(1);
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(port));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        perror("[Server] bind() error : ");
        exit(1);
    }
    if (listen(serv_sock, MAX_CLNT) == -1)
    {
        perror("[Server] listen() error : ");
    }

    // receive other receiver ip and port from server
    recvPkt(sock, &num_connect, sizeof(int));
    recvPkt(sock, &num_accept, sizeof(int));
    printf("num_connect %d \n", num_connect);
    printf("--- Ohter receiver Info ---\n");
    for (int i = 0; i < num_connect; i++)
    {
        recvPkt(sock, &addr_info[i], sizeof(struct sockaddr_in));
        printf("IP : %s PORT : %d \n", inet_ntoa(addr_info[i].sin_addr), ntohs(addr_info[i].sin_port));
    }

    accept_thread_arg.clnt_socks = clnt_socks;
    accept_thread_arg.serv_sock = serv_sock;
    accept_thread_arg.num_accept = num_accept;

    connect_thread_arg.addr_info = addr_info;
    connect_thread_arg.serv_socks = serv_socks;
    connect_thread_arg.num_connect = num_connect;

    printf("\n--- Start connect accept thread ---\n");

    pthread_create(&connect_thread, NULL, connectThread, (void *)&connect_thread_arg);
    pthread_create(&accept_thread, NULL, acceptThread, (void *)&accept_thread_arg);

    pthread_join(connect_thread, NULL);
    pthread_join(accept_thread, NULL);

    printf("Part1 end");
    while (1)
    {
    }

    close(sock);
}
void showHelp(char *this_name)
{
    printf("Usage : %s [OPTION] [CONTENTS]\n"
           "-s                            SET SENDING PEER\n"
           "-r                            SET RECEIVING PEER\n"
           "-n [ MAX_NUM ]                NUMBER of receving peer - for sending peer\n"
           "-f [ FILE_NAME ]              FILE NAME to send\n"
           "-g [ SEGMENT_SIZE [KB] ]      SEGMENT SIZE (KB)\n"
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

void *acceptThread(void *arg)
{
    accept_thread_t *accept_thread_arg = (accept_thread_t *)arg;
    struct sockaddr_in clnt_adr;
    socklen_t clnt_adr_sz;

    int *clnt_socks;
    int serv_sock;
    int num_accept;

    clnt_socks = accept_thread_arg->clnt_socks;
    serv_sock = accept_thread_arg->serv_sock;
    num_accept = accept_thread_arg->num_accept;

    printf("[Accept Thread] num_accept : %d \n", num_accept);

    for (int i = 0; i < num_accept; i++)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        printf("[Accept Thread] before accept \n");
        clnt_socks[i] = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        printf("[Accept Thread] %d connected \n", clnt_socks[i]);
    }

    return NULL;
}
void *connectThread(void *arg)
{
    connect_thread_t *connect_thread_arg = (connect_thread_t *)arg;
    struct sockaddr_in *addr_info;
    int *serv_socks;
    int num_connect;

    addr_info = connect_thread_arg->addr_info;
    serv_socks = connect_thread_arg->serv_socks;
    num_connect = connect_thread_arg->num_connect;

    printf("[Connect Thread] num_connect : %d \n", num_connect);

    for (int i = 0; i < num_connect; i++)
    {
        serv_socks[i] = socket(PF_INET, SOCK_STREAM, 0);
        connect(serv_socks[i], (struct sockaddr *)&addr_info[i], sizeof(addr_info[i]));
        printf("[Connect Thread] Connected IP : %s PORT : %d \n", inet_ntoa(addr_info[i].sin_addr), ntohs(addr_info[i].sin_port));
    }

    return NULL;
}
