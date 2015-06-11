#ifndef _BASE_CODERS_
#define _BASE_CODERS_

enum CodingReturnValue {
    CODING_ERROR,
    CODING_DONE,
    CODING_PARTIAL // run it again
};

class UncompressedComponents;
class iostream;
class BaseDecoder {
 public:
    virtual ~BaseDecoder(){}
    virtual void initialize(iostream *input) = 0;
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
    virtual CodingReturnValue encode_chunk(const UncompressedComponents *input, iostream *output) = 0;
};

#endif
