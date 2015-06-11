/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "base_coders.hh"
class SimpleComponentEncoder : BaseEncoder {
public:
    CodingReturnValue encode_chunk(const UncompressedComponents *input, iostream *output);
    ~SimpleComponentEncoder();
};
