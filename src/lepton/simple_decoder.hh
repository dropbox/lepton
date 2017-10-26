/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "base_coders.hh"
namespace Sirikata {
class DecoderReader;
}

class SimpleComponentDecoder : public BaseDecoder {
    bool started_scan[4];
    int cur_read_batch[4];
    int target[4];
    Sirikata::DecoderReader * str_in;
    std::vector<ThreadHandoff> thread_handoffs_;
    unsigned int batch_size;
public:
    SimpleComponentDecoder();
    ~SimpleComponentDecoder();
    virtual void initialize(Sirikata::DecoderReader *input,
                            const std::vector<ThreadHandoff>& thread_transition_info);

    CodingReturnValue decode_chunk(UncompressedComponents* colldata);
    virtual void registerWorkers(GenericWorker *, unsigned int num_workers) {}
    unsigned int getNumWorkers() const {
        return 0;
    }
    void reset_all_comm_buffers(){}
    GenericWorker *getWorker(unsigned int i) {
        return NULL;
    }
    std::vector<ThreadHandoff> initialize_baseline_decoder(const UncompressedComponents * const colldata,
                                     Sirikata::Array1d<BlockBasedImagePerChannel<true>,
                                                       MAX_NUM_THREADS>& framebuffer){
        return thread_handoffs_;
    }
    void decode_row(int thread_state_id,
                    BlockBasedImagePerChannel<true>& image_data, // FIXME: set image_data to true
                    Sirikata::Array1d<uint32_t,
                                      (uint32_t)ColorChannel::
                                      NumBlockTypes> component_size_in_blocks,
                    int component,
                    int curr_y);

    virtual void clear_thread_state(int thread_id, int target_thread_state, BlockBasedImagePerChannel<true>& framebuffer) {}
    virtual void flush(){}
    virtual void map_logical_thread_to_physical_thread(int thread_id, int target_thread_state) {}
    size_t get_model_memory_usage() const {
        return 0;
    }
    size_t get_model_worker_memory_usage() const {
        return 0;
    }
};
