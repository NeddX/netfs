#include <cssockets.h>
#include <csthreads.h>
#include <stdnfs.h>

#define MAX_CLIENTS 256
#define BUFFER_SIZE 1024 * 1024

typedef struct _netfs_connection {
    Socket* socket;
    bool available;
    usize id;
    Thread* owning_thread;
} Connection;

typedef enum _netfs_packet_header_type {
    PacketType_Message,
    PacketType_StringCommand
} PacketHeaderType;

typedef struct _netfs_packet_header {
    PacketHeaderType id;
    usize size;
} PacketHeader;


typedef struct _netfs_packet {
    PacketHeader header;
    u8* buffer;
} Packet;

Packet* Packet_New(const PacketHeaderType type, const u8* restrict buffer, const usize size) {
    Packet* packet = (Packet*)malloc(sizeof(Packet));
    packet->buffer = (u8*)malloc(size);
    memcpy(packet->buffer, buffer, size);

    packet->header.id = type;
    packet->header.size = size;
    return packet;
}


void Packet_AddBuffer(Packet* restrict p, const u8* buffer, const usize size) {
    if (size > 0) {
        p->buffer = (u8*)realloc(p->buffer, p->header.size + size);
        memcpy(p->buffer + p->header.size, buffer, size);
        p->header.size += size;
    }
}

void Packet_Dispose(Packet* restrict p) {
    free(p->buffer);
    free(p);
}

Connection* g_clients = NULL;
usize g_client_count = 0;
usize g_active_clients = 0;
Thread** g_threads;
Mutex* g_threads_mutex = NULL;

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

ThreadArg net_connection_handler(ThreadArg args) {
    Connection* c = (Connection*)args;

    u8* buffer = (u8*)malloc(BUFFER_SIZE);
    i32 received_bytes = 0;
    while (c->socket->connected) {
        memset(buffer, 0, BUFFER_SIZE);
        if (Socket_Receieve(c->socket, buffer, BUFFER_SIZE, 0) == CS_SOCKET_ERROR) {
            printf("Client (%lu) [%s:%hu] disconnected.\n", c->id, c->socket->remote_ep.address.str, c->socket->remote_ep.port);
            break;
        }

        if (!strncmp((const char*)buffer, "killself", 8)) {
            puts("Client is requesting disconnect.");
            // Disconnect, note that disconnect doesn't dispose the socket object.
            // It is still valid and can be reinitialized using Socket_From().
            Socket_Disconnect(c->socket);
        }
    }

    // The thread can safely terminate self if not other threads depend on it or are signlaed that
    // this thread is suspended, the user should handle that.
    free(buffer);
    Socket_Dispose(c->socket);
    Thread_Dispose(c->owning_thread);
    c->id = 0;
    c->owning_thread = NULL;
    c->available = false;
    return NULL;
}

i32 main() {
    CSSocket_Init();

    srand(time(0));

    u16 port = rand() % 9999;
    Socket* server = Socket_New(AddressFamily_InterNetwork, SocketType_Stream, ProtocolType_Tcp);

    IPEndPoint ep = { IPAddress_New(IPAddressType_Any), AddressFamily_InterNetwork, port };
    if (Socket_Bind(server, ep) == CS_SOCKET_ERROR) {
        exit(EXIT_FAILURE);
    }

    if (Socket_Listen(server, MAX_CLIENTS) == CS_SOCKET_ERROR) {
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
        Socket* new_client = Socket_Accept(server);
        if (new_client) {
            Connection* conn = get_next_available_slot();
            if (conn) {
                conn->socket = new_client;
                conn->available = true;
                conn->id = g_active_clients++;

                ThreadAttributes attr;
                attr.args = (ThreadArg)conn;
                attr.initial_stack_size = 0;
                attr.routine = net_connection_handler;
                // TODO: Memory leak, threads do not get properly freed.
                // write a thread manager.
                conn->owning_thread = Thread_New(&attr);

                printf("Client (%lu) [%s:%hu] connected.\n", conn->id, new_client->remote_ep.address.str, new_client->remote_ep.port);
            } else {
                printf("Client [%s:%hu] tried to connect but max connection capacity (%d) exceeded.\n", new_client->remote_ep.address.str, new_client->remote_ep.port, MAX_CLIENTS);

                const char* error_msg = "Max connection capacity exceeded.\n";
                Socket_Send(new_client, (const u8*)error_msg, strlen(error_msg) + 1, 0);
                Socket_Dispose(new_client);
            }
        }
    }

    free(g_clients);
    free(g_threads);
    Mutex_Dispose(g_threads_mutex);
    Socket_Dispose(server);


    CSSocket_Dispose();
    return 0;
}
