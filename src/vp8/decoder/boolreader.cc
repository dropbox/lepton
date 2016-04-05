/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "../util/memory.hh"
#include <stdlib.h>
#include <string.h>
//#include "./vpx_config.h"

#include "boolreader.hh"



//#include "vpx_dsp/prob.h"
//#include "vpx_ports/mem.h"
//#include "vpx_mem/vpx_mem.h"
//#include "vpx_util/endian_inl.h"

int r_bitcount = 0;

int vpx_reader_init(vpx_reader *r,
                    const uint8_t *buffer,
                    size_t size) {
  if (size && !buffer) {
    return 1;
  } else {
    r->buffer_end = buffer + size;
    r->buffer = buffer;
    r->value = 0;
    r->count = -8;
    r->range = 255;
    vpx_reader_fill(r);
    return vpx_read(r, 128, Billing::HEADER) != 0;  // marker bit
  }
}





const uint8_t *vpx_reader_find_end(vpx_reader *r) {
  // Find the end of the coded buffer
  while (r->count > CHAR_BIT && r->count < BD_VALUE_SIZE) {
    r->count -= CHAR_BIT;
    r->buffer--;
  }
  return r->buffer;
}
