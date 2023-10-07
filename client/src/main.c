#include <stdnfs.h>

#if defined(__linux__) || defined(__APPLE__)
typedef int32_t socket_t;
#elif defined(_WIN32)
typedef uint32_t socket_t;
#endif

i32 main(const i32 argc, const char* argv[]) {
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
    server_addr.sin_port = htons(7878);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        fprintf(stderr, "Connectionn with the server failed :(\n");
        perror("connect");
        exit(EXIT_FAILURE);
    }
    puts("Conected.");

    char buffer[1024];
    usize n;
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        printf("> ");
        n = 0;

        while ((buffer[n] = getchar()) != '\n' && buffer[n++] != EOF) ;

        write(sockfd, buffer, sizeof(buffer));
        memset(buffer, 0, sizeof(buffer));
        read(sockfd, buffer, sizeof(buffer));
        printf("recv: %s\n", buffer);

        if (!strncmp(buffer, "exit", 4)) {
            puts("Connection termination requested.");
            break;
        }
    }

    close(sockfd);
    return 0;
}
