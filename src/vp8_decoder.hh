/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "base_coders.hh"

class VP8ComponentDecoder : public BaseDecoder {
    iostream * str_in {};

    unsigned char current_component_ {};
    std::array<bool, 4> started_scan_ {{}};
    std::array<int, 4> cur_read_batch_ {{}};

    bool started_scan() const;
    bool & mutable_started_scan();

    int cur_read_batch() const;
    int & mutable_cur_read_batch();

public:
    VP8ComponentDecoder() {}
    void initialize(iostream *input);
    CodingReturnValue vp8_decoder( UncompressedComponents * const colldata,
                                   iostream * const str_in );
    static void vp8_continuous_decoder( UncompressedComponents * const colldata,
                                        iostream * const str_in);
    //necessary to implement the BaseDecoder interface. Thin wrapper around vp8_decoder
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst);
};
