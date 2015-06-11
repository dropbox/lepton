/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "base_coders.hh"
class iostream;

class SimpleComponentDecoder : public BaseDecoder {
    bool started_scan[4];
    int cmp;
    int cur_read_batch[4];
    iostream *str_in;
public:
    SimpleComponentDecoder();
    void initialize(iostream *str_in);
    ~SimpleComponentDecoder();
    CodingReturnValue decode_chunk(UncompressedComponents* colldata);
    static void simple_continuous_decoder(UncompressedComponents* colldata, iostream *str_in);
};
