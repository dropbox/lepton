/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <array>
#include "base_coders.hh"
#include "lepton_codec.hh"
#include "../../io/MuxReader.hh"
#include "aligned_block.hh"
#include "bool_decoder.hh"
#include "vp8_encoder.hh"

class VP8ComponentDecoder : public BaseDecoder, public VP8ComponentEncoder {
    Sirikata::DecoderReader *str_in {};
    //const std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> > *file_;
    Sirikata::MuxReader mux_reader_;
    std::vector<int> file_luma_splits_;
    Sirikata::Array1d<std::pair <Sirikata::MuxReader::ResizableByteBuffer::const_iterator,
                                 Sirikata::MuxReader::ResizableByteBuffer::const_iterator>,
                      Sirikata::MuxReader::MAX_STREAM_ID> streams_;

    VP8ComponentDecoder(const VP8ComponentDecoder&) = delete;
    VP8ComponentDecoder& operator=(const VP8ComponentDecoder&) = delete;
    static void worker_thread(ThreadState *, int thread_id, UncompressedComponents * const colldata);
    void initialize_thread_id(int thread_id, int target_thread_state,
                              UncompressedComponents * const colldata);
    int virtual_thread_id_;
public:
    VP8ComponentDecoder(Sirikata::Array1d<GenericWorker, (NUM_THREADS - 1)>::Slice workers);
    VP8ComponentDecoder();
    ~VP8ComponentDecoder();
    void initialize(Sirikata::DecoderReader *input);
    //necessary to implement the BaseDecoder interface. Thin wrapper around vp8_decoder
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst);

};
