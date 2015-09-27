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
                         Sirikata::Array1d<BoolEncoder, SIMD_WIDTH> &bool_encoder);
    void process_row_range(int thread_id,
                           const UncompressedComponents * const colldata,
                           int min_y,
                           int max_y,
                           std::vector<uint8_t> *streams[SIMD_WIDTH]);
    bool do_threading_ = false;
public:
    CodingReturnValue vp8_full_encoder( const UncompressedComponents * const colldata,
                                               Sirikata::DecoderWriter *);
    CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                   Sirikata::DecoderWriter *);
    void enable_threading() {
        do_threading_ = true;
    }
    void disable_threading() {
        do_threading_ = false;
    }

};
