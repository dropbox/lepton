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
    virtual void enable_threading(Sirikata::Array1d<GenericWorker,
                                                    (NUM_THREADS
                                                     - 1)>::Slice){}
    void disable_threading() {}

    CodingReturnValue decode_chunk(UncompressedComponents* colldata);
    static void simple_continuous_decoder(UncompressedComponents* colldata,
                                          Sirikata::DecoderReader *);
};
