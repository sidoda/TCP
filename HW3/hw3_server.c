#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>

#define BUF_SIZE 1024

typedef struct
{
    int type;
    char name[BUF_SIZE];
    off_t size;

} info_pkt_t;

typedef struct
{
    char name[BUF_SIZE];
    off_t size;
    char content[BUF_SIZE];
    int cur_size;
} file_pkt_t;

void read_childproc(int sig);
void sendList(int clnt_sock);
void changeDir(int clnt_sock);
void sendFile(int clnt_sock);
void recvFile(int clnt_sock);
int recvPkt(int sock, void *dest, int pkt_size);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    int menu;
    char message[BUF_SIZE];
    socklen_t adr_sz;

    struct sockaddr_in serv_adr, clnt_adr;
    struct sigaction act;
    struct timeval timeout;

    fd_set reads, cpy_reads;
    int fd_max, fd_num, i;

    if (argc != 2)
    {
        printf("Usage : %s <port> \n", argv[0]);
        exit(1);
    }

    act.sa_handler = read_childproc;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, 0);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        perror("bind() error: ");
        exit(1);
    }
    if (listen(serv_sock, 5) == -1)
    {
        perror("listen() error: ");
        exit(1);
    }

    FD_ZERO(&reads);
    FD_SET(serv_sock, &reads);
    fd_max = serv_sock;

    while (1)
    {
        cpy_reads = reads;
        timeout.tv_sec = 2;
        timeout.tv_usec = 2000;

        if ((fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
        {
            printf("here\n");
            break;
        }
        if (fd_num == 0) // timeout
            continue;

        for (i = 0; i < fd_max + 1; i++)
        {
            if (FD_ISSET(i, &cpy_reads))
            {
                if (i == serv_sock)
                {
                    adr_sz = sizeof(clnt_adr);
                    clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);
                    FD_SET(clnt_sock, &reads);
                    if (fd_max < clnt_sock)
                        fd_max = clnt_sock;
                    printf("Connected new client : %d \n", clnt_sock);
                }

                else
                {
                    recvPkt(i, message, BUF_SIZE);
                    menu = atoi(message);
                    if (menu == 0)
                    {
                        FD_CLR(i, &reads);
                        close(i);
                        printf("closed client : %d \n", i);
                    }
                    else if (menu == 1)
                    {
                        printf("client %d sendList() \n", i);
                        sendList(clnt_sock);
                    }

                    else if (menu == 2)
                    {
                        printf("client %d changeDir() \n", i);
                        changeDir(clnt_sock);
                    }

                    else if (menu == 3)
                    {
                        printf("client %d sendFile() \n", i);
                        sendFile(clnt_sock);
                    }

                    else if (menu == 4)
                    {
                        printf("client %d recvFile() \n", i);
                        recvFile(clnt_sock);
                    }
                }
            }
        }
    }

    close(serv_sock);
    return 0;
}

void read_childproc(int sig)
{
    pid_t pid;
    int status;
    pid = waitpid(-1, &status, WNOHANG);
    printf("[Server] removed proc id : %d \n", pid);
}

void sendList(int clnt_sock)
{
    int count = 0;
    DIR *dir;
    struct dirent *ent;
    struct stat fileStat;
    info_pkt_t *send_pkt;
    char message[BUF_SIZE];

    char filePath[BUF_SIZE];

    dir = opendir("./");
    if (dir == NULL)
    {
        perror("opendir() error: ");
        exit(1);
    }

    // count # of files
    while ((ent = readdir(dir)) != NULL)
    {
        count++;
    }

    // send a # of files to client
    sprintf(message, "%d", count);
    write(clnt_sock, message, sizeof(message));

    // send a content of directory
    rewinddir(dir);
    send_pkt = (info_pkt_t *)malloc(sizeof(info_pkt_t));

    while ((ent = readdir(dir)) != NULL)
    {

        sprintf(filePath, "./%s", ent->d_name);
        if (stat(filePath, &fileStat) == -1)
        {
            perror("stat() error: ");
            exit(1);
        }

        // make send_pkt
        memset(send_pkt, 0, sizeof(info_pkt_t));
        if (S_ISREG(fileStat.st_mode) || S_ISDIR(fileStat.st_mode))
        {
            if (S_ISREG(fileStat.st_mode))
                send_pkt->type = 1;
            else
                send_pkt->type = 0;

            strcpy(send_pkt->name, ent->d_name);
            send_pkt->size = fileStat.st_size;

            // send_pkt
            write(clnt_sock, send_pkt, sizeof(info_pkt_t));
        }
    }

    // deallocate memory
    free(send_pkt);
    closedir(dir);
}

void changeDir(int clnt_sock)
{
    char dirname[BUF_SIZE];
    struct stat fileStat;

    read(clnt_sock, dirname, sizeof(dirname));

    if (access(dirname, R_OK) != -1)
    {
        if (stat(dirname, &fileStat) == -1)
        {
            perror("changeDir() / stat() error: ");
            exit(1);
        }

        if (S_ISDIR(fileStat.st_mode))
        {
            chdir(dirname);
            sprintf(dirname, "1");
            write(clnt_sock, dirname, sizeof(dirname));
            return;
        }
    }

    sprintf(dirname, "0");
    write(clnt_sock, dirname, sizeof(dirname));
}

void sendFile(int clnt_sock)
{
    char fileName[BUF_SIZE];
    struct stat fileStat;
    file_pkt_t *send_pkt;
    info_pkt_t *info = malloc(sizeof(info_pkt_t));
    FILE *fp;

    read(clnt_sock, fileName, sizeof(fileName));
    strcpy(info->name, fileName);
    if (access(fileName, W_OK) != -1)
    {
        if (stat(fileName, &fileStat) == -1)
        {
            perror("send File() / stat() error: ");
            exit(1);
        }

        if (S_ISREG(fileStat.st_mode))
        {
            info->size = fileStat.st_size;
            info->type = 1;
            write(clnt_sock, info, sizeof(info_pkt_t));

            fp = fopen(fileName, "rb");
            int read_bytes = 0;

            send_pkt = (file_pkt_t *)malloc(sizeof(file_pkt_t));
            strcpy(send_pkt->name, fileName);
            send_pkt->size = fileStat.st_size;

            while ((read_bytes = fread(send_pkt->content, 1, BUF_SIZE, fp)) > 0)
            {
                send_pkt->cur_size = read_bytes;
                write(clnt_sock, send_pkt, sizeof(file_pkt_t));
            }
            free(send_pkt);
            free(info);
            fclose(fp);
        }

        else
        {
            info->size = 0;
            info->type = 0;
            write(clnt_sock, info, sizeof(info_pkt_t));
        }
    }

    else
    {
        info->size = 0;
        info->type = 0;
        write(clnt_sock, info, sizeof(info_pkt_t));
    }
}

void recvFile(int clnt_sock)
{
    char file_name[BUF_SIZE];
    int file_size;
    FILE *fp;
    file_pkt_t *recv_pkt = malloc(sizeof(file_pkt_t));
    info_pkt_t *info = malloc(sizeof(info_pkt_t));

    // get a filename
    recvPkt(clnt_sock, info, sizeof(info_pkt_t));
    if (info->size == -1)
    {
        return;
    }

    strcpy(file_name, info->name);
    file_size = info->size;

    printf("[debug] %s %ld \n", info->name, info->size);

    fp = fopen(file_name, "wb");

    int total_bytes = 0;
    int recv_bytes = 0;

    while (total_bytes < (int)file_size)
    {
        memset(recv_pkt, 0, sizeof(file_pkt_t));
        recvPkt(clnt_sock, recv_pkt, sizeof(file_pkt_t));

        recv_bytes = recv_pkt->cur_size;
        total_bytes += recv_bytes;
        fwrite(recv_pkt->content, 1, recv_bytes, fp);
        printf("%s %d / %ld \n", recv_pkt->name, total_bytes, recv_pkt->size);
    }

    // deallocate memory
    fclose(fp);
    free(recv_pkt);
    free(info);
    printf("[Server] Complete %s recived \n\n", file_name);
}

int recvPkt(int sock, void *dest, int pkt_size)
{
    int recv_size;
    char *temp = malloc(pkt_size);

    // memset(dest, 0, pkt_size);
    while (1)
    {
        recv_size = recv(sock, temp, pkt_size, MSG_PEEK);

        if (recv_size == -1)
            perror("recv error : ");

        if (recv_size == pkt_size)
            break;
    }

    recv_size = recv(sock, temp, pkt_size, 0);
    memcpy(dest, temp, pkt_size);

    free(temp);

    return recv_size;
}
