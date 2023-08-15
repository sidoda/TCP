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
    int seq;
    char *content;
    int cur_size;
} data_pkt_t;

struct node
{
    data_pkt_t data;
    struct node *next;
};

void setOption(int argc, char *argv[], char *option_type,
               int *s_peer, int *r_peer, int *max_peer, char *file_name,
               int *seg_size, char *s_peer_ip, char *s_peer_port, char *port);
void showHelp(char *this_name);

void *acceptThread(void *arg);
void *connectThread(void *arg);

void insert(struct node **head_ref, struct node *new_node);
void delete(struct node **head_ref);
struct node *newNode(int seq, char *content, int cur_size);
void printList(struct node *head);
int getTopSeq(struct node *head);
int getSize(struct node *head);

int recvPkt(int sock, void *dest, int pkt_size);

// option parsing fuction
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

// linked-list function
void insert(struct node **head_ref, struct node *new_node)
{
    struct node *cur;

    if (*head_ref == NULL || (*head_ref)->data.seq >= new_node->data.seq)
    {
        new_node->next = *head_ref;
        *head_ref = new_node;
    }

    else
    {
        cur = *head_ref;
        while (cur->next != NULL && cur->next->data.seq < new_node->data.seq)
            cur = cur->next;
        new_node->next = cur->next;
        cur->next = new_node;
    }
}
void delete(struct node **head_ref)
{
    if (*head_ref == NULL)
        return;

    struct node *temp = *head_ref;

    *head_ref = (*head_ref)->next;

    free(temp->data.content);
    free(temp);
}
struct node *newNode(int seq, char *content, int cur_size)
{
    struct node *new_node = (struct node *)malloc(sizeof(struct node));

    new_node->data.seq = seq;
    new_node->data.cur_size = cur_size;
    new_node->data.content = malloc(sizeof(char) * cur_size);
    memcpy(new_node->data.content, content, cur_size);

    new_node->next = NULL;

    return new_node;
}
void printList(struct node *head)
{
    struct node *temp = head;
    while (temp != NULL)
    {
        printf("seq: %d cur_size: %d \n", temp->data.seq, temp->data.cur_size);

        for (int i = 0; i < temp->data.cur_size; i++)
        {
            printf("%c", temp->data.content[i]);
        }
        printf("\n");
        temp = temp->next;
    }
}
int getTopSeq(struct node *head)
{
    if (head == NULL)
        return -1;
    struct node *temp = head;

    return temp->data.seq;
}
int getSize(struct node *head)
{
    struct node *cur = head;
    int count = 0;

    while (cur != NULL)
    {
        count++;
        cur = cur->next;
    }

    return count;
}

// Thread
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

    for (int i = 0; i < num_accept; i++)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_socks[i] = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        printf("[Accept Thread] %d connected \n", clnt_socks[i]);
    }

    printf("[Accept Thread] end \n");

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

    for (int i = 0; i < num_connect; i++)
    {
        serv_socks[i] = socket(PF_INET, SOCK_STREAM, 0);
        if (connect(serv_socks[i], (struct sockaddr *)&addr_info[i], sizeof(addr_info[i])) == -1)
        {
            perror("[Connect Thread] connect() error : ");
            exit(1);
        }
        printf("[Connect Thread] Connected IP : %s PORT : %d \n", inet_ntoa(addr_info[i].sin_addr), ntohs(addr_info[i].sin_port));
    }

    printf("[Connect Thread] end \n");

    return NULL;
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
