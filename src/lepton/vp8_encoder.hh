/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef VP8_COMPONENT_ENCODER_HH_
#define VP8_COMPONENT_ENCODER_HH_
#include "base_coders.hh"
#include "lepton_codec.hh"
#include "model.hh"
#include "../io/MuxReader.hh"
#include "lepton_codec.hh"

template<class BoolDecoder> class VP8ComponentEncoder : protected LeptonCodec<BoolDecoder>, public BaseEncoder {
    template<class Left, class Middle, class Right, class BoolEncoder>
    static void process_row(ProbabilityTablesBase&pt,
                            Left & left_model,
                         Middle& middle_model,
                         Right& right_model,
                         int curr_y,
                         const UncompressedComponents * const colldata,
                         Sirikata::Array1d<ConstBlockContext,
                                           (uint32_t)ColorChannel::NumBlockTypes> &context,
                         BoolEncoder &bool_encoder);
    template <class BoolEncoder> void process_row_range(unsigned int thread_id,
                           const UncompressedComponents * const colldata,
                           int min_y,
                           int max_y,
                           Sirikata::MuxReader::ResizableByteBuffer *stream,
                           BoolEncoder *bool_encoder,
                           Sirikata::Array1d<std::vector<NeighborSummary>,
                                             (uint32_t)ColorChannel::NumBlockTypes> *num_nonzeros);
    bool mUseAnsEncoder;
    template<class BoolEncoder> void threaded_encode_inner(const UncompressedComponents * const colldata,
                                                           IOUtil::FileWriter *str_out,
                                                           const ThreadHandoff * selected_splits,
                                                           unsigned int num_selected_splits,
                                                           BoolEncoder bool_encoder[MAX_NUM_THREADS],
                                                           Sirikata::MuxReader::ResizableByteBuffer stream[Sirikata::MuxReader::MAX_STREAM_ID]);

public:
    VP8ComponentEncoder(bool do_threading, bool use_ans_encoder);
    void registerWorkers(GenericWorker * workers, unsigned int num_workers) {
        this->LeptonCodec<BoolDecoder>::registerWorkers(workers, num_workers);
    }
    bool do_threading() const {
        return this->do_threading_;
    }
    CodingReturnValue vp8_full_encoder( const UncompressedComponents * const colldata,
                                        IOUtil::FileWriter *,
                                        const ThreadHandoff * selected_splits,
                                        unsigned int num_selected_splits,
                                        bool use_ans_encoder);

    CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                   IOUtil::FileWriter *,
                                   const ThreadHandoff * selected_splits,
                                   unsigned int num_selected_splits);
    size_t get_decode_model_memory_usage() const {
        return this->model_memory_used();
    }
    size_t get_decode_model_worker_memory_usage() const {
        return this->model_worker_memory_used();
    }

};
#endif
