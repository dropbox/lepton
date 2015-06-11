/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "simple_decoder.hh"
#include <algorithm>
SimpleComponentDecoder::SimpleComponentDecoder() {
    for (int i = 0; i < 4; ++i) {
        cur_read_batch[i] = 0;
        started_scan[i] = false;
    }
    cmp = 0;
}
void SimpleComponentDecoder::simple_continuous_decoder(UncompressedComponents* colldata, iostream *str_in) {
    colldata->worker_wait_for_begin_signal();

    SimpleComponentDecoder scd;
    while(scd.simple_decoder(colldata, str_in) == DECODER_PARTIAL) {
        
    }
}
DecoderReturnValue SimpleComponentDecoder::simple_decoder(UncompressedComponents* colldata, iostream *str_in) {
    
	char ujpg_mrk[ 64 ] = "CMP";
    int batch_size = 1600;
    colldata->worker_update_band_progress(64); // we are optimizing for baseline only atm
    colldata->worker_update_bit_progress(16); // we are optimizing for baseline only atm
	// read actual decompressed coefficient data from file
    if (!started_scan[cmp]) {
        if (cmp != 0) {
            str_in->read( ujpg_mrk, 1, 4 );
        } else {
            str_in->read( ujpg_mrk + 3, 1, 1 );
        }
        started_scan[cmp] = true;
        // check marker
        if (strncmp( ujpg_mrk, "CMP", 3 ) != 0) {
            sprintf( errormessage, "CMP%i marker not found", cmp );
            errorlevel = 2;    
            return DECODER_ERROR;
        }
    }
    {
        // read coefficient data from file
        signed short * start = colldata->full_component_write( cmp );
        int target = colldata->component_size_in_blocks(cmp);
        while (cur_read_batch[cmp] < target) {
            int cur_read_size = std::min(batch_size, target - cur_read_batch[cmp]);
            ssize_t retval = str_in->read(start + cur_read_batch[cmp] * 64 , int(sizeof( short ) * 64), cur_read_size);
            if (retval != cur_read_size) {
                sprintf( errormessage, "unexpected end of file blocks %ld !=  %d", retval, cur_read_size);
                errorlevel = 2;
                return DECODER_ERROR;
            }
            cur_read_batch[cmp] += cur_read_size;
            colldata->worker_update_cmp_progress(cmp, cur_read_size);
            if (cur_read_batch[cmp] < target) {
                return DECODER_PARTIAL;
            }
        }
    }
    if (cmp < cmpc) {
        ++cmp;
        return DECODER_PARTIAL;
    } else {
        return DECODER_DONE;
    }
}
