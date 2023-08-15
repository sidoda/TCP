#include "p2ph.h"

void sendingPeer(int max_peer, char *file_name, int seg_size, char *port);
void receivingPeer(char *s_peer_port, char *s_peer_ip, char *port);
void *FromReceiverThread(void *arg);
void *FromSenderThread(void *arg);
void *WriteFileThread(void *arg);
void PrintSenderPercent(int file_total_size, int total_size, int id, int cur_size, double total_time_sec, double part_time_sec);

typedef struct
{
    int sock;
    int *socks;
    int sd_size;
    int seg_size;
} from_sender_thread_t;

typedef struct
{
    int sock;
    int seg_size;
} from_receiver_thread_t;

typedef struct
{
    FILE *fp;
    int seg_size;
} write_file_thread_t;

struct node *head = NULL;
pthread_mutex_t linked_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    pthread_mutex_init(&linked_mutex, NULL);
    pthread_mutex_init(&print_mutex, NULL);

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

    printf("MAX_PEER is %d\n", max_peer);
    seg_size *= 1024;
    printf("SEGMENT_SIZE is %d [bytes]\n", seg_size);

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        perror("[Sender] sock() error: ");
        exit(1);
    }

    int optval = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

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

    printf("\n--- Clienct list ---\n");
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
        value = seg_size;
        write(clnt_socks[i - 1], &value, sizeof(int));
        write(clnt_socks[i - 1], file_name, BUF_SIZE);

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

    printf("\n--- sock descripter list ---\n");
    for (int i = 0; i < max_peer; i++)
    {
        printf("receiver %d : %d \n", i, clnt_socks[i]);
    }

    printf("\n[Sender] p2p init complte ! \n\n");

    // ----------------- part 2 -------------------
    char *content = malloc(sizeof(char) * seg_size);
    int cur_size;
    int seq = 0;
    int file_total_size = 0;
    int total_size;

    long long elapsed_ns;
    double total_time_sec;
    double part_time_sec;
    FILE *fp = fopen(file_name, "rb");

    fseek(fp, 0, SEEK_END);
    file_total_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // send file to receivers
    struct timespec total_start_time, total_end_time;
    struct timespec part_start_time, part_end_time;

    for (int i = 0; i < max_peer + 1; i++)
        printf("\n");
    printf("\x1b[%dA\r", max_peer + 1);
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &total_start_time);
    while ((cur_size = fread(content, 1, seg_size, fp)) > 0)
    {
        clock_gettime(CLOCK_MONOTONIC, &part_start_time);

        write(clnt_socks[seq % max_peer], &seq, sizeof(int));
        write(clnt_socks[seq % max_peer], content, seg_size);
        write(clnt_socks[seq % max_peer], &cur_size, sizeof(int));
        total_size += cur_size;

        clock_gettime(CLOCK_MONOTONIC, &part_end_time);
        elapsed_ns = (part_end_time.tv_sec - part_start_time.tv_sec) * 1000000000LL +
                     (part_end_time.tv_nsec - part_start_time.tv_nsec);
        part_time_sec = (double)elapsed_ns / 1000000000.0;

        clock_gettime(CLOCK_MONOTONIC, &total_end_time);
        elapsed_ns = (total_end_time.tv_sec - total_start_time.tv_sec) * 1000000000LL +
                     (total_end_time.tv_nsec - total_start_time.tv_nsec);
        total_time_sec = (double)elapsed_ns / 1000000000.0;

        PrintSenderPercent(file_total_size, total_size, (seq % max_peer) + 1, cur_size, total_time_sec, part_time_sec);

        seq++;
    }

    printf("\x1b[%dB\r", max_peer + 1);
    printf("\n");
    fflush(stdout);

    // send a signal to receivers for notice end
    for (int i = 0; i < max_peer; i++)
    {
        seq = -1;
        write(clnt_socks[i], &seq, sizeof(int));
    }
    printf("[Server] Good Bye\n");

    for (int i = 0; i < max_peer; i++)
        close(clnt_socks[i]);
    close(serv_sock);
}

void receivingPeer(char *s_peer_port, char *s_peer_ip, char *port)
{
    int sock, serv_sock, sd_size;
    int num_connect, num_accept;
    int value;
    int seg_size;
    struct sockaddr_in serv_adr, serv_addr;
    u_short listen_port = htons(atoi(port));

    int *clnt_socks;
    int *serv_socks;
    int *socks;
    struct sockaddr_in *addr_info;
    char file_name[BUF_SIZE];

    accept_thread_t accept_thread_arg;
    connect_thread_t connect_thread_arg;
    pthread_t accept_thread;
    pthread_t connect_thread;

    from_sender_thread_t from_sender_thread_arg;
    from_receiver_thread_t *from_receiver_thread_arg;
    pthread_t from_sender_thread;
    pthread_t *from_receiver_thread;

    write_file_thread_t write_file_thread_arg;
    pthread_t write_file_thread;
    FILE *fp;

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

    int optval = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

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
    recvPkt(sock, &seg_size, sizeof(int)); // seg_size
    recvPkt(sock, file_name, BUF_SIZE);

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
    printf("sender : %d\n", sock);
    printf("listen : %d\n", serv_sock);
    for (int i = 0; i < sd_size; i++)
    {
        printf("receiver : %d \n", socks[i]);
    }

    printf("--- End Thread ---\n");
    value = 1;
    write(sock, &value, sizeof(int));
    printf("\n[Receiver] P2P init complete \n");

    // ----------------------- part 2 ------------------------------

    // From Sender Thread
    from_sender_thread_arg.sock = sock;
    from_sender_thread_arg.socks = socks;
    from_sender_thread_arg.sd_size = sd_size;
    from_sender_thread_arg.seg_size = seg_size;
    pthread_create(&from_sender_thread, NULL, FromSenderThread, (void *)&from_sender_thread_arg);

    // From Receiver Thread
    from_receiver_thread = malloc(sizeof(pthread_t) * sd_size);
    from_receiver_thread_arg = malloc(sizeof(from_receiver_thread_t) * sd_size);

    for (int i = 0; i < sd_size; i++)
    {
        from_receiver_thread_arg[i].sock = socks[i];
        from_receiver_thread_arg[i].seg_size = seg_size;
        pthread_create(&from_receiver_thread[i], NULL, FromReceiverThread, (void *)&from_receiver_thread_arg[i]);
    }

    // File Write Trhead
    fp = fopen(file_name, "wb");
    write_file_thread_arg.fp = fp;
    write_file_thread_arg.seg_size = seg_size;
    pthread_create(&write_file_thread, NULL, WriteFileThread, (void *)&write_file_thread_arg);

    // Join Part
    for (int i = 0; i < sd_size; i++)
        pthread_join(from_receiver_thread[i], NULL);
    pthread_join(from_sender_thread, NULL);
    pthread_join(write_file_thread, NULL);

    printf("[Receiver] Good Bye~\n");

    // Deallocate Part
    free(from_receiver_thread_arg);
    free(from_receiver_thread);
    free(addr_info);
    free(socks);
    free(serv_socks);
    free(clnt_socks);
    close(sock);
}

void *FromReceiverThread(void *arg)
{
    from_receiver_thread_t *from_receiver_thread_arg = (from_receiver_thread_t *)arg;
    int sock = from_receiver_thread_arg->sock;
    int seg_size = from_receiver_thread_arg->seg_size;

    int seq;
    int cur_size;
    char *content = malloc(sizeof(char) * seg_size);

    pthread_mutex_lock(&print_mutex);
    printf("[FR_Thread %d] Init \n", sock);
    pthread_mutex_unlock(&print_mutex);

    while (1)
    {
        recvPkt(sock, &seq, sizeof(int));
        if (seq == -1)
            break;
        recvPkt(sock, content, seg_size);
        recvPkt(sock, &cur_size, sizeof(int));

        pthread_mutex_lock(&print_mutex);
        printf("[FR_thread %d] seq : %d cur_size : %d\n", sock, seq, cur_size);
        pthread_mutex_unlock(&print_mutex);

        pthread_mutex_lock(&linked_mutex);
        insert(&head, newNode(seq, content, cur_size));
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

    int seq;
    int cur_size;
    char *content = malloc(sizeof(char) * seg_size);

    while (1)
    {
        recvPkt(sock, &seq, sizeof(int));
        if (seq == -1)
        {
            for (int i = 0; i < sd_size; i++)
                write(socks[i], &seq, sizeof(int)); // send end condition to other reciever

            pthread_mutex_lock(&print_mutex);
            printf("[FS_Thread %d] before break\n", sock);
            pthread_mutex_unlock(&print_mutex);
            break; // end roop
        }
        recvPkt(sock, content, seg_size);
        recvPkt(sock, &cur_size, sizeof(int)); // given data from server

        pthread_mutex_lock(&print_mutex);
        printf("[FS_Thread %d] seq : %d cur_size : %d \n", sock, seq, cur_size);
        pthread_mutex_unlock(&print_mutex);

        pthread_mutex_lock(&linked_mutex);
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

            pthread_mutex_lock(&print_mutex);
            printf("[FW_Thread] %d saved %d %d\n", cur_seq, cur_size, seg_size);
            pthread_mutex_unlock(&print_mutex);

            delete (&head);
            cur_seq++;

            if (cur_size < seg_size)
                break;

            pthread_mutex_unlock(&linked_mutex);
        }
        else
        {
            pthread_mutex_unlock(&linked_mutex);
            usleep(1000 * 200);
        }
    }

    return NULL;
}

void PrintSenderPercent(int file_total_size, int total_size, int id, int cur_size, double total_time_sec, double part_time_sec)
{
    double completion_percentage = (double)total_size / file_total_size * 100;
    int star_num = (int)(completion_percentage / 4);
    double M = 1024 * 1024;
    double total_throughput = (total_size / total_time_sec) / M;
    double part_throughput = (cur_size / part_time_sec) / M;

    usleep(1000 * 100);
    printf("Sending Peer [");

    for (int i = 0; i < star_num; i++)
    {
        printf("*");
    }

    for (int i = star_num; i < 25; i++)
    {
        printf(" ");
    }

    printf("] %.2f%%  (%d/%dBytes)  %fMbps  (%fs)", completion_percentage, total_size, file_total_size, total_throughput, total_time_sec);

    printf("\x1b[%dB\r", id); // cusor down
    fflush(stdout);

    printf("To Receiving Peer #%d : %.1fMbps (%d Bytes Sent / %.6fs)", id, part_throughput, cur_size, part_time_sec);
    printf("\x1b[%dA\r", id); // cusor up
    fflush(stdout);

    usleep(1000 * 100);
}
