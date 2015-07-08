/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <array>
#include "base_coders.hh"

#include "option.hh"
#include "model.hh"
#include "plane.hh"
#include "block.hh"
#include "bool_decoder.hh"

class VP8ComponentDecoder : public BaseDecoder {
    Sirikata::SwitchableDecompressionReader<Sirikata::
                                            SwitchableXZBase> *str_in {};

    ProbabilityTables probability_tables_;
    Optional<BoolDecoder> bool_decoder_;
    std::vector<Plane<Block>> vp8_blocks_;
    std::vector<uint8_t> file_;
    int jpeg_y_[4];
    
public:
    VP8ComponentDecoder();
    void initialize(Sirikata::
                    SwitchableDecompressionReader<Sirikata::SwitchableXZBase> *input);

    CodingReturnValue vp8_decoder( UncompressedComponents * const colldata);
    static void vp8_continuous_decoder( UncompressedComponents * const colldata,
                                        Sirikata::
                                        SwitchableDecompressionReader<Sirikata::
                                                                      SwitchableXZBase> *input);
    //necessary to implement the BaseDecoder interface. Thin wrapper around vp8_decoder
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst);
};
