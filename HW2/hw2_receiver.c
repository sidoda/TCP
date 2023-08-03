#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/signal.h>

#define BUF_SIZE 1024
#define filename "car.mp4"

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
	int serv_sock;
	int total_bytes = 0;
	int seq = 0;
	int recv_size;
	socklen_t clnt_adr_sz;

	file_pkt_t *file_pkt = malloc(sizeof(file_pkt_t));
	ack_pkt_t *ack_pkt = malloc(sizeof(ack_pkt_t));
	FILE *fp = fopen(filename, "wb");

	struct sockaddr_in serv_adr, clnt_adr;
	struct sigaction act;

	if (argc != 2)
	{
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (serv_sock == -1)
		error_handling("UDP socket creation error");

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	act.sa_handler = alarm_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGALRM, &act, 0);

	if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");

	memset(ack_pkt, 0, sizeof(ack_pkt_t));
	memset(file_pkt, 0, sizeof(file_pkt_t));

	while (1)
	{
		alarm(8);
		recv_size = recvfrom(serv_sock, file_pkt, sizeof(file_pkt_t), 0,
							 (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
		if (recv_size > 0 && file_pkt->seq == seq)
		{
			total_bytes += file_pkt->size;
			printf("%d / %ld\n", total_bytes, file_pkt->total_size);
			fwrite(file_pkt->data, 1, file_pkt->size, fp);
			seq++;
		}
		ack_pkt->seq = file_pkt->seq;
		ack_pkt->ack = 1;

		sendto(serv_sock, ack_pkt, sizeof(ack_pkt_t), 0,
			   (struct sockaddr *)&clnt_adr, clnt_adr_sz);

		if (file_pkt->size < BUF_SIZE)
			break;
	}

	printf("%s copied \n", file_pkt->name);

	free(file_pkt);
	free(ack_pkt);
	fclose(fp);
	close(serv_sock);
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
	exit(-1);
}
