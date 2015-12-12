#ifndef _BASE_CODERS_
#define _BASE_CODERS_
#include "../vp8/util/nd_array.hh"
#include "../vp8/util/generic_worker.hh"

struct GenericWorker;
enum CodingReturnValue {
    CODING_ERROR,
    CODING_DONE,
    CODING_PARTIAL // run it again
};
class UncompressedComponents;

namespace Sirikata {
class SwitchableXZBase;
class DecoderCompressionWriter;
class DecoderReader;
class DecoderWriter;
template <class T> class SwitchableDecompressionReader;
template <class T> class SwitchableCompressionWriter;
}
namespace IOUtil {
class FileWriter;
}
class BaseDecoder {
 public:
    virtual ~BaseDecoder(){}
    virtual void initialize(Sirikata::DecoderReader *input) = 0;
    virtual void enable_threading(Sirikata::Array1d<GenericWorker,
                                                    (NUM_THREADS
                                                     - 1)>::Slice)=0;
    virtual void disable_threading() = 0;
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst) = 0;
    static CodingReturnValue generic_continuous_decoder(BaseDecoder *d,
                                                        UncompressedComponents* colldata){
        CodingReturnValue retval;
        do {
            retval = d->decode_chunk(colldata);
        }while (retval == CODING_PARTIAL);
        return retval;
    }
};


class BaseEncoder {
 public:
    virtual void enable_threading(Sirikata::Array1d<GenericWorker,
                                                    (NUM_THREADS
                                                     - 1)>::Slice)=0;
    virtual void disable_threading() = 0;
    virtual ~BaseEncoder(){}
    virtual CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                           IOUtil::FileWriter *) = 0;
};

#endif
