#ifndef NETFS_PACKET_H
#define NETFS_PACKET_H

#include "stdnfs.h"

typedef enum _netfs_packet_header_type {
    NetPacketType_Message,
    PacketType_ListEntires,
    PacketTpye_RemoveEntry,
    PacketType_FileDownloadRequest,
    PacketType_FileUploadRequest,
    PacketType_FileDownloadData,
    PacketType_FileUploadData
} PacketHeaderType;

typedef struct _netfs_packet_header {
    PacketHeaderType id;
    usize size;
} PacketHeader;


typedef struct _netfs_packet {
    PacketHeader header;
    u8* buffer;
} NetPacket;

NetPacket* NetPacket_New(const PacketHeaderType type, const u8* restrict buffer, const usize size) {
    NetPacket* packet = (NetPacket*)malloc(sizeof(NetPacket));
    if (size > 0) {
        packet->buffer = (u8*)malloc(size);
        memcpy(packet->buffer, buffer, size);
    } else {
        packet->buffer = NULL;
    }

    packet->header.id = type;
    packet->header.size = size;
    return packet;
}

void NetPacket_AddBuffer(NetPacket* restrict p, const u8* buffer, const usize size) {
    if (size > 0) {
        p->buffer = (u8*)realloc(p->buffer, p->header.size + size);
        memcpy(p->buffer + p->header.size, buffer, size);
        p->header.size += size;
    }
}

void NetPacket_Dispose(NetPacket* restrict p) {
    free(p->buffer);
    free(p);
}

#endif // NETFS_PACKET_H
