/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <array>
#include "base_coders.hh"
#include "lepton_codec.hh"
#include "../../io/MuxReader.hh"
#include "aligned_block.hh"
#include "bool_decoder.hh"

class VP8ComponentDecoder : protected LeptonCodec, public BaseDecoder {
    Sirikata::DecoderReader *str_in {};
    //const std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> > *file_;
    Sirikata::MuxReader mux_reader_;
    std::vector<int> file_luma_splits_;
    VP8ComponentDecoder(const VP8ComponentDecoder&) = delete;
    VP8ComponentDecoder& operator=(const VP8ComponentDecoder&) = delete;
    static void worker_thread(ThreadState *, int thread_id, UncompressedComponents * const colldata);
public:

    VP8ComponentDecoder();
    ~VP8ComponentDecoder();
    void initialize(Sirikata::DecoderReader *input);
    static void vp8_continuous_decoder( UncompressedComponents * const colldata,
                                        Sirikata::DecoderReader *input);
    //necessary to implement the BaseDecoder interface. Thin wrapper around vp8_decoder
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst);
    void enable_threading(Sirikata::Array1d<GenericWorker,
                                            (NUM_THREADS - 1)>::Slice workers){
        LeptonCodec::enable_threading(workers);
    }
    void disable_threading() {
        LeptonCodec::disable_threading();
    }

};
