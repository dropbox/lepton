/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "base_coders.hh"

class VP8ComponentEncoder : public BaseEncoder {
public:
    static void vp8_full_encoder( const UncompressedComponents * const colldata,
                                  iostream * const str_out );
    CodingReturnValue encode_chunk(const UncompressedComponents *input, iostream *output);
};
