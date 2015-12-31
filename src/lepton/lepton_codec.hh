#ifndef _LEPTON_CODEC_HH_
#define _LEPTON_CODEC_HH_
#include "model.hh"
#include "bool_decoder.hh"
#include "base_coders.hh"
class UncompressedComponents;

class LeptonCodec {
protected:
    struct ThreadState {
        ProbabilityTablesBase model_;
        // the splits this thread is concerned with...always 1 more than the number of work items
        std::vector<int> luma_splits_;
        Sirikata::Array1d<bool, (size_t)ColorChannel::NumBlockTypes> is_top_row_;
        Sirikata::Array1d<VContext, (size_t)ColorChannel::NumBlockTypes > context_;
        //the last 2 rows of the image for each channel
        Sirikata::Array1d<std::vector<NeighborSummary>, (size_t)ColorChannel::NumBlockTypes> num_nonzeros_;
        bool is_valid_range_;
        BoolDecoder bool_decoder_;
        template<class Left, class Middle, class Right>
        void decode_row(Left & left_model,
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
    std::thread *workers[NUM_THREADS];
    ThreadState *thread_state_[NUM_THREADS];
    void enable_threading(Sirikata::Array1d<GenericWorker,
                                            (NUM_THREADS - 1)>::Slice workers){
        spin_workers_ = workers;
        do_threading_ = true;
    }
    void disable_threading() {
        do_threading_ = false;
    }

};
#endif
