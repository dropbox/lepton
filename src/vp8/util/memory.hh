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
enum class ExitCode {
    SUCCESS=0,
    ASSERTION_FAILURE=1,
    CODING_ERROR=2,
    SHORT_READ=3,
    UNSUPPORTED_4_COLORS=4,
    THREAD_PROTOCOL_ERROR=5,
    COEFFICIENT_OUT_OF_RANGE=6,
    STREAM_INCONSISTENT=7,
    PROGRESSIVE_UNSUPPORTED=8,
    FILE_NOT_FOUND=9,
    EARLY_EXIT=35,
    MALLOCED_NULL=36,
    OOM=37,
    TOO_MUCH_MEMORY_NEEDED=38,
};

#if __cplusplus > 199711L
[[noreturn]]
#endif
void custom_exit(ExitCode exit_code);

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
