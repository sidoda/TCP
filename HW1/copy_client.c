#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int printFileList(char **f_name, off_t *f_size, int file_count);

int main(int argc, char* argv[])
{
	int sock, file_count, s_num = 0;
	struct sockaddr_in serv_addr;
	char message[1024];

	char **f_name;
	off_t *f_size;
	char filePath[1024];

	FILE *fp;


	if(argc != 3)
	{
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}

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

	// recieve # of file
	read(sock, message, sizeof(message));
	file_count = atoi(message);

	// allocate memory
	f_size = (off_t *)malloc(sizeof(off_t) * file_count);
	f_name = (char **)malloc(sizeof(char *) * file_count);
	for(int i = 0; i < file_count; i++)
	{
		f_name[i] = (char *)malloc(sizeof(char) * 1024);	
	}

	printf("[Client] server has %d files\n", file_count);
	
	// recieve file info
	for(int i = 0; i < file_count; i++)
	{
		off_t *pt = (off_t *)message;
		char file_name[1024];
		
		memset(&message, 0, sizeof(message));
		read(sock, message, sizeof(message));
		strcpy(f_name[i], message+sizeof(off_t));
		f_size[i] = pt[0];
	}
	
	while(1)
	{
		// get index of file
		while(s_num <= 0 || s_num > file_count)
		{
			s_num = printFileList(f_name, f_size, file_count);
			if(s_num == 0)
				break;
			if(s_num == -1)
				printf("[Client] selected num is out of range\n\n");
		}
		
		// send index to server
		memset(message, 0, sizeof(message));
		sprintf(message, "%d", s_num);
		write(sock, message, sizeof(message));

		if(s_num == 0)
		{
			printf("[Client] Good Bye\n");
			break;
		}
		
		// write file
		int recv_bytes = 0;
		int total_bytes = 0;
		printf("[Client] Reciving File......\n")
		sprintf(filePath, "./%s", f_name[s_num-1]);
		fp = fopen(filePath, "wb+");
		while(total_bytes < (int)f_size[s_num-1])
		{
			recv_bytes = read(sock, message, sizeof(message));
			total_bytes += recv_bytes;
			fwrite(message, 1, recv_bytes, fp);
		}
		fclose(fp);
		printf("[Client] Complete %s recived\n\n", f_name[s_num-1]);
		s_num = 0;
	}
	

	// close and deallocate memory
	free(f_size);
	for(int i = 0; i < file_count; i++)
	{
		free(f_name[i]);
	}
	free(f_name);
	close(sock);
	return 0;
}

int printFileList(char **f_name, off_t *f_size, int file_count)
{
	char input_num[64];
	int s_num;

	printf("********** File List(server) **********\n");

	for(int i = 0; i < file_count; i++)
	{
		printf("%d. %s - %ld bytes\n", i+1, f_name[i], f_size[i]);
	}

	printf("Select the index of file you want to copy (Quit: q): ");

	scanf("%s", input_num);
	getchar();
	if(strcmp(input_num, "q") == 0 || strcmp(input_num, "Q") == 0)
		return 0;
	else
	{
		s_num = atoi(input_num);
	}

	if(s_num <= 0 || s_num > file_count)
		return -1;

	return s_num;
}
