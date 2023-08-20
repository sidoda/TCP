#include "my_threads.h"

typedef struct
{
    int num_connect;
    int num_accept;
    int seg_size;
    char file_name[BUF_SIZE];
    int id;
    long total_file_size;
} s2r_pkt_t;

void setOption(int argc, char *argv[], char *option_type,
               int *s_peer, int *r_peer, int *max_peer, char *file_name,
               int *seg_size, char *s_peer_ip, char *s_peer_port, char *port);
void showHelp(char *this_name);

void sendingPeer(int max_peer, char *file_name, int seg_size, char *port);
void receivingPeer(char *s_peer_port, char *s_peer_ip, char *port);

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

    long total_file_size = 0;
    FILE *fp = fopen(file_name, "rb");
    fseek(fp, 0, SEEK_END);
    total_file_size = ftell(fp);
    fclose(fp);

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
        printf("[Sender] socket number %d ID : %d\n", clnt_socks[i], i + 1);
        printf("[Sender] IP : %s listen_PORT : %d \n", inet_ntoa(addr_info[i].sin_addr), addr_info[i].sin_port);
    }
    // Send the ip and port of recevier to recevier
    for (int i = 1; i < max_peer + 1; i++)
    {
        value = max_peer - i;
        write(clnt_socks[i - 1], &value, sizeof(int)); // num_connect
        value = i - 1;
        write(clnt_socks[i - 1], &value, sizeof(int)); // num_accept
        value = seg_size;
        write(clnt_socks[i - 1], &value, sizeof(int)); // seg_size
        write(clnt_socks[i - 1], file_name, BUF_SIZE); // file_name
        value = i;
        write(clnt_socks[i - 1], &value, sizeof(int));            // id
        write(clnt_socks[i - 1], &total_file_size, sizeof(long)); // total_file_size

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
        printf("receiver %d : %d \n", i + 1, clnt_socks[i]);
    }

    printf("\n[Sender] p2p init complte ! \n\n");

    // ----------------- part 2 -------------------
    char *content = malloc(sizeof(char) * seg_size);
    int cur_size = 0;
    int seq = 0;
    int total_size = 0;

    long long elapsed_ns = 0;
    double total_time_sec = 0;
    double part_time_sec = 0;

    for (int i = 0; i < max_peer + 1; i++)
        printf("\n");
    printf("\x1b[%dA\r", max_peer + 1);
    fflush(stdout);

    // send file to receivers
    fp = fopen(file_name, "rb");
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

        PrintSenderPercent(total_file_size, total_size, (seq % max_peer) + 1, cur_size, total_time_sec, part_time_sec);

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
    int num_connect = 0, num_accept = 0;
    int value;
    int seg_size = 0, id;
    long total_file_size = 0;
    struct sockaddr_in serv_adr, serv_addr;
    u_short listen_port = htons(atoi(port));

    int *clnt_socks;
    int *serv_socks;
    int *socks;
    struct sockaddr_in *addr_info;
    int *ids;
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

    recvPkt(sock, &num_connect, sizeof(int));      // num_connet
    recvPkt(sock, &num_accept, sizeof(int));       // num_accept
    recvPkt(sock, &seg_size, sizeof(int));         // seg_size
    recvPkt(sock, file_name, BUF_SIZE);            // file_name
    recvPkt(sock, &id, sizeof(int));               // id
    recvPkt(sock, &total_file_size, sizeof(long)); // total_file_size

    sd_size = num_accept + num_connect;
    socks = malloc(sizeof(int) * sd_size);
    ids = malloc(sizeof(int) * sd_size);
    addr_info = malloc(sizeof(struct sockaddr_in) * num_connect);
    clnt_socks = malloc(sizeof(int) * num_accept);
    serv_socks = malloc(sizeof(int) * num_connect);

    printf("--- Ohter receiver Info ---\n");
    for (int i = 0; i < num_connect; i++)
    {
        recvPkt(sock, &addr_info[i], sizeof(struct sockaddr_in));
        printf("[Receiver] IP : %s PORT : %d \n", inet_ntoa(addr_info[i].sin_addr), ntohs(addr_info[i].sin_port));
    }

    // accept & connect thread part
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

    // merge clnt_socks and serv_socks to socks
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

    value = 1;
    write(sock, &value, sizeof(int));
    printf("\n[Receiver] P2P init complete \n");

    // ----------------------- part 2 ------------------------------

    // exchange id with each other
    for (int i = 0; i < sd_size; i++)
        write(socks[i], &id, sizeof(int));
    for (int i = 0; i < sd_size; i++)
        recvPkt(socks[i], &ids[i], sizeof(int));

    printf("\n --- sock descripter list ---\n");
    printf("sender : %d\n", sock);
    printf("listen : %d\n", serv_sock);
    for (int i = 0; i < sd_size; i++)
    {
        printf("receiver %d : %d \n", ids[i], socks[i]);
    }
    printf("\n");

    // timer and terminal cursor control
    for (int i = 0; i < sd_size + 2; i++)
        printf("\n");
    printf("\x1b[%dA\r", sd_size + 2);
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &total_start_time);

    // From Sender Thread (get file from sender)
    from_sender_thread_arg.sock = sock;
    from_sender_thread_arg.socks = socks;
    from_sender_thread_arg.sd_size = sd_size;
    from_sender_thread_arg.seg_size = seg_size;
    from_sender_thread_arg.total_file_size = total_file_size;
    from_sender_thread_arg.my_id = id;

    pthread_create(&from_sender_thread, NULL, FromSenderThread, (void *)&from_sender_thread_arg);

    // From Receiver Thread (get file from other receiver)
    from_receiver_thread = malloc(sizeof(pthread_t) * sd_size);
    from_receiver_thread_arg = malloc(sizeof(from_receiver_thread_t) * sd_size);

    for (int i = 0; i < sd_size; i++)
    {
        from_receiver_thread_arg[i].sock = socks[i];
        from_receiver_thread_arg[i].seg_size = seg_size;
        from_receiver_thread_arg[i].total_file_size = total_file_size;
        from_receiver_thread_arg[i].my_id = id;
        from_receiver_thread_arg[i].id = ids[i];
        from_receiver_thread_arg[i].position = i;

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

    printf("\x1b[%dB\r", sd_size + 2);
    printf("\n");
    fflush(stdout);
    printf("[Receiver] Good Bye~\n");

    // Deallocate Part
    free(ids);
    free(from_receiver_thread_arg);
    free(from_receiver_thread);
    free(addr_info);
    free(socks);
    free(serv_socks);
    free(clnt_socks);
    close(sock);
}

// option parsing fuction
void showHelp(char *this_name)
{
    printf("Usage : %s [OPTION] [CONTENTS]\n"
           "-s                            SET SENDING PEER\n"
           "-r                            SET RECEIVING PEER\n"
           "-n [ MAX_NUM ]                NUMBER of receving peer - for sending peer\n"
           "-f [ FILE_NAME ]              FILE NAME to send\n"
           "-g [ SEGMENT_SIZE [KiB] ]      SEGMENT SIZE (KiB)\n"
           "-a [ <IP> <PORT> ]            IP and PORT for sending peer\n"
           "-p [ PORT ]                   PORT for listen\n",
           this_name);
}
void setOption(int argc, char *argv[], char *option_type,
               int *s_peer, int *r_peer, int *max_peer, char *file_name,
               int *seg_size, char *s_peer_ip, char *s_peer_port, char *port)
{

    int opt;
    struct stat fileStat;

    while ((opt = getopt(argc, argv, option_type)) != -1)
    {
        switch (opt)
        {
        case 'h':
            showHelp(argv[0]);
            break;

        case 's':
            *s_peer = 1;
            printf("This process is sending peer\n");
            break;

        case 'r':
            *r_peer = 1;
            printf("This process is receiving peer\n");
            break;

        case 'n':
            *max_peer = atoi(optarg);
            if (*max_peer > MAX_CLNT)
            {
                printf("MAX_PEER limit is %d", MAX_CLNT);
                exit(1);
            }
            break;

        case 'f':
            strcpy(file_name, optarg);
            if (access(file_name, R_OK) != 0)
            {
                printf("%s does not exist or no permission\n", file_name);
                exit(1);
            }
            stat(file_name, &fileStat);
            if (!S_ISREG(fileStat.st_mode))
            {
                printf("%s is not a file \n", file_name);
                exit(1);
            }
            break;

        case 'g':
            *seg_size = atoi(optarg);
            if (*seg_size > 2097151)
            {
                printf("Max segment size is  2097151\n");
                exit(1);
            }
            break;

        case 'a':
            strcpy(s_peer_ip, optarg);
            if (argv[optind][0] == '-')
            {
                printf("Inaccurate -a option\n");
                exit(1);
            }
            strcpy(s_peer_port, argv[optind]);
            optind++;
            if (argv[optind][0] != '-')
            {
                printf("Inaccurate -a option\n");
                exit(1);
            }
            break;

        case 'p':
            strcpy(port, optarg);
            break;

        case '?':
            printf("unkown command\n");
            showHelp(argv[0]);
            exit(1);
            break;
        }
    }

    // IF TYPE NOT DEFINED
    if ((*s_peer == 1) && (*r_peer == 1))
    {
        printf("TYPE NOT FIXED!! \n");
        showHelp(argv[0]);
        exit(1);
    }

    else if ((*s_peer == 0) && (*r_peer == 0))
    {
        printf("TYPE NOT FIXED!! \n");
        showHelp(argv[0]);
        exit(1);
    }

    // if sending peer
    if (*s_peer == 1)
    {
        if (*max_peer == 0) // -n
        {
            printf("[SENDING PEER] -n option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (strcmp(file_name, "") == 0) // -f
        {
            printf("[SENDING PEER] -f option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (*seg_size == 0) // -g
        {
            printf("[SENDING PEER] -g option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (strcmp(port, "") == 0) // -p
        {
            printf("[SENDING PEER] -p option needed\n");

            showHelp(argv[0]);
            exit(1);
        }
    }

    // if receiving peer
    else if (*r_peer == 1)
    {
        if (strcmp(s_peer_ip, "") == 0) // -a
        {
            printf("[RECEVING PEER] -a option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (strcmp(s_peer_port, "") == 0) // -a
        {
            printf("[RECEVING PEER] -a option needed\n");

            showHelp(argv[0]);
            exit(1);
        }

        if (strcmp(port, "") == 0) // -p
        {
            printf("[RECEVING PEER] -p option needed\n");

            showHelp(argv[0]);
            exit(1);
        }
    }
}
