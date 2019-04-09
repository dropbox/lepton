#ifndef LEPTON_CODEC_HH_
#define LEPTON_CODEC_HH_
#include "jpgcoder.hh"
#include "model.hh"
#include "bool_decoder.hh"
#include "base_coders.hh"
#include "../../io/MuxReader.hh"
#include "stream_interfaces.hh"
class UncompressedComponents;
    struct LeptonCodec_RowSpec {
        int min_row_luma_y;
        int next_row_luma_y;
        int luma_y;
        int component;
        int curr_y;
        int mcu_row_index;
        bool last_row_to_complete_mcu;
        bool skip;
        bool done;
    };

template<class BoolDecoder> struct IsDecoderAns {
};
#ifdef ENABLE_ANS_EXPERIMENTAL
template<> struct IsDecoderAns<ANSBoolReader> {
    enum {
        IS_ANS = true
    };
};
#endif
template<> struct IsDecoderAns<VPXBoolReader> {
    enum {
        IS_ANS = false
    };
};



class VP8ComponentDecoder_SendToActualThread;

template<class BlockBasedImagePerChannels>
static LeptonCodec_RowSpec LeptonCodec_row_spec_from_index(uint32_t decode_index,
                                               const BlockBasedImagePerChannels& image_data,
                                               int mcuv, // number of mcus
                                               Sirikata::Array1d<uint32_t,
                                               (size_t)ColorChannel
                                               ::NumBlockTypes> max_coded_heights) {
    
    uint32_t num_cmp = (uint32_t)ColorChannel::NumBlockTypes;
    uint32_t heights[(uint32_t)ColorChannel::NumBlockTypes] = {0};
    uint32_t component_multiple[(uint32_t)ColorChannel::NumBlockTypes] = {0};
    uint32_t mcu_multiple = 0;
    for (uint32_t i = 0; i < num_cmp; ++i) {
        heights[i] = image_data[i] ? image_data[i]->original_height() : 0;
        component_multiple[i] = heights[i] / mcuv;
        mcu_multiple += component_multiple[i];
    }
    uint32_t mcu_row = decode_index / mcu_multiple;
    LeptonCodec_RowSpec retval = {0, 0, 0, 0, 0, 0, false, false, false};
    retval.skip = false;
    retval.done = false;
    retval.mcu_row_index = mcu_row;
    uint32_t place_within_scan = decode_index - mcu_row * mcu_multiple;
    retval.component = num_cmp;
    retval.min_row_luma_y = (mcu_row) * component_multiple[0];
    retval.next_row_luma_y =  retval.min_row_luma_y + component_multiple[0];
    retval.luma_y = retval.min_row_luma_y;
    for (uint32_t i = num_cmp - 1; true; --i) {
        if (place_within_scan < component_multiple[i]) {
            retval.component = i;
            retval.curr_y = mcu_row * component_multiple[i] + place_within_scan;
            retval.last_row_to_complete_mcu = (place_within_scan + 1 == component_multiple[i]
                                               && i == 0);
            if (retval.curr_y >= int( max_coded_heights[i] ) ) {
                retval.skip = true;
                retval.done = true; // assume true, but if we find something that needs coding, set false
                for (uint32_t j = 0; j < num_cmp - 1; ++j) {
                    if (mcu_row * component_multiple[j] < max_coded_heights[j]) {
                        retval.done = false; // we want to make sure to write out any partial rows,
                        // so set done only when all items in this mcu are really skips
                        // i.e. round down
                    }
                }
            }
            if (i == 0) {
                retval.luma_y = retval.curr_y;
            }
            break;
        } else {
            place_within_scan -= component_multiple[i];
        }
        if (i == 0) {
            dev_assert(false);
            retval.skip = true;
            retval.done = true;
            break;
        }
    }
    return retval;
}

template <class BoolDecoder> class LeptonCodec{
protected:
    struct ThreadState {
        ProbabilityTablesBase model_;
        BoolDecoder bool_decoder_;
        // the splits this thread is concerned with...always 1 more than the number of work items
        std::vector<int> luma_splits_;
        Sirikata::Array1d<bool, (size_t)ColorChannel::NumBlockTypes> is_top_row_;
        Sirikata::Array1d<BlockContext, (size_t)ColorChannel::NumBlockTypes > context_;
        //the last 2 rows of the image for each channel
        Sirikata::Array1d<std::vector<NeighborSummary>, (size_t)ColorChannel::NumBlockTypes> num_nonzeros_;
        uint32_t decode_index_;
        bool is_valid_range_;
        template<class Left, class Middle, class Right, bool should_force_memory_optimization>
        void decode_row(Left & left_model,
                        Middle& middle_model,
                        Right& right_model,
                        int curr_y,
                        BlockBasedImagePerChannel<should_force_memory_optimization>&image_data,
                        int component_size_in_block);

        void decode_rowf(BlockBasedImagePerChannel<false>& image_data,
                         Sirikata::Array1d<uint32_t,
                         (uint32_t)ColorChannel::
                         NumBlockTypes> component_size_in_block,
                         int component,
                         int curr_y);
        void decode_rowt(BlockBasedImagePerChannel<true>& image_data,
                         Sirikata::Array1d<uint32_t,
                         (uint32_t)ColorChannel::
                         NumBlockTypes> component_size_in_block,
                         int component,
                         int curr_y);
        CodingReturnValue vp8_decode_thread(unsigned int thread_id,
                                            UncompressedComponents * const colldata);
    private:
        template<bool force_memory_optimization>
        void decode_row_internal(BlockBasedImagePerChannel<force_memory_optimization>& image_data,
                                 Sirikata::Array1d<uint32_t,
                                 (uint32_t)ColorChannel::
                                 NumBlockTypes> component_size_in_block,
                                 int component,
                                 int curr_y);
        void decode_row_wrapper(BlockBasedImagePerChannel<true>& image_data,
                                Sirikata::Array1d<uint32_t,
                                                  (uint32_t)ColorChannel::
                                                  NumBlockTypes> component_size_in_blocks,
                                int component,
                                int curr_y);

    };
    static uint32_t gcd(uint32_t a, uint32_t b) {
        while(b) {
            uint32_t tmp = a % b;
            a = b;
            b = tmp;
        }
        return a;
    }
public:
    static void worker_thread(ThreadState *, int thread_id, UncompressedComponents * const colldata,
                              int8_t thread_target[Sirikata::MuxReader::MAX_STREAM_ID],
                              GenericWorker*worker,
                              VP8ComponentDecoder_SendToActualThread*data_receiver);

protected:
    bool do_threading_;
    GenericWorker* spin_workers_;
    unsigned int num_registered_workers_;
    Sirikata::Array1d<ThreadState*, MAX_NUM_THREADS> thread_state_;

    void reset_thread_model_state(int thread_id) {
        TimingHarness::timing[thread_id][TimingHarness::TS_MODEL_INIT_BEGIN] = TimingHarness::get_time_us();

        if (!thread_state_[thread_id]) {
            thread_state_[thread_id] = new ThreadState;
        }
        thread_state_[thread_id]->model_.model().set_tables_identity();
        TimingHarness::timing[thread_id][TimingHarness::TS_MODEL_INIT] = TimingHarness::get_time_us();
    }
    void registerWorkers(GenericWorker* workers, unsigned int num_workers) {
        num_registered_workers_ = num_workers;
        spin_workers_ = workers;
    }
    size_t model_worker_memory_used() const {
        size_t retval = 0;
        for (size_t i = 1;i < thread_state_.size(); ++i) {
            if (thread_state_[i]) {
                retval += sizeof(ProbabilityTablesBase);
            }
        }
        return retval;
    }

    size_t model_memory_used() const {
        size_t retval = 0;
        for (size_t i = 0;i < thread_state_.size(); ++i) {
            if (thread_state_[i]) {
                retval += sizeof(ProbabilityTablesBase);
            }
        }
        return retval;
    }
    LeptonCodec(bool do_threading) {
        spin_workers_ = NULL;
        num_registered_workers_ = 0; // need to wait
        do_threading_ = do_threading;
        unsigned int num_threads = 1;
        if (do_threading) {
            num_threads = NUM_THREADS;
        }
        thread_state_.memset(0);
        always_assert(num_threads <= MAX_NUM_THREADS);
        
        for (unsigned int i = 0; i < num_threads; ++i) {

            //thread_state_[i] = new ThreadState;
            //thread_state_[i]->model_.model().set_tables_identity();
            //thread_state_[i]->model_.load_probability_tables();
        }
    }
    ~LeptonCodec() {
        for (unsigned int i = 0; i < thread_state_.size(); ++i) {
            if (thread_state_[i]) {
                delete thread_state_[i];
            }
        }
    }

};

#endif
