#ifndef _BASE_CODERS_
#define _BASE_CODERS_

enum CodingReturnValue {
    CODING_ERROR,
    CODING_DONE,
    CODING_PARTIAL // run it again
};
template<bool, typename> class BaseUncompressedComponents;
typedef BaseUncompressedComponents<false, int> UncompressedComponents;

namespace Sirikata {
class SwitchableXZBase;
class DecoderCompressionWriter;
class DecoderReader;
class DecoderWriter;
template <class T> class SwitchableDecompressionReader;
template <class T> class SwitchableCompressionWriter;
}

class BaseDecoder {
 public:
    virtual ~BaseDecoder(){}
    virtual void initialize(Sirikata::DecoderReader *input) = 0;
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
    virtual ~BaseEncoder(){}
    virtual CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                           Sirikata::DecoderWriter *) = 0;
};

#endif
