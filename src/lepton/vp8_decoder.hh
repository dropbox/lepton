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
    std::vector<ThreadHandoff> thread_handoff_;
    Sirikata::Array1d<std::pair <Sirikata::MuxReader::ResizableByteBuffer::const_iterator,
                                 Sirikata::MuxReader::ResizableByteBuffer::const_iterator>,
                      Sirikata::MuxReader::MAX_STREAM_ID> streams_;

    VP8ComponentDecoder(const VP8ComponentDecoder&) = delete;
    VP8ComponentDecoder& operator=(const VP8ComponentDecoder&) = delete;
    static void worker_thread(ThreadState *, int thread_id, UncompressedComponents * const colldata);
    template <bool force_memory_optimized>
    void initialize_thread_id(int thread_id, int target_thread_state,
                              BlockBasedImagePerChannel<force_memory_optimized>& framebuffer);

    int virtual_thread_id_;
public:
    VP8ComponentDecoder(bool do_threading);
    // reads the threading information and uses mux_reader_ to create the streams_ 
    // returns the bound of each threads' max_luma (non inclusive) responsibility in the file
    template <bool force_memory_optimized>
    std::vector<ThreadHandoff> initialize_decoder_state(
        const UncompressedComponents * const colldata,
        // quantization_tables
        Sirikata::Array1d<BlockBasedImagePerChannel<force_memory_optimized>,
                          NUM_THREADS>& framebuffer); // framebuffer
    virtual std::vector<ThreadHandoff> initialize_baseline_decoder(const UncompressedComponents * const colldata,
                                             Sirikata::Array1d<BlockBasedImagePerChannel<true>,
                                                               NUM_THREADS>& framebuffer);
    void registerWorkers(Sirikata::Array1d<GenericWorker, (NUM_THREADS - 1)>::Slice workers) {
        this->VP8ComponentEncoder::registerWorkers(workers);
    }
    GenericWorker *getWorker(int i) {
        return &spin_workers_.at(i);
    }

    ~VP8ComponentDecoder();
    void initialize(Sirikata::DecoderReader *input,
                    const std::vector<ThreadHandoff>& thread_transition_info);
    //necessary to implement the BaseDecoder interface. Thin wrapper around vp8_decoder
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst);
    virtual void decode_row(int target_thread_id,
                            BlockBasedImagePerChannel<true>& image_data, // FIXME: set image_data to true
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::
                                              NumBlockTypes> component_size_in_blocks,
                            int component,
                            int curr_y);
    virtual void clear_thread_state(int thread_id, int target_thread_state, BlockBasedImagePerChannel<true>& framebuffer);
};
