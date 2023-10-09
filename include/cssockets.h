#ifndef CROSSPLATFORM_SOCKETS_H
#define CROSSPLATFORM_SOCKETS_H

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
#define CS_PLATFORM_NT ;
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "Ws2_32.lib")

typedef uintptr_t socket_t;

#define restrict __restrict

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

typedef intptr_t socket_t;

#define CS_CLOSE_SOCKET(s) close(s)
#define CS_CLEANUP() ;
#define CS_INVALID_SOCKET -1
#define CS_SOCKET_ERROR -1
#define CS_SOCKET_SUCCESS 0
#endif

typedef enum _cs_address_family {
    AddressFamily_InterNetwork = AF_INET
} AddressFamily;

typedef enum _cs_socket_type {
    SocketType_Stream = SOCK_STREAM,
    SocketType_Dgram = SOCK_DGRAM,
    SocketType_Raw = SOCK_RAW
} SocketType;

typedef enum _cs_protocol_type {
    ProtocolType_Tcp = IPPROTO_TCP,
    ProtocolType_Udp = IPPROTO_UDP
} ProtocolType;

typedef enum _cs_ip_address_type {
    IPAddressType_Any = INADDR_ANY,
    IPAddressType_Broadcast = INADDR_BROADCAST,
    IPAddressType_None = INADDR_NONE,
    IPAddressType_IPv4LPStr = 0xFEEFAAF00,
    IPAddressType_IPv6LPStr = 0xDEADFACE0
} IPAddressType;

typedef struct _cs_ip_address {
    IPAddressType type;
    struct sockaddr_in ipv4_addr;
    struct sockaddr_in6 ipv6_addr;
    char addr_str[CS_IPV6_MAX];
} IPAddress;

IPAddress IPAddress_New(const IPAddressType type) {
    IPAddress addr;
    addr.type = type;
    return addr;
}

IPAddress IPAddress_Parse(const char* restrict addrv4_str) {
    IPAddress addr;
    addr.type = IPAddressType_IPv4LPStr;
    memset(&addr.ipv4_addr, 0, sizeof(addr.ipv4_addr));
    const int32_t res = inet_pton(AF_INET, addrv4_str, &(addr.ipv4_addr.sin_addr));
    if (res != 1) {
        fprintf(stderr, "CS_Socket: Failed to parse %s as a valid IPv4 address.\n", addrv4_str);
        perror("native error");
        CS_CLEANUP();
        exit(EXIT_FAILURE);
    }
    strcpy(addr.addr_str, addrv4_str);
    return addr;
}

IPAddress IPAddress_ParseV6(const char* restrict addrv6_str) {
    IPAddress addr;
    memset(&addr.ipv6_addr, 0, sizeof(addr.ipv6_addr));
    addr.type = IPAddressType_IPv6LPStr;
    const int32_t res = inet_pton(AF_INET6, addrv6_str, &(addr.ipv6_addr.sin6_addr));
    if (res != 1) {
        fprintf(stderr, "CS_Socket: Failed to parse %s as a valid IPv6 address.\n", addrv6_str);
        perror("native error");
        CS_CLEANUP();
        exit(EXIT_FAILURE);
    }
    strcpy(addr.addr_str, addrv6_str);
    return addr;
}

const char* IPAddress_ToString(const IPAddress* restrict addr) {
    return NULL;
}

typedef struct _cs_ip_endpoint {
    IPAddress address;
    AddressFamily addressFamily;
    uint16_t port;
} IPEndPoint;

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

int32_t CSSocket_Dispose() {
#ifdef CS_PLATFORM_NT
    return WSACleanup();
#else
    return CS_SOCKET_SUCCESS;
#endif
}

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
        CS_CLEANUP();
        free(s);
        return NULL;
    }
    return s;
}

void Socket_Dispose(Socket* restrict s) {
    CS_CLOSE_SOCKET(s->_native_socket);
    memset(s, 0, sizeof(Socket));
    free(s);
}

int32_t Socket_Bind(Socket* restrict s, IPEndPoint ep) {
    s->local_ep = ep;

    void* server_addr = NULL;
    size_t addr_size = 0;
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

    int32_t bind_res = bind(s->_native_socket, (struct sockaddr*)server_addr, addr_size);
    if (bind_res == CS_SOCKET_ERROR) {
        fputs("CS_Sockets: Failed to bind socket.\n", stderr);
        perror("native error");
        CS_CLOSE_SOCKET(s->_native_socket);
        CS_CLEANUP();
        return CS_SOCKET_ERROR;
    }
    return CS_SOCKET_SUCCESS;
}

int32_t Socket_Listen(Socket* restrict s, const size_t max_clients) {
    if (listen(s->_native_socket, max_clients) == CS_SOCKET_ERROR) {
        fputs("CS_Socket: Listening failed.\n", stderr);
        CS_CLOSE_SOCKET(s->_native_socket);
        CS_CLEANUP();
        return CS_SOCKET_ERROR;
    }
    return CS_SOCKET_SUCCESS;
}

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

    client->connected = true;
    client->remote_ep.addressFamily = client->remote_ep.address.ipv4_addr.sin_family;
    client->remote_ep.port = client->remote_ep.address.ipv4_addr.sin_port;
    strcpy(client->remote_ep.address.addr_str, inet_ntoa(client->remote_ep.address.ipv4_addr.sin_addr));
    return client;
}

int32_t Socket_Receieve(Socket* restrict s, uint8_t* restrict buffer, const size_t buffer_size, const int32_t flags) {
    int32_t received_bytes = recv(s->_native_socket, buffer, buffer_size, flags);
    if (received_bytes == 0 || received_bytes == CS_SOCKET_ERROR) {
        s->connected = false;
        CS_CLOSE_SOCKET(s->_native_socket);
    }
    return received_bytes;
}

int32_t Socket_Send(Socket* restrict s, const uint8_t* restrict buffer, const size_t buffer_size, const int32_t flags) {
    int32_t sent_bytes = send(s->_native_socket, buffer, buffer_size, flags);
    if (sent_bytes == CS_SOCKET_ERROR) {
        s->connected = false;
        CS_CLOSE_SOCKET(s->_native_socket);
    }
    return sent_bytes;
}

#endif // CROSSPLATFORM_SOCKETS_H
