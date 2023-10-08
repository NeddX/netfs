#include <stdnfs.h>

#if defined(__linux__) || defined(__APPLE__)
typedef int32_t socket_t;
#elif defined(_WIN32)
typedef uint32_t socket_t;
#endif

typedef struct _netfs_connection {
    socket_t socket;
    struct sockaddr_in addr;
    usize id;
} NetConnection;

void parse_command(char* restrict str, const char*** args, usize* args_size, usize* arg_count) {
    // Parse the command by splitting it into tokens seperated by space, tab and new line characters.
    i32 i = 0;
    **args = strtok(str, " \t\n");
    while (*(*args + i)) {
        if (i >= *args_size / sizeof(char*) - 1) {
            *args_size *= 2;
            *args = (const char**)realloc(*args, *args_size);
        }
        *(*args + ++i) = strtok(NULL, " \t\n");
    }
    *arg_count = i;
}

void* handle_client_async(void* args) {
    const usize BUFFER_SIZE = 1024 * 1024;

    NetConnection* c = (NetConnection*)args;
    u8* buffer = (u8*)malloc(BUFFER_SIZE);
    i32 bytes_received;

    usize args_size = 10 * sizeof(char*);
    usize arg_count = 0;
    const char** cargs = (const char**)malloc(args_size);

    while (true) {
        bytes_received = recv(c->socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            close(c->socket);
            free(args);
            free(buffer);
            puts("Client disconected.");
            break;
        }
        buffer[bytes_received] = 0;
        printf("recv from [%s:%u] (%zu): %s\n", inet_ntoa(c->addr.sin_addr), c->addr.sin_port, c->id, buffer);

        parse_command((char*)buffer, &cargs, &args_size, &arg_count);

        for (usize i = 0; i < arg_count; ++i) {
            puts(*(cargs + i));
        }

        //send(c->socket, buffer, strlen((const char*)buffer), 0);
    }

    free(buffer);
    free(args);
    pthread_exit(NULL);
}

i32 main(const i32 argc, const char* argv[]) {
    const u32 HOST = INADDR_ANY;
    const u16 PORT = 7878;
    const u32 MAX_CONNS = 1;
    const usize MAX_BUFFER_SIZE = 1024;

    socket_t server_socket, new_socket;
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

    usize id = 0;
    while (true) {
        new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &len);

        printf("New client conected %s:%u\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        NetConnection* conn = (NetConnection*)malloc(sizeof(NetConnection));
        conn->socket = new_socket;
        conn->addr = client_addr;
        conn->id = id;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client_async, (void*)conn) != 0) {
            fprintf(stderr, "Failed to create a thread.\n");
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    close(server_socket);
    return 0;
}
