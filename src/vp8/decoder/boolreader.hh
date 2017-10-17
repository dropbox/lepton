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


typedef size_t BD_VALUE;

#define BD_VALUE_SIZE ((int)sizeof(BD_VALUE) * CHAR_BIT)

// This is meant to be a large, positive constant that can still be efficiently
// loaded as an immediate (on platforms like ARM, for example).
// Even relatively modest values like 100 would work fine.
#define LOTS_OF_BITS 0x40000000
static std::atomic<uint32_t> test_packet_reader_atomic_test;
typedef std::pair<const uint8_t*, const uint8_t*> ROBuffer;
class PacketReader{
protected:
    bool isEof;
public:
    PacketReader() {
        isEof = false;
    }
    // returns a buffer with at least sizeof(BD_VALUE) before it
    virtual ROBuffer getNext() = 0;
    bool eof()const {
        return isEof;
    }
    virtual void setFree(ROBuffer buffer) = 0;
    virtual ~PacketReader(){}
};
class TestPacketReader :public PacketReader{
    const uint8_t*cur;
    const uint8_t*end;
public:
    TestPacketReader(const uint8_t *start, const uint8_t *ed) {
        isEof = false;
        cur = start;
        end = ed;
    }
    ROBuffer getNext(){
        if (cur == end) {
            isEof = true;
            return {NULL, NULL};
        }
        if (end - cur > 16) {
            size_t val = (test_packet_reader_atomic_test += 7)%16 + 1;
            cur += val;
            return {cur - val, cur};
        }
        const uint8_t *ret = cur;
        cur = end;
        return {ret, end};
    }
    bool eof()const {
        return isEof;
    }
    void setFree(ROBuffer buffer){}
};
class BiRope {
public:
    ROBuffer rope[2];
    // if we want partial data from a previous valuex
    uint8_t backing[sizeof(BD_VALUE)];
    BiRope() {
        memset(&backing[0], 0, sizeof(BD_VALUE));
        for (size_t i= 0; i < sizeof(rope)/sizeof(rope[0]); ++i) {
            rope[i] = {NULL, NULL};
        }
    }
    void push(ROBuffer data) {
        if(rope[0].first == NULL) {
            rope[0] = data;
        }else {
            always_assert(rope[1].first == NULL);
            rope[1] = data;
        }
    }
    size_t size() const {
        return (rope[0].second-rope[0].first) +
            (rope[1].second - rope[1].first);
    }
    void memcpy_ro(uint8_t *dest, size_t size) const {
        if ((ptrdiff_t)size < rope[0].second-rope[0].first) {
            memcpy(dest, rope[0].first, size);
            return;
        }
        size_t del = rope[0].second-rope[0].first;
        if (del) {
            memcpy(dest, rope[0].first, del);
        }
        dest += del;
        size -=del;
        if (size) {
            always_assert(rope[1].second - rope[1].first >= (ptrdiff_t)size);
            memcpy(dest, rope[1].first, size);
        }
    }
    void operator += (size_t del) {
        if ((ptrdiff_t)del < rope[0].second - rope[0].first) {
            rope[0].first += del;
            return;
        }
        del -= rope[0].second - rope[0].first;
        rope[0] = rope[1];
        rope[1] = {NULL, NULL};
        always_assert((ptrdiff_t)del <= rope[0].second - rope[0].first);
        rope[0].first += del;
        if (rope[0].first == rope[0].second) {
            rope[0] = {NULL, NULL};
        }
    }
    /*
    void memcpy_pop(uint8_t *dest, size_t size) {
        if (size < rope[0].second-rope[0].first) {
            memcpy(dest, rope[0].first, size);
            rope[0].first += size;
            return;
        } else {
            size_t del = rope[0].second-rope[0].first;
            memcpy(dest, rope[0].first, del);
            dest += del;
            size -= del;
            rope[0] = rope[1];
            rope[1] = {NULL, NULL};
        }
        if (size) {
            always_assert(rope[0].second - rope[0].first < size);
            memcpy(dest, rope[0].first, size);
            rope[0].first += size;
            if (rope[0].first == rope[0].second) {
                rope[0] = {NULL, NULL};
            }
        }
        }*/
};
typedef struct {
  // Be careful when reordering this struct, it may impact the cache negatively.
  BD_VALUE value;
  unsigned int range;
  int count;
  BiRope buffer;
  PacketReader *reader;
//  vpx_decrypt_cb decrypt_cb;
//  void *decrypt_state;
} vpx_reader;

int vpx_reader_init(vpx_reader *r,
                    PacketReader *reader);

static INLINE void vpx_reader_fill(vpx_reader *r) {
    BD_VALUE value = r->value;
    int count = r->count;
    size_t bytes_left = r->buffer.size();
    size_t bits_left = bytes_left * CHAR_BIT;
    int shift = BD_VALUE_SIZE - CHAR_BIT - (count + CHAR_BIT);
    if (bits_left <= BD_VALUE_SIZE && !r->reader->eof()) {
        // pull some from reader
        uint8_t local_buffer[sizeof(BD_VALUE)] = {0};
        r->buffer.memcpy_ro(local_buffer, bytes_left);
        r->buffer += bytes_left; // clear it out
        while(true) {
            auto next = r->reader->getNext();
            if (next.second - next.first + bytes_left <= sizeof(BD_VALUE)) {
                if (next.first != next.second) {
                    memcpy(local_buffer + bytes_left, next.first, next.second - next.first);
                }
                bytes_left += next.second - next.first;
            } else {
                if (bytes_left) {
                    memcpy(r->buffer.backing, local_buffer, bytes_left);
                    r->buffer.push({r->buffer.backing, r->buffer.backing + bytes_left});
                }
                r->buffer.push(next);
                break;
            }
            if (r->reader->eof()) {
                always_assert(bytes_left <= sizeof(BD_VALUE)); // otherwise we'd have break'd
                memcpy(r->buffer.backing, local_buffer, bytes_left);
                r->buffer.push({r->buffer.backing, r->buffer.backing + bytes_left});
                break; // setup a simplistic rope that just points to the backing store
            }
        }
        bytes_left = r->buffer.size();
        bits_left = bytes_left * CHAR_BIT;
    }
    if (bits_left > BD_VALUE_SIZE) {
        const int bits = (shift & 0xfffffff8) + CHAR_BIT;
        BD_VALUE nv;
        BD_VALUE big_endian_values;
        r->buffer.memcpy_ro((uint8_t*)&big_endian_values, sizeof(BD_VALUE));
        if (sizeof(BD_VALUE) == 8) {
            big_endian_values = htobe64(big_endian_values);
        } else {
            big_endian_values = htobe32(big_endian_values);
        }
        nv = big_endian_values >> (BD_VALUE_SIZE - bits);
        count += bits;
        r->buffer += (bits >> 3);
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
                uint8_t cur_val = 0;
                r->buffer.memcpy_ro(&cur_val, 1);
                r->buffer += 1;
                value |= ((BD_VALUE)cur_val) << shift;
                shift -= CHAR_BIT;
            }
        }
    }
    // NOTE: Variable 'buffer' may not relate to 'r->buffer' after decryption,
    // so we increase 'r->buffer' by the amount that 'buffer' moved, rather than
    // assign 'buffer' to 'r->buffer'.
    r->value = value;
    r->count = count;
}


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
    dev_assert(v);
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
  int split = (r->range * prob + (256 - prob)) >> CHAR_BIT;
  BD_VALUE value = r->value;
  int count = r->count;
  BD_VALUE bigsplit = (BD_VALUE)split << (BD_VALUE_SIZE - CHAR_BIT);
  bool bit = value >= bigsplit;
  unsigned int range;
#if 0
  BD_VALUE mask = -(long long)bit;
  value -= mask & bigsplit;
  range = (r->range & mask) + (split ^ mask) - mask;
#else
  if (bit) {
      range = r->range - split;
      value = value - bigsplit;
  } else {
      range = split;
  }
#endif
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

#endif  // VPX_DSP_BITREADER_H_
