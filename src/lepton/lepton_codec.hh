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
        CodingReturnValue vp8_decoder(const UncompressedComponents * const colldata);
        CodingReturnValue vp8_decode_thread(int thread_id, UncompressedComponents * const colldata);
    };
    bool do_threading_;
    Sirikata::Array1d<GenericWorker,
                      (NUM_THREADS - 1)>::Slice spin_workers_;
    std::thread *workers[NUM_THREADS];
    ThreadState *thread_state_[NUM_THREADS];

    void reset_thread_model_state(int thread_id) {
        (&thread_state_[thread_id]->model_)->~ProbabilityTablesBase();
        new (&thread_state_[thread_id]->model_) ProbabilityTablesBase();
    }
    void registerWorkers(Sirikata::Array1d<GenericWorker, (NUM_THREADS - 1)>::Slice workers) {
        spin_workers_ = workers;
    }
    LeptonCodec(bool do_threading) {
        do_threading_ = do_threading;
        int num_threads = 1;
        if (do_threading) {
            num_threads = NUM_THREADS;
        }
        for (int i = 0; i < num_threads; ++i) {
            thread_state_[i] = new ThreadState;
            thread_state_[i]->model_.load_probability_tables();
        }
    }
    ~LeptonCodec() {
        for (int i = 0; i < NUM_THREADS; ++i) {
            if (thread_state_[i]) {
                delete thread_state_[i];
            }
        }
    }

};
#endif
