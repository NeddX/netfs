#include <stdnfs.h>
#include <cs_sockets.h>
#include <cs_threads.h>
#include <cs_systemio.h>
#include <net_common.h>

#define BUFFER_SIZE 64
#define DEF_ARG_COUNT 256

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
        NetPacket* packet = NetPacket_New(NetPacketType_Message, NULL, 0);

        printf("> ");
        fgets(buffer, BUFFER_SIZE, stdin);
        char* n = strrchr(buffer, '\n');
        *n = 0;

        parse_command(buffer, &cmd_args, &args_size, &arg_count);

        if (!strcmp(cmd_args[0], "ls")) {
            packet->header.id = NetPacketType_ListEntries;
            NetPacket_Send(s, &packet);
        } else if (!strcmp(cmd_args[0], "fget")) {
            packet->header.id = NetPacketType_FileDownloadRequest;
            NetPacket_AddData(packet, (u8*)cmd_args[1], strlen(cmd_args[1]) + 1);
            NetPacket_Send(s, &packet);
        } else if (!strcmp(cmd_args[0], "fup")) {
            packet->header.id = NetPacketType_FileUploadRequest;
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
            NetPacket_Send(s, &packet);
        }
        
        NetPacket_Dispose(packet);
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
        NetPacket* recv_packet = NetPacket_New(NetPacketType_None, NULL, 0);

        i32 res = Socket_Receieve(server, (u8*)&recv_packet->header, sizeof(recv_packet->header), 0);
        if (res == CS_SOCKET_ERROR) {
lc1:
            fputs("Disconnected from server.\n", stderr);
            NetPacket_Dispose(recv_packet);
            break;
        }

        switch (recv_packet->header.id) {
            case NetPacketType_Message:
                NetPacket_AddData(recv_packet, NULL, recv_packet->header.size);
                res = Socket_Receieve(server, recv_packet->buffer, recv_packet->header.size, 0);
                if (res == CS_SOCKET_ERROR)
                    goto lc1;
                printf("Server: %s\n", (const char*)packet->buffer);
                break;
            default:
                break;
        }

        NetPacket_Dispose(recv_packet);
    }

    // NOTE: Terminating a thread doesn't dispose the Thread object.
    Thread_Terminate(server_handler);
    Thread_Dispose(server_handler);
    Socket_Dispose(server);
    return 0;
}
