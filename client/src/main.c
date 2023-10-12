#include <stdnfs.h>
#include <cs_sockets.h>
#include <cs_threads.h>
#include <cs_systemio.h>
#include <net_packet.h>

#define BUFFER_SIZE 64

ThreadArg net_server_handler(ThreadArg args) {
    Socket* s = (Socket*)args;

    u8* buffer = (u8*)malloc(BUFFER_SIZE);

    NetPacket* packet = NetPacket_New(NetPacketType_Message, NULL, 0);
    usize sent_bytes = 0;
    while (s->connected) {
        printf("> ");
        fgets((char*)buffer, BUFFER_SIZE, stdin);
        char* n = strrchr((char*)buffer, '\n');
        *n = 0;

        usize len = strlen((const char*)buffer) + 1;
        packet->buffer = (u8*)malloc(len);
        packet->header.id = NetPacketType_Message;
        packet->header.size = len;
        memcpy(packet->buffer, buffer, len);

        sent_bytes = Socket_Send(s, (u8*)&packet->header, sizeof(packet->header), 0);

        DirectoryInfo* dir = Directory_Open(".");
        for (usize i = 0; i < dir->entries_count; ++i) {
            printf("[%zu]\t(%c)\t%zu\t%s\n", i, (dir->entries[i].type == EntryType_File) ? 'f' : 'd', dir->entries[i].size, dir->entries[i].name);
        }
        Directory_Close(dir);

        if (sent_bytes == CS_SOCKET_ERROR) {
lc0:
            fputs("Disconnected from server.\n", stderr);
            break;
        }

        sent_bytes = Socket_Send(s, packet->buffer, packet->header.size, 0);
        if (sent_bytes == CS_SOCKET_ERROR)
            goto lc0;
    }

    NetPacket_Dispose(packet);
    free(buffer);
    return NULL;
}

i32 main(const i32 argc, const char* argv[]) {
    u16 port = 0;
    const char* ipv4 = NULL;

    /*
    if (argc > 1) {
        ipv4 = argv[1];
        port = atoi(argv[2]);
    } else {
        puts("Usage: nfc [ IPv4 ] [ port ]");
        return 0;
    }
    */

    ipv4 = "127.0.0.1";
    port = 7777;

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

    u8* buffer = (u8*)malloc(BUFFER_SIZE);
    while (server->connected) {
        memset((char*)buffer, 0, BUFFER_SIZE);

        if (Socket_Receieve(server, buffer, BUFFER_SIZE, 0) == CS_SOCKET_ERROR) {
            fputs("Disconnected from server.\n", stderr);
        }

        printf("\n%s", (const char*)buffer);
    }

    // NOTE: Terminating a thread doesn't dispose the Thread object.
    Thread_Terminate(server_handler);
    Thread_Dispose(server_handler);
    free(buffer);
    Socket_Dispose(server);
    return 0;
}
