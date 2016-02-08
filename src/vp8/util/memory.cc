#include "memory.hh"
#include <unistd.h>
#include <errno.h>
#ifdef __linux
#include <sys/syscall.h>
#endif
#if defined(__APPLE__) || __cplusplus <= 199711L
#define thread_local __thread
#endif
const char *ExitString(ExitCode ec) {
  switch (ec) {
  case ExitCode::SUCCESS:
    return "";
  case ExitCode::ASSERTION_FAILURE:
    return "ASSERTION_FAILURE";
  case ExitCode::CODING_ERROR:
    return "CODING_ERROR";
  case ExitCode::SHORT_READ:
    return "SHORT_READ";
  case ExitCode::UNSUPPORTED_4_COLORS:
    return "UNSUPPORTED_4_COLORS";
  case ExitCode::THREAD_PROTOCOL_ERROR:
    return "THREAD_PROTOCOL_ERROR";
  case ExitCode::COEFFICIENT_OUT_OF_RANGE:
    return "COEFFICIENT_OUT_OF_RANGE";
  case ExitCode::STREAM_INCONSISTENT:
    return "STREAM_INCONSISTENT";
  case ExitCode::PROGRESSIVE_UNSUPPORTED:
    return "PROGRESSIVE_UNSUPPORTED";
  case ExitCode::FILE_NOT_FOUND:
    return "FILE_NOT_FOUND";
  case ExitCode::SAMPLING_BEYOND_FOUR_UNSUPPORTED:
    return "SAMPLING_BEYOND_FOUR_UNSUPPORTED";
  case ExitCode::HEADER_TOO_LARGE:
    return "HEADER_TOO_LARGE";
  case ExitCode::DIMENSIONS_TOO_LARGE:
    return "DIMENSIONS_TOO_LARGE";
  case ExitCode::MALLOCED_NULL:
    return "MALLOCED_NULL";
  case ExitCode::OS_ERROR:
    return "OS_ERROR";
  case ExitCode::OOM:
    return "OOM";
  case ExitCode::TOO_MUCH_MEMORY_NEEDED:
    return "TOO_MUCH_MEMORY_NEEDED";
  case ExitCode::EARLY_EXIT:
    return "EARLY_EXIT";
  }
  return "UNREACHABLE";
}
extern "C" {
void* custom_malloc (size_t size) {
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
    return malloc(size);
#else
    void * retval = Sirikata::memmgr_alloc(size);
    if (retval == 0) {// did malloc succeed?
        if (!g_use_seccomp) {
            assert(false && "Out of memory error");
        }
        custom_exit(ExitCode::OOM); // ran out of memory
    }
    return retval;
#endif
}

void* custom_realloc (void * old, size_t size) {
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
    return realloc(old, size);
#else
    size_t actual_size = 0;
    void * retval = Sirikata::MemMgrAllocatorRealloc(old, size, &actual_size, true, NULL);
    if (retval == 0) {// did malloc succeed?
        if (!g_use_seccomp) {
            assert(false && "Out of memory error");
        }
        custom_exit(ExitCode::OOM); // ran out of memory
    }
    return retval;
#endif
}
void custom_free(void* ptr) {
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
    free(ptr);
#else
    Sirikata::memmgr_free(ptr);
#endif
}

void * custom_calloc(size_t size) {
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
    return calloc(size, 1);
#else
    void * retval = Sirikata::memmgr_alloc(size); // guaranteed to return 0'd memory
    if (retval == 0) {// did malloc succeed?
        if (!g_use_seccomp) {
            assert(false && "Out of memory error");
        }
        custom_exit(ExitCode::OOM); // ran out of memory
    }
    return retval;
#endif
}
}
bool g_use_seccomp =
#ifndef __linux
    false
#else
    true
#endif
    ;
void* operator new (size_t size) throw(std::bad_alloc){
 void* ptr = custom_malloc(size); 
 if (ptr == 0) {// did malloc succeed?
     if (!g_use_seccomp) {
         assert(false && "Out of memory error");
     }
     custom_exit(ExitCode::OOM); // ran out of memory
 }
 return ptr;
}

void* operator new[] (size_t size) throw(std::bad_alloc){
 void* ptr = custom_malloc(size); 
 if (ptr == 0) {// did malloc succeed?
     if (!g_use_seccomp) {
         assert(false && "Out of memory error");
     }
     custom_exit(ExitCode::OOM); // ran out of memory
 }
 return ptr;
}

void operator delete (void* ptr) throw(){
    custom_free(ptr);
}
void operator delete[] (void* ptr) throw(){
    custom_free(ptr);
}
thread_local int l_emergency_close_signal = -1;
thread_local void (*atexit_f)(void*) = nullptr;
thread_local void *atexit_arg = nullptr;
void custom_atexit(void (*atexit)(void*) , void *arg) {
    assert(!atexit_f);
    atexit_f = atexit;
    atexit_arg = arg;
}
void close_thread_handle() {
    if (l_emergency_close_signal != -1) {
        const unsigned char close_data[1] = {255};
        int handle = l_emergency_close_signal;
        while (write(handle, close_data, 1) < 0 && errno == EINTR) {
        }
    }
}
void set_close_thread_handle(int handle) {
    assert(l_emergency_close_signal == -1);
    l_emergency_close_signal = handle;
}
void reset_close_thread_handle() {
    l_emergency_close_signal = -1;
}

void custom_terminate_this_thread(uint8_t exit_code) {
    close_thread_handle();
#ifdef __linux
    syscall(SYS_exit, exit_code);
#endif
}
void custom_exit(ExitCode exit_code) {
    close_thread_handle();
    if (atexit_f) {
        (*atexit_f)(atexit_arg);
        atexit_f = nullptr;
    }
    if (exit_code != ExitCode::SUCCESS) {
        while(write(2, ExitString(exit_code), strlen(ExitString(exit_code))) < 0
            && errno == EINTR) {
        }
        while(write(2, "\n", 1) < 0
              && errno == EINTR) {
        }
    }
#ifdef __linux
    syscall(SYS_exit, (int)exit_code);
#else
    exit((int)exit_code);
#endif
    abort();
}
