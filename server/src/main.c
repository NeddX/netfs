#include <cssockets.h>
#include <csthreads.h>
#include <stdnfs.h>

u64 counter = 0;

ThreadArg net_client_handler(ThreadArg args) {
    Socket* client = (Socket*)args;

    usize buffer_size = 1024 * 1024;
    u8* buffer = (u8*)malloc(buffer_size);
    while (client->connected) {
        memset(buffer, 0, buffer_size);
        i32 bytes_received = Socket_Receieve(client, buffer, buffer_size, 0);
        if (!client->connected) {
            puts("Client disconnected.");
            break;
        }
        printf("Received from [%s:%hu]: %s", client->remote_ep.address.addr_str, client->remote_ep.port, (const char*)buffer);
        Socket_Send(client, buffer, buffer_size, 0);
    }
    return client;
}

ThreadArg count(ThreadArg args) {
    Mutex* mutex = (Mutex*)args;
    while (true) {
        Mutex_Lock(mutex);
        ++counter;
        Mutex_Unlock(mutex);
    }
    return NULL;
}

i32 main() {
    srand(time(0));
    CSSocket_Init();

    Mutex* mutex = Mutex_New();

    while (true) {
        ThreadAttributes attr;
        attr.initial_stack_size = 0;
        attr.routine = count;
        attr.args = (ThreadArg)mutex;

        for (usize i = 0; i < 10; ++i) {
            Thread_New(&attr);
        }

        while (true) {
            printf("\rCount: %lu                    ", counter);
        }
    }

    Mutex_Dispose(mutex);

    Socket* socket = Socket_New(AddressFamily_InterNetwork, SocketType_Stream, ProtocolType_Tcp);

    IPEndPoint ep;
    ep.address = IPAddress_New(IPAddressType_Any);
    ep.port = rand() % 9999;
    ep.addressFamily = AddressFamily_InterNetwork;
    if (Socket_Bind(socket, ep) == CS_SOCKET_ERROR) {
        exit(EXIT_FAILURE);
    }

    Socket_Listen(socket, 10);
    printf("Listening on 127.0.0.1:%hu...\n", ep.port);

    while (true) {
        Socket* client = Socket_Accept(socket);
        if (client)
            printf("Client [%s:%hu] connected.\n", client->remote_ep.address.addr_str, client->remote_ep.port);
        
        // Intentional memory leak.
        ThreadAttributes attr;
        attr.initial_stack_size = 0;
        attr.args = (ThreadArg)client;
        attr.routine = net_client_handler;
        Thread* client_thread = Thread_New(&attr);

        while (client_thread->result_ptr == NULL) ;

        Socket_Dispose((Socket*)client_thread->result_ptr);
    }

    Socket_Dispose(socket);
    CSSocket_Dispose();
    return 0;
}
