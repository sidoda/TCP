#ifndef __MY_THREADS_H__
#define __MY_THREADS_H__

#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>

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
    int sock;
    int *socks;
    int sd_size;
    int seg_size;
    long total_file_size;
    int my_id;
} from_sender_thread_t;

typedef struct
{
    int sock;
    int seg_size;
    long total_file_size;
    int my_id;
    int id;
    int position;
} from_receiver_thread_t;

typedef struct
{
    FILE *fp;
    int seg_size;
} write_file_thread_t;

extern struct node *head;
extern pthread_mutex_t linked_mutex;

extern int receiver_total_size;
extern struct timespec total_start_time, total_end_time;
extern struct timespec part_start_time, part_end_time;

int recvPkt(int sock, void *dest, int pkt_size);

void *acceptThread(void *arg);
void *connectThread(void *arg);

void *FromReceiverThread(void *arg);
void *FromSenderThread(void *arg);
void *WriteFileThread(void *arg);
void PrintSenderPercent(long total_file_size, int total_size, int id, int cur_size, double total_time_sec, double part_time_sec);
void PrintReceiverPercentR(long total_file_size, int total_size, int my_id, int id, int cur_size, double total_time_sec, double part_time_sec, int position);
void PrintReceiverPercentS(long total_file_size, int total_size, int my_id, int cur_size, double total_time_sec, double part_time_sec);

// Thread for connect
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

// Thread for download file
void *FromReceiverThread(void *arg)
{
    from_receiver_thread_t *from_receiver_thread_arg = (from_receiver_thread_t *)arg;
    int sock = from_receiver_thread_arg->sock;
    int seg_size = from_receiver_thread_arg->seg_size;
    long total_file_size = from_receiver_thread_arg->total_file_size;
    int my_id = from_receiver_thread_arg->my_id;
    int id = from_receiver_thread_arg->id;
    int position = from_receiver_thread_arg->position;

    int seq;
    int cur_size;
    char *content = malloc(sizeof(char) * seg_size);

    long long elapsed_ns;
    double total_time_sec;
    double part_time_sec;

    while (1)
    {
        recvPkt(sock, &seq, sizeof(int));
        if (seq == -1)
            break;

        pthread_mutex_lock(&linked_mutex);

        clock_gettime(CLOCK_MONOTONIC, &part_start_time);

        recvPkt(sock, content, seg_size);
        recvPkt(sock, &cur_size, sizeof(int));
        receiver_total_size += cur_size;

        clock_gettime(CLOCK_MONOTONIC, &part_end_time);
        elapsed_ns = (part_end_time.tv_sec - part_start_time.tv_sec) * 1000000000LL +
                     (part_end_time.tv_nsec - part_start_time.tv_nsec);
        part_time_sec = (double)elapsed_ns / 1000000000.0;

        clock_gettime(CLOCK_MONOTONIC, &total_end_time);
        elapsed_ns = (total_end_time.tv_sec - total_start_time.tv_sec) * 1000000000LL +
                     (total_end_time.tv_nsec - total_start_time.tv_nsec);
        total_time_sec = (double)elapsed_ns / 1000000000.0;
        PrintReceiverPercentR(total_file_size, receiver_total_size, my_id, id, cur_size, total_time_sec, part_time_sec, position);

        insert(&head, newNode(seq, content, cur_size)); // save to linked list

        pthread_mutex_unlock(&linked_mutex);
    }

    return NULL;
}
void *FromSenderThread(void *arg)
{
    from_sender_thread_t *from_sender_thread_arg = (from_sender_thread_t *)arg;
    int sock = from_sender_thread_arg->sock;
    int *socks = from_sender_thread_arg->socks;
    int sd_size = from_sender_thread_arg->sd_size;
    int seg_size = from_sender_thread_arg->seg_size;
    long total_file_size = from_sender_thread_arg->total_file_size;
    int my_id = from_sender_thread_arg->my_id;

    int seq;
    int cur_size;
    char *content = malloc(sizeof(char) * seg_size);

    long long elapsed_ns;
    double total_time_sec;
    double part_time_sec;

    while (1)
    {
        recvPkt(sock, &seq, sizeof(int));
        if (seq == -1)
        {
            for (int i = 0; i < sd_size; i++)
                write(socks[i], &seq, sizeof(int)); // send end condition to other reciever
            break;                                  // end roop
        }

        pthread_mutex_lock(&linked_mutex);

        clock_gettime(CLOCK_MONOTONIC, &part_start_time);

        recvPkt(sock, content, seg_size);
        recvPkt(sock, &cur_size, sizeof(int)); // given data from server
        receiver_total_size += cur_size;

        clock_gettime(CLOCK_MONOTONIC, &part_end_time);
        elapsed_ns = (part_end_time.tv_sec - part_start_time.tv_sec) * 1000000000LL +
                     (part_end_time.tv_nsec - part_start_time.tv_nsec);
        part_time_sec = (double)elapsed_ns / 1000000000.0;

        clock_gettime(CLOCK_MONOTONIC, &total_end_time);
        elapsed_ns = (total_end_time.tv_sec - total_start_time.tv_sec) * 1000000000LL +
                     (total_end_time.tv_nsec - total_start_time.tv_nsec);
        total_time_sec = (double)elapsed_ns / 1000000000.0;
        PrintReceiverPercentS(total_file_size, receiver_total_size, my_id, cur_size, total_time_sec, part_time_sec);

        insert(&head, newNode(seq, content, cur_size)); // save to linked list

        pthread_mutex_unlock(&linked_mutex);

        for (int i = 0; i < sd_size; i++) // send data to other reciever
        {
            write(socks[i], &seq, sizeof(int));
            write(socks[i], content, seg_size);
            write(socks[i], &cur_size, sizeof(int));
        }
    }

    return NULL;
}

// Thread for file write
void *WriteFileThread(void *arg)
{

    write_file_thread_t *write_file_thread_arg = (write_file_thread_t *)arg;
    int cur_seq = 0;
    int cur_size = 0;
    FILE *fp = write_file_thread_arg->fp;
    int seg_size = write_file_thread_arg->seg_size;

    while (1)
    {
        pthread_mutex_lock(&linked_mutex);
        if (cur_seq == getTopSeq(head))
        {

            cur_size = head->data.cur_size;
            fwrite(head->data.content, 1, cur_size, fp);

            delete (&head);
            cur_seq++;

            if (cur_size < seg_size)
                break;

            pthread_mutex_unlock(&linked_mutex);
        }
        else
        {
            pthread_mutex_unlock(&linked_mutex);
            usleep(1000 * 300);
        }
    }

    return NULL;
}

// console function
void PrintSenderPercent(long total_file_size, int total_size, int id, int cur_size, double total_time_sec, double part_time_sec)
{
    double completion_percentage = (double)total_size / total_file_size * 100;
    int star_num = (int)(completion_percentage / 4);
    double M = 1024 * 1024;
    double total_throughput = (total_size / total_time_sec) / M;
    double part_throughput = (cur_size / part_time_sec) / M;

    printf("\033[K"); // clear line
    printf("Sending Peer [");

    for (int i = 0; i < star_num; i++)
    {
        printf("*");
    }

    for (int i = star_num; i < 25; i++)
    {
        printf(" ");
    }

    printf("] %.2f%%  (%d/%ld Bytes)  %f Mbps  (%f s) ", completion_percentage, total_size, total_file_size, total_throughput, total_time_sec);

    printf("\x1b[%dB\r", id); // cusor down
    fflush(stdout);

    printf("\033[K"); // clear line
    printf("To Receiving Peer #%d : %.1f Mbps (%d Bytes Sent / %.6f s) ", id, part_throughput, cur_size, part_time_sec);
    printf("\x1b[%dA\r", id); // cusor up
    fflush(stdout);

    usleep(1000 * 30);
}
void PrintReceiverPercentS(long total_file_size, int total_size, int my_id, int cur_size, double total_time_sec, double part_time_sec)
{
    double completion_percentage = (double)total_size / total_file_size * 100;
    int star_num = (int)(completion_percentage / 4);
    double M = 1024 * 1024;
    double total_throughput = (total_size / total_time_sec) / M;
    double part_throughput = (cur_size / part_time_sec) / M;

    printf("\033[K"); // clear line
    printf("Receiving Peer %d [", my_id);

    for (int i = 0; i < star_num; i++)
    {
        printf("*");
    }

    for (int i = star_num; i < 25; i++)
    {
        printf(" ");
    }

    printf("] %.2f%%  (%d/%ld Bytes)  %f Mbps  (%f s)", completion_percentage, total_size, total_file_size, total_throughput, total_time_sec);
    printf("\x1b[%dB\r", 1); // cusor down
    fflush(stdout);

    printf("\033[K"); // clear line
    printf("From Sending Peer : %.1f Mbps (%d Bytes Sent / %.6f s)", part_throughput, cur_size, part_time_sec);
    printf("\x1b[%dA\r", 1); // cusor up
    fflush(stdout);

    usleep(1000 * 30);
}
void PrintReceiverPercentR(long total_file_size, int total_size, int my_id, int id, int cur_size, double total_time_sec, double part_time_sec, int position)
{
    double completion_percentage = (double)total_size / total_file_size * 100;
    int star_num = (int)(completion_percentage / 4);
    double M = 1024 * 1024;
    double total_throughput = (total_size / total_time_sec) / M;
    double part_throughput = (cur_size / part_time_sec) / M;

    printf("\033[K"); // clear line
    printf("Receiving Peer %d [", my_id);

    for (int i = 0; i < star_num; i++)
    {
        printf("*");
    }

    for (int i = star_num; i < 25; i++)
    {
        printf(" ");
    }

    printf("] %.2f%%  (%d/%ld Bytes) %f Mbps (%f s)", completion_percentage, total_size, total_file_size, total_throughput, total_time_sec);
    printf("\x1b[%dB\r", position + 2); // cusor down
    fflush(stdout);

    printf("\033[K"); // clear line
    printf("From Receiving Peer #%d : %.1f Mbps (%d Bytes Sent / %.6f s)", id, part_throughput, cur_size, part_time_sec);
    printf("\x1b[%dA\r", position + 2); // cusor up
    fflush(stdout);

    usleep(1000 * 30);
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

#endif
