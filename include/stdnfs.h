#ifndef NETFS_COMMON_H
#define NETFS_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Kernel user-space API
//#include <unistd.h>

// Concurrency
//#include <pthread.h>

// I/O
//#include <dirent.h>

// Handy header only cross-platform (Linux & Windows) net wrapper around BSD sockets
#include <cssockets.h>

#define true 1
#define false 0

typedef int8_t bool;
typedef size_t usize;
typedef uintptr_t uintptr;
typedef intptr_t intptr;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#endif // NETFS_COMMON_H
