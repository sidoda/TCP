#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>

#define BUF_SIZE 1024

typedef struct {
	int type;
	char name[BUF_SIZE];
	off_t size;

}	info_pkt_t;

typedef struct {
	char name[BUF_SIZE];
	off_t size;
	char content[BUF_SIZE];
	int cur_size;
}	file_pkt_t;

int showMenu();
void showList(int sock);
void recvFile(int sock);

int main(int argc, char* argv[])
{
	int sock;
	int menu;
	struct sockaddr_in serv_addr;
	char message[1024];

	if(argc != 3)
	{
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}

	// socket setting
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock == -1)
	{
		perror("socket() error: ");
		exit(1);
	}
	
	memset(&serv_addr, 0 , sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
	{
		perror("connect() error: ");
		exit(1);
	}

	printf("[Client] Connected!\n");
	
	while (1)
	{
		
		// get a command
		while ((menu = showMenu()) == -1);
	
		printf("\n");
		sprintf(message, "%d", menu);
		write(sock, message, sizeof(message));

		if (menu == 0)
			break;

		else if (menu == 1)
			showList(sock);

		else if (menu == 2)
			recvFile(sock);
	}
	
	close(sock);
	return 0;
}

int showMenu()
{
	int menu;
	printf("\n******** Command ******** \n");
	printf("0. Quit \n");
	printf("1. Show File and direcotry of server \n");
	printf("2. Recived the file from server \n");

	printf("Select the command (0~4) : ");
	scanf("%d", &menu);
	getchar();

	if (menu >= 0 && menu <= 2)
		return menu;
	else
		return -1;
}

void showList(int sock)
{
	int file_count, type;
	char** f_name;
	off_t *f_size;
	char message[BUF_SIZE];
	info_pkt_t *recv_pkt;

	// recieve # of file
	read(sock, message, sizeof(message));
	file_count = atoi(message);

	// allocate memory
	f_size = (off_t *)malloc(sizeof(off_t) * file_count);
	f_name = (char **)malloc(sizeof(char *) * file_count);
	recv_pkt = (info_pkt_t *)malloc(sizeof(info_pkt_t));
	for(int i = 0; i < file_count; i++)
	{
		f_name[i] = (char *)malloc(sizeof(char) * 1024);	
	}

	
	printf("\n****** Server List *******\n");
	// recieve file info
	for(int i = 0; i < file_count; i++)
	{
		memset(recv_pkt, 0, sizeof(info_pkt_t));
		read(sock, recv_pkt, sizeof(info_pkt_t));
		
		type = recv_pkt->type;
		strcpy(f_name[i], recv_pkt->name);
		f_size[i] = recv_pkt->size;

		// 0 - Directory / 1 - File
		if (type == 0)
		{
			printf("[Directory] %s - %ld bytes \n",f_name[i], f_size[i]);
		}
		else
		{
			printf("[File] %s - %ld bytes \n", f_name[i], f_size[i]);
		}
	}


	// deallocate memory
	for(int i = 0; i < file_count; i++)
	{
		free(f_name[i]);
	}
	
	free(recv_pkt);
	free(f_size);
	free(f_name);
}

void recvFile(int sock)
{
	char fileName[BUF_SIZE];
	int fileSize;
	FILE *fp;
	file_pkt_t *recv_pkt = malloc(sizeof(file_pkt_t));
	info_pkt_t *info = malloc(sizeof(info_pkt_t));

	// get a filename
	printf("[Client] Write the file name you want to copy : ");
	scanf("%s", fileName);
	getchar();
	write(sock, fileName, sizeof(fileName));
	read(sock, info, sizeof(info_pkt_t));

	if (info->type != 1) // condition of file
	{
		printf("%s not existed or permission denied \n", fileName);
		return;
	}

	fileSize = (int)info->size;

	// write file
	int recv_bytes = 0;
	int total_bytes = 0;
	printf("[Client] Reciving File......\n");
	fp = fopen(fileName, "wb");
	while(total_bytes < fileSize)
	{
		memset(recv_pkt, 0, sizeof(file_pkt_t));
		read(sock, recv_pkt, sizeof(file_pkt_t));

		recv_bytes = recv_pkt->cur_size;
		total_bytes += recv_bytes;
		fwrite(recv_pkt->content, 1, recv_bytes, fp);
		printf("%s %d / %ld \n", recv_pkt->name, total_bytes, recv_pkt->size);
	}

	// deallocate memory
	fclose(fp);
	free(recv_pkt);
	free(info);
	printf("[Client] Complete %s recived \n\n", fileName);
}
