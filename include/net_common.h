#ifndef NETFS_PACKET_H
#define NETFS_PACKET_H

#include "stdnfs.h"
#include "cs_sockets.h"

typedef struct _netfs_file_info {
    const char* name;
    size_t size;
} FileInfo;

typedef enum _netfs_packet_header_type {
    NetPacketType_None,
    NetPacketType_Message,
    NetPacketType_Error,
    NetPacketType_ListEntries,
    NetPacketTpye_RemoveEntry,
    NetPacketType_FileInfo,
    NetPacketType_FileDownloadRequest,
    NetPacketType_FileUploadRequest,
    NetPacketType_FileDownloadData,
    NetPacketType_FileUploadData
} NetPacketHeaderType;

typedef struct _netfs_packet_header {
    NetPacketHeaderType id;
    usize size;
} PacketHeader;


typedef struct _netfs_packet {
    PacketHeader header;
    u8* buffer;
} NetPacket;

NetPacket* NetPacket_New(const NetPacketHeaderType type, const u8* restrict buffer, const usize size) {
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
        free(p->buffer);
        free(p);
    }
}

void NetPacket_Send(Socket* restrict s, NetPacket** restrict p) {
    int32_t res = Socket_Send(s, (uint8_t*)&(*p)->header, sizeof((*p)->header), 0);
    if (res != CS_SOCKET_ERROR) {
        res = Socket_Send(s, (*p)->buffer, (*p)->header.size, 0);
        if (res == CS_SOCKET_ERROR) {
            NetPacket_Dispose(*p);
            *p = NULL;
        }
    }
}


#endif // NETFS_PACKET_H
