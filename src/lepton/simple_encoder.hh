/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "base_coders.hh"
class SimpleComponentEncoder : public BaseEncoder {
    int cur_read_batch[4];
    int target[4];
public:
    SimpleComponentEncoder();
    CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                   IOUtil::FileWriter *,
                                   const ThreadHandoff* selected_splits,
                                   unsigned int num_selected_splits) ;

    virtual void registerWorkers(GenericWorker*, unsigned int num_workers) {}
    ~SimpleComponentEncoder();
    size_t get_decode_model_memory_usage() const {
        return 0;
    }
    size_t get_decode_model_worker_memory_usage() const {
        return 0;
    }
};
