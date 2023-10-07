#include <stdnfs.h>

i32 main(const i32 argc, const char* argv[]) {
    const u32 HOST = INADDR_ANY;
    const u16 PORT = 7878;
    const u32 MAX_CONNS = 1;
    const usize MAX_BUFFER_SIZE = 1024;

    i32 server_socket, client_socket;
    u32 len;
    struct sockaddr_in server_addr, client_addr;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_family = AF_INET;

    if ((bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)))) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CONNS)) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on 127.0.0.1:%u\n", PORT);

    len = sizeof(client_addr);
    client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &len);
    if (client_socket < 0) {
        fprintf(stderr, "Client failed to connect.");
        perror("accept");
    }

    char buffer[MAX_BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, sizeof(buffer));

        read(client_socket, buffer, sizeof(buffer));

        printf("recv: %s", buffer);
        //strcat(buffer, ": cool");
        write(client_socket, buffer, sizeof(buffer));

        if (!strncmp(buffer, "exit", 4)) {
            puts("Connection termination requested.");
            break;
        }
    }

    close(server_socket);
    return 0;
}
