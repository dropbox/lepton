/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "base_coders.hh"

class VP8ComponentDecoder : public BaseDecoder {
    iostream * str_in;
    bool started_scan[4];
    int cmp;
    int cur_read_batch[4];
public:
    VP8ComponentDecoder();
    void initialize(iostream *input);
    CodingReturnValue vp8_decoder(UncompressedComponents* colldata, iostream *str_in);
    static void vp8_continuous_decoder(UncompressedComponents* colldata, iostream *str_in);
    //necessary to implement the BaseDecoder interface. Thin wrapper around vp8_decoder
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst);
};
