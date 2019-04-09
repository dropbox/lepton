/*  Sirikata Jpeg Texture Transfer -- Texture Transfer management system
 *  JpegArithmeticCoder.cc
 *
 *  Copyright (c) 2015, The Sirikata Authors
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file was adapted from libjpeg-turbo

/*
 * j[cd]arith.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Developed 1997-2009 by Guido Vollbeding.
 * It was modified by The libjpeg-turbo Project to include only code relevant
 * to libjpeg-turbo.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains portable arithmetic entropy encoding routines for JPEG
 * (implementing the ISO/IEC IS 10918-1 and CCITT Recommendation ITU-T T.81).
 *
 * Both sequential and progressive modes are supported in this single module.
 *
 * Suspension is not currently supported in this module.
 */
#ifndef JPEG_ARITHMETIC_CODER_HH_
#define JPEG_ARITHMETIC_CODER_HH_

#include "../../io/Reader.hh"

namespace Sirikata {
class DecoderWriter;
class DecoderReader;
class SIRIKATA_EXPORT ArithmeticCoder {
    int32_t c; // C register, base of coding interval, layout as in sec D.1.3
    int32_t a; // A register, normalized size of coding interval
    int ct; // bit shift counter, determines # bits left in bit buffer part of C
    // init: ct = -16  run: ct = 0...7  error: ct = -1
    // for output, determines whenb yte will be written

    signed int buffer;
    int sc;
    int zc;


    //deprecated state for reader
    int unread_marker;
public:
    ArithmeticCoder(bool encoding) {
        zc = 0;
        sc = 0;
        buffer = -1;
        ct = encoding ? 11 : -16;
        c = 0;
        a = encoding ? 0x10000L : 0;
        unread_marker = 0;
    }
    void arith_encode(DecoderWriter *output, unsigned char *state, bool value);
    void finish_encode(DecoderWriter *output);
    bool arith_decode(DecoderReader *input, unsigned char *state);

};
class SIRIKATA_EXPORT ArithmeticWriter : ArithmeticCoder {
    DecoderWriter *mBase;
public:
    ArithmeticWriter(DecoderWriter *writer) : ArithmeticCoder(true) {
        mBase = writer;
    }
    void WriteBit(unsigned char *state, bool value) {
        arith_encode(mBase, state, value);
    }
    void Finish() {
        finish_encode(mBase);
    }
};

class SIRIKATA_EXPORT ArithmeticReader : ArithmeticCoder{
    DecoderReader *mBase;
public:
    ArithmeticReader(DecoderReader *reader) : ArithmeticCoder(false) {
        mBase = reader;
    }
    bool ReadBit(unsigned char *state) {
        return arith_decode(mBase, state);
    }
};
}
#endif

