#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>

#define BUF_SIZE 1024
#define filename "car.mpr4"

typedef struct
{
    int seq;
    char name[BUF_SIZE];
    off_t total_size;
    int size;
    char data[BUF_SIZE];
} file_pkt_t;

typedef struct
{
    int seq;
    int ack;
} ack_pkt_t;

void error_handling(char *message);
void alarm_handler(int signo);

int main(int argc, char *argv[])
{
    int sock;
    int read_bytes;
    int seq = 0;
    socklen_t adr_sz;

    file_pkt_t *file_pkt = malloc(sizeof(file_pkt_t));
    ack_pkt_t *ack_pkt = malloc(sizeof(ack_pkt_t));

    FILE *fp = fopen(filename, "rb");

    struct sockaddr_in serv_adr, from_adr;
    struct sigaction act;
    struct stat fileStat;

    clock_t start, end;
    if (argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    act.sa_handler = alarm_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, 0);

    memset(ack_pkt, 0, sizeof(ack_pkt_t));
    memset(file_pkt, 0, sizeof(file_pkt_t));

    strcpy(file_pkt->name, filename);
    stat(filename, &fileStat);
    file_pkt->total_size = fileStat.st_size;

    while ((read_bytes = fread(file_pkt->data, 1, BUF_SIZE, fp)) > 0)
    {
        file_pkt->size = read_bytes;

        while (1)
        {
            memset(ack_pkt, 0, sizeof(ack_pkt_t));
            file_pkt->seq = seq;

            sendto(sock, file_pkt, sizeof(file_pkt_t), 0,
                   (struct sockaddr *)&serv_adr, sizeof(serv_adr));

            alarm(1);
            adr_sz = sizeof(from_adr);
            recvfrom(sock, ack_pkt, sizeof(ack_pkt_t), 0,
                     (struct sockaddr *)&from_adr, &adr_sz);

            printf("seq %d ack %d \n", ack_pkt->seq, ack_pkt->ack);

            if (ack_pkt->ack == 1 && ack_pkt->seq == seq)
            {
                printf("seq %d sucess \n", seq);
                if (seq == 0)
                    start = clock();
                break;
            }

            else
                printf("loss \n");
        }

        seq++;
    }
    end = clock();

    printf("\nTime : %.1lf ms\n", (double)(end - start));
    printf("Troughput : %.1lf bytes/ms\n", file_pkt->total_size / (double)(end - start));

    free(file_pkt);
    free(ack_pkt);
    fclose(fp);
    close(sock);
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void alarm_handler(int signo)
{
    return;
}
