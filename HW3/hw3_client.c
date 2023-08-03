#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>

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

int showMenu();
void showFile();
void showList(int sock);
void changedir(int sock);
void recvFile(int sock);
void sendFile(int sock);
int recvPkt(int sock, void *dest, int pkt_size);

int main(int argc, char *argv[])
{
    int sock;
    int menu;
    struct sockaddr_in serv_addr;
    char message[1024];

    if (argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket() error: ");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("connect() error: ");
        exit(1);
    }

    printf("[Client] Connected!\n");

    while (1)
    {

        // send command
        while ((menu = showMenu()) == -1)
            ;

        printf("\n");
        sprintf(message, "%d", menu);
        write(sock, message, sizeof(message));

        if (menu == 0)
            break;

        else if (menu == 1)
            showList(sock);

        else if (menu == 2)
            changedir(sock);

        else if (menu == 3)
            recvFile(sock);

        else if (menu == 4)
            sendFile(sock);
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
    printf("2. Change Directory \n");
    printf("3. Recived the file from server \n");
    printf("4. Send the file to server \n");

    printf("Select the command (0~4) : ");
    scanf("%d", &menu);
    getchar();

    if (menu >= 0 && menu <= 4)
        return menu;
    else
        return -1;
}

void showList(int sock)
{
    int file_count, type;
    char **f_name;
    off_t *f_size;
    char message[BUF_SIZE];
    info_pkt_t *recv_pkt = malloc(sizeof(info_pkt_t));

    printf("showList() recv before \n");
    // recieve # of file
    recvPkt(sock, message, BUF_SIZE);
    file_count = atoi(message);
    printf("[Client] server has %d files \n", file_count);

    // allocate memory
    f_size = (off_t *)malloc(sizeof(off_t) * file_count);
    f_name = (char **)malloc(sizeof(char *) * file_count);
    for (int i = 0; i < file_count; i++)
    {
        f_name[i] = (char *)malloc(sizeof(char) * 1024);
    }

    printf("\n****** Server List *******\n");
    // recieve file info
    for (int i = 0; i < file_count; i++)
    {
        recvPkt(sock, recv_pkt, sizeof(info_pkt_t));

        type = recv_pkt->type;
        strcpy(f_name[i], recv_pkt->name);
        f_size[i] = recv_pkt->size;

        // 0 - Directory / 1 - File
        if (type == 0)
        {
            printf("[Directory] %s - %ld bytes \n", f_name[i], f_size[i]);
        }
        else
        {
            printf("[File] %s - %ld bytes \n", f_name[i], f_size[i]);
        }
    }

    // deallocate memory
    for (int i = 0; i < file_count; i++)
    {
        free(f_name[i]);
    }

    free(recv_pkt);
    free(f_size);
    free(f_name);
}

void changedir(int sock)
{
    char dirname[BUF_SIZE];
    char st[BUF_SIZE];
    int s;
    printf("[Client] Write the directory : ");
    scanf("%s", dirname);
    getchar();

    write(sock, dirname, sizeof(dirname));
    recvPkt(sock, st, BUF_SIZE);

    s = atoi(st);

    if (s == 1)
        printf("[Client] Success to change directory %s \n", dirname);
    else
        printf("[Client] Fail to change directory");
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
    recvPkt(sock, info, sizeof(info_pkt_t));

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
    while (total_bytes < fileSize)
    {
        memset(recv_pkt, 0, sizeof(file_pkt_t));
        recvPkt(sock, recv_pkt, sizeof(file_pkt_t));

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

void sendFile(int sock)
{
    char fileName[BUF_SIZE];
    char message[BUF_SIZE];
    int read_bytes;
    FILE *fp;
    struct stat fileStat;
    file_pkt_t *send_pkt = malloc(sizeof(file_pkt_t));
    info_pkt_t *info = malloc(sizeof(info_pkt_t));

    showFile();

    printf("[Client] Write the file name you want to send : ");
    scanf("%s", fileName);
    getchar();

    strcpy(info->name, fileName);

    if (access(fileName, F_OK) != -1)
    {
        if (stat(fileName, &fileStat) == -1)
        {
            perror("sendFile() / stat() error: ");
            exit(1);
        }

        if (!S_ISREG(fileStat.st_mode))
        {

            printf("[Client] %s is not a file \n", fileName);

            info->size = -1;
            write(sock, info, sizeof(info_pkt_t));
            return;
        }

        info->size = fileStat.st_size;
        write(sock, info, sizeof(info_pkt_t));

        fp = fopen(fileName, "rb");

        strcpy(send_pkt->name, fileName);
        send_pkt->size = fileStat.st_size;
        while ((read_bytes = fread(send_pkt->content, 1, BUF_SIZE, fp)) > 0)
        {
            send_pkt->cur_size = read_bytes;
            write(sock, send_pkt, sizeof(file_pkt_t));
        }

        free(info);
        free(send_pkt);
        fclose(fp);
    }

    else
    {
        printf("[Client] %s not existed or permission dinied\n", fileName);

        sprintf(message, "%d", -1);
        write(sock, message, sizeof(message));
        return;
    }
}

void showFile()
{
    DIR *dir;
    struct dirent *ent;
    struct stat fileStat;

    dir = opendir("./");
    if (dir == NULL)
    {
        perror("opendir() error: ");
        exit(1);
    }

    printf("\n****** File List(client) ******\n");

    while ((ent = readdir(dir)) != NULL)
    {
        if (stat(ent->d_name, &fileStat) == -1)
        {
            perror("stat() error: ");
            exit(1);
        }

        if (S_ISREG(fileStat.st_mode))
            printf("%s - %ld bytes\n", ent->d_name, fileStat.st_size);
    }

    printf("\n");
}

int recvPkt(int sock, void *dest, int pkt_size)
{
    int recv_size;
    char *temp = malloc(pkt_size);

    // memset(dest, 0, pkt_size);
    while (1)
    {
        recv_size = recv(sock, temp, pkt_size, MSG_PEEK | MSG_DONTWAIT);

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
