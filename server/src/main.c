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

typedef enum _netfs_packet_type {
NET_PACKET_MESSAGE,
NET_PACKET_COMMAND,
NET_PACKET_DOWNLOAD
} NetPacketType;

typedef struct _netfs_packet {

} NetPacket;

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

void cmd_ls(NetConnection* restrict c, const char** args) {
    char cwd[100];
    if (getcwd(cwd, sizeof(cwd)) != 0) {

    } else {

    }

    struct direntry* entry;


    //DIR* dp = opendir();
}

void cmd_download(NetConnection* restrict c, const char** args) {

}

void cmd_upload(NetConnection* restrict c, const char** args) {

}

void* handle_client_send_async(void* args) {
    const usize BUFFER_SIZE = 1024 * 1024;

    NetConnection* c = (NetConnection*)args;
    u8* buffer = (u8*)malloc(BUFFER_SIZE);

    while (true) {
        printf("> ");
        scanf("%s", buffer);

        send(c->socket, buffer, strlen((const char*)buffer), 0);
    }
}

void* handle_client_async(void* args) {
    const usize BUFFER_SIZE = 1024 * 1024;

    NetConnection* c = (NetConnection*)args;
    u8* buffer = (u8*)malloc(BUFFER_SIZE);
    i32 bytes_received;

    usize args_size = 10 * sizeof(char*);
    usize arg_count = 0;
    const char** cargs = (const char**)malloc(args_size);

    pthread_t thread;
    if (pthread_create(&thread, NULL, handle_client_send_async, (void*)c) != 0) {
        fprintf(stderr, "Failed to create a thread.\n");
        perror("pthread_create");
        free(buffer);
        free(args);
        pthread_exit(NULL);
    }

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
        printf("Recv from [%s:%u] (%zu): %s\n", inet_ntoa(c->addr.sin_addr), c->addr.sin_port, c->id, buffer);

        //parse_command((char*)buffer, &cargs, &args_size, &arg_count);

        /*
        if (!strcmp(cargs[0], "ls")) {
            cmd_ls(c, args);
        } else if (!strcmp(cargs[0], "download")) {
            cmd_download(c, cargs);
        } else if (!strcmp(cargs[0], "upload")) {
            cmd_upload(c, args);
        }
        */

    }

    free(buffer);
    free(args);
    pthread_exit(NULL);
}

i32 main(const i32 argc, const char* argv[]) {
    srand(time(0));
    const u32 HOST = INADDR_ANY;
    const u16 PORT = rand() % 8000;
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
        conn->id = id++;

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
