/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <array>
#include "base_coders.hh"
#include "../../io/MuxReader.hh"
#include "option.hh"
#include "model.hh"
#include "aligned_block.hh"
#include "bool_decoder.hh"

class VP8ComponentDecoder : public BaseDecoder {

    Sirikata::Array1d<VContext, (size_t)ColorChannel::NumBlockTypes > context_;
    Sirikata::SwitchableDecompressionReader<Sirikata::
                                            SwitchableXZBase> *str_in {};

    Optional<BoolDecoder> bool_decoder_;
    const std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> > *file_;
    template<class Left, class Middle, class Right>
    void process_row(Left & left_model,
                      Middle& middle_model,
                      Right& right_model,
                      int block_width,
                      UncompressedComponents * const colldata);
    Sirikata::MuxReader mux_reader_;
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
