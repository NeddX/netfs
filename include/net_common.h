#ifndef NETFS_PACKET_H
#define NETFS_PACKET_H

#include "stdnfs.h"
#include "cs_sockets.h"

typedef struct _netfs_file_info {
    const char* name;
    size_t size;
} FileInfo;

typedef enum _netfs_packet_header_type {
    NetPacketType_Message,
    NetPacketType_Error,
    NetPacketType_ListEntries,
    NetPacketTpye_RemoveEntry,
    NetPacketType_FileInfo,
    NetPacketType_FileDownloadRequest,
    NetPacketType_FileUploadRequest,
    NetPacketType_FileDownloadData,
    NetPacketType_FileUploadData,
    NetPacketType_None
} NetPacketType;

typedef struct _netfs_packet_header {
    NetPacketType id;
    usize size;
} PacketHeader;

typedef struct _netfs_packet {
    PacketHeader header;
    u8* buffer;
} NetPacket;

NetPacket* NetPacket_New(const NetPacketType type, const u8* restrict buffer, const usize size) {
    NetPacket* packet = (NetPacket*)malloc(sizeof(NetPacket));
    if (size > 0) {
        packet->buffer = (u8*)malloc(size);

        if (buffer != NULL) {
            memcpy(packet->buffer, buffer, size);
        }
    } else {
        packet->buffer = NULL;
    }

    packet->header.id = type;
    packet->header.size = size;
    return packet;
}

const char* NetPacket_GetTypeStr(const NetPacket* restrict p) {
    static const char* types_str[] = {
        "NetPacketType_None",
        "NtPacketType_Message",
        "NetPacketType_Error",
        "NtPacketType_ListEntries",
        "NetPacketTpye_RemoveEntry",
        "NtPacketType_FileInfo",
        "NetPacketType_FileDownloadRequest",
        "NtPacketType_FileUploadRequest",
        "NetPacketType_FileDownloadData",
        "NtPacketType_FileUploadData"};
    if ((size_t)p->header.id <= 0 && (size_t)p->header.id <= NetPacketType_None)
        return types_str[(size_t)p->header.id];
    return types_str[0];
}

void NetPacket_AddData(NetPacket* restrict p, const u8* buffer, const usize size) {
    if (size > 0) {
        p->buffer = (u8*)realloc(p->buffer, p->header.size + size);

        if (buffer != NULL) {
            memcpy(p->buffer + p->header.size, buffer, size);
        }
        p->header.size += size;
    }
}

void NetPacket_Dispose(NetPacket* restrict p) {
    if (p != NULL) {
        if (p->buffer != NULL) {
            free(p->buffer);
        }
        free(p);
    }
}

NetPacket* NetPacket_Receive(Socket* restrict s) {
    NetPacket* incoming = NetPacket_New(NetPacketType_None, NULL, 0);
    i32 res = Socket_Receive(s, (u8*)&incoming->header, sizeof(incoming->header), 0);
    if (res == CS_SOCKET_ERROR) {
lc0:
        NetPacket_Dispose(incoming);
        return NULL;
    }

    if (incoming->header.size > 0) {
        NetPacket_AddData(incoming, NULL, incoming->header.size);
        res = Socket_Receive(s, incoming->buffer, incoming->header.size, 0);
        if (res == CS_SOCKET_ERROR)
            goto lc0;
    }
    return incoming;
}

int32_t NetPacket_Send(Socket* restrict s, NetPacket* restrict p) {
    i32 res = Socket_Send(s, (u8*)&p->header, sizeof(p->header), 0);
    if (res != CS_SOCKET_ERROR) {
        res = Socket_Send(s, p->buffer, p->header.size, 0);
        if (res != CS_SOCKET_ERROR) {
            return CS_SOCKET_SUCCESS;
        }
    }
    return CS_SOCKET_ERROR;
}


#endif // NETFS_PACKET_H
