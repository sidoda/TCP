#include "p2ph.h"

void sendingPeer(int max_peer, char *file_name, int seg_size, char *port);
void receivingPeer(char *s_peer_port, char *s_peer_ip, char *port);
void *acceptThread(void *arg);
void *connectThread(void *arg);

int main(int argc, char *argv[])
{
    char option_type[BUF_SIZE] = "srn:f:g:a:p:h";
    int s_peer = 0, r_peer = 0, max_peer = 0;
    int seg_size = 0;
    char file_name[BUF_SIZE] = "";
    char s_peer_ip[BUF_SIZE] = "";
    char s_peer_port[BUF_SIZE] = "";
    char port[BUF_SIZE] = "";

    setOption(argc, argv, option_type,
              &s_peer, &r_peer, &max_peer, file_name,
              &seg_size, s_peer_ip, s_peer_port, port);

    if (s_peer == 1)
        sendingPeer(max_peer, file_name, seg_size, port);

    else
        receivingPeer(s_peer_port, s_peer_ip, port);

    return 0;
}

void sendingPeer(int max_peer, char *file_name, int seg_size, char *port)
{
    int serv_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    int value;

    int clnt_count = 0;
    int clnt_socks[MAX_CLNT];
    struct sockaddr_in addr_info[MAX_CLNT];

    FILE *fp;

    printf("MAX_PEER is %d\n", max_peer);
    seg_size *= 1024;
    printf("SEGMENT_SIZE is %d [bytes]\n", seg_size);

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        perror("[Sender] sock() error: ");
        exit(1);
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(port));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        perror("[Sender] bind() error : ");
        exit(1);
    }
    if (listen(serv_sock, max_peer) == -1)
    {
        perror("[Sender] listen() error : ");
    }

    // Connect with receiver and save ip and port info
    while (clnt_count < max_peer)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_socks[clnt_count] = accept(serv_sock, (struct sockaddr *)&clnt_adr, (socklen_t *)&clnt_adr_sz);

        memcpy(&addr_info[clnt_count], &clnt_adr, sizeof(struct sockaddr_in));
        clnt_count++;
    }

    for (int i = 0; i < max_peer; i++)
    {
        recvPkt(clnt_socks[i], &addr_info[i].sin_port, sizeof(u_short));
    }

    printf("--- Clienct list ---\n");
    for (int i = 0; i < max_peer; i++)
    {
        printf("[Sender] socket number %d \n", clnt_socks[i]);
        printf("[Sender] IP : %s listen_PORT : %d \n", inet_ntoa(addr_info[i].sin_addr), addr_info[i].sin_port);
    }
    // Send the ip and port of recevier to recevier
    for (int i = 1; i < max_peer + 1; i++)
    {
        value = max_peer - i;
        write(clnt_socks[i - 1], &value, sizeof(int));
        value = i - 1;
        write(clnt_socks[i - 1], &value, sizeof(int));

        for (int j = i; j < max_peer; j++)
        {
            write(clnt_socks[i - 1], &addr_info[j], sizeof(struct sockaddr_in));
        }
    }

    for (int i = 0; i < max_peer; i++)
    {
        recvPkt(clnt_socks[i], &value, sizeof(int));
        if (value != 1)
        {
            printf("[Sender] Error in connect between receiver \n");
            exit(1);
        }
    }

    printf("\n[Sender] p2p init complte ! \n");
    // ----------------- part 2 -------------------
    while (1)
    {
    }

    for (int i = 0; i < max_peer; i++)
        close(clnt_socks[i]);
    close(serv_sock);
}

void receivingPeer(char *s_peer_port, char *s_peer_ip, char *port)
{
    int sock, serv_sock, sd_size;
    int num_connect, num_accept;
    int value;
    struct sockaddr_in serv_adr, serv_addr;
    u_short listen_port = htons(atoi(port));
    // int clnt_adr_sz;

    // int clnt_count = 0;
    int *clnt_socks;
    int *serv_socks;
    int *socks;
    struct sockaddr_in *addr_info;

    accept_thread_t accept_thread_arg;
    connect_thread_t connect_thread_arg;
    pthread_t accept_thread;
    pthread_t connect_thread;

    // sock to connect with sender
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket() error: ");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(s_peer_ip);
    serv_addr.sin_port = htons(atoi(s_peer_port));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("connect() error: ");
        exit(1);
    }

    printf("Connected with sender \n");
    write(sock, &listen_port, sizeof(u_short)); // send listen port

    // sock to connect with other recevier
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        perror("[Receiver] sock() error: ");
        exit(1);
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(port));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        perror("[Receiver] bind() error : ");
        exit(1);
    }
    if (listen(serv_sock, MAX_CLNT) == -1)
    {
        perror("[Receiver] listen() error : ");
    }

    // receive other receiver ip and port from server
    recvPkt(sock, &num_connect, sizeof(int));
    recvPkt(sock, &num_accept, sizeof(int));

    sd_size = num_accept + num_connect;
    socks = malloc(sizeof(int) * sd_size);
    addr_info = malloc(sizeof(struct sockaddr_in) * num_connect);
    clnt_socks = malloc(sizeof(int) * num_accept);
    serv_socks = malloc(sizeof(int) * num_connect);

    printf("--- Ohter receiver Info ---\n");
    for (int i = 0; i < num_connect; i++)
    {
        recvPkt(sock, &addr_info[i], sizeof(struct sockaddr_in));
        printf("[Receiver] IP : %s PORT : %d \n", inet_ntoa(addr_info[i].sin_addr), ntohs(addr_info[i].sin_port));
    }

    accept_thread_arg.clnt_socks = clnt_socks;
    accept_thread_arg.serv_sock = serv_sock;
    accept_thread_arg.num_accept = num_accept;

    connect_thread_arg.addr_info = addr_info;
    connect_thread_arg.serv_socks = serv_socks;
    connect_thread_arg.num_connect = num_connect;

    printf("\n--- Start Thread ---\n");

    pthread_create(&accept_thread, NULL, acceptThread, (void *)&accept_thread_arg);
    pthread_create(&connect_thread, NULL, connectThread, (void *)&connect_thread_arg);

    pthread_join(accept_thread, NULL);
    pthread_join(connect_thread, NULL);

    value = 0;
    for (int i = 0; i < num_accept; i++)
    {
        socks[value] = clnt_socks[i];
        value++;
    }
    for (int i = 0; i < num_connect; i++)
    {
        socks[value] = serv_socks[i];
        value++;
    }

    printf("\n --- sock descripter list ---\n");
    for (int i = 0; i < sd_size; i++)
    {
        printf("%d \n", socks[i]);
    }

    printf("--- End Thread ---\n");
    printf("\n[Receiver] P2P init complete \n");
    value = 1;
    write(sock, &value, sizeof(int));
    // ----------------------- part 2 ------------------------------

    while (1)
    {
    }

    free(addr_info);
    free(socks);
    free(serv_socks);
    free(clnt_socks);
    close(sock);
}

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
