#include <stdnfs.h>

void* handle_server_read(void* args) {
    socket_t sockfd = (socket_t)args;
    char buffer[2048];

    while (true) {
        read(sockfd, buffer, sizeof(buffer));
        printf("Recv from Server: %s\n", buffer);
    }
}

i32 main(const i32 argc, const char* argv[]) {
    u16 port;

    printf("Port> ");
    scanf("%hu", &port);

    socket_t sockfd;
    i32 connfd;

    struct sockaddr_in server_addr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "Failed to create socket.\n");
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr, 0, sizeof(struct sockaddr_in));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        fprintf(stderr, "Connectionn with the server failed :(\n");
        perror("connect");
        exit(EXIT_FAILURE);
    }
    puts("Conected.");

    char buffer[1024];
    usize n;

    pthread_t thread;
    if (pthread_create(&thread, NULL, handle_server_read, (void*)sockfd) != 0) {
        fprintf(stderr, "Failed to create a thread.\n");
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        printf("> ");
        n = 0;

        while ((buffer[n] = getchar()) != '\n' && buffer[n++] != EOF) ;

        write(sockfd, buffer, sizeof(buffer));
        memset(buffer, 0, sizeof(buffer));
    }

    close(sockfd);
    return 0;
}
