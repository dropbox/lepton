#ifndef _MEMORY_HH_
#define _MEMORY_HH_

#include <new>
#include <cstdlib>
#include <assert.h>
#include <cstdio>
#include <cstring>
#include "../../io/DecoderPlatform.hh"
#include "../../io/MemMgrAllocator.hh"
extern bool g_use_seccomp;
void custom_exit(uint8_t exit_code);
void custom_atexit(void (*atexit)(void*) , void *arg);

inline void* custom_malloc (size_t size) {
#if 0
    return malloc(size);
#else
    return Sirikata::memmgr_alloc(size);
#endif
}
inline void custom_free(void* ptr) {
#if 0
    free(ptr);
#else
    Sirikata::memmgr_free(ptr);
#endif
}

inline void * custom_calloc(size_t size) {
    return memset(custom_malloc(size), 0, size);
}
#endif
