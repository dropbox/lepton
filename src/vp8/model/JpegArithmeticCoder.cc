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
Most of libjpeg-turbo inherits the non-restrictive, BSD-style license used by
libjpeg (see README.)  The TurboJPEG wrapper (both C and Java versions) and
associated test programs bear a similar license, which is reproduced below:

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
- Neither the name of the libjpeg-turbo Project nor the names of its
  contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/
#include <assert.h>
#include "JpegArithmeticCoder.hh"



/* The following #define specifies the packing of the four components
 * into the compact INT32 representation.
 * Note that this formula must match the actual arithmetic encoder
 * and decoder implementation.  The implementation has to be changed
 * if this formula is changed.
 * The current organization is leaned on Markus Kuhn's JBIG
 * implementation (jbig_tab.c).
 */




#define V(i,a,b,c,d) (((int32_t)a << 16) | ((int32_t)c << 8) | ((int32_t)d << 7) | b)

const int32_t jpeg_aritab[256] = {
/*
 * Index, Qe_Value, Next_Index_LPS, Next_Index_MPS, Switch_MPS
 */
  V(   0, 0x5a1d,   1,   1, 1 ),
  V(   1, 0x2586,  14,   2, 0 ),
  V(   2, 0x1114,  16,   3, 0 ),
  V(   3, 0x080b,  18,   4, 0 ),
  V(   4, 0x03d8,  20,   5, 0 ),
  V(   5, 0x01da,  23,   6, 0 ),
  V(   6, 0x00e5,  25,   7, 0 ),
  V(   7, 0x006f,  28,   8, 0 ),
  V(   8, 0x0036,  30,   9, 0 ),
  V(   9, 0x001a,  33,  10, 0 ),
  V(  10, 0x000d,  35,  11, 0 ),
  V(  11, 0x0006,   9,  12, 0 ),
  V(  12, 0x0003,  10,  13, 0 ),
  V(  13, 0x0001,  12,  13, 0 ),
  V(  14, 0x5a7f,  15,  15, 1 ),
  V(  15, 0x3f25,  36,  16, 0 ),
  V(  16, 0x2cf2,  38,  17, 0 ),
  V(  17, 0x207c,  39,  18, 0 ),
  V(  18, 0x17b9,  40,  19, 0 ),
  V(  19, 0x1182,  42,  20, 0 ),
  V(  20, 0x0cef,  43,  21, 0 ),
  V(  21, 0x09a1,  45,  22, 0 ),
  V(  22, 0x072f,  46,  23, 0 ),
  V(  23, 0x055c,  48,  24, 0 ),
  V(  24, 0x0406,  49,  25, 0 ),
  V(  25, 0x0303,  51,  26, 0 ),
  V(  26, 0x0240,  52,  27, 0 ),
  V(  27, 0x01b1,  54,  28, 0 ),
  V(  28, 0x0144,  56,  29, 0 ),
  V(  29, 0x00f5,  57,  30, 0 ),
  V(  30, 0x00b7,  59,  31, 0 ),
  V(  31, 0x008a,  60,  32, 0 ),
  V(  32, 0x0068,  62,  33, 0 ),
  V(  33, 0x004e,  63,  34, 0 ),
  V(  34, 0x003b,  32,  35, 0 ),
  V(  35, 0x002c,  33,   9, 0 ),
  V(  36, 0x5ae1,  37,  37, 1 ),
  V(  37, 0x484c,  64,  38, 0 ),
  V(  38, 0x3a0d,  65,  39, 0 ),
  V(  39, 0x2ef1,  67,  40, 0 ),
  V(  40, 0x261f,  68,  41, 0 ),
  V(  41, 0x1f33,  69,  42, 0 ),
  V(  42, 0x19a8,  70,  43, 0 ),
  V(  43, 0x1518,  72,  44, 0 ),
  V(  44, 0x1177,  73,  45, 0 ),
  V(  45, 0x0e74,  74,  46, 0 ),
  V(  46, 0x0bfb,  75,  47, 0 ),
  V(  47, 0x09f8,  77,  48, 0 ),
  V(  48, 0x0861,  78,  49, 0 ),
  V(  49, 0x0706,  79,  50, 0 ),
  V(  50, 0x05cd,  48,  51, 0 ),
  V(  51, 0x04de,  50,  52, 0 ),
  V(  52, 0x040f,  50,  53, 0 ),
  V(  53, 0x0363,  51,  54, 0 ),
  V(  54, 0x02d4,  52,  55, 0 ),
  V(  55, 0x025c,  53,  56, 0 ),
  V(  56, 0x01f8,  54,  57, 0 ),
  V(  57, 0x01a4,  55,  58, 0 ),
  V(  58, 0x0160,  56,  59, 0 ),
  V(  59, 0x0125,  57,  60, 0 ),
  V(  60, 0x00f6,  58,  61, 0 ),
  V(  61, 0x00cb,  59,  62, 0 ),
  V(  62, 0x00ab,  61,  63, 0 ),
  V(  63, 0x008f,  61,  32, 0 ),
  V(  64, 0x5b12,  65,  65, 1 ),
  V(  65, 0x4d04,  80,  66, 0 ),
  V(  66, 0x412c,  81,  67, 0 ),
  V(  67, 0x37d8,  82,  68, 0 ),
  V(  68, 0x2fe8,  83,  69, 0 ),
  V(  69, 0x293c,  84,  70, 0 ),
  V(  70, 0x2379,  86,  71, 0 ),
  V(  71, 0x1edf,  87,  72, 0 ),
  V(  72, 0x1aa9,  87,  73, 0 ),
  V(  73, 0x174e,  72,  74, 0 ),
  V(  74, 0x1424,  72,  75, 0 ),
  V(  75, 0x119c,  74,  76, 0 ),
  V(  76, 0x0f6b,  74,  77, 0 ),
  V(  77, 0x0d51,  75,  78, 0 ),
  V(  78, 0x0bb6,  77,  79, 0 ),
  V(  79, 0x0a40,  77,  48, 0 ),
  V(  80, 0x5832,  80,  81, 1 ),
  V(  81, 0x4d1c,  88,  82, 0 ),
  V(  82, 0x438e,  89,  83, 0 ),
  V(  83, 0x3bdd,  90,  84, 0 ),
  V(  84, 0x34ee,  91,  85, 0 ),
  V(  85, 0x2eae,  92,  86, 0 ),
  V(  86, 0x299a,  93,  87, 0 ),
  V(  87, 0x2516,  86,  71, 0 ),
  V(  88, 0x5570,  88,  89, 1 ),
  V(  89, 0x4ca9,  95,  90, 0 ),
  V(  90, 0x44d9,  96,  91, 0 ),
  V(  91, 0x3e22,  97,  92, 0 ),
  V(  92, 0x3824,  99,  93, 0 ),
  V(  93, 0x32b4,  99,  94, 0 ),
  V(  94, 0x2e17,  93,  86, 0 ),
  V(  95, 0x56a8,  95,  96, 1 ),
  V(  96, 0x4f46, 101,  97, 0 ),
  V(  97, 0x47e5, 102,  98, 0 ),
  V(  98, 0x41cf, 103,  99, 0 ),
  V(  99, 0x3c3d, 104, 100, 0 ),
  V( 100, 0x375e,  99,  93, 0 ),
  V( 101, 0x5231, 105, 102, 0 ),
  V( 102, 0x4c0f, 106, 103, 0 ),
  V( 103, 0x4639, 107, 104, 0 ),
  V( 104, 0x415e, 103,  99, 0 ),
  V( 105, 0x5627, 105, 106, 1 ),
  V( 106, 0x50e7, 108, 107, 0 ),
  V( 107, 0x4b85, 109, 103, 0 ),
  V( 108, 0x5597, 110, 109, 0 ),
  V( 109, 0x504f, 111, 107, 0 ),
  V( 110, 0x5a10, 110, 111, 1 ),
  V( 111, 0x5522, 112, 109, 0 ),
  V( 112, 0x59eb, 112, 111, 1 ),
/*
 * This last entry is used for fixed probability estimate of 0.5
 * as recommended in Section 10.3 Table 5 of ITU-T Rec. T.851.
 */
  V( 113, 0x5a1d, 113, 113, 0 ),

  V( 114, 0x5b12,  65,  65, 1 ),
  V( 115, 0x5b12,  65,  65, 1 ),
  V( 116, 0x5b12,  65,  65, 1 ),
  V( 117, 0x5b12,  65,  65, 1 ),
  V( 118, 0x5b12,  65,  65, 1 ),
  V( 119, 0x5b12,  65,  65, 1 ),
  V( 120, 0x5b12,  65,  65, 1 ),
  V( 121, 0x5b12,  65,  65, 1 ),
  V( 122, 0x5b12,  65,  65, 1 ),
  V( 123, 0x5b12,  65,  65, 1 ),
  V( 124, 0x5b12,  65,  65, 1 ),
  V( 125, 0x5b12,  65,  65, 1 ),
  V( 126, 0x5b12,  65,  65, 1 ),
  V( 127, 0x5b12,  65,  65, 1 ),
  V( 128, 0x5b12,  65,  65, 1 ),
  V( 129, 0x5b12,  65,  65, 1 ),
  V( 130, 0x5b12,  65,  65, 1 ),
  V( 131, 0x5b12,  65,  65, 1 ),
  V( 132, 0x5b12,  65,  65, 1 ),
  V( 133, 0x5b12,  65,  65, 1 ),
  V( 134, 0x5b12,  65,  65, 1 ),
  V( 135, 0x5b12,  65,  65, 1 ),
  V( 136, 0x5b12,  65,  65, 1 ),
  V( 137, 0x5b12,  65,  65, 1 ),
  V( 138, 0x5b12,  65,  65, 1 ),
  V( 139, 0x5b12,  65,  65, 1 ),
  V( 140, 0x5b12,  65,  65, 1 ),
  V( 141, 0x5b12,  65,  65, 1 ),
  V( 142, 0x5b12,  65,  65, 1 ),
  V( 143, 0x5b12,  65,  65, 1 ),
  V( 144, 0x5b12,  65,  65, 1 ),
  V( 145, 0x5b12,  65,  65, 1 ),
  V( 146, 0x5b12,  65,  65, 1 ),
  V( 147, 0x5b12,  65,  65, 1 ),
  V( 148, 0x5b12,  65,  65, 1 ),
  V( 149, 0x5b12,  65,  65, 1 ),
  V( 150, 0x5b12,  65,  65, 1 ),
  V( 151, 0x5b12,  65,  65, 1 ),
  V( 152, 0x5b12,  65,  65, 1 ),
  V( 153, 0x5b12,  65,  65, 1 ),
  V( 154, 0x5b12,  65,  65, 1 ),
  V( 155, 0x5b12,  65,  65, 1 ),
  V( 156, 0x5b12,  65,  65, 1 ),
  V( 157, 0x5b12,  65,  65, 1 ),
  V( 158, 0x5b12,  65,  65, 1 ),
  V( 159, 0x5b12,  65,  65, 1 ),
  V( 160, 0x5b12,  65,  65, 1 ),

  V( 161, 0x5b12,  65,  65, 1 ),
  V( 162, 0x5b12,  65,  65, 1 ),
  V( 163, 0x5b12,  65,  65, 1 ),
  V( 164, 0x5b12,  65,  65, 1 ),
  V( 165, 0x5b12,  65,  65, 1 ),
  V( 166, 0x5b12,  65,  65, 1 ),
  V( 167, 0x5b12,  65,  65, 1 ),
  V( 168, 0x5b12,  65,  65, 1 ),
  V( 169, 0x5b12,  65,  65, 1 ),

  V( 170, 0x5b12,  65,  65, 1 ),
  V( 171, 0x5b12,  65,  65, 1 ),
  V( 172, 0x5b12,  65,  65, 1 ),
  V( 173, 0x5b12,  65,  65, 1 ),
  V( 174, 0x5b12,  65,  65, 1 ),
  V( 175, 0x5b12,  65,  65, 1 ),
  V( 176, 0x5b12,  65,  65, 1 ),
  V( 177, 0x5b12,  65,  65, 1 ),
  V( 178, 0x5b12,  65,  65, 1 ),
  V( 179, 0x5b12,  65,  65, 1 ),
  V( 180, 0x5b12,  65,  65, 1 ),
  V( 181, 0x5b12,  65,  65, 1 ),
  V( 182, 0x5b12,  65,  65, 1 ),
  V( 183, 0x5b12,  65,  65, 1 ),
  V( 184, 0x5b12,  65,  65, 1 ),
  V( 185, 0x5b12,  65,  65, 1 ),
  V( 186, 0x5b12,  65,  65, 1 ),
  V( 187, 0x5b12,  65,  65, 1 ),
  V( 188, 0x5b12,  65,  65, 1 ),
  V( 189, 0x5b12,  65,  65, 1 ),
  V( 190, 0x5b12,  65,  65, 1 ),
  V( 191, 0x5b12,  65,  65, 1 ),
  V( 192, 0x5b12,  65,  65, 1 ),
  V( 193, 0x5b12,  65,  65, 1 ),
  V( 194, 0x5b12,  65,  65, 1 ),
  V( 195, 0x5b12,  65,  65, 1 ),
  V( 196, 0x5b12,  65,  65, 1 ),
  V( 197, 0x5b12,  65,  65, 1 ),
  V( 198, 0x5b12,  65,  65, 1 ),
  V( 199, 0x5b12,  65,  65, 1 ),

  V( 200, 0x5b12,  65,  65, 1 ),
  V( 201, 0x5b12,  65,  65, 1 ),
  V( 202, 0x5b12,  65,  65, 1 ),
  V( 203, 0x5b12,  65,  65, 1 ),
  V( 204, 0x5b12,  65,  65, 1 ),
  V( 205, 0x5b12,  65,  65, 1 ),
  V( 206, 0x5b12,  65,  65, 1 ),
  V( 207, 0x5b12,  65,  65, 1 ),
  V( 298, 0x5b12,  65,  65, 1 ),
  V( 209, 0x5b12,  65,  65, 1 ),
  V( 210, 0x5b12,  65,  65, 1 ),
  V( 211, 0x5b12,  65,  65, 1 ),
  V( 212, 0x5b12,  65,  65, 1 ),
  V( 213, 0x5b12,  65,  65, 1 ),
  V( 214, 0x5b12,  65,  65, 1 ),
  V( 215, 0x5b12,  65,  65, 1 ),
  V( 216, 0x5b12,  65,  65, 1 ),
  V( 217, 0x5b12,  65,  65, 1 ),
  V( 218, 0x5b12,  65,  65, 1 ),
  V( 219, 0x5b12,  65,  65, 1 ),
  V( 220, 0x5b12,  65,  65, 1 ),
  V( 221, 0x5b12,  65,  65, 1 ),
  V( 222, 0x5b12,  65,  65, 1 ),
  V( 223, 0x5b12,  65,  65, 1 ),
  V( 224, 0x5b12,  65,  65, 1 ),
  V( 225, 0x5b12,  65,  65, 1 ),
  V( 226, 0x5b12,  65,  65, 1 ),
  V( 227, 0x5b12,  65,  65, 1 ),
  V( 228, 0x5b12,  65,  65, 1 ),
  V( 229, 0x5b12,  65,  65, 1 ),
  V( 230, 0x5b12,  65,  65, 1 ),
  V( 231, 0x5b12,  65,  65, 1 ),
  V( 232, 0x5b12,  65,  65, 1 ),
  V( 233, 0x5b12,  65,  65, 1 ),
  V( 234, 0x5b12,  65,  65, 1 ),
  V( 235, 0x5b12,  65,  65, 1 ),
  V( 236, 0x5b12,  65,  65, 1 ),
  V( 237, 0x5b12,  65,  65, 1 ),
  V( 238, 0x5b12,  65,  65, 1 ),
  V( 239, 0x5b12,  65,  65, 1 ),
  V( 240, 0x5b12,  65,  65, 1 ),
  V( 241, 0x5b12,  65,  65, 1 ),
  V( 242, 0x5b12,  65,  65, 1 ),
  V( 243, 0x5b12,  65,  65, 1 ),
  V( 244, 0x5b12,  65,  65, 1 ),
  V( 245, 0x5b12,  65,  65, 1 ),
  V( 246, 0x5b12,  65,  65, 1 ),
  V( 247, 0x5b12,  65,  65, 1 ),
  V( 248, 0x5b12,  65,  65, 1 ),
  V( 249, 0x5b12,  65,  65, 1 ),
  V( 250, 0x5b12,  65,  65, 1 ),
  V( 251, 0x5b12,  65,  65, 1 ),
  V( 252, 0x5b12,  65,  65, 1 ),
  V( 253, 0x5b12,  65,  65, 1 ),
  V( 254, 0x5b12,  65,  65, 1 ),
  V( 255, 0x5b12,  65,  65, 1 ),

};

namespace Sirikata {
static void emit_byte(int byte_data, DecoderWriter *cinfo) {
    unsigned char data[1] = {(uint8_t)byte_data};
    cinfo->Write(data, 1);
}
static int get_byte(DecoderReader *cinfo) {
    static int num_bad = 0;
    unsigned char data[1] = {0};
    std::pair<int, JpegError> x = cinfo->Read(data, 1);
    if (x.first == 0 || x.second != JpegError::nil()) {
        if (num_bad++%2 ==0) {
            return 0xff;
        }
        return 0xd9;
    }
    return data[0];
}
/*
 * The core arithmetic encoding routine (common in JPEG and JBIG).
 * This needs to go as fast as possible.
 * Machine-dependent optimization facilities
 * are not utilized in this portable implementation.
 * However, this code should be fairly efficient and
 * may be a good base for further optimizations anyway.
 *
 * Parameter 'val' to be encoded may be 0 or 1 (binary decision).
 *
 * Note: I've added full "Pacman" termination support to the
 * byte output routines, which is equivalent to the optional
 * Discard_final_zeros procedure (Figure D.15) in the spec.
 * Thus, we always produce the shortest possible output
 * stream compliant to the spec (no trailing zero bytes,
 * except for FF stuffing).
 *
 * I've also introduced a new scheme for accessing
 * the probability estimation state machine table,
 * derived from Markus Kuhn's JBIG implementation.
 */

void ArithmeticCoder::arith_encode(DecoderWriter *cinfo, unsigned char *st, bool val) {

  ArithmeticCoder *e = this;
  unsigned char nl, nm;
  int32_t qe, temp;
  int sv;

  /* Fetch values from our compact representation of Table D.2:
   * Qe values and probability estimation state machine
   */
  sv = *st;
  qe = jpeg_aritab[sv & 0x7F];  /* => Qe_Value */
  assert( qe != 0);
  nl = qe & 0xFF; qe >>= 8;     /* Next_Index_LPS + Switch_MPS */
  nm = qe & 0xFF; qe >>= 8;     /* Next_Index_MPS */

  /* Encode & estimation procedures per sections D.1.4 & D.1.5 */
  e->a -= qe;
  assert(e->a > 0);
  if ((int)val != (sv >> 7)) {
    /* Encode the less probable symbol */
    if (e->a >= qe) {
      /* If the interval size (qe) for the less probable symbol (LPS)
       * is larger than the interval size for the MPS, then exchange
       * the two symbols for coding efficiency, otherwise code the LPS
       * as usual: */
      e->c += e->a;
      e->a = qe;
      assert(e->a > 0);
    }
    *st = (sv & 0x80) ^ nl;     /* Estimate_after_LPS */
  } else {
    /* Encode the more probable symbol */
    if (e->a >= 0x8000L)
      return;  /* A >= 0x8000 -> ready, no renormalization required */
    if (e->a < qe) {
      /* If the interval size (qe) for the less probable symbol (LPS)
       * is larger than the interval size for the MPS, then exchange
       * the two symbols for coding efficiency: */
      e->c += e->a;
      e->a = qe;
    }
    *st = (sv & 0x80) ^ nm;     /* Estimate_after_MPS */
  }
  assert(e->a > 0);
  /* Renormalization & data output per section D.1.6 */
  do {
    e->a <<= 1;
    e->c <<= 1;
    if (--e->ct == 0) {
      /* Another byte is ready for output */
      temp = e->c >> 19;
      if (temp > 0xFF) {
        /* Handle overflow over all stacked 0xFF bytes */
        if (e->buffer >= 0) {
          if (e->zc)
            do emit_byte(0x00, cinfo);
            while (--e->zc);
          emit_byte(e->buffer + 1, cinfo);
          if (e->buffer + 1 == 0xFF)
            emit_byte(0x00, cinfo);
        }
        e->zc += e->sc;  /* carry-over converts stacked 0xFF bytes to 0x00 */
        e->sc = 0;
        /* Note: The 3 spacer bits in the C register guarantee
         * that the new buffer byte can't be 0xFF here
         * (see page 160 in the P&M JPEG book). */
        e->buffer = temp & 0xFF;  /* new output byte, might overflow later */
      } else if (temp == 0xFF) {
        ++e->sc;  /* stack 0xFF byte (which might overflow later) */
      } else {
        /* Output all stacked 0xFF bytes, they will not overflow any more */
        if (e->buffer == 0)
          ++e->zc;
        else if (e->buffer >= 0) {
          if (e->zc)
            do emit_byte(0x00, cinfo);
            while (--e->zc);
          emit_byte(e->buffer, cinfo);
        }
        if (e->sc) {
          if (e->zc)
            do emit_byte(0x00, cinfo);
            while (--e->zc);
          do {
            emit_byte(0xFF, cinfo);
            emit_byte(0x00, cinfo);
          } while (--e->sc);
        }
        e->buffer = temp & 0xFF;  /* new output byte (can still overflow) */
      }
      e->c &= 0x7FFFFL;
      e->ct += 8;
    }
  } while (e->a < 0x8000L);
}

void ArithmeticCoder::finish_encode(DecoderWriter *cinfo) {
  ArithmeticCoder * e = this;
  int32_t temp;

  /* Section D.1.8: Termination of encoding */

  /* Find the e->c in the coding interval with the largest
   * number of trailing zero bits */
  if ((temp = (e->a - 1 + e->c) & 0xFFFF0000L) < e->c)
    e->c = temp + 0x8000L;
  else
    e->c = temp;
  /* Send remaining bytes to output */
  e->c <<= e->ct;
  if (e->c & 0xF8000000L) {
    /* One final overflow has to be handled */
    if (e->buffer >= 0) {
      if (e->zc)
        do emit_byte(0x00, cinfo);
        while (--e->zc);
      emit_byte(e->buffer + 1, cinfo);
      if (e->buffer + 1 == 0xFF)
        emit_byte(0x00, cinfo);
    }
    e->zc += e->sc;  /* carry-over converts stacked 0xFF bytes to 0x00 */
    e->sc = 0;
  } else {
    if (e->buffer == 0)
      ++e->zc;
    else if (e->buffer >= 0) {
      if (e->zc)
        do emit_byte(0x00, cinfo);
        while (--e->zc);
      emit_byte(e->buffer, cinfo);
    }
    if (e->sc) {
      if (e->zc)
        do emit_byte(0x00, cinfo);
        while (--e->zc);
      do {
        emit_byte(0xFF, cinfo);
        emit_byte(0x00, cinfo);
      } while (--e->sc);
    }
  }
  /* Output final bytes only if they are not 0x00 */
  if (e->c & 0x7FFF800L) {
    if (e->zc)  /* output final pending zero bytes */
      do emit_byte(0x00, cinfo);
      while (--e->zc);
    emit_byte((e->c >> 19) & 0xFF, cinfo);
    if (((e->c >> 19) & 0xFF) == 0xFF)
      emit_byte(0x00, cinfo);
    if (e->c & 0x7F800L) {
      emit_byte((e->c >> 11) & 0xFF, cinfo);
      if (((e->c >> 11) & 0xFF) == 0xFF)
        emit_byte(0x00, cinfo);
    }
  }
}
bool ArithmeticCoder::arith_decode(DecoderReader *cinfo, unsigned char *st) {
  ArithmeticCoder *e = this;
  unsigned char nl, nm;
  int32_t qe, temp;
  int sv, data;

  /* Renormalization & data input per section D.2.6 */
  while (e->a < 0x8000L) {
    if (--e->ct < 0) {
      /* Need to fetch next data byte */
      if (e->unread_marker)
        data = 0;               /* stuff zero data */
      else {
        data = get_byte(cinfo); /* read next input byte */
        if (data == 0xFF) {     /* zero stuff or marker code */
          do data = get_byte(cinfo);
          while (data == 0xFF); /* swallow extra 0xFF bytes */
          if (data == 0)
            data = 0xFF;        /* discard stuffed zero byte */
          else {
            /* Note: Different from the Huffman decoder, hitting
             * a marker while processing the compressed data
             * segment is legal in arithmetic coding.
             * The convention is to supply zero data
             * then until decoding is complete.
             */
            e->unread_marker = data;
            data = 0;
          }
        }
      }
      e->c = (e->c << 8) | data; /* insert data into C register */
      if ((e->ct += 8) < 0)      /* update bit shift counter */
        /* Need more initial bytes */
        if (++e->ct == 0)
          /* Got 2 initial bytes -> re-init A and exit loop */
          e->a = 0x8000L; /* => e->a = 0x10000L after loop exit */
    }
    e->a <<= 1;
  }

  /* Fetch values from our compact representation of Table D.2:
   * Qe values and probability estimation state machine
   */
  sv = *st;
  qe = jpeg_aritab[sv & 0x7F];  /* => Qe_Value */
  nl = qe & 0xFF; qe >>= 8;     /* Next_Index_LPS + Switch_MPS */
  nm = qe & 0xFF; qe >>= 8;     /* Next_Index_MPS */

  /* Decode & estimation procedures per sections D.2.4 & D.2.5 */
  temp = e->a - qe;
  e->a = temp;
  temp <<= e->ct;
  if (e->c >= temp) {
    e->c -= temp;
    /* Conditional LPS (less probable symbol) exchange */
    if (e->a < qe) {
      e->a = qe;
      *st = (sv & 0x80) ^ nm;   /* Estimate_after_MPS */
    } else {
      e->a = qe;
      *st = (sv & 0x80) ^ nl;   /* Estimate_after_LPS */
      sv ^= 0x80;               /* Exchange LPS/MPS */
    }
  } else if (e->a < 0x8000L) {
    /* Conditional MPS (more probable symbol) exchange */
    if (e->a < qe) {
      *st = (sv & 0x80) ^ nl;   /* Estimate_after_LPS */
      sv ^= 0x80;               /* Exchange LPS/MPS */
    } else {
      *st = (sv & 0x80) ^ nm;   /* Estimate_after_MPS */
    }
  }

  return sv >> 7;

}
}

