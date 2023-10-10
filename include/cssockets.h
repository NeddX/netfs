#ifndef CROSSPLATFORM_SOCKETS_H
#define CROSSPLATFORM_SOCKETS_H

// Cross-platform wrapper for BSD sockets.
// Lucikly, WinSocks is based off of BSD sockets so
// implementing this was quite easy.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define true 1
#define false 0
#define CS_IPV4_MAX (size_t)17
#define CS_IPV6_MAX (size_t)40

#ifdef _WIN32
#define CS_PLATFORM_NT 
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#ifdef _MSC_VER
// Statically link winsocks if on MSVC.
#pragma comment(lib, "Ws2_32.lib")
// MSVC shenanigans...
#define restrict __restrict
#endif

// A socket is an unsigned pointer in winsocks.
typedef uintptr_t socket_t;

// Functions and Error coded that differ in WinSocks and BSD sockets.
#define CS_CLOSE_SOCKET(s) closesocket(s)
#define CS_CLEANUP() WSACleanup()
#define CS_INVALID_SOCKET INVALID_SOCKET
#define CS_SOCKET_ERROR SOCKET_ERROR
#define CS_SOCKET_SUCCESS 0

#elif defined(__linux__) || defined(__APPLE__)
#define CS_PLATFORM_UNIX

#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

// A socket is a signed pointer in Linux sockets.
typedef intptr_t socket_t;

// Functions and Error codes that differ in Linux sockets.
#define CS_CLOSE_SOCKET(s) close(s)
#define CS_CLEANUP() ;
#define CS_INVALID_SOCKET -1
#define CS_SOCKET_ERROR -1
#define CS_SOCKET_SUCCESS 0
#endif

// Address family enum abstraction layer.
typedef enum _cs_address_family {
    AddressFamily_InterNetwork = AF_INET
} AddressFamily;

// Socket type enum abstraction layer.
typedef enum _cs_socket_type {
    SocketType_Stream = SOCK_STREAM,
    SocketType_Dgram = SOCK_DGRAM,
    SocketType_Raw = SOCK_RAW
} SocketType;

// Socket protocol type abstraction layer.
typedef enum _cs_protocol_type {
    ProtocolType_Tcp = IPPROTO_TCP,
    ProtocolType_Udp = IPPROTO_UDP
} ProtocolType;

// IP address type abstraction layer.
typedef enum _cs_ip_address_type {
    IPAddressType_Any = INADDR_ANY,
    IPAddressType_Broadcast = INADDR_BROADCAST,
    IPAddressType_None = INADDR_NONE,
    IPAddressType_IPv4LPStr = 0xFEEFAAF00,
    IPAddressType_IPv6LPStr = 0xDEADFACE0
} IPAddressType;

// Holds information about the IP address such as
// the address type (ipv4 or ipv6 although iov6 is not supported)
// address string and the native address handler.
typedef struct _cs_ip_address {
    IPAddressType type;
    struct sockaddr_in ipv4_addr;
    struct sockaddr_in6 ipv6_addr;
    char str[CS_IPV6_MAX];
} IPAddress;

// Generic constructor for IPAddress.
IPAddress IPAddress_New(const IPAddressType type) {
    IPAddress addr;
    addr.type = type;
    return addr;
}

// Construct IPAddress from an IPv4 string.
IPAddress IPAddress_Parse(const char* restrict addrv4_str) {
    IPAddress addr;
    addr.type = IPAddressType_IPv4LPStr;
    memset(&addr.ipv4_addr, 0, sizeof(addr.ipv4_addr));
    const int32_t res = inet_pton(AF_INET, addrv4_str, &(addr.ipv4_addr.sin_addr));
    if (res != 1) {
        fprintf(stderr, "CS_Socket: Failed to parse %s as a valid IPv4 address.\n", addrv4_str);
        perror("native error");
        exit(EXIT_FAILURE);
    }
    strcpy(addr.str, addrv4_str);
    return addr;
}

// Construct IPAddress object from an IPv6.
IPAddress IPAddress_ParseV6(const char* restrict addrv6_str) {
    IPAddress addr;
    memset(&addr.ipv6_addr, 0, sizeof(addr.ipv6_addr));
    addr.type = IPAddressType_IPv6LPStr;
    const int32_t res = inet_pton(AF_INET6, addrv6_str, &(addr.ipv6_addr.sin6_addr));
    if (res != 1) {
        fprintf(stderr, "CS_Socket: Failed to parse %s as a valid IPv6 address.\n", addrv6_str);
        perror("native error");
        exit(EXIT_FAILURE);
    }
    strcpy(addr.str, addrv6_str);
    return addr;
}

// Stores information about an end-point such as
// the ip address, family and the port.
typedef struct _cs_ip_endpoint {
    IPAddress address;
    AddressFamily addressFamily;
    uint16_t port;
} IPEndPoint;

// Socket struct, holds the state of the current socket.
// such as the family: The network family.
// socket type: Data-Gram (for sending small packets), Stream (for sending stream of packets).
// protocol type: Tcp (reliable, slow) or Udp (unreliable, fast).
// local endpoint: What endpoint is the socket listening on.
// remote endpoint: What endpoint is the socket connected to.
// connected: If the socket is still connected to the remote.
// timeout: How long to wait when connecting.
// _native_socket: Native socket handler, the user is not supposed to interact with this field.
typedef struct _cs_socket {
    AddressFamily family;
    SocketType stype;
    ProtocolType ptype;
    IPEndPoint local_ep;
    IPEndPoint remote_ep;
    uint8_t connected;
    uint16_t timeout;

    socket_t _native_socket;
} Socket;

// WinSocks requires the user to initialize.
int32_t CSSocket_Init() {
#ifdef CS_PLATFORM_NT
    WSADATA wsa_data;
    int32_t res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (res != 0) {
        fputs("CS_Sockets: Failed to initialize WinSocks v2.2.\n", stderr);
        return -1;
    }
#else
    return CS_SOCKET_SUCCESS;
#endif
}

// WinSocks also requires deinitialization.
int32_t CSSocket_Dispose() {
#ifdef CS_PLATFORM_NT
    return WSACleanup();
#else
    return CS_SOCKET_SUCCESS;
#endif
}

// Constructor for Socket.
Socket* Socket_New(const AddressFamily family, const SocketType stype, const ProtocolType ptype) {
    Socket* s = (Socket*)malloc(sizeof(Socket));
    s->family = family;
    s->stype = stype;
    s->ptype = ptype;
    s->connected = false;
    s->timeout = 5000;

    s->_native_socket = CS_INVALID_SOCKET;
    s->_native_socket = socket(s->family, s->stype, s->ptype);
    if (s->_native_socket == CS_INVALID_SOCKET) {
        fputs("CS_Sockets: Failed to create socket.\n", stderr);
        perror("native error");
        CS_CLOSE_SOCKET(s->_native_socket);
        free(s);
        return NULL;
    }
    return s;
}

int32_t Socket_From(Socket* restrict s, const AddressFamily family, const SocketType stype, const ProtocolType ptype) {
    memset(s, 0, sizeof(Socket));

    s->family = family;
    s->stype = stype;
    s->ptype = ptype;
    s->connected = false;
    s->timeout = 5000;

    s->_native_socket = CS_INVALID_SOCKET;
    s->_native_socket = socket(s->family, s->stype, s->ptype);
    if (s->_native_socket == CS_INVALID_SOCKET) {
        CS_CLOSE_SOCKET(s->_native_socket);
        return CS_SOCKET_ERROR;
    }
    return CS_SOCKET_SUCCESS;
}

// Destructor for our socket.
void Socket_Dispose(Socket* restrict s) {
    CS_CLOSE_SOCKET(s->_native_socket);
    memset(s, 0, sizeof(Socket));
    free(s);
}

// Disconnect the socket but keep the object.
int32_t Socket_Disconnect(Socket* restrict s) {
    if (CS_CLOSE_SOCKET(s->_native_socket) == CS_SOCKET_ERROR)
        return CS_SOCKET_ERROR;
    return CS_SOCKET_SUCCESS;
}

// Try and bind our socket to the provided endpoint.
int32_t Socket_Bind(Socket* restrict s, IPEndPoint ep) {
    s->local_ep = ep;

    void* server_addr = NULL;
    size_t addr_size = 0;

    // Resolve the endpoint.
    switch (ep.address.type) {
        case IPAddressType_IPv4LPStr:
            ep.address.ipv4_addr.sin_port = htons(ep.port);
            ep.address.ipv4_addr.sin_family = ep.addressFamily;
            server_addr = (void*)&ep.address.ipv4_addr;
            addr_size = sizeof(ep.address.ipv4_addr);
            break;
        case IPAddressType_IPv6LPStr:
            ep.address.ipv6_addr.sin6_port = htons(ep.port);
            ep.address.ipv6_addr.sin6_family = ep.addressFamily;
            server_addr = (void*)&ep.address.ipv6_addr;
            addr_size = sizeof(ep.address.ipv6_addr);
            break;
        default:
            ep.address.ipv4_addr.sin_addr.s_addr = ep.address.type;
            ep.address.ipv4_addr.sin_port = htons(ep.port);
            ep.address.ipv4_addr.sin_family = ep.addressFamily;
            server_addr = (void*)&ep.address.ipv4_addr;
            addr_size = sizeof(ep.address.ipv4_addr);
            break;
    }

	// Try and bind.
    int32_t bind_res = bind(s->_native_socket, (struct sockaddr*)server_addr, addr_size);
    if (bind_res == CS_SOCKET_ERROR) {
        fputs("CS_Sockets: Failed to bind socket.\n", stderr);
        perror("native error");
        CS_CLOSE_SOCKET(s->_native_socket);
        return CS_SOCKET_ERROR;
    }
    return CS_SOCKET_SUCCESS;
}

// Try and listen on the bound endpoint.
int32_t Socket_Listen(Socket* restrict s, const size_t max_clients) {
    if (listen(s->_native_socket, max_clients) == CS_SOCKET_ERROR) {
        fputs("CS_Socket: Listening failed.\n", stderr);
        CS_CLOSE_SOCKET(s->_native_socket);
        return CS_SOCKET_ERROR;
    }
    return CS_SOCKET_SUCCESS;
}

// Try to connection to an endpoint.
// TODO: Implement timeout system.
int32_t Socket_Connect(Socket* restrict s) {
    if (connect(
            s->_native_socket,
            (struct sockaddr*)&s->remote_ep.address.ipv4_addr,
            sizeof(s->remote_ep.address.ipv4_addr)) == CS_SOCKET_ERROR) {
        fputs("CS_Sockets: Connection with the remote failed.\n", stderr);
        perror("native error");
        return CS_SOCKET_ERROR;
    }
    return CS_SOCKET_SUCCESS;
}

// Accept a incoming connection and construct a new Socket object representing the newly connected client.
Socket* Socket_Accept(Socket* restrict s) {
    Socket* client = (Socket*)malloc(sizeof(Socket));
    memcpy(client, s, sizeof(Socket));

    socklen_t addr_len = sizeof(client->remote_ep.address.ipv4_addr);
    client->_native_socket = accept(
        s->_native_socket,
        (struct sockaddr*)&client->remote_ep.address.ipv4_addr,
        &addr_len
    );

    if (client->_native_socket == CS_INVALID_SOCKET) {
        free(client);
        return NULL;
    }

	// Resolve the client endpoint.
    client->connected = true;
    client->remote_ep.addressFamily = client->remote_ep.address.ipv4_addr.sin_family;
    client->remote_ep.port = client->remote_ep.address.ipv4_addr.sin_port;
    strcpy(client->remote_ep.address.str, inet_ntoa(client->remote_ep.address.ipv4_addr.sin_addr));
    return client;
}

// Try and receive data from the Socket, shut the socket down if receive fails indicating that the client has disconnected.
// If successful, return the amount of bytes received.
int32_t Socket_Receieve(Socket* restrict s, uint8_t* restrict buffer, const size_t buffer_size, const int32_t flags) {
    int32_t received_bytes = recv(s->_native_socket, buffer, buffer_size, flags);
    if (received_bytes == 0 || received_bytes == CS_SOCKET_ERROR) {
        s->connected = false;
        CS_CLOSE_SOCKET(s->_native_socket);
        return CS_SOCKET_ERROR;
    }
    return received_bytes;
}

// Try and send data from the Socket, shut the socket down if receive fails indicating that the client has disconnected.
// If successful, return the amount of bytes sent.
int32_t Socket_Send(Socket* restrict s, const uint8_t* restrict buffer, const size_t buffer_size, const int32_t flags) {
    int32_t sent_bytes = send(s->_native_socket, buffer, buffer_size, flags);
    if (sent_bytes == CS_SOCKET_ERROR) {
        s->connected = false;
        CS_CLOSE_SOCKET(s->_native_socket);
    }
    return sent_bytes;
}

#endif // CROSSPLATFORM_SOCKETS_H
