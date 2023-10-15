#include <stdnfs.h>
#include <cs_sockets.h>
#include <cs_threads.h>
#include <cs_systemio.h>
#include <net_common.h>

#define BUFFER_SIZE 64
#define DEF_ARG_COUNT 256

NetPacketQueue* g_packet_queue = NULL;

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

ThreadArg net_server_handler(ThreadArg args) {
    Socket* s = (Socket*)args;

    char* buffer = (char*)malloc(BUFFER_SIZE);
    usize arg_count = DEF_ARG_COUNT;
    usize args_size = arg_count * sizeof(char*);
    const char** cmd_args = (const char**)malloc(args_size);

    usize sent_bytes = 0;
    while (s->connected) {
        printf("> ");

        char* f = fgets(buffer, BUFFER_SIZE, stdin);
        if (f == NULL || *f == '\n')
            continue;

        char* n = strrchr(buffer, '\n');
        *n = 0;

        parse_command(buffer, &cmd_args, &args_size, &arg_count);

        if (!strcmp(cmd_args[0], "ls")) {
            NetPacket* packet = NetPacket_New(NetPacketType_ListEntries, NULL, 0);
            NetPacket_Send(s, packet);
            NetPacket_Dispose(packet);
        } else if (!strcmp(cmd_args[0], "fget")) {
            NetPacket* packet = NetPacket_New(NetPacketType_FileDownloadRequest, NULL, 0);
            NetPacket_AddData(packet, (u8*)cmd_args[1], strlen(cmd_args[1]) + 1);
            NetPacket_Send(s, packet);
            NetPacket_Dispose(packet);

            packet = NULL;
            while ((packet = NetPacketQueue_TryPop(g_packet_queue)) == NULL) ;
            if (packet) {
                if (packet->header.id == NetPacketType_Error) {
                    printf("Received an error from the server: %s\n", (const char*)packet->buffer);
                } else if (packet->header.id == NetPacketType_FileInfo) {
                    const usize file_size = *(usize*)packet->buffer;
                    usize bytes_received = 0;

                    bool error = false;
                    printf("Download started, file size: %zu\n", file_size);
                    char file_name[2048];
                    strcpy(file_name, cmd_args[1]);
                    strcat(file_name, ".__b");
                    FILE* fs = fopen(file_name, "w");
                    if (fs) {
                        while (bytes_received < file_size) {
                            NetPacket* file_packet = NULL;
                            while ((file_packet = NetPacketQueue_TryPop(g_packet_queue)) == NULL) ;
                            if (file_packet->header.id == NetPacketType_FileDownloadData) {
                                bytes_received += file_packet->header.size;
                                fwrite(file_packet->buffer, bytes_received, 1, fs);
                                NetPacket_Dispose(file_packet);
                                printf("\rProgress: %f   ", (bytes_received / file_size) * 100.0);
                            } else if (file_packet->header.id == NetPacketType_Error) {
                                fprintf(stderr, "File download error: %s\n", (const char*)file_packet->buffer);
                                error = true;
                                break;
                            }
                        }
                        if (!error)
                            puts("\nDownload finished.");
                        else
                            puts("\nDownload failed.");
                        fclose(fs);
                    }
                } else {
                    puts("What the fuck did i just receive?");
                }
            } else {
                fputs("Received a null packet\n", stderr);
            }
            NetPacket_Dispose(packet);
        } else if (!strcmp(cmd_args[0], "fup")) {
            NetPacket* packet = NetPacket_New(NetPacketType_FileUploadRequest, NULL, 0);
            NetPacket_AddData(packet, (u8*)cmd_args[1], strlen(cmd_args[1]) + 1);

            usize file_size;
            FILE* fs = fopen(cmd_args[1], "r");
            if (fs) {
                fseek(fs, 0, SEEK_SET);
                file_size = ftell(fs);
                fclose(fs);
            } else {
                fprintf(stderr, "Failed to open file %s for reading.\n", cmd_args[1]);
                NetPacket_Dispose(packet);
                continue;
            }

            printf("File: %s Size: %zu\n", cmd_args[1], file_size);
            NetPacket_AddData(packet, (u8*)file_size, sizeof(usize));
            NetPacket_Send(s, packet);
            NetPacket_Dispose(packet);
        } else if (!strcmp(cmd_args[0], "exit")) {
            Socket_Shutdown(s, CS_SD_BOTH);
            Socket_Close(s);
        }
    }

    free(cmd_args);
    free(buffer);
    return NULL;
}

i32 main(const i32 argc, const char* argv[]) {
    u16 port = 0;
    const char* ipv4 = NULL;

    if (argc > 1) {
        ipv4 = argv[1];
        port = atoi(argv[2]);
    } else {
        puts("Usage: nfc [ IPv4 ] [ port ]");
        return 0;
    }

    CSSocket_Init();

    g_packet_queue = NetPacketQueue_New();

    Socket* server = Socket_New(AddressFamily_InterNetwork, SocketType_Stream, ProtocolType_Tcp);
    IPEndPoint ep = IPEndPoint_New(IPAddress_Parse(ipv4), AddressFamily_InterNetwork, port);
    if (Socket_Connect(server, ep) == CS_SOCKET_ERROR) {
        fprintf(stderr, "Failed to connect to [%s:%hu].\n", ep.address.str, ep.port);
        exit(EXIT_FAILURE);
    }
    printf("Connected to [%s:%hu].\n", ep.address.str, ep.port);

    ThreadAttributes attr;
    attr.args = (ThreadArg)server;
    attr.initial_stack_size = 0;
    attr.detached = false;
    attr.routine = net_server_handler;
    Thread* server_handler = Thread_New(&attr);

    while (server->connected) {
        NetPacket* recv_packet = NetPacket_Receive(server);

        if (!recv_packet)
            continue;

        switch (recv_packet->header.id) {
            case NetPacketType_Message:
                printf("\n%s", (const char*)recv_packet->buffer);
                NetPacket_Dispose(recv_packet);
                break;
            default:
                //printf("Received packet. Type: %s\n", NetPacket_GetTypeStr(recv_packet));
                if (!NetPacketQueue_TryAdd(g_packet_queue, recv_packet)) {
                    fputs("Failed to add packet to the queue.\n", stderr);
                    NetPacket_Dispose(recv_packet);
                }
                //puts("Placed into the queue.");
                break;
        }
    }

    NetPacketQueue_Dispose(g_packet_queue);
    Thread_Join(server_handler);
    Thread_Dispose(server_handler);
    Socket_Dispose(server);
    return 0;
}
