/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "base_coders.hh"
namespace Sirikata {
class DecoderReader;
}

class SimpleComponentDecoder : public BaseDecoder {
    bool started_scan[4];
    int cur_read_batch[4];
    int target[4];
    Sirikata::DecoderReader * str_in;
    unsigned int batch_size;
public:
    SimpleComponentDecoder();
    ~SimpleComponentDecoder();
    virtual void initialize(Sirikata::DecoderReader *input);

    CodingReturnValue decode_chunk(UncompressedComponents* colldata);
    virtual void registerWorkers(Sirikata::Array1d<GenericWorker, (NUM_THREADS - 1)>::Slice workers) {}
    bool initialize_baseline_decoder(const UncompressedComponents * const colldata,
                                     Sirikata::Array1d<BlockBasedImagePerChannel<false>,
                                                       NUM_THREADS>& framebuffer){}
    void decode_row(int thread_state_id,
                    BlockBasedImagePerChannel<false>& image_data, // FIXME: set image_data to true
                    Sirikata::Array1d<uint32_t,
                                      (uint32_t)ColorChannel::
                                      NumBlockTypes> component_size_in_blocks,
                    int component,
                    int curr_y);

};
