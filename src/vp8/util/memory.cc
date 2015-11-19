#include "memory.hh"
#include <unistd.h>
#include <errno.h>
#ifdef __linux
#include <sys/syscall.h>
#endif
#if defined(__APPLE__) || __cplusplus <= 199711L
#define thread_local __thread
#endif
extern "C" {
void* custom_malloc (size_t size) {
#if 0
    return malloc(size);
#else
    void * retval = Sirikata::memmgr_alloc(size);
    if (retval == 0) {// did malloc succeed?
        if (!g_use_seccomp) {
            assert(false && "Out of memory error");
        }
        custom_exit(37); // ran out of memory
    }
    return retval;
#endif
}

void* custom_realloc (void * old, size_t size) {
#if 0
    return realloc(old, size);
#else
    size_t actual_size = 0;
    void * retval = Sirikata::MemMgrAllocatorRealloc(old, size, &actual_size, true, NULL);
    if (retval == 0) {// did malloc succeed?
        if (!g_use_seccomp) {
            assert(false && "Out of memory error");
        }
        custom_exit(37); // ran out of memory
    }
    return retval;
#endif
}
void custom_free(void* ptr) {
#if 0
    free(ptr);
#else
    Sirikata::memmgr_free(ptr);
#endif
}

void * custom_calloc(size_t size) {
#if 0
    return calloc(size, 1);
#else
    void * retval = Sirikata::memmgr_alloc(size); // guaranteed to return 0'd memory
    if (retval == 0) {// did malloc succeed?
        if (!g_use_seccomp) {
            assert(false && "Out of memory error");
        }
        custom_exit(37); // ran out of memory
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
     custom_exit(37); // ran out of memory
 }
 return ptr;
}

void* operator new[] (size_t size) throw(std::bad_alloc){
 void* ptr = custom_malloc(size); 
 if (ptr == 0) {// did malloc succeed?
     if (!g_use_seccomp) {
         assert(false && "Out of memory error");
     }
     custom_exit(37); // ran out of memory
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
void custom_exit(uint8_t exit_code) {
    close_thread_handle();
    if (atexit_f) {
        (*atexit_f)(atexit_arg);
        atexit_f = nullptr;
    }
#ifdef __linux
    syscall(SYS_exit, exit_code);
#else
    exit(exit_code);
#endif
}
