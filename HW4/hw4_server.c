#include "datatype.h"
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#define BUF_SIZE 1024
#define MAX_CLNT 30
#define BACKSPACE 127
#define ENTER 10
#define ESC 27

int clnt_count = 0;
int clnt_socks[MAX_CLNT];
int database_size = 0;
pthread_mutex_t mutex;
Trie *trie;

void databaseSplit(char *str, Trie *head);
void *clnt_service(void *arg);
int recvPkt(int sock, void *dest, int pkt_size);

int main(int argc, char *argv[])
{
    FILE *fp;
    char msg[BUF_SIZE];

    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    int opt_val = 1;

    pthread_t p_id;

    if (argc != 3)
    {
        printf("Usage : %s <port> <data base file>\n", argv[0]);
        exit(1);
    }

    // read database and insert in Trie
    fp = fopen(argv[2], "rb");
    trie = getNewTrieNode();

    while (fp != NULL)
    {
        if (fgets(msg, BUF_SIZE, fp) == NULL)
            break;
        msg[strlen(msg) - 1] = 0;
        databaseSplit(msg, trie);
        database_size++;
    }

    // server connection
    pthread_mutex_init(&mutex, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        perror("[Server] bind() error : ");
        exit(1);
    }
    if (listen(serv_sock, 5) == -1)
    {
        perror("[Server] listen() error : ");
    }

    while (1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        setsockopt(clnt_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&opt_val, sizeof(opt_val));

        pthread_mutex_lock(&mutex);
        clnt_socks[clnt_count++] = clnt_sock;
        pthread_mutex_unlock(&mutex);

        pthread_create(&p_id, NULL, clnt_service, (void *)&clnt_sock);
        pthread_detach(p_id);
        printf("Connected client IP : %s \n", inet_ntoa(clnt_adr.sin_addr));
    }

    return 0;
}

void databaseSplit(char *str, Trie *head)
{
    char search_word[WORD_SIZE] = "";
    int search_num = 0;
    char *start;
    char *end;
    char *word_ptr = search_word;

    start = str;
    end = str;

    while (1)
    {
        end++;
        if (*end == 0)
        {
            search_num = atoi(start);
            break;
        }

        else if (*end == ' ' && *start != ' ')
        {
            *end = 0;
            strcpy(word_ptr, start);
            word_ptr = search_word + strlen(search_word);
            *word_ptr = ' ';
            word_ptr++;
            start = end + 1;
        }
    }

    word_ptr--;
    *word_ptr = 0;

    TireInsert(head, search_word, search_num);
}

void *clnt_service(void *arg)
{
    int clnt_sock = *((int *)arg);
    search_info_t **info_pkt;
    char buffer[BUF_SIZE];
    char msg[BUF_SIZE];
    char *bufptr = buffer;
    int data_size = 0;

    info_pkt = malloc(sizeof(search_info_t *) * database_size);
    for (int i = 0; i < database_size; i++)
    {
        info_pkt[i] = malloc(sizeof(search_info_t));
    }
    while (1)
    {
        data_size = 0;
        recvPkt(clnt_sock, buffer, BUF_SIZE);

        if (strcmp(buffer, "__ESC__") == 0)
            break;

        search(trie, buffer, info_pkt, &data_size);
        selectionSort(info_pkt, &data_size);

        if (strcmp(buffer, "") == 0)
            data_size = 0;

        printf("\nsearch word : %s %d\n", buffer, data_size);

        if (data_size > 10)
            data_size = 10;

        sprintf(msg, "%d", data_size);
        write(clnt_sock, msg, BUF_SIZE);

        for (int i = 0; i < data_size; i++)
        {
            printf("%s %d\n", info_pkt[i]->search_word, info_pkt[i]->search_num);
            write(clnt_sock, info_pkt[i], sizeof(search_info_t));
        }
    }

    // Disconnect and deallocate memory
    for (int i = 0; i < data_size; i++)
        free(info_pkt[i]);
    free(info_pkt);

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < clnt_count; i++)
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i++ < clnt_count - 1)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }
    clnt_count--;
    pthread_mutex_unlock(&mutex);
    printf("close %d client \n", clnt_sock);
    close(clnt_sock);
    return NULL;
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
            perror("recv(): ");

        if (recv_size == pkt_size)
            break;
    }

    recv_size = recv(sock, temp, pkt_size, 0);
    memcpy(dest, temp, pkt_size);

    free(temp);
    return recv_size;
}
