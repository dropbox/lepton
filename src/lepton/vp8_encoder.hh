/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "base_coders.hh"
#include "model.hh"
class VP8ComponentEncoder : public BaseEncoder {
    Sirikata::Array1d<ProbabilityTablesBase*, NUM_THREADS> model_;
    template<class Left, class Middle, class Right>
    static void process_row(ProbabilityTablesBase&pt,
                            Left & left_model,
                         Middle& middle_model,
                         Right& right_model,
                         int block_width,
                         const UncompressedComponents * const colldata,
                         Sirikata::Array1d<KVContext,
                                           (uint32_t)ColorChannel::NumBlockTypes> &context,
                         BoolEncoder &bool_encoder);
    void process_row_range(int thread_id,
                           const UncompressedComponents * const colldata,
                           int min_y,
                           int max_y,
                           std::vector<uint8_t> *stream);
    bool do_threading_ = false;
    Sirikata::Array1d<GenericWorker, (NUM_THREADS - 1)>::Slice spin_workers_;
public:
    VP8ComponentEncoder();
    CodingReturnValue vp8_full_encoder( const UncompressedComponents * const colldata,
                                        IOUtil::FileWriter *);
    CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                   IOUtil::FileWriter *);
    virtual void enable_threading(Sirikata::Array1d<GenericWorker,
                                                    (NUM_THREADS
                                                     - 1)>::Slice workers) {
        do_threading_ = true;
        spin_workers_ = workers;
    }
    void disable_threading() {
        do_threading_ = false;
    }

};
