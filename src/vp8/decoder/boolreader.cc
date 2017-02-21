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
                    PacketReader *upstream_reader) {
    r->buffer = BiRope();
    r->reader = upstream_reader;
    r->value = 0;
    r->count = -8;
    r->range = 255;
    vpx_reader_fill(r);
    return vpx_read(r, 128, Billing::HEADER) != 0;  // marker bit
}



