/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "base_coders.hh"
class SimpleComponentEncoder : public BaseEncoder {
    int cur_read_batch[4];
    int target[4];
public:
    SimpleComponentEncoder();
    CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                   Sirikata::
                                   SwitchableCompressionWriter<Sirikata::
                                                               DecoderCompressionWriter> *);

    ~SimpleComponentEncoder();
};
