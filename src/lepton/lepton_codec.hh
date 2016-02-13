#ifndef _LEPTON_CODEC_HH_
#define _LEPTON_CODEC_HH_
#include "model.hh"
#include "bool_decoder.hh"
#include "base_coders.hh"
class UncompressedComponents;

class LeptonCodec {
protected:
    struct ThreadState {
        ProbabilityTablesBase model_;
        // the splits this thread is concerned with...always 1 more than the number of work items
        std::vector<int> luma_splits_;
        Sirikata::Array1d<bool, (size_t)ColorChannel::NumBlockTypes> is_top_row_;
        Sirikata::Array1d<VContext, (size_t)ColorChannel::NumBlockTypes > context_;
        //the last 2 rows of the image for each channel
        Sirikata::Array1d<std::vector<NeighborSummary>, (size_t)ColorChannel::NumBlockTypes> num_nonzeros_;
        bool is_valid_range_;
        uint32_t decode_index;
        BoolDecoder bool_decoder_;
        template<class Left, class Middle, class Right, bool should_force_memory_optimization>
        void decode_row(Left & left_model,
                        Middle& middle_model,
                        Right& right_model,
                        int block_width,
                        BlockBasedImagePerChannel<should_force_memory_optimization>&image_data,
                        int component_size_in_block);
        template<bool force_memory_optimization>
        void decode_row(BlockBasedImagePerChannel<force_memory_optimization>& image_data,
                                          Sirikata::Array1d<uint32_t,
                                                            (uint32_t)ColorChannel::
                                                            NumBlockTypes> component_size_in_block,
                                                  int component,
                                                  int curr_y);

        CodingReturnValue vp8_decode_thread(int thread_id, UncompressedComponents * const colldata);
    };
    static uint32_t gcd(uint32_t a, uint32_t b) {
        while(b) {
            uint32_t tmp = a % b;
            a = b;
            b = tmp;
        }
        return a;
    }
    struct RowSpec {
        int min_row_luma_y;
        int next_row_luma_y;
        int luma_y;
        int component;
        int component_y;
        int mcu_row_index;
        bool last_row_to_complete_mcu;
    };
    template<class BlockBasedImagePerChannels>
    static RowSpec row_spec_from_index(uint32_t decode_index,
                                    const BlockBasedImagePerChannels& image_data) {
        uint32_t luma_height = image_data[0]->original_height();
        uint32_t num_cmp = (uint32_t)ColorChannel::NumBlockTypes;
        uint32_t overall_gcd = luma_height;
        uint32_t heights[(uint32_t)ColorChannel::NumBlockTypes] = {0};
        uint32_t component_multiple[(uint32_t)ColorChannel::NumBlockTypes] = {0};
        for (int i = 0; i < num_cmp; ++i) {
            uint32_t cur_height = heights[i] = image_data[i] ? image_data[i]->original_height() : 0;
            if (cur_height) {
                overall_gcd = gcd(overall_gcd, cur_height);
            }
        }
        uint32_t mcu_multiple = 0;
        for (uint32_t i = 0; i < num_cmp; ++i) {
            component_multiple[i] = heights[i] / overall_gcd;
            mcu_multiple += component_multiple[i];
        }
        uint32_t mcu_row = decode_index / mcu_multiple;
        RowSpec retval = {};
        retval.mcu_row_index = mcu_row;
        uint32_t place_within_scan = decode_index - mcu_row * mcu_multiple;
        retval.component = num_cmp;
        retval.min_row_luma_y = (mcu_row) * component_multiple[0];
        retval.next_row_luma_y =  retval.min_row_luma_y + component_multiple[0];
        retval.luma_y = retval.min_row_luma_y;
        for (uint32_t i = num_cmp - 1; true; --i) {
            if (place_within_scan < component_multiple[i]) {
                retval.component = i;
                retval.component_y = mcu_row * component_multiple[i] + place_within_scan;
                retval.last_row_to_complete_mcu = (place_within_scan + 1 == component_multiple[i]
                                                   && i == 0);
                if (i == 0) {
                    retval.luma_y = retval.component_y;
                }
                break;
            } else {
                place_within_scan -= component_multiple[i];
            }
            if (i == 0) {
                assert(false);
                break;
            }
        }
        return retval;
    }
    bool do_threading_;
    Sirikata::Array1d<GenericWorker,
                      (NUM_THREADS - 1)>::Slice spin_workers_;
    std::thread *workers[NUM_THREADS];
    ThreadState *thread_state_[NUM_THREADS];

    void reset_thread_model_state(int thread_id) {
        (&thread_state_[thread_id]->model_)->~ProbabilityTablesBase();
        new (&thread_state_[thread_id]->model_) ProbabilityTablesBase();
    }
    void registerWorkers(Sirikata::Array1d<GenericWorker, (NUM_THREADS - 1)>::Slice workers) {
        spin_workers_ = workers;
    }
    LeptonCodec(bool do_threading) {
        do_threading_ = do_threading;
        int num_threads = 1;
        if (do_threading) {
            num_threads = NUM_THREADS;
        }
        for (int i = 0; i < num_threads; ++i) {
            thread_state_[i] = new ThreadState;
            thread_state_[i]->model_.load_probability_tables();
        }
    }
    ~LeptonCodec() {
        for (int i = 0; i < NUM_THREADS; ++i) {
            if (thread_state_[i]) {
                delete thread_state_[i];
            }
        }
    }

};
#endif
