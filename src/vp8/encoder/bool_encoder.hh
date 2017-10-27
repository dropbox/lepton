#ifndef BOOL_ENCODER_HH
#define BOOL_ENCODER_HH

#include <vector>
#include <iostream>

#include <assert.h>
#include "branch.hh"
#include "../../io/MemReadWriter.hh"
#include "JpegArithmeticCoder.hh"
#include "vpx_bool_writer.hh"
#ifdef ENABLE_ANS_EXPERIMENTAL
#include "ans_bool_writer.hh"
#endif
/* Routines taken from ISO/IEC 10918-1 : 1993(E) */

class JpegBoolEncoder : public Sirikata::MemReadWriter {
    Sirikata::ArithmeticCoder jpeg_coder_;
public:
    JpegBoolEncoder(const Sirikata::JpegAllocator<unsigned char>&alloc=Sirikata::JpegAllocator<unsigned char>())
        : MemReadWriter(alloc), jpeg_coder_(true) {
    }
    void put( const bool value, Branch & branch ) {
        jpeg_coder_.arith_encode(this, &branch.probability_, value);
    }
    void finish(std::vector< uint8_t, Sirikata::JpegAllocator<unsigned char> >& retval) {
        jpeg_coder_.finish_encode(this);
        
        return retval.swap(buffer());
    }
};

#ifdef JPEG_ENCODER
//class BoolEncoder : public JpegBoolEncoder{};
#else
//class BoolEncoder : public VPXBoolWriter{};
#endif
#endif /* BOOL_ENCODER_HH */
