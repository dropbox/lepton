/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <atomic>
#include <functional>
#include <algorithm>
#include <string.h>
#include <assert.h>
#include <thread>
#include "base_coders.hh"
#include "component_info.hh"
#include "../vp8/model/color_context.hh"
#include "../vp8/util/block_based_image.hh"
struct componentInfo;
#define EXIT_CODE_CODING_ERROR 2

class Block;



template <bool use_threading, typename counter_type> class BaseUncompressedComponents {
    class ExtendedComponentInfo {
        ExtendedComponentInfo(const ExtendedComponentInfo&); // not implemented
        ExtendedComponentInfo operator=(const ExtendedComponentInfo&); // not implemented
    public:
        BlockBasedImage component_;
        counter_type dpos_block_progress_;
        componentInfo info_;
        int trunc_bcv_; // the number of vertical components in this (truncated) image
        int trunc_bc_;
        ExtendedComponentInfo() :dpos_block_progress_(0),
                                 trunc_bcv_(0), trunc_bc_(0) {
        }
    };

    int cmpc_; // the number of components
    ExtendedComponentInfo header_[4];

    counter_type coefficient_position_progress_;
    counter_type bit_progress_;
    counter_type worker_start_read_signal_;
    int allocated_;
    BaseDecoder *decoder_;
    BaseUncompressedComponents(const BaseUncompressedComponents&);// not implemented
    BaseUncompressedComponents&operator=(const BaseUncompressedComponents&);// not implemented
    int bch_(int component) const {
        return header_[component].info_.bch;
    }
    int bcv_(int component) const {
        return header_[component].trunc_bcv_;
    }
    static void decoder_worker_thread_function(BaseUncompressedComponents*thus){
        thus->worker_wait_for_begin_signal();
        CodingReturnValue ret;
        while ((ret = thus->decoder_->decode_chunk(thus)) != CODING_ERROR) {   
        }
    }
public:
    BaseUncompressedComponents() : coefficient_position_progress_(0), bit_progress_(0), worker_start_read_signal_(0) {
        decoder_ = NULL;
        allocated_ = 0;
    }
    unsigned short *get_quantization_tables(BlockType component) const {
        return header_[(int)component].info_.qtable;
    }
    void worker_wait_for_begin_signal() {
        assert(!use_threading);
#if 0
        if (use_threading) {
            while ((worker_start_read_signal_ += 0) == 0) {
            }
            std::atomic_thread_fence(std::memory_order_acquire);
        }
#endif
    }
    BlockColorContextIndices get_color_context(int curr_x,
                                               const Sirikata::Array1d<VContext, 3>&curr_y,
                                               int curr_component) const {
        BlockColorContextIndices retval; // zero initialize
#ifdef USE_COLOR_VALUES
        if (curr_component > 0) {
            size_t ratioX = std::max(header_[0].info_.bch/header_[curr_component].info_.bch, 1);
            size_t ratioY = std::max(header_[0].info_.bcv/header_[curr_component].info_.bcv, 1);
            for (size_t i = 0; i < ratioY && i < sizeof(retval.luminanceIndex)/sizeof(retval.luminanceIndex[0]); ++i) {
                for (size_t j = 0;
                     j < ratioX && i < sizeof(retval.luminanceIndex[0])/sizeof(retval.luminanceIndex[0][0]);
                     ++j) {
                    retval.luminanceIndex[i][j] = std::pair<int, int>(curr_y[0].y - ratioY + i, curr_x * ratioX + j);
                }
            }
            if (curr_component > 1) {
                retval.chromaIndex = std::pair<int, int>(curr_y[1].y - 1, curr_x);
            }
        }
#endif
        return retval;
    }
    template<class Context> bool get_next_component(const Sirikata::Array1d<Context, (size_t)ColorChannel::NumBlockTypes> &curr_y,
                                                    BlockType *out_component,
                                                    int *out_luma_y) const {
        int min_height = header_[0].info_.bcv;
        for (int i = 1; i < cmpc_&& i < (int)ColorChannel::NumBlockTypes; ++i) {
            min_height = std::min(header_[i].info_.bcv, min_height);
        }
        int adj_y[sizeof(header_)/sizeof(header_[0])];
        for (int i = 0; i < cmpc_ && i < (int)ColorChannel::NumBlockTypes; ++i) {
            adj_y[i] = (uint32_t)(((uint64_t)curr_y[i].y * (uint64_t)min_height)/(uint64_t)header_[i].info_.bcv);
        }
        int max_component = std::min(cmpc_, (int)ColorChannel::NumBlockTypes);
        int original = -1;
        int best_selection = original;
        for (int i = best_selection + 1; i < max_component ; ++i) {
            if ((best_selection == original || adj_y[best_selection] > adj_y[i])
                && curr_y[i].y < header_[i].trunc_bcv_) {
                best_selection = i;
            }
        }

/*
        int best_selection = max_component;
        for (int i = best_selection - 1; i >= 0; --i) {
            if ((best_selection == max_component || adj_y[best_selection] > adj_y[i]) && curr_y[i].y < header_[i].trunc_bcv_) {
                best_selection = i;
            }
        }
*/
        if (best_selection != original) {
            //fprintf(stderr, "BEST COMPONNET for %d = %d\n", curr_y[best_selection].y, best_selection);
            *out_component = (BlockType)best_selection;
            if (best_selection == 0) {
                *out_luma_y = curr_y[0].y;
            } else {
                *out_luma_y = curr_y[0].y - 1;
            }
            return true;
        }
        return false;
    }
    int get_num_components() const{
        return cmpc_;
    }
    void copy_data_to_main_thread() {
        assert(!use_threading);
#if 0
        if (use_threading) {
            std::atomic_thread_fence(std::memory_order_release);
        }
#endif
    }
    void copy_data_from_worker_thread() {
        assert(!use_threading);
#if 0
        if (use_threading) {
            std::atomic_thread_fence(std::memory_order_acquire);
        }
#endif
    }
    void worker_update_bit_progress(int add_bit_progress) {
        copy_data_to_main_thread();
        bit_progress_ += add_bit_progress;
    }
    void worker_update_coefficient_position_progress(int add_coefficient_position_progress) {
        copy_data_to_main_thread();
        coefficient_position_progress_ += add_coefficient_position_progress;
    }
    void worker_update_cmp_progress(BlockType cmp, int add_bit_progress) {
        copy_data_to_main_thread();
        header_[(int)cmp].dpos_block_progress_ += add_bit_progress;
    }
    void worker_mark_cmp_finished(BlockType cmp) {
        copy_data_to_main_thread();
        counter_type dpos_block_progress_ = header_[(int)cmp].trunc_bc_;
        header_[(int)cmp].dpos_block_progress_ = dpos_block_progress_;
    }
    CodingReturnValue do_more_work() {
        if (use_threading) {
            return CODING_DONE;
        } else {
            return decoder_->decode_chunk(this);
        }
    }
    void init(componentInfo cmpinfo[ sizeof(header_)/sizeof(header_[0]) ], int cmpc) {
        if (cmpc > (int)ColorChannel::NumBlockTypes) {
            cmpc = (int)ColorChannel::NumBlockTypes;
            //abort here: we probably can't support this kind of image
            assert(cmpc <= (int)ColorChannel::NumBlockTypes && "We only support 3 color channels or less");
            custom_exit(2);
        }
        cmpc_ = cmpc;
        allocated_ = 0;
        for (int cmp = 0; cmp < cmpc; cmp++) {
            header_[cmp].info_ = cmpinfo[cmp];
            header_[cmp].trunc_bcv_ = cmpinfo[cmp].bcv;
            header_[cmp].trunc_bc_ = cmpinfo[cmp].bc;
            allocated_ += cmpinfo[cmp].bc * 64;
        }
        for (int cmp = 0; cmp < (int)sizeof(header_)/(int)sizeof(header_[0]) && cmp < cmpc; cmp++) {
            this->header_[cmp].component_.init(cmpinfo[cmp].bch, cmpinfo[cmp].bcv, cmpinfo[cmp].bc);

        }
    }
    void set_block_count_dpos(ExtendedComponentInfo *ci, int trunc_bc) {
        assert(ci->info_.bcv == ci->info_.bc / ci->info_.bch + (ci->info_.bc % ci->info_.bch ?  1 : 0));
        int vertical_scanlines = std::min(trunc_bc / ci->info_.bch + (trunc_bc % ci->info_.bch ? 1 : 0), ci->info_.bcv);
        
        assert(vertical_scanlines <= ci->info_.bcv);
        ci->trunc_bcv_ = vertical_scanlines;
        ci->trunc_bc_ = trunc_bc;
    }
    void set_truncation_bounds(int /*max_cmp*/, int /*max_bpos*/,
                               int max_dpos[sizeof(header_)/sizeof(header_[0])], int /*max_sah*/) {
        for (int i = 0; i < cmpc_; ++i) {
            set_block_count_dpos(&header_[i], max_dpos[i] + 1);
        }
    }
    void start_decoder_worker_thread(BaseDecoder *decoder) {
        decoder_ = decoder;
        if (use_threading) {
            std::function<void()> f(std::bind(&BaseUncompressedComponents::decoder_worker_thread_function, this));
            std::thread thread(f);
            thread.detach();
        }
    }
    void wait_for_worker_on_bit(int bit) {
        bool have_data = true;
        while (bit >= (bit_progress_ += 0)) {
            have_data = false;
            CodingReturnValue retval = do_more_work();
            if (retval == CODING_ERROR) {
                assert(false && "Incorrectly coded item");
                custom_exit(EXIT_CODE_CODING_ERROR);
            }
            //fprintf(stderr, "Waiting for bit %d > %d\n", bit, bit_progress_ += 0);
        }
        if (!have_data) {
            copy_data_from_worker_thread();
        }
    }
    void wait_for_worker_on_bpos(int bpos) {
        bool have_data = true;
        while (bpos >= (coefficient_position_progress_ += 0)) {
            have_data = false;
            CodingReturnValue retval = do_more_work();
            if (retval == CODING_ERROR) {
                assert(false && "Incorrectly coded item");
                custom_exit(EXIT_CODE_CODING_ERROR);
            }
            //fprintf(stderr, "Waiting for coefficient_position %d > %d\n", bpos, coefficient_position_progress_ += 0);
        }
        if (!have_data) {
            copy_data_from_worker_thread();
        }
    }
    void wait_for_worker_on_dpos(int cmp, int dpos) {
        bool have_data = true;
        dpos = std::min(dpos, header_[cmp].trunc_bc_ - 1);
        while (dpos >= (header_[cmp].dpos_block_progress_ += 0)) {
            have_data = false;
            CodingReturnValue retval = do_more_work();
            if (retval == CODING_ERROR) {
                assert(false && "Incorrectly coded item");
                custom_exit(EXIT_CODE_CODING_ERROR);
            }
        }
        if (!have_data) {
            copy_data_from_worker_thread();
        }
        if (!have_data) {
            // DEBUG ONLY fprintf(stderr, "Was waiting for dpos[%d] %d > %d\n", cmp, dpos, (header_[cmp].dpos_block_progress_ += 0));
        }
    }
    void signal_worker_should_begin() {
        //std::atomic_thread_fence(std::memory_order_release);
        worker_start_read_signal_++;
    }
    unsigned int component_size_in_bytes(int cmp) const {
        return sizeof(short) * header_[cmp].trunc_bc_ * 64;
    }
    unsigned int component_size_in_shorts(int cmp) const {
        return header_[cmp].trunc_bc_ * 64;
    }
    unsigned int component_size_in_blocks(int cmp) const {
        return header_[cmp].trunc_bc_;
    }
    BlockBasedImage& full_component_write(BlockType cmp) {
        return header_[(int)cmp].component_;
    }
    const BlockBasedImage& full_component_nosync(int cmp) const{
        return header_[cmp].component_;

    }
    signed short at(BlockType cmp, int bpos, int x, int y) {
        int dpos = header_[cmp].info.bch * y + x;
        wait_for_worker_on_dpos(cmp, dpos);
        return header_[cmp].component_[64 * dpos + unzigzag[bpos]]; // fixme: do we care bout nch?
    }
    signed short&set(BlockType cmp, int bpos, int dpos) {
        return header_[(int)cmp].component_.
            raster(dpos).mutable_coefficients_zigzag(bpos);
    }
    signed short at(BlockType cmp, int bpos, int dpos) {
        wait_for_worker_on_dpos((int)cmp, dpos);
        return header_[(int)cmp].component_.
            raster(dpos).coefficients_zigzag(bpos);
    }
    signed short at_nosync(BlockType cmp, int bpos, int dpos) const {
        return header_[(int)cmp].component_.
            raster(dpos).coefficients_zigzag(bpos);
    }

    int block_height( const int cmp ) const
    {
        return bcv_(cmp);
    }

    int block_width( const int cmp ) const
    {
        return bch_(cmp);
    }
    
    int block_width( const BlockType cmp ) const
    {
        return bch_((int)cmp);
    }
    
    void reset() {
        bit_progress_ -= bit_progress_;
    }
    ~BaseUncompressedComponents() {
        reset();
    }
};
typedef BaseUncompressedComponents<false, int> UncompressedComponents;

