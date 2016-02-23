#include "options.hh"
#include "memory.hh"
#include <unistd.h>
#include <errno.h>
#ifdef __linux
#include <sys/syscall.h>
#endif
#if defined(__APPLE__) || __cplusplus <= 199711L
#define THREAD_LOCAL_STORAGE __thread
#else
#define THREAD_LOCAL_STORAGE thread_local
#endif
unsigned int NUM_THREADS = MAX_NUM_THREADS;
const char *ExitString(ExitCode ec) {
  FOREACH_EXIT_CODE(GENERATE_EXIT_CODE_RETURN)
  static char data[] = "XXXX_EXIT_CODE_BEYOND_EXIT_CODE_ARRAY";
  data[0] = ((int)ec / 1000) + '0';
  data[1] = ((int)ec / 100 % 10) + '0';
  data[2] = ((int)ec / 10 % 10) + '0';
  data[3] = ((int)ec % 10) + '0';
  sprintf(data, "%d", (int)ec);
  return data;
}
extern "C" {
void always_assert_inner(bool value, const char * expr, const char * file, int line){
    if (!value) {
        while (write(2, "Assert Failed: ", strlen("Assert Failed: ")) < 0 && errno == EINTR) {

        }
        while (write(2, expr, strlen(expr)) < 0 && errno == EINTR) {

        }
        while (write(2, " at (", 5) < 0 && errno == EINTR) {

        }
        while (write(2, file, strlen(file)) < 0 && errno == EINTR) {

        }
        while (write(2, ":", 1) < 0 && errno == EINTR) {

        }
        fprintf(stderr, "%d)\n", line);
        custom_exit(ExitCode::ASSERTION_FAILURE);
    }
}
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
THREAD_LOCAL_STORAGE int l_emergency_close_signal = -1;
THREAD_LOCAL_STORAGE void (*atexit_f)(void*, uint64_t) = nullptr;
THREAD_LOCAL_STORAGE void *atexit_arg0 = nullptr;
THREAD_LOCAL_STORAGE uint64_t atexit_arg1 = 0;
void custom_atexit(void (*atexit)(void*, uint64_t) , void *arg0, uint64_t arg1) {
    assert(!atexit_f);
    atexit_f = atexit;
    atexit_arg0 = arg0;
    atexit_arg1 = arg1;
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
        (*atexit_f)(atexit_arg0, atexit_arg1);
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
