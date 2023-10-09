#ifndef CROSSPLATFORM_THREADS_H
#define CROSSPLATFORM_THREADS_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define CT_PLATFORM_NT

#ifdef _WIN64
#define CT_ARCH_64
#else
#define CT_ARCH_32
#endif

#ifdef _MSC_VER
#define restrict __restrict
#define stdcall __stdcall
#elif defined(__clang__) || defined(__GNUC__)
#define stdcall __attribute__((stdcall))
#endif

#define WIN32_MEAN_AND_LEAN
#include <windows.h>

#elif defined(__linux__) || defined(__APPLE__)
#define CT_PLATFORM_UNIX

#ifdef __LP64__
#define CT_ARCH_64
#else
#define CT_ARCH_32
#endif

#include <pthread.h>

#if defined(__clang__) || defined(__GNUC__)
#define stdcall __attribute__((stdcall))
#endif

#endif

typedef enum _cs_mutex_result {
	MutexResult_Success,
	MutexResult_Error
} MutexResult;

typedef struct _ct_mutex {
#ifdef CT_PLATFORM_NT
	HANDLE _native_mutex;
#elif defined(CT_PLATFORM_UNIX)
	pthread_mutex_t _native_mutex;
#endif
} Mutex;

Mutex* Mutex_New() {
	Mutex* mut = (Mutex*)malloc(sizeof(Mutex));
#ifdef CT_PLATFORM_NT
	mut->_native_mutex = CreateMutex(NULL, FALSE, NULL);
	if (!mut->_native_mutex) {
		perror("CS_Threads: Native mutex creation failed.");
		free(mut);
		return NULL;
	}
#elif defined(CT_PLATFORM_UNIX)
	mut->_native_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
	return mut;
}

MutexResult Mutex_Lock(Mutex* restrict m) {
#ifdef CT_PLATFORM_NT
	uint32_t ret = WaitForSingleObject(m->_native_mutex, INFINITE);
#elif defined(CT_PLATFORM_NT)
	int32_t ret = pthread_mutex_lock(&m->_native_mutex);
#endif
	if (!ret)
		return MutexResult_Success;
	else
		return MutexResult_Error;
}

MutexResult Mutex_Release(Mutex* restrict m) {
#ifdef CT_PLATFORM_NT
	if (ReleaseMutex(m->_native_mutex) != 0)
		return MutexResult_Success;
	else
		return MutexResult_Error;
#elif defined(CT_PLATFORM_UNIX)
	if (!pthread_mutex_unlock(m->_native_mutex))
		return MutexResult_Success;
	else
		return MutexResult_Error;
#endif
}

void Mutex_Dispose(Mutex* restrict m) {
#ifdef CT_PLATFORM_NT
	CloseHandle(m->_native_mutex);
#elif defined(CT_PLATFORM_UNIX)
	pthread_mutex_destroy(&m->_native_mutex);
#endif
	free(m);
}

typedef enum _ct_thread_state {
	ThreadState_Detached,
	ThreadState_Attached
} ThreadState;

typedef enum _ct_thread_result {
	ThreadResult_Success,
	ThreadResult_Error
} ThreadResult;

typedef unsigned long ThreadID;
typedef void* ThreadArg;
typedef ThreadArg (*ThreadRoutine)(ThreadArg);

typedef struct _ct_thread_attributes {
	size_t initial_stack_size;
	ThreadRoutine routine;
	ThreadArg args;
} ThreadAttributes;

typedef struct _ct_thread {
#ifdef CT_PLATFORM_NT
	HANDLE _native_thread;
#elif defined(CT_PLATFORM_UNIX)
	pthread_t _native_thread;
#endif
	ThreadAttributes _thread_attribs;

	ThreadArg result_ptr;
	ThreadID id;
} Thread;

struct _ct_thread_routine_info {
	ThreadRoutine routine;
	ThreadArg args_ptr;
	Thread* owner;
};

#ifdef CT_PLATFORM_NT
unsigned long
#elif defined(CT_PLATFORM_UNIX)
void*
#endif
stdcall
_ct_thread_routine_bootstrap
(void* restrict args) {
	struct _ct_thread_routine_info* info = (struct _ct_thread_routine_info*)args;
	info->owner->result_ptr = info->routine(info->args_ptr);
	free(info);
#ifdef CT_PLATFORM_NT
	return 0;
#elif defined(CT_PLATFORM_UNIX)
	return NULL;
#endif
}

Thread* Thread_New(ThreadAttributes* restrict attribs) {
	Thread* thd = (Thread*)malloc(sizeof(Thread));

	struct _ct_thread_routine_info* info = (struct _ct_thread_routine_info*)malloc(
		sizeof(struct _ct_thread_routine_info)
	);

	info->owner = thd;
	info->routine = attribs->routine;
	info->args_ptr = attribs->args;
#ifdef CT_PLATFORM_NT
	thd->_native_thread = CreateThread(
		NULL, 
		attribs->initial_stack_size, 
		_ct_thread_routine_bootstrap, 
		(void*)info,
		0,
		&thd->id
	);

	if (!thd->_native_thread) {
		fputs("CS_Threads: Native thread creation failed.\n", stderr);
		//perror("native error");
		free(thd);
		free(info);
		return NULL;
	}
#elif defined(CT_PLATFORM_UNIX)
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, attribs->initial_stack_size);

	pthread_create(&thd->_native_thread, &attr, &_ct_thread_routine_bootstrap, (void*)info);
	thd->id = (ThreadID)thd->_native_thread;

	pthread_attr_destroy(&attr);
#endif
	return thd;
}

ThreadResult Thread_Join(Thread* restrict t) {
#ifdef CT_PLATFORM_NT
	if (WaitForSingleObject(t->_native_thread, INFINITE) != 0)
		return ThreadResult_Error;
#elif defined(CT_PLATFORM_UNIX)
	if (pthread_join(t->_native_thread, NULL) != 0)
		return ThreadResult_Error;
#endif
	return ThreadResult_Success;
}

void Thread_Dispose(Thread* restrict t) {
#ifdef CT_PLATFORM_NT
	CloseHandle(t->_native_thread);
#endif
	free(t);
}

#endif // CROSSPLATFORM_THREADS_H
