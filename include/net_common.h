#ifndef NETFS_PACKET_H
#define NETFS_PACKET_H

#include "stdnfs.h"
#include "cs_sockets.h"
#include "cs_threads.h"

typedef struct _netfs_file_info {
    const char* name;
    size_t size;
} FileInfo;

typedef enum _netfs_packet_header_type : uint8_t {
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
        memset(packet->buffer, 0, size);

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
        "NetPacketType_Message",
        "NetPacketType_Error",
        "NetPacketType_ListEntries",
        "NetPacketTpye_RemoveEntry",
        "NetPacketType_FileInfo",
        "NetPacketType_FileDownloadRequest",
        "NetPacketType_FileUploadRequest",
        "NetPacketType_FileDownloadData",
        "NetPacketType_FileUploadData",
        "NetPacketType_None"};
    if ((size_t)p->header.id >= 0 && (size_t)p->header.id <= NetPacketType_None)
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

typedef struct _netfs_packet_queue {
    NetPacket** _packets;
    usize _count;
    usize _available_packets;
    Mutex* _access_mutex;
} NetPacketQueue;

NetPacketQueue* NetPacketQueue_New() {
    NetPacketQueue* q = (NetPacketQueue*)malloc(sizeof(NetPacketQueue));
    q->_count = 256;
    q->_packets = (NetPacket**)malloc(sizeof(NetPacket*) * q->_count);
    q->_available_packets = 0;
    q->_access_mutex = Mutex_New();
    return q;
}

void NetPacketQueue_Dispose(NetPacketQueue* restrict q) {
    free(q->_packets);
    Mutex_Dispose(q->_access_mutex);
    free(q);
}

i32 NetPacketQueue_TryAdd(NetPacketQueue* restrict q, NetPacket* restrict p) {
    if (Mutex_Lock(q->_access_mutex) == MutexResult_Success) {
        if (q->_available_packets >= q->_count) {
            q->_count *= 2;
            q->_packets = (NetPacket**)malloc(sizeof(NetPacket*) * q->_count);
        }
        q->_packets[q->_available_packets++] = p;
        Mutex_Unlock(q->_access_mutex);
        return 1;
    } else
        return 0;
}

NetPacket* NetPacketQueue_TryPop(NetPacketQueue* restrict q) {
    if (Mutex_Lock(q->_access_mutex) == MutexResult_Success) {
        if (q->_available_packets <= 0) {
            Mutex_Unlock(q->_access_mutex);
            return NULL;
        }
        NetPacket* packet = q->_packets[--q->_available_packets];
        Mutex_Unlock(q->_access_mutex);
        return packet;
    } else
        return NULL;
}

#endif // NETFS_PACKET_H
