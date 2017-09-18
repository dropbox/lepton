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
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <cstdint>
#include "DecoderPlatform.hh"
#include "MemMgrAllocator.hh"
#if (defined(__APPLE__) || __cplusplus <= 199711L) && !defined(_WIN32)
#define THREAD_LOCAL_STORAGE __thread
#else
#include <atomic>
#define THREAD_LOCAL_STORAGE thread_local
#endif
#ifdef DEBUG_MEMMGR
volatile int last_adj = 0;
#endif
namespace Sirikata {

void memmgr_free_helper(void*, bool);

using std::size_t;
union mem_header_union
{
    typedef char Align[16];
    struct
    {
        // Pointer to the next block in the free list
        //
        union mem_header_union* next;

        // Size of the block (in quantas of sizeof(mem_header_t))
        //
        size_t size;
    } s;

    // Used to align headers in memory to a boundary
    //
    Align align_dummy;
};

typedef union mem_header_union mem_header_t;
size_t min_pool_alloc_quantas = 256;

struct MemMgrState {
    mem_header_t base;
// Start of free list
//
    mem_header_t* freep;
// Initial empty list
//
    size_t pool_free_pos;
// Static pool for new allocations
//
    uint8_t *pool;
    size_t pool_size;
    size_t total_ever_allocated;
    bool used_calloc;
};
size_t  memmgr_num_memmgrs = 0;
MemMgrState *memmgrs = NULL;
size_t memmgr_bytes_allocated = 0;
#if __cplusplus <= 199711L && !(defined (_WIN32))
AtomicValue<size_t> bytes_currently_used(0);
AtomicValue<size_t> bytes_ever_allocated(0);
#else
std::atomic<size_t> bytes_currently_used(0);
std::atomic<size_t> bytes_ever_allocated(0);
#endif
THREAD_LOCAL_STORAGE int memmgr_thread_id_plus_one = 0;
#if __cplusplus <= 199711L && !defined(_WIN32)
AtomicValue<int> memmgr_allocated_threads((0));
#else
std::atomic<int> memmgr_allocated_threads((0));
#endif
MemMgrState& get_local_memmgr(){
    int id = memmgr_thread_id_plus_one;
    if (!id) {
        memmgr_thread_id_plus_one = id = ++memmgr_allocated_threads;
        if (id > (int)memmgr_num_memmgrs) {
            always_assert(false && "Too many threads have requested access to memory-managers:"
                   "init with higher thread count");
        }
    }
    return memmgrs[id - 1];
}
/// caution: need to call this once per thread
void memmgr_destroy() {
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
    return;
#endif
    memmgr_thread_id_plus_one = 0; // only clears this thread
    if (memmgrs) {
#if defined(USE_MMAP) && defined(__linux__) // only linux guarantees all zeros
        if (!memmgrs->used_calloc) {
            munmap(memmgrs, memmgr_bytes_allocated);
        } else 
#endif
        {
            free(memmgrs);
        }
    }
    memmgr_bytes_allocated = 0;
    memmgr_num_memmgrs = 0;
    memmgrs = NULL;
    int last = 0;
    if (memmgr_allocated_threads.load()) {
        while ((last = --memmgr_allocated_threads) > 0) { // there needed to be at least one
        }
        while (last < 0) {
            ++memmgr_allocated_threads; // this shouldn't hit
        }
    }
}
void setup_memmgr(MemMgrState& memmgr, uint8_t *data, size_t size) {
    memset(&memmgr, 0, sizeof(MemMgrState));
    memmgr.base.s.next = 0;
    memmgr.base.s.size = 0;
    memmgr.freep = 0;
    memmgr.pool_free_pos = 0;
    memmgr.pool = data;
    memmgr.pool_size = size;
}
void memmgr_init(size_t main_thread_pool_size, size_t worker_thread_pool_size, size_t num_workers, size_t x_min_pool_alloc_quantas, bool needs_huge_pages)
{
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
    return;
#endif
#ifdef __APPLE__
    // in apple, the thread_local storage winds up different when destroying the thread
    num_workers *= 2;
#endif
    min_pool_alloc_quantas = x_min_pool_alloc_quantas;
    memmgr_num_memmgrs = num_workers + 1;
    
    size_t pool_overhead_size = sizeof(MemMgrState) * (1 + num_workers);
    size_t total_size = pool_overhead_size + main_thread_pool_size + worker_thread_pool_size * num_workers;
    uint8_t * data = NULL;
    bool used_calloc = false;
#if defined(USE_MMAP) && defined(__linux__) // only linux guarantees all zeros
    if (needs_huge_pages) {
        data = (uint8_t*)mmap(NULL, total_size, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
        if (data == MAP_FAILED) {
            const char * error = "Huge pages unsupported: falling back to ordinary pages\n";
            int ret = write(2, error, strlen(error));
            (void)ret;
        }
    }
    if (data == MAP_FAILED || !needs_huge_pages) {
        data = (uint8_t*)mmap(NULL, total_size, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (data == MAP_FAILED) {
            perror("mmap");
            data = NULL;
        }
    }
#endif
    if (!data) {
        used_calloc = true;
        data = (uint8_t*)calloc(total_size, 1);
    }
    if (!data) {
        fprintf(stderr, "Insufficient memory: unable to mmap or calloc %lu bytes\n", (unsigned long)total_size);
        fflush(stderr);
        exit(37);
    }
    memmgrs = (MemMgrState*)data;
    memmgrs->used_calloc = used_calloc;
    memmgr_bytes_allocated = pool_overhead_size + main_thread_pool_size + worker_thread_pool_size * num_workers;
    data += pool_overhead_size;
    setup_memmgr(memmgrs[0], data, main_thread_pool_size);
    data += main_thread_pool_size;
    for (int i = 0; i < (int)num_workers; ++i) {
        setup_memmgr(memmgrs[i + 1],
                     data,
                     worker_thread_pool_size);
        data += worker_thread_pool_size;
    }
    always_assert((size_t)(data - (uint8_t*)memmgrs) == total_size);
    MemMgrState & main_thread_state = get_local_memmgr();
    (void)main_thread_state;
    always_assert(main_thread_state.pool_size == main_thread_pool_size);
}
size_t memmgr_size_allocated() {
    //MemMgrState& memmgr = get_local_memmgr();
    //return memmgr.pool_free_pos;
    return bytes_currently_used.load();
}
size_t memmgr_size_left() {
  MemMgrState& memmgr = get_local_memmgr();
  return memmgr.pool_size - memmgr.pool_free_pos;
}
size_t memmgr_total_size_ever_allocated() {
  return bytes_ever_allocated.load();
}
void memmgr_tally_external_bytes(ptrdiff_t bytes) {
    bytes_ever_allocated += bytes;
    bytes_currently_used += bytes;
}
void memmgr_print_stats()
{
    MemMgrState& memmgr = get_local_memmgr();
    (void)memmgr;
#ifdef DEBUG_MEMMGR_SUPPORT_STATS
    mem_header_t* p;

    printf("------ Memory manager stats ------\n\n");
    printf("Workers consumed: %d\n", memmgr_allocated_threads.load());
    printf(    "Memmgr.Pool: free_pos = %lu (%lu uint8_ts left)\n\n",
            memmgr.pool_free_pos, memmgr.pool_size - memmgr.pool_free_pos);

    p = (mem_header_t*) memmgr.pool;

    while (p < (mem_header_t*) (memmgr.pool + memmgr.pool_free_pos))
    {
        printf(    "  * Addr: 0x%8p; Size: %8lu\n",
                p, p->s.size);

        p += p->s.size;
    }

    printf("\nFree list:\n\n");

    if (memmgr.freep)
    {
        p = memmgr.freep;

        while (1)
        {
            printf(    "  * Addr: 0x%8p; Size: %8lu; Next: 0x%8p\n",
                    p, p->s.size, p->s.next);

            p = p->s.next;

            if (p == memmgr.freep)
                break;
        }
    }
    else
    {
        printf("Empty\n");
    }

    printf("\n");
    #endif // DEBUG_MEMMGR_SUPPORT_STATS
}


static mem_header_t* get_mem_from_pool(MemMgrState& memmgr, size_t nquantas, mem_header_t** blessed_zero)
{
    size_t total_req_size;

    mem_header_t* h;

    if (nquantas < min_pool_alloc_quantas)
            nquantas = min_pool_alloc_quantas;

    total_req_size = nquantas * sizeof(mem_header_t);
    //fprintf(stderr, "+%ld\n", total_req_size);
    if (memmgr.pool_free_pos + total_req_size <= memmgr.pool_size)
    {
        h = (mem_header_t*) (memmgr.pool + memmgr.pool_free_pos);
        h->s.size = nquantas;
        memmgr_free_helper((void*) (h + 1), false);
        memmgr.pool_free_pos += total_req_size;
        bytes_currently_used += total_req_size;
    }
    else
    {
        *blessed_zero = NULL;
        return 0;
    }
    *blessed_zero = h;
    return memmgr.freep;
}

namespace {
bool is_zero(const void * data, size_t size) {
    const char * cdata = (const char *)data;
    struct Zilch {
        uint64_t a, b;
    };
    Zilch zilch = {0, 0};
    int retval = 0;
    size_t i;
    for (i = 0; i + sizeof(zilch) <= size; i+= sizeof(zilch)) {
        retval |= memcmp(cdata + i, &zilch, sizeof(zilch));
    }
    if (i != size) {
        retval |= memcmp(cdata + i, &zilch, size - i);
    }
    return retval == 0;
}
}
// Allocations are done in 'quantas' of header size.
// The search for a free block of adequate size begins at the point 'memmgr.freep'
// where the last block was found.
// If a too-big block is found, it is split and the tail is returned (this
// way the header of the original needs only to have its size adjusted).
// The pointer returned to the user points to the free space within the block,
// which begins one quanta after the header.
//
void* memmgr_alloc(size_t nuint8_ts)
{
    MemMgrState& memmgr = get_local_memmgr();
    mem_header_t* blessed_zero = NULL;
    mem_header_t* p;
    mem_header_t* prevp;
    // Calculate how many quantas are required: we need enough to house all
    // the requested uint8_ts, plus the header. The -1 and +1 are there to make sure
    // that if nuint8_ts is a multiple of nquantas, we don't allocate too much
    //
    size_t nquantas = (nuint8_ts + sizeof(mem_header_t) - 1) / sizeof(mem_header_t) + 1;
    memmgr.total_ever_allocated += std::max(nquantas, min_pool_alloc_quantas)
      * sizeof(mem_header_t);
    bytes_ever_allocated += std::max(nquantas, min_pool_alloc_quantas)
      * sizeof(mem_header_t);
    //fprintf(stderr, "A %ld\n", std::max(nquantas, min_pool_alloc_quantas) * sizeof(mem_header_t));
    // First alloc call, and no free list yet ? Use 'base' for an initial
    // degenerate block of size 0, which points to itself
    //
    if ((prevp = memmgr.freep) == 0)
    {
        memmgr.base.s.next = memmgr.freep = prevp = &memmgr.base;
        memmgr.base.s.size = 0;
    }

    for (p = prevp->s.next; ; prevp = p, p = p->s.next)
    {
        // big enough ?
        if (p->s.size >= nquantas)
        {
            // exactly ?
            if (p->s.size == nquantas)
            {
                // just eliminate this block from the free list by pointing
                // its prev's next to its next
                //
                prevp->s.next = p->s.next;
            }
            else // too big
            {
                p->s.size -= nquantas;
                p += p->s.size;
                p->s.size = nquantas;
            }

            memmgr.freep = prevp;
#ifdef DEBUG_MEMMGR
            last_adj =  p->s.size * sizeof(mem_header_t);
            fprintf(stderr, "%08ld ALLOC\n", p->s.size * sizeof(mem_header_t));
#endif
            if (blessed_zero == p) {
                dev_assert(is_zero(p + 1, nuint8_ts) && "The item returned from the new pool must be zero");
                return p + 1;
            } else {
#ifndef _WIN32
                (void)is_zero;
#endif
#ifdef DEBUG_MEMMGR
                last_adj = 0;
#endif
                return memset((p + 1), 0, nuint8_ts); // this makes sure we always return zero'd data
            }
        }
        // Reached end of free list ?
        // Try to allocate the block from the memmgr.pool. If that succeeds,
        // get_mem_from_pool adds the new block to the free list and
        // it will be found in the following iterations. If the call
        // to get_mem_from_pool doesn't succeed, we've run out of
        // memory
        //
        else if (p == memmgr.freep)
        {
            if ((p = get_mem_from_pool(memmgr, nquantas, &blessed_zero)) == 0)
            {
                #ifdef DEBUG_MEMMGR_FATAL
                printf("!! Memory allocation failed !!\n");
                #endif
#ifdef MEMMGR_EXIT_OOM
                custom_exit(ExitCode::TOO_MUCH_MEMORY_NEEDED);
#endif
#ifdef DEBUG_MEMMGR
                last_adj = 0;
#endif
                return 0;
            }
        }
    }
#ifdef DEBUG_MEMMGR
    last_adj = std::max(nquantas, min_pool_alloc_quantas)
      * sizeof(mem_header_t);
    fprintf(stderr, "%08ld ALLOC\n", std::max(nquantas, min_pool_alloc_quantas)
            * sizeof(mem_header_t));
    last_adj = 0;
#endif
}

// Scans the free list, starting at memmgr.freep, looking the the place to insert the
// free block. This is either between two existing blocks or at the end of the
// list. In any case, if the block being freed is adjacent to either neighbor,
// the adjacent blocks are combined.
//
void memmgr_free(void* ap){
    memmgr_free_helper(ap, true);
}

// same as memmgr_free but with some differences in debug logging
void memmgr_free_helper(void* ap, bool actually_free)
{
    MemMgrState& memmgr = get_local_memmgr();
    if ((uint8_t*)ap >= memmgr.pool + memmgr.pool_size
        || (uint8_t*)ap < memmgr.pool) {
        // illegal address or on another thread.
#ifdef DEBUG_MEMMGR_FATAL
        fprintf(stderr, "Memory freed on another thread than it was allocated on\n");
#endif
        return;
    }
    mem_header_t* block;
    mem_header_t* p;

    // acquire pointer to block header
    block = ((mem_header_t*) ap) - 1;
#ifdef DEBUG_MEMMGR
    if (actually_free) {
        fprintf(stderr, "%08ld FREE\n", block->s.size * sizeof(mem_header_t));
        last_adj = -(int)(block->s.size * sizeof(mem_header_t));
    }
#endif
   // Find the correct place to place the block in (the free list is sorted by
    // address, increasing order)
    //
    for (p = memmgr.freep; !(block > p && block < p->s.next); p = p->s.next)
    {
        // Since the free list is circular, there is one link where a
        // higher-addressed block points to a lower-addressed block.
        // This condition checks if the block should be actually
        // inserted between them
        //
        if (p >= p->s.next && (block > p || block < p->s.next))
            break;
    }

    // Try to combine with the higher neighbor
    //
    if (block + block->s.size == p->s.next)
    {
        block->s.size += p->s.next->s.size;
        block->s.next = p->s.next->s.next;
    }
    else
    {
        block->s.next = p->s.next;
    }

    // Try to combine with the lower neighbor
    //
    if (p + p->s.size == block)
    {
        p->s.size += block->s.size;
        p->s.next = block->s.next;
    }
    else
    {
        p->s.next = block;
    }

    memmgr.freep = p;
#ifdef DEBUG_MEMMGR
    if (actually_free) {
    last_adj = 0;
    }
#endif
}



void *MemMgrAllocatorMalloc(void *opaque, size_t nmemb, size_t size) {
    return memmgr_alloc(nmemb * size);
}
void MemMgrAllocatorFree (void *opaque, void *ptr) {
    memmgr_free(ptr);
}
void * MemMgrAllocatorInit(size_t prealloc_size, size_t worker_size, size_t num_workers, unsigned char alignment, bool needs_huge_pages) {
    dev_assert(alignment <= sizeof(mem_header_union::Align));
    memmgr_init(prealloc_size, worker_size, num_workers, 256, needs_huge_pages);
    return memmgr_alloc(1);
}
void MemMgrAllocatorDestroy(void *opaque) {
    memmgr_free(opaque);
    memmgr_destroy();
}
size_t MemMgrAllocatorMsize(void * ptr, void *opaque) {
    mem_header_t* block = ((mem_header_t*) ptr) - 1;
    return block->s.size * sizeof(mem_header_t);
}
void *MemMgrAllocatorRealloc(void * ptr, size_t amount, size_t *ret_size, unsigned int movable, void *opaque) {
    if (amount == 0) {
        memmgr_free(ptr);
        return NULL;
    }
    size_t ptr_actual_size = 0;
    if (ptr) {
        ptr_actual_size = MemMgrAllocatorMsize(ptr, opaque);
        if (ptr_actual_size >= amount) {
            *ret_size = ptr_actual_size;
            return ptr;
        }
        if (!movable) {
            return NULL;
        }
    }
    void * retval = memmgr_alloc(amount);
    *ret_size = MemMgrAllocatorMsize(retval, opaque);
    if (ptr) {
        memcpy(retval, ptr, std::min(amount, ptr_actual_size));
        memmgr_free(ptr);
    }
    return retval;
}

}
