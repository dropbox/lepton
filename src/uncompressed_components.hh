/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <atomic>
#include <functional>
#include <string.h>
#include <mutex>
#include "base_coders.hh"
struct componentInfo;
enum DecoderReturnValue {
    DECODER_ERROR,
    DECODER_DONE,
    DECODER_PARTIAL // run it again
};
class UncompressedComponents {
    signed short *cmpoffset_[4]; // pointers to the beginning of each component
    int bch_[4];
    int bcv_[4];
    signed short *colldata_; // we may want to swizzle this for locality
    
    std::atomic<int> cmp0_dpos_block_progress_;
    std::atomic<int> cmp1_dpos_block_progress_;
    std::atomic<int> cmp2_dpos_block_progress_;
    std::atomic<int> cmp3_dpos_block_progress_;
    std::atomic<int> band_progress_;
    std::atomic<int> bit_progress_;
    std::atomic<int> worker_start_read_signal_;
    int allocated_;
    int last_component_progress_[4]; // only accessed from main thread
    int last_band_progress_;
    int last_bit_progress_;
    UncompressedComponents(const UncompressedComponents&);// not implemented
    UncompressedComponents&operator=(const UncompressedComponents&);// not implemented
public:
    UncompressedComponents() : cmp0_dpos_block_progress_(0), cmp1_dpos_block_progress_(0), cmp2_dpos_block_progress_(0), cmp3_dpos_block_progress_(0), band_progress_(0), bit_progress_(0), worker_start_read_signal_(0) {
        colldata_ = NULL;
        allocated_ = 0;
        memset(bch_, 0, sizeof(int) * 4);
        memset(bcv_, 0, sizeof(int) * 4);
        memset(cmpoffset_, 0, sizeof(signed short*) * 4);
        memset(last_component_progress_, 0, 4 * sizeof(int));
    }
    void worker_update_bit_progress(int add_bit_progress) {
        std::atomic_thread_fence(std::memory_order_release);
        bit_progress_ += add_bit_progress;
    }
    void worker_update_band_progress(int add_band_progress) {
        std::atomic_thread_fence(std::memory_order_release);
        band_progress_ += add_band_progress;
    }
    void worker_update_cmp_progress(int cmp, int add_bit_progress) {
        std::atomic_thread_fence(std::memory_order_release);
        switch(cmp) {
          case 0: cmp0_dpos_block_progress_ += add_bit_progress;
            return;
          case 1: cmp1_dpos_block_progress_ += add_bit_progress;
            return;
          case 2: cmp2_dpos_block_progress_ += add_bit_progress;
            return;
          case 3: cmp3_dpos_block_progress_ += add_bit_progress;
            return;
        }
    }
    void init(componentInfo cmpinfo[ 4 ], int cmpc);
    void start_decoder_worker_thread(const std::function<CodingReturnValue()> &decoder_worker);
    void wait_for_worker(int cmp, int bpos, int dpos, int bit=15) {
        if (bpos < last_band_progress_ && dpos < last_component_progress_[cmp] && bit < last_bit_progress_) {
            return;
        }
        int cur_worker_progress = 0;
        if (dpos >= last_component_progress_[cmp]) {
            do {
                switch(cmp) {
                  case 0:
                    cur_worker_progress = cmp0_dpos_block_progress_.load(std::memory_order_relaxed);
                    break;
                  case 1:
                    cur_worker_progress = cmp1_dpos_block_progress_.load(std::memory_order_relaxed);
                    break;
                  case 2:
                    cur_worker_progress = cmp2_dpos_block_progress_.load(std::memory_order_relaxed);
                    break;
                  case 3:
                    cur_worker_progress = cmp3_dpos_block_progress_.load(std::memory_order_relaxed);
                    break;
                }
                if (cur_worker_progress <= dpos ) {
                    fprintf(stderr, "Waiting for cmp[%d] %d > %d\n", cmp, dpos, cur_worker_progress);
                    continue;
                }
                last_component_progress_[cmp] = cur_worker_progress;
                std::atomic_thread_fence(std::memory_order_acquire);
                break;
            }while (true);
        }
        if (bpos >= last_band_progress_) {
            do {
                cur_worker_progress = band_progress_.load(std::memory_order_relaxed);
                if (cur_worker_progress <= bpos ) {
                    fprintf(stderr, "Waiting for band %d > %d\n", bpos, cur_worker_progress);
                    continue;
                }
                last_band_progress_ = cur_worker_progress;
                std::atomic_thread_fence(std::memory_order_acquire);
                break;
            }while (true);
        }
        if (bit >= last_bit_progress_) {
            do {
                cur_worker_progress = bit_progress_.load(std::memory_order_relaxed);
                if (cur_worker_progress <= bit ) {
                    fprintf(stderr, "Waiting for bit %d > %d\n", bit, cur_worker_progress);
                    continue;
                }
                last_bit_progress_ = cur_worker_progress;
                std::atomic_thread_fence(std::memory_order_acquire);
                break;
            }while (true);
        }
    }
    void signal_worker_should_begin() {
        std::atomic_thread_fence(std::memory_order_release);        
        worker_start_read_signal_++;
    }
    void worker_wait_for_begin_signal() {
        while (worker_start_read_signal_.load(std::memory_order_relaxed) == 0) {
        }
        std::atomic_thread_fence(std::memory_order_acquire);
    }
    unsigned int component_size_in_bytes(int cmp) const {
        return sizeof(short) * bch_[cmp] * bcv_[cmp] * 64;
    }
    unsigned int component_size_in_shorts(int cmp) const {
        return bch_[cmp] * bcv_[cmp] * 64;
    }
    unsigned int component_size_in_blocks(int cmp) const {
        return bch_[cmp] * bcv_[cmp];
    }
    signed short* full_component_write(int cmp) const {
        return cmpoffset_[cmp];
    }
    const signed short* full_component_nosync(int cmp) {
        return cmpoffset_[cmp];
    }
    const signed short* full_component_read(int cmp) {
        wait_for_worker(cmp, 63, bch_[cmp] * bcv_[cmp] - 1);
        return full_component_nosync(cmp);
    }
    signed short&set(int cmp, int bpos, int x, int y) {
        return cmpoffset_[cmp][64 * (y * bch_[cmp] + x) + bpos]; // fixme: do we care bout nch?
    }
    signed short at(int cmp, int bpos, int x, int y) {
        wait_for_worker(cmp, bpos, bch_[cmp] * y + x);
        return cmpoffset_[cmp][64 * (y * bch_[cmp] + x) + bpos]; // fixme: do we care bout nch?
    }
    signed short&set(int cmp, int bpos, int dpos) {
        return cmpoffset_[cmp][dpos * 64 + bpos];
    }
    signed short at(int cmp, int bpos, int dpos) {
        wait_for_worker(cmp, bpos, dpos);
        return cmpoffset_[cmp][dpos * 64 + bpos];
    }
    signed short at_nosync(int cmp, int bpos, int dpos) const {
        return cmpoffset_[cmp][dpos * 64 + bpos];
    }
/*
    signed short* block(int cmp, int x, int y) {
        return &cmpoffset_[cmp][(y * bch_[cmp] + x) * 64]; // fixme: do we care bout nch?
    }
    const signed short* block(int cmp, int x, int y) const {
        return &cmpoffset_[cmp][(y * bch_[cmp] + x) * 64]; // fixme: do we care bout nch?
    }
    const signed short* block(int cmp, int dpos) const{
        return &cmpoffset_[cmp][dpos * 64];
    }
    signed short* block(int cmp, int dpos) {
        return &cmpoffset_[cmp][dpos * 64];
    }
*/
    void reset() {
        if (colldata_) {
            delete []colldata_;
        }
        bit_progress_.store(0);
        colldata_ = NULL;
    }
    ~UncompressedComponents() {
        reset();
    }
};
