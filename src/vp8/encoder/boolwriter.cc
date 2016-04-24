/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "boolwriter.hh"

int w_bitcount = 0;

void vpx_start_encode(vpx_writer *br, uint8_t *source) {
  br->lowvalue = 0;
  br->range    = 255;
  br->count    = -24;
  br->buffer   = source;
  br->pos      = 0;
  vpx_write(br, 0, 128, Billing::HEADER);
}

void vpx_stop_encode(vpx_writer *br) {
  int i;

  for (i = 0; i < 32; i++)
      vpx_write(br, 0, 128, Billing::DELIMITERS);

  // Ensure there's no ambigous collision with any index marker bytes
  if ((br->buffer[br->pos - 1] & 0xe0) == 0xc0)
    br->buffer[br->pos++] = 0;
}

