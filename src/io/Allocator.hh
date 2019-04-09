/*  Sirikata Jpeg Memory Allocator -- Texture Transfer management system
 *  main.cpp
 *
 *  Copyright (c) 2015, Daniel Reiter Horn
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef SIRIKATA_JPEG_ARHC_ALLOCATOR_HPP_
#define SIRIKATA_JPEG_ARHC_ALLOCATOR_HPP_
#include <stdlib.h>
#include "DecoderPlatform.hh"
namespace Sirikata {

template<class T> class JpegAllocator {
    typedef std::true_type propagate_on_container_move_assignment;
    typedef std::true_type propagate_on_container_swap;
    template<class U> friend class JpegAllocator;
    typedef void *(CustomAllocate)(void *opaque, size_t nmemb, size_t size);
    typedef void (CustomDeallocate)(void *opaque, void *ptr);
    // the required functions for lzham (note the requirement of opaque ptr at the end)
    typedef void *(CustomReallocate)(void * ptr, size_t size, size_t *actualSize, unsigned int movable, void *opaque);
    typedef size_t (CustomMsize)(void * ptr, void *opaque);
    CustomAllocate *custom_allocate;
    CustomDeallocate *custom_deallocate;
    CustomReallocate *custom_reallocate;
    CustomMsize *custom_msize;
    void * opaque;
    static void *malloc_wrapper(void *, size_t nmemb, size_t size) {
        return custom_malloc(nmemb * size);
    }
    static void free_wrapper(void *, void *ptr) {
        custom_free(ptr);
    }
public:
    template <class U> bool operator == (const JpegAllocator<U> &other) const {
        return custom_allocate == other.custom_allocate && custom_deallocate == other.custom_deallocate && opaque == other.opaque;
    }
    template <class U> bool operator != (const JpegAllocator<U> &other) const {
        return !((*this) == other);
    }
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    CustomAllocate* get_custom_allocate() const{
        return custom_allocate;
    }
    CustomDeallocate* get_custom_deallocate() const{
        return custom_deallocate;
    }
    CustomReallocate* get_custom_reallocate() const{
        return custom_reallocate;
    }
    CustomMsize* get_custom_msize() const{
        return custom_msize;
    }
    void *get_custom_state() const {
        return opaque;
    }
    ///starts up with malloc/free implementation
    JpegAllocator() throw() {
        custom_allocate = &malloc_wrapper;
        custom_deallocate = &free_wrapper;
        custom_reallocate = NULL;
        custom_msize = NULL;
        opaque = NULL;
    }
    template <class U> struct rebind { typedef JpegAllocator<U> other; };
    JpegAllocator(const JpegAllocator&other)throw() {
        custom_allocate = other.custom_allocate;
        custom_deallocate = other.custom_deallocate;
        custom_reallocate = other.custom_reallocate;
        custom_msize = other.custom_msize;
        opaque = other.opaque;
    }
    template <typename U> JpegAllocator(const JpegAllocator<U>&other) throw(){
        custom_allocate = other.custom_allocate;
        custom_deallocate = other.custom_deallocate;
        custom_reallocate = other.custom_reallocate;
        custom_msize = other.custom_msize;
        opaque = other.opaque;
    }
    ~JpegAllocator()throw() {}

     //this sets up the memory subsystem with the arg for this and all copied allocators
    void setup_memory_subsystem(size_t arg,
                                unsigned char alignment,
                                void *(custom_init)(size_t prealloc_size, unsigned char alignment),
                                CustomAllocate *custom_allocate,
                                CustomDeallocate *custom_deallocate,
                                CustomReallocate *custom_reallocate,
                                CustomMsize *custom_msize) {
        this->opaque = custom_init(arg, alignment);
        this->custom_allocate = custom_allocate;
        this->custom_deallocate = custom_deallocate;
        this->custom_reallocate = custom_reallocate;
        this->custom_msize = custom_msize;
    }
    // this tears down all users of this memory subsystem
    void teardown_memory_subsystem(void (*custom_deinit)(void *opaque)) {
        (*custom_deinit)(opaque);
        opaque = NULL;
    }

    pointer allocate(size_type s, void const * = 0) {
        if (0 == s)
            return NULL;
        pointer temp = (pointer)(*custom_allocate)(opaque, 1, s * sizeof(T)); 
        if (temp == NULL) {
#ifdef __EXCEPTIONS

            throw std::bad_alloc();
#else
	    custom_exit(ExitCode::TOO_MUCH_MEMORY_NEEDED);
#endif
	}
        return temp;
    }

    void deallocate(pointer p, size_type) {
        (*custom_deallocate)(opaque, p);
    }

    size_type max_size() const throw() { 
        return 0xffffffff / sizeof(T); 
    }

    void construct(pointer p, const T& val) {
        new((void *)p) T(val);
    }

    void destroy(pointer p) {
        p->~T();
    }
    
};
}
#endif
