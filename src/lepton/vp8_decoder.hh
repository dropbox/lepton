/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <array>
#include "base_coders.hh"
#include "../../io/MuxReader.hh"
#include "option.hh"
#include "model.hh"
#include "aligned_block.hh"
#include "bool_decoder.hh"

class VP8ComponentDecoder : public BaseDecoder {
    struct ThreadState {
        ProbabilityTablesBase model_;
        // the splits this thread is concerned with...always 1 more than the number of work items
        std::vector<int> luma_splits_;
        Sirikata::Array1d<bool, (size_t)ColorChannel::NumBlockTypes> is_top_row_;
        Sirikata::Array1d<VContext, (size_t)ColorChannel::NumBlockTypes > context_;
        //the last 2 rows of the image for each channel
        Sirikata::Array1d<std::vector<uint8_t>, (size_t)ColorChannel::NumBlockTypes> num_nonzeros_;
        bool is_valid_range_;
        Sirikata::Array1d<BoolDecoder, SIMD_WIDTH> bool_decoder_;
        template<class Left, class Middle, class Right>
        void process_row(Left & left_model,
                         Middle& middle_model,
                         Right& right_model,
                         int block_width,
                         UncompressedComponents * const colldata);
        CodingReturnValue vp8_decoder(UncompressedComponents * const colldata);
        CodingReturnValue vp8_decode_thread(int thread_id, UncompressedComponents * const colldata);
    };
    bool do_threading_;
    Sirikata::Array1d<GenericWorker,
                      (NUM_THREADS - 1)>::Slice spin_workers_;
    Sirikata::DecoderReader *str_in {};
    const std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> > *file_;
    Sirikata::MuxReader mux_reader_;
    std::vector<int> file_luma_splits_;
    std::thread *workers[NUM_THREADS];
    ThreadState *thread_state_[NUM_THREADS];
    VP8ComponentDecoder(const VP8ComponentDecoder&) = delete;
    VP8ComponentDecoder& operator=(const VP8ComponentDecoder&) = delete;
    static void worker_thread(ThreadState *, int thread_id, UncompressedComponents * const colldata);
public:

    VP8ComponentDecoder();
    ~VP8ComponentDecoder();
    void enable_threading(Sirikata::Array1d<GenericWorker,
                                            (NUM_THREADS - 1)>::Slice workers){
        spin_workers_ = workers;
        do_threading_ = true;
    }
    void disable_threading() {
        do_threading_ = false;
    }
    void initialize(Sirikata::DecoderReader *input);
    static void vp8_continuous_decoder( UncompressedComponents * const colldata,
                                        Sirikata::DecoderReader *input);
    //necessary to implement the BaseDecoder interface. Thin wrapper around vp8_decoder
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst);
};
