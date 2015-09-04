/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "base_coders.hh"

class VP8ComponentEncoder : public BaseEncoder {
        template<class Left, class Middle, class Right>
        static void process_row(Left & left_model,
                         Middle& middle_model,
                         Right& right_model,
                         int block_width,
                         const UncompressedComponents * const colldata,
                         Sirikata::Array1d<VContext,
                                           (uint32_t)ColorChannel::NumBlockTypes> &context,
                         Sirikata::Array1d<BlockBasedImage,
                                           (uint32_t)ColorChannel::NumBlockTypes> &vp8_blocks,
                         BoolEncoder &bool_encoder);
public:
    static CodingReturnValue vp8_full_encoder( const UncompressedComponents * const colldata,
                                               Sirikata::
                                               SwitchableCompressionWriter<Sirikata::
                                                                           DecoderCompressionWriter> *);
    CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                   Sirikata::
                                   SwitchableCompressionWriter<Sirikata::
                                                               DecoderCompressionWriter> *);
};
