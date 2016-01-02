/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef _VP8_COMPONENT_ENCODER_HH_
#define _VP8_COMPONENT_ENCODER_HH_
#include "base_coders.hh"
#include "lepton_codec.hh"
#include "model.hh"
class BoolEncoder;
class VP8ComponentEncoder : protected LeptonCodec, public BaseEncoder {
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
                           std::vector<uint8_t> *stream,
                           BoolEncoder *bool_encoder,
                           Sirikata::Array1d<std::vector<NeighborSummary>,
                                             (uint32_t)ColorChannel::NumBlockTypes> *num_nonzeros);
protected:
    void reset_thread_model_state(int thread_id);
public:
    VP8ComponentEncoder(Sirikata::Array1d<GenericWorker, (NUM_THREADS - 1)>::Slice workers);
    VP8ComponentEncoder();
    CodingReturnValue vp8_full_encoder( const UncompressedComponents * const colldata,
                                        IOUtil::FileWriter *);
    CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                   IOUtil::FileWriter *);
};
#endif
