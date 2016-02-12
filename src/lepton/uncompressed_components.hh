/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <atomic>
#include <functional>
#include <algorithm>
#include <string.h>
#include <assert.h>
#include <thread>
#include <unistd.h>
#include "base_coders.hh"
#include "component_info.hh"
#include "../vp8/model/color_context.hh"
#include "../vp8/util/block_based_image.hh"
struct componentInfo;

class Block;



class UncompressedComponents {
    typedef int CounterType;
    class ExtendedComponentInfo {
        ExtendedComponentInfo(const ExtendedComponentInfo&); // not implemented
        ExtendedComponentInfo operator=(const ExtendedComponentInfo&); // not implemented
    public:
        BlockBasedImage component_;
        CounterType dpos_block_progress_;
        componentInfo info_;
        int trunc_bcv_; // the number of vertical components in this (truncated) image
        int trunc_bc_;
        ExtendedComponentInfo() :dpos_block_progress_(0),
                                 trunc_bcv_(0), trunc_bc_(0) {
        }
    };
    int cmpc_; // the number of components
    ExtendedComponentInfo header_[4];

    CounterType coefficient_position_progress_;
    CounterType bit_progress_;
    CounterType worker_start_read_signal_;
    int allocated_;
    BaseDecoder *decoder_;
    UncompressedComponents(const UncompressedComponents&);// not implemented
    UncompressedComponents&operator=(const UncompressedComponents&);// not implemented
    int bch_(int component) const {
        return header_[component].info_.bch;
    }
    int bcv_(int component) const {
        return header_[component].trunc_bcv_;
    }
public:
    UncompressedComponents() : coefficient_position_progress_(0), bit_progress_(0), worker_start_read_signal_(0) {
        decoder_ = NULL;
        allocated_ = 0;
    }
    unsigned short *get_quantization_tables(BlockType component) const {
        return header_[(int)component].info_.qtable;
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
        int original = max_component;
        int best_selection = original;
        for (int i = best_selection - 1; i >= 0; --i) {
            if ((best_selection == max_component || adj_y[best_selection] > adj_y[i]) && curr_y[i].y < header_[i].trunc_bcv_) {
                best_selection = i;
            }
        }
        if (best_selection != original) {
            //fprintf(stderr, "Best next component for y=%d is %d\n", curr_y[best_selection].y, best_selection);
            *out_component = (BlockType)best_selection;
            *out_luma_y = curr_y[0].y;
            return true;
        }
        return false;
    }
    bool is_memory_optimized(int cmp) const {
        return header_[cmp].component_.is_memory_optimized();
    }
    int get_num_components() const{
        return cmpc_;
    }

    void worker_update_bit_progress(int add_bit_progress) {
        bit_progress_ += add_bit_progress;
    }
    void worker_update_coefficient_position_progress(int add_coefficient_position_progress) {
        coefficient_position_progress_ += add_coefficient_position_progress;
    }
    void worker_update_cmp_progress(BlockType cmp, int add_bit_progress) {
        header_[(int)cmp].dpos_block_progress_ += add_bit_progress;
    }
    void worker_mark_cmp_finished(BlockType cmp) {
        CounterType dpos_block_progress_ = header_[(int)cmp].trunc_bc_;
        header_[(int)cmp].dpos_block_progress_ = dpos_block_progress_;
    }
    void start_decoder(BaseDecoder *decoder) {
        decoder_ = decoder;
    }
    CodingReturnValue do_more_work() {
        return decoder_->decode_chunk(this);
    }
    void init(componentInfo cmpinfo[ sizeof(header_)/sizeof(header_[0]) ], int cmpc, bool memory_optimized_image) {
        if (cmpc > (int)ColorChannel::NumBlockTypes) {
            cmpc = (int)ColorChannel::NumBlockTypes;
            //abort here: we probably can't support this kind of image
            const char * errmsg = "We only support 3 color channels or fewer\n";
            int err = write(2, errmsg, strlen(errmsg));
            (void)err;
            assert(cmpc <= (int)ColorChannel::NumBlockTypes && "We only support 3 color channels or less");
            custom_exit(ExitCode::UNSUPPORTED_4_COLORS);
        }
        cmpc_ = cmpc;
        allocated_ = 0;
        for (int cmp = 0; cmp < cmpc; cmp++) {
            header_[cmp].info_ = cmpinfo[cmp];
            header_[cmp].trunc_bcv_ = cmpinfo[cmp].bcv;
            header_[cmp].trunc_bc_ = cmpinfo[cmp].bc;
            allocated_ += cmpinfo[cmp].bc * 64;
        }
        uint64_t total_req_blocks = 0;
        for (int cmp = 0; cmp < (int)sizeof(header_)/(int)sizeof(header_[0]) && cmp < cmpc; cmp++) {
            total_req_blocks += cmpinfo[cmp].bcv * cmpinfo[cmp].bch;
        }
        for (int cmp = 0; cmp < (int)sizeof(header_)/(int)sizeof(header_[0]) && cmp < cmpc; cmp++) {
            int bc_allocated = cmpinfo[cmp].bc;
            int64_t max_cmp_bc = max_number_of_blocks;
            max_cmp_bc *= cmpinfo[cmp].bcv * cmpinfo[cmp].bch;
            max_cmp_bc /= total_req_blocks;
            if (bc_allocated > max_cmp_bc) {
                bc_allocated = max_cmp_bc - (max_cmp_bc % cmpinfo[cmp].bch);
            }
            this->header_[cmp].component_.init(cmpinfo[cmp].bch,
                                               cmpinfo[cmp].bcv,
                                               bc_allocated,
                                               memory_optimized_image);

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
    void wait_for_worker_on_bit(int bit) {
        while (bit >= (bit_progress_ += 0)) {
            CodingReturnValue retval = do_more_work();
            if (retval == CODING_ERROR) {
                assert(false && "Incorrectly coded item");
                custom_exit(ExitCode::CODING_ERROR);
            }
            //fprintf(stderr, "Waiting for bit %d > %d\n", bit, bit_progress_ += 0);
        }
    }
    void wait_for_worker_on_bpos(int bpos) {
        while (bpos >= (coefficient_position_progress_ += 0)) {
            CodingReturnValue retval = do_more_work();
            if (retval == CODING_ERROR) {
                assert(false && "Incorrectly coded item");
                custom_exit(ExitCode::CODING_ERROR);
            }
            //fprintf(stderr, "Waiting for coefficient_position %d > %d\n", bpos, coefficient_position_progress_ += 0);
        }
    }
    void wait_for_worker_on_dpos(int cmp, int dpos) {
        dpos = std::min(dpos, header_[cmp].trunc_bc_ - 1);
        while (dpos >= (header_[cmp].dpos_block_progress_ += 0)) {
            CodingReturnValue retval = do_more_work();
            if (retval == CODING_ERROR) {
                assert(false && "Incorrectly coded item");
                custom_exit(ExitCode::CODING_ERROR);
            }
        }
    }
    void signal_worker_should_begin() {
        //std::atomic_thread_fence(std::memory_order_release);
        worker_start_read_signal_++;
    }
    unsigned int component_size_allocated(int cmp) const {
        return header_[cmp].component_.bytes_allocated();
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
    const AlignedBlock& block(BlockType cmp, int dpos) {
        wait_for_worker_on_dpos((int)cmp, dpos);
        return header_[(int)cmp].component_.raster(dpos);
    }
    const AlignedBlock& block_nosync(BlockType cmp, int dpos) const {
        return header_[(int)cmp].component_.raster(dpos);
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
    ~UncompressedComponents() {
        reset();
    }
    static int max_number_of_blocks;


    // the following functions are progressive-only functions (recode_jpeg)
    // or decode-only functions (decode_jpeg, check_value_range)
    // these are the only functions able to access the components
    friend bool decode_jpeg(void);
    friend bool recode_jpeg(void);
    friend bool check_value_range(void);
private:
    AlignedBlock& mutable_block(BlockType cmp, int dpos) {
        return header_[(int)cmp].component_.raster(dpos);
    }
    signed short at(BlockType cmp, int bpos, int dpos) {
        wait_for_worker_on_dpos((int)cmp, dpos);
        return header_[(int)cmp].component_.
            raster(dpos).coefficients_zigzag(bpos);
    }
    signed short&set(BlockType cmp, int bpos, int dpos) {
        return header_[(int)cmp].component_.
            raster(dpos).mutable_coefficients_zigzag(bpos);
    }

};


