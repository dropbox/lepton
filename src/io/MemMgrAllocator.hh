//----------------------------------------------------------------
// Statically-allocated memory manager
//
// by Eli Bendersky (eliben@gmail.com)
//  
// This code is in the public domain.

/*  Sirikata Memory Management system
 *  
 *
 *  Copyright (c) 2015 Eli Bendersky, Daniel Reiter Horn
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
#ifndef MEM_MGR_ALLOCATOR_HH_
#define MEM_MGR_ALLOCATOR_HH_

#include "DecoderPlatform.hh"
namespace Sirikata {
// Initialize the memory manager. This function should be called
// exactly once per thread that wishes to allocate memory
//
SIRIKATA_FUNCTION_EXPORT void memmgr_init(size_t main_thread_size, size_t worker_thread_size, size_t num_workers, size_t min_pool_alloc_quantas = 256,
    bool needs_huge_pages=false);

// Uninitialize the memory manager. This function should be called
// exactly once per thread that exits
SIRIKATA_FUNCTION_EXPORT void memmgr_destroy();

// 'malloc' clone
//
SIRIKATA_FUNCTION_EXPORT void* memmgr_alloc(size_t nbytes);

// 'free' clone
//
SIRIKATA_FUNCTION_EXPORT void memmgr_free(void* ap);

// Prints statistics about the current state of the memory
// manager
//
SIRIKATA_FUNCTION_EXPORT void memmgr_print_stats();
SIRIKATA_FUNCTION_EXPORT size_t memmgr_size_allocated();
SIRIKATA_FUNCTION_EXPORT size_t memmgr_total_size_ever_allocated();
SIRIKATA_FUNCTION_EXPORT size_t memmgr_size_left();
SIRIKATA_FUNCTION_EXPORT void memmgr_tally_external_bytes(ptrdiff_t bytes);
}
namespace Sirikata {
SIRIKATA_FUNCTION_EXPORT void *MemMgrAllocatorMalloc(void *opaque, size_t nmemb, size_t size);
SIRIKATA_FUNCTION_EXPORT void MemMgrAllocatorFree (void *opaque, void *ptr);
SIRIKATA_FUNCTION_EXPORT void * MemMgrAllocatorInit(size_t prealloc_size, size_t worker_size, size_t num_workers, unsigned char alignment);
SIRIKATA_FUNCTION_EXPORT void MemMgrAllocatorDestroy(void *opaque);
SIRIKATA_FUNCTION_EXPORT void* MemMgrAllocatorRealloc(void * ptr, size_t size, size_t *actualSize, unsigned int movable, void *opaque);
SIRIKATA_FUNCTION_EXPORT size_t MemMgrAllocatorMsize(void * ptr, void *opaque);

}
#endif
