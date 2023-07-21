#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>


int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	int s_num;

	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	socklen_t clnt_addr_size;

	char message[1024];
	char filePath[1024];

	DIR *dir;
	struct dirent *ent;
	int file_count = 0;
	struct stat fileStat;

	char **f_name;
	off_t *f_size;

	FILE *fp;

	if(argc != 2)
	{
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	//open server
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1)
	{
		perror("socket() error: ");
		exit(1);
	}
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_addr.sin_port=htons(atoi(argv[1]));

	if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
	{
		perror("bind() error: ");
		exit(1);
	}
	if(listen(serv_sock, 5) == -1)
	{
		perror("listen() error: ");
		exit(1);
	}

	clnt_addr_size = sizeof(clnt_addr);
	clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_addr, &clnt_addr_size);
	if(clnt_sock == -1)
	{
		perror("accept() error: ");
		exit(1);
	}


	// count # of file in server
	dir = opendir("./");
	if(dir == NULL)
	{
		perror("opendir() error: ");
		exit(1);
	}

	printf("********** File List[server] **********\n");
	while((ent = readdir(dir)) != NULL)
	{
		char filePath[1024];
		sprintf(filePath, "./%s", ent->d_name);
		stat(filePath, &fileStat);

		if(S_ISREG(fileStat.st_mode))
		{
			printf("%d. %s %ld bytes\n", file_count+1, ent->d_name, fileStat.st_size);
			file_count++;
		}
	}
	closedir(dir);

	f_size = (off_t *)malloc(sizeof(off_t) * file_count);
	f_name = (char **)malloc(sizeof(char *) * file_count);
	for(int i = 0; i < file_count; i++)
	{
		f_name[i] = (char *)malloc(sizeof(char) * file_count);
	}
	
	// send a # of file to client
	sprintf(message, "%d", file_count);
	write(clnt_sock, message, sizeof(message));

	//	send a file_name to client
	dir = opendir("./");
	if(dir == NULL)
	{
		perror("opendir() error: ");
		exit(1);
	}
	
	file_count = 0;
	while((ent = readdir(dir)) != NULL)
	{
		sprintf(filePath, "./%s", ent->d_name);
		if(stat(filePath, &fileStat) == -1)
		{
			perror("stat() error: ");
			exit(1);
		}
		// check file is regular file or not
		if(S_ISREG(fileStat.st_mode))
		{
			memset(&message, 0, sizeof(message));
			memcpy(message, &fileStat.st_size, sizeof(off_t));
			strcpy(message+sizeof(off_t), ent->d_name);
			strcpy(f_name[file_count], ent->d_name);
			f_size[file_count] = fileStat.st_size;
			file_count++;
			write(clnt_sock, message, sizeof(message));
		}
	}
	closedir(dir);
	
	while(1)
	{
		// recive selected index
		read(clnt_sock, message, sizeof(message));
		s_num = atoi(message);
		if(s_num == 0)
		{
			printf("[Server] Good Bye\n");
			break;
		}
		printf("[Server] %d. %s file selected\n", s_num, f_name[s_num-1]);

		// send a data of file
		printf("[Server] Sending File......\n");
		sprintf(filePath, "./%s", f_name[s_num-1]);
		fp = fopen(filePath, "rb");
		int read_bytes = 0;

		while((read_bytes = fread(message, 1, sizeof(message), fp)) >  0)
		{
			write(clnt_sock, message, read_bytes);
		}
		fclose(fp);
		printf("[Server] Complete\n\n");
	}
	

	// clsoe socket and deallocate memory
	free(f_size);
	for(int i = 0; i < file_count; i++)
	{
		free(f_name[i]);
	}
	free(f_name);
	close(clnt_sock);
	close(serv_sock);
	return 0;

}

