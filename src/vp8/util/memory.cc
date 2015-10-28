#include "memory.hh"
#ifdef __linux
#include <unistd.h>
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
    return Sirikata::memmgr_alloc(size);
#endif
}

void* custom_realloc (void * old, size_t size) {
#if 0
    return realloc(old, size);
#else
    size_t actual_size = 0;
    return Sirikata::MemMgrAllocatorRealloc(old, size, &actual_size, true, NULL);
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
    return Sirikata::memmgr_alloc(size); // guaranteed to return 0'd memory
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
     assert(false && "Out of memory error");
     custom_exit(37); // ran out of memory
 }
 return ptr;
}

void* operator new[] (size_t size) throw(std::bad_alloc){
 void* ptr = custom_malloc(size); 
 if (ptr == 0) {// did malloc succeed?
     assert(false && "Out of memory error");
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
thread_local void (*atexit_f)(void*) = nullptr;
thread_local void *atexit_arg = nullptr;
void custom_atexit(void (*atexit)(void*) , void *arg) {
    assert(!atexit_f);
    atexit_f = atexit;
    atexit_arg = arg;
}
void custom_terminate_this_thread(uint8_t exit_code) {
#ifdef __linux
    syscall(SYS_exit, exit_code);
#endif
}
void custom_exit(uint8_t exit_code) {
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
