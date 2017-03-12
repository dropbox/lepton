/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef _VP8_COMPONENT_ENCODER_HH_
#define _VP8_COMPONENT_ENCODER_HH_
#include "base_coders.hh"
#include "lepton_codec.hh"
#include "model.hh"
#include "../io/MuxReader.hh"
class BoolEncoder;
class VP8ComponentEncoder : protected LeptonCodec, public BaseEncoder {
    template<class Left, class Middle, class Right>
    static void process_row(ProbabilityTablesBase&pt,
                            Left & left_model,
                         Middle& middle_model,
                         Right& right_model,
                         int curr_y,
                         const UncompressedComponents * const colldata,
                         Sirikata::Array1d<ConstBlockContext,
                                           (uint32_t)ColorChannel::NumBlockTypes> &context,
                         BoolEncoder &bool_encoder);
    void process_row_range(unsigned int thread_id,
                           const UncompressedComponents * const colldata,
                           int min_y,
                           int max_y,
                           Sirikata::MuxReader::ResizableByteBuffer *stream,
                           BoolEncoder *bool_encoder,
                           Sirikata::Array1d<std::vector<NeighborSummary>,
                                             (uint32_t)ColorChannel::NumBlockTypes> *num_nonzeros);
public:
    VP8ComponentEncoder(bool do_threading);
    void registerWorkers(GenericWorker * workers, unsigned int num_workers) {
        this->LeptonCodec::registerWorkers(workers, num_workers);
    }

    CodingReturnValue vp8_full_encoder( const UncompressedComponents * const colldata,
                                        IOUtil::FileWriter *,
                                        const ThreadHandoff * selected_splits,
                                        unsigned int num_selected_splits);

    CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                   IOUtil::FileWriter *,
                                   const ThreadHandoff * selected_splits,
                                        unsigned int num_selected_splits);
    size_t get_decode_model_memory_usage() const {
        return model_memory_used();
    }
    size_t get_decode_model_worker_memory_usage() const {
        return model_worker_memory_used();
    }

};
#endif
