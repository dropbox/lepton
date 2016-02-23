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

#define FOREACH_EXIT_CODE(CB)                   \
    CB(SUCCESS,0)                               \
    CB(ASSERTION_FAILURE,1)                     \
    CB(CODING_ERROR,2)                          \
    CB(SHORT_READ,3)                            \
    CB(UNSUPPORTED_4_COLORS,4)                  \
    CB(THREAD_PROTOCOL_ERROR,5)                 \
    CB(COEFFICIENT_OUT_OF_RANGE,6)              \
    CB(STREAM_INCONSISTENT,7)                   \
    CB(PROGRESSIVE_UNSUPPORTED,8)               \
    CB(FILE_NOT_FOUND,9)                        \
    CB(SAMPLING_BEYOND_TWO_UNSUPPORTED,10)      \
    CB(SAMPLING_BEYOND_FOUR_UNSUPPORTED,11)     \
    CB(THREADING_PARTIAL_MCU,12)                \
    CB(VERSION_UNSUPPORTED,13)                  \
    CB(OS_ERROR,33)                             \
    CB(HEADER_TOO_LARGE,34)                     \
    CB(DIMENSIONS_TOO_LARGE,35)                 \
    CB(MALLOCED_NULL,36)                        \
    CB(OOM,37)                                  \
    CB(TOO_MUCH_MEMORY_NEEDED,38)               \
    CB(EARLY_EXIT,40)

#define MAKE_EXIT_CODE_ENUM(ITEM, VALUE) ITEM=VALUE,
#define GENERATE_EXIT_CODE_RETURN(ITEM, VALUE) {if ((ec) == ExitCode::ITEM) { return #ITEM;}}

#if __cplusplus <= 199711L
namespace ExitCode { enum ExitCode_ {
#else
enum class ExitCode {
#endif
FOREACH_EXIT_CODE(MAKE_EXIT_CODE_ENUM)
#if __cplusplus > 199711L
    };
#else
};
}
#endif

#if __cplusplus > 199711L
[[noreturn]]
void custom_exit(ExitCode exit_code);
#else
void custom_exit(ExitCode::ExitCode_ exit_code);
#endif
#define always_assert(EXPR) always_assert_inner((EXPR), #EXPR, __FILE__, __LINE__)
void custom_terminate_this_thread(uint8_t exit_code);
void custom_atexit(void (*atexit)(void*, uint64_t) , void *arg0, uint64_t arg1);
extern "C" {
#else
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#endif
void always_assert_inner(bool value, const char * expr, const char * file, int line);
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
