#include <cs_sockets.h>
#include <cs_threads.h>
#include <cs_systemio.h>
#include <stdnfs.h>
#include <net_common.h>

#define MAX_CLIENTS 256
#define BUFFER_SIZE 64
#define DEF_ARG_COUNT 256

typedef struct _netfs_connection {
    Socket* socket;
    bool available;
    usize id;
    Thread* owning_thread;
    Mutex* mutex;
} Connection;


Socket* g_server = NULL;
Connection* g_clients = NULL;
usize g_client_count = 0;
usize g_active_clients = 0;
Thread** g_threads;
Mutex* g_threads_mutex = NULL;
char g_root_dir[CIO_PATH_MAX];

Connection* get_next_available_slot() {
    Mutex_Lock(g_threads_mutex);
    for (usize i = 0; i < g_client_count; ++i) {
        if (!g_clients[i].available) {
            Mutex_Unlock(g_threads_mutex);
            return g_clients + i;
        }
    }
    Mutex_Unlock(g_threads_mutex);
    return NULL;
}

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

ThreadArg net_connection_handler(ThreadArg args) {
    Connection* c = (Connection*)args;
    char cwd[CIO_PATH_MAX];
    strcpy(cwd, g_root_dir);

    while (c->socket->connected &&
           Mutex_Lock(c->mutex) != MutexResult_Error &&
           c->available &&
           Mutex_Unlock(c->mutex) != MutexResult_Error) {
        NetPacket* recv_packet = NetPacket_Receive(c->socket);
        if (!recv_packet) {
            printf("Client (%zu) [%s:%hu] disconnected.\n", c->id, c->socket->remote_ep.address.str, c->socket->remote_ep.port);
            break;
        }

        switch (recv_packet->header.id) {
            case NetPacketType_Message:
                printf("Received message from [%s:%hu]: %s\n",
                       c->socket->remote_ep.address.str,
                       c->socket->remote_ep.port,
                       (const char*)recv_packet->buffer);
                break;
            case NetPacketType_ListEntries: {
                NetPacket* send_packet = NetPacket_New(NetPacketType_Message, NULL, 0);
                DirectoryInfo* dirinf = Directory_Open(cwd);
                if (dirinf) {
                    const usize DIR_BUFFER_SIZE = CIO_PATH_MAX + 1024;
                    char* dir_buffer = (char*)malloc(sizeof(char) * DIR_BUFFER_SIZE);
                    for (usize i = 0; i < dirinf->entries_count; ++i) {
                        memset(dir_buffer, 0, DIR_BUFFER_SIZE);
                        snprintf(
                            dir_buffer,
                            DIR_BUFFER_SIZE,
                            "[%zu] (%c) %zu\t%s\n",
                            i,
                            (dirinf->entries[i].type == EntryType_File) ? 'f' : 'd',
                            dirinf->entries[i].size,
                            dirinf->entries[i].name);
                        NetPacket_AddData(
                            send_packet,
                            (u8*)dir_buffer,
                            (i == dirinf->entries_count - 1) ? strlen(dir_buffer) + 1 : strlen(dir_buffer));
                    }
                    free(dir_buffer);
                    Directory_Close(dirinf);
                } else {
                    send_packet->header.id = NetPacketType_Error;
                    NetPacket_AddData(send_packet, (const u8*)"File not found", strlen("File not found") + 1);
                }
                NetPacket_Send(c->socket, send_packet);
                NetPacket_Dispose(send_packet);
                break;
            }
            case NetPacketType_FileDownloadRequest: {
                NetPacket* send_packet = NetPacket_New(NetPacketType_FileInfo, NULL, 0);
                const char* name = (const char*)recv_packet->buffer;

                FILE* fs = fopen(name, "rb");
                if (fs) {
                    fseek(fs, 0, SEEK_END);
                    usize file_size = ftell(fs);
                    fseek(fs, 0, SEEK_SET);

                    send_packet->header.id = NetPacketType_FileInfo;
                    NetPacket_AddData(send_packet, (u8*)&file_size, sizeof(file_size));
                    i32 res = NetPacket_Send(c->socket, send_packet);
                    NetPacket_Dispose(send_packet);
                    if (res == CS_SOCKET_ERROR) {
lc2:
                        fputs("Download failed.\n", stderr);
                        break;
                    }

                    const usize FILE_BUFFER_SIZE = 2048;
                    usize bytes_sent = 0;
                    u8* buffer = (u8*)malloc(FILE_BUFFER_SIZE);
                    while (bytes_sent < file_size) {
                        bytes_sent = fread(buffer, FILE_BUFFER_SIZE, 1, fs);
                        if (bytes_sent == 0) {
                            if (feof(fs))
                                bytes_sent = file_size;
                            else if (ferror(fs)) {
                                send_packet = NetPacket_New(NetPacketType_Error, (const u8*)"File read error", strlen("File read error") + 1);
                                res = NetPacket_Send(c->socket, send_packet);
                                NetPacket_Dispose(send_packet);
                                if (res == CS_SOCKET_ERROR) {
                                    goto lc2;
                                }
                                break;
                            }
                        }
                        send_packet = NetPacket_New(NetPacketType_FileDownloadData, buffer, bytes_sent);
                        NetPacket_Send(c->socket, send_packet);
                        NetPacket_Dispose(send_packet);
                    }
                    free(buffer);
                } else {
                    NetPacket_AddData(send_packet, (const u8*)"File not found", strlen("File not found") + 1);
                    NetPacket_Send(c->socket, send_packet);
                    NetPacket_Dispose(send_packet);
                }
                break;
            }
            default:
                break;
        }
        NetPacket_Dispose(recv_packet);
    }

    Socket_Dispose(c->socket);
    c->id = 0;
    c->owning_thread = NULL;
    c->available = false;
    --g_active_clients;
    return NULL;
}

void clean_man() {
    Socket_Dispose(g_server);
    free(g_clients);
    free(g_threads);
    Mutex_Dispose(g_threads_mutex);
    CSSocket_Dispose();
}

void sigint_handler(i32 signo) {
    exit(1);
}

i32 main(const i32 argc, const char* argv[]) {
    atexit(clean_man);
    signal(SIGINT, sigint_handler);

    CSSocket_Init();

    if (File_GetCurrentDirectory(g_root_dir, sizeof(g_root_dir)) == CIO_FILE_ERROR) {
        fputs("Failed to retrieve current working directory.", stderr);
        perror("nfserver");
        exit(EXIT_FAILURE);
    }

    u16 port = 0;
    if (argc > 1) {
        for (usize i = 1; i < argc; ++i) {
            if (!strcmp(argv[i], "-r")) {
                strcpy(g_root_dir, argv[++i]);
                if (chdir(g_root_dir) == -1) {
                    fputs("Bad root directory.", stderr);
                    perror("nfserver");
                    exit(EXIT_FAILURE);
                }
            } else if (!strcmp(argv[i], "-p")) {
                port = atoi(argv[++i]);
            }
        }
    } else {
        puts("Usage: nfs -r [ root_dir ] -p [ port ]");
        return 0;
    }
    if (port == 0) {
        puts("Usage: nfs -r [ root_dir ] -p [ port ]");
        return 0;
    }

    g_server = Socket_New(AddressFamily_InterNetwork, SocketType_Stream, ProtocolType_Tcp);
    IPEndPoint ep = IPEndPoint_New(IPAddress_New(IPAddressType_Any), AddressFamily_InterNetwork, port);
    if (Socket_Bind(g_server, ep) == CS_SOCKET_ERROR) {
        exit(EXIT_FAILURE);
    }

    if (Socket_Listen(g_server, MAX_CLIENTS) == CS_SOCKET_ERROR) {
        exit(EXIT_FAILURE);
    }
    printf("Listening on 127.0.0.1:%hu\n", ep.port);

    g_client_count = MAX_CLIENTS;
    g_clients = (Connection*)malloc(MAX_CLIENTS * sizeof(Connection));
    memset(g_clients, 0, g_client_count * sizeof(Connection));
    g_threads = (Thread**)malloc(sizeof(Thread*) * g_client_count);
    g_threads_mutex = Mutex_New();

    bool running = true;
    while (running) {
        Socket* new_client = Socket_Accept(g_server);
        if (new_client) {
            Connection* conn = get_next_available_slot();
            if (conn) {
                conn->socket = new_client;
                conn->available = true;
                conn->id = g_active_clients++;
                conn->mutex = Mutex_New();

                ThreadAttributes attr;
                attr.args = (ThreadArg)conn;
                attr.initial_stack_size = 0;
                attr.routine = net_connection_handler;

                // NOTE: We want our threads here to be detached, meaning they themselves will
                // handle their disposal (lol) and resource deallocation after finishing execution.
                // This is usually dangerous but we do not care because we are C programmers therefore ballsy,
                // no but for real, the main thread does not depend on any of the child threads
                // and we do not access them after creation (return value), or join them so this is safe.
                // I do believe this is better than writing some form of a thread manager that manages
                // client threads which itself is managed by the main thread.
                attr.detached = true;

                conn->owning_thread = Thread_New(&attr);

                printf("Client (%zu) [%s:%hu] connected.\n", conn->id, new_client->remote_ep.address.str, new_client->remote_ep.port);
            } else {
                printf("Client [%s:%hu] tried to connect but max connection capacity (%d) exceeded.\n", new_client->remote_ep.address.str, new_client->remote_ep.port, MAX_CLIENTS);

                const char* error_msg = "Max connection capacity exceeded.\n";
                Socket_Send(new_client, (const u8*)error_msg, strlen(error_msg) + 1, 0);
                Socket_Dispose(new_client);
            }
        }
    }

    clean_man();
    return 0;
}
