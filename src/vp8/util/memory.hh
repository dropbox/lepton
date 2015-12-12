#ifndef _MEMORY_HH_
#define _MEMORY_HH_
#if defined(__cplusplus) || defined(c_plusplus)
#include <new>
#include <cstdlib>
#include <assert.h>
#include <cstdio>
#include <cstring>
#include "../../io/DecoderPlatform.hh"
#include "../../io/MemMgrAllocator.hh"
extern bool g_use_seccomp;
#if __cplusplus > 199711L
[[noreturn]]
#endif
void custom_exit(uint8_t exit_code);

void custom_terminate_this_thread(uint8_t exit_code);
void custom_atexit(void (*atexit)(void*) , void *arg);
extern "C" {
#else
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#endif
void* custom_malloc (size_t size);
void* custom_realloc (void * old, size_t size);
void custom_free(void* ptr);

void * custom_calloc(size_t size);
void set_close_thread_handle(int handle);
void reset_close_thread_handle();
#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
