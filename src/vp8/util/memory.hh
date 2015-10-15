#ifndef _MEMORY_HH_
#define _MEMORY_HH_

#include <new>
#include <cstdlib>
#include <assert.h>
#include <cstdio>
#include <cstring>
inline void* custom_malloc (size_t size) {
    return malloc(size);
}
inline void* custom_realloc (void * data, size_t size) {
    return realloc(data, size);
}
inline void custom_free(void* ptr) {
    free(ptr);
}

inline void * custom_calloc(size_t size) {
    return memset(custom_malloc(size), 0, size);
}
#endif
