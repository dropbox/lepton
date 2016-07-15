/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE banner below
 *  An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the VPX_AUTHORS file in this directory
 */
/*
Copyright (c) 2010, Google Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

Neither the name of Google nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VPX_DSP_BITREADER_H_
#define VPX_DSP_BITREADER_H_

#include <stddef.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include "vpx_config.hh"
#include "billing.hh"
#include "../model/numeric.hh"
//#include "vpx_ports/mem.h"
//#include "vpx/vp8dx.h"
//#include "vpx/vpx_integer.h"
//#include "vpx_dsp/prob.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef size_t BD_VALUE;

#define BD_VALUE_SIZE ((int)sizeof(BD_VALUE) * CHAR_BIT)

// This is meant to be a large, positive constant that can still be efficiently
// loaded as an immediate (on platforms like ARM, for example).
// Even relatively modest values like 100 would work fine.
#define LOTS_OF_BITS 0x40000000

typedef struct {
  // Be careful when reordering this struct, it may impact the cache negatively.
  BD_VALUE value;
  unsigned int range;
  int count;
  const uint8_t *buffer_end;
  const uint8_t *buffer;
//  vpx_decrypt_cb decrypt_cb;
//  void *decrypt_state;
  uint8_t clear_buffer[sizeof(BD_VALUE) + 1];
} vpx_reader;

int vpx_reader_init(vpx_reader *r,
                    const uint8_t *buffer,
                    size_t size);

static INLINE void vpx_reader_fill(vpx_reader *r) {
    const uint8_t *const buffer_end = r->buffer_end;
    const uint8_t *buffer = r->buffer;
    const uint8_t *buffer_start = buffer;
    BD_VALUE value = r->value;
    int count = r->count;
    const size_t bytes_left = buffer_end - buffer;
    const size_t bits_left = bytes_left * CHAR_BIT;
    int shift = BD_VALUE_SIZE - CHAR_BIT - (count + CHAR_BIT);
    
    if (bits_left > BD_VALUE_SIZE) {
        const int bits = (shift & 0xfffffff8) + CHAR_BIT;
        BD_VALUE nv;
        BD_VALUE big_endian_values;
        memcpy(&big_endian_values, buffer, sizeof(BD_VALUE));
        if (sizeof(BD_VALUE) == 8) {
            big_endian_values = htobe64(big_endian_values);
        } else {
            big_endian_values = htobe32(big_endian_values);
        }
        nv = big_endian_values >> (BD_VALUE_SIZE - bits);
        count += bits;
        buffer += (bits >> 3);
        value = r->value | (nv << (shift & 0x7));
    } else {
        const int bits_over = (int)(shift + CHAR_BIT - bits_left);
        int loop_end = 0;
        if (bits_over >= 0) {
            count += LOTS_OF_BITS;
            loop_end = bits_over;
        }
        
        if (bits_over < 0 || bits_left) {
            while (shift >= loop_end) {
                count += CHAR_BIT;
                value |= (BD_VALUE)*buffer++ << shift;
                shift -= CHAR_BIT;
            }
        }
    }
    // NOTE: Variable 'buffer' may not relate to 'r->buffer' after decryption,
    // so we increase 'r->buffer' by the amount that 'buffer' moved, rather than
    // assign 'buffer' to 'r->buffer'.
    r->buffer += buffer - buffer_start;
    r->value = value;
    r->count = count;
}

const uint8_t *vpx_reader_find_end(vpx_reader *r);

  // Check if we have reached the end of the buffer.
  //
  // Variable 'count' stores the number of bits in the 'value' buffer, minus
  // 8. The top byte is part of the algorithm, and the remainder is buffered
  // to be shifted into it. So if count == 8, the top 16 bits of 'value' are
  // occupied, 8 for the algorithm and 8 in the buffer.
  //
  // When reading a byte from the user's buffer, count is filled with 8 and
  // one byte is filled into the value buffer. When we reach the end of the
  // data, count is additionally filled with LOTS_OF_BITS. So when
  // count == LOTS_OF_BITS - 1, the user's data has been exhausted.
  //
  // 1 if we have tried to decode bits after the end of stream was encountered.
  // 0 No error.
#define vpx_reader_has_error(r) ((r)->count > BD_VALUE_SIZE && (r)->count < LOTS_OF_BITS)

extern int r_bitcount;
constexpr static uint8_t vpx_norm[256] = {
        0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
/*
inline unsigned int count_leading_zeros_uint8(uint8_t split) {
    unsigned int shift = 0;
    if (split < 128) {
        shift = 1;
    }
    if (split < 64) {
        shift = 2;
    }
    if (split < 32) {
        shift = 3;
    }
    if (split < 16) {
        shift = 4;
    }
    if (split < 8) {
        shift = 5;
    }
    if (split < 4) {
        shift = 6;
    }
    if (split == 1) {
        shift = 7;
    }
    return shift;
}
    */
#ifndef _WIN32
__attribute__((always_inline))
#endif
inline uint8_t count_leading_zeros_uint8(uint8_t v) {
    return vpx_norm[v];
    assert(v);
    return __builtin_clz((uint32_t)v) - 24; // slower
    uint8_t r = 0; // result of log2(v) will go here
    if (v & 0xf0) {
        r |= 4;
        v >>= 4;
    }
    if (v & 0xc) {
        v >>= 2;
        r |= 2;
    }
    if (v & 0x2) {
        v >>= 1;
        r |= 1;
    }
    return 7 - r;
}

inline bool vpx_reader_fill_and_read(vpx_reader *r, unsigned int split, Billing bill) {
    BD_VALUE bigsplit = (BD_VALUE)split << (BD_VALUE_SIZE - CHAR_BIT);
    vpx_reader_fill(r);
    BD_VALUE value = r->value;
    bool bit = (value >= bigsplit);
    int count = r->count;


    unsigned int range;

    if (bit) {
        range = r->range - split;
        value = value - bigsplit;
    } else {
        range = split;
    }
    //unsigned int shift = vpx_norm[range];
    unsigned int shift = count_leading_zeros_uint8(range);
    range <<= shift;
    value <<= shift;
    count -= shift;
    write_bit_bill(bill, true, shift);
    r->value = value;
    r->count = count;
    r->range = range;

    return bit;
}
#ifndef _WIN32
__attribute__((always_inline))
#endif
inline bool vpx_read(vpx_reader *r, int prob, Billing bill) {
  unsigned int split = (r->range * prob + (256 - prob)) >> CHAR_BIT;
  BD_VALUE value = r->value;
  int count = r->count;
  BD_VALUE bigsplit = (BD_VALUE)split << (BD_VALUE_SIZE - CHAR_BIT);
  bool bit = (value >= bigsplit);
  unsigned int range;
  if (bit) {
    range = r->range - split;
    value = value - bigsplit;
  } else {
    range = split;
  }
  if (__builtin_expect(r->count < 0, 0)) {
      bit = vpx_reader_fill_and_read(r, split, bill);
#ifdef DEBUG_ARICODER
      fprintf(stderr, "R %d %d %d\n", r_bitcount++, prob, bit);
#endif
      return bit;
  }
  //unsigned int shift = vpx_norm[range];
  unsigned int shift = count_leading_zeros_uint8(range);
  range <<= shift;
  value <<= shift;
  count -= shift;
  write_bit_bill(bill, true, shift);
  r->value = value;
  r->count = count;
  r->range = range;
#ifdef DEBUG_ARICODER
  fprintf(stderr, "R %d %d %d\n", r_bitcount++, prob, bit);
#endif

  return bit;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_DSP_BITREADER_H_
