/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <algorithm>
#include <cassert>

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_decoder.hh"

void VP8ComponentDecoder::initialize(iostream *input) {
    str_in = input;
}

CodingReturnValue VP8ComponentDecoder::decode_chunk(UncompressedComponents *dst) {
    return vp8_decoder(dst, str_in);
}

void VP8ComponentDecoder::vp8_continuous_decoder( UncompressedComponents * const colldata,
                                                  iostream * const str_in) {
    colldata->worker_wait_for_begin_signal();

    VP8ComponentDecoder scd;
    scd.initialize(str_in);
    while(scd.decode_chunk(colldata) == CODING_PARTIAL) {
        
    }
}

bool VP8ComponentDecoder::started_scan() const
{
    return started_scan_.at( current_component_ );
}

void VP8ComponentDecoder::set_started_scan()
{
    started_scan_.at( current_component_ ) = true;
}

CodingReturnValue VP8ComponentDecoder::vp8_decoder( UncompressedComponents * const colldata,
                                                     iostream * const str_in) {    
    /* verify JFIF is Y' or Y'CbCr */
    assert( cmpc >= 1 );
    assert( cmpc <= 3 );

    char ujpg_mrk[ 64 ] = "CMP";
    int batch_size = 1600;
    colldata->worker_update_band_progress( 64 ); // we are optimizing for baseline only atm
    colldata->worker_update_bit_progress( 16 ); // we are optimizing for baseline only atm

	// read actual decompressed coefficient data from file
    if ( not started_scan() ) {
        if (current_component_ != 0) {
            str_in->read( ujpg_mrk, 1, 4 );
        } else {
            str_in->read( ujpg_mrk + 3, 1, 1 );
        }

        set_started_scan();

        // check marker
        if (strncmp( ujpg_mrk, "CMP", 3 ) != 0) {
            sprintf( errormessage, "CMP%i marker not found", current_component_ );
            errorlevel = 2;    
            return CODING_ERROR;
        }
    }
    {
        // read coefficient data from file
        signed short * start = colldata->full_component_write( current_component_ );
        int target = colldata->component_size_in_blocks(current_component_);
        while (cur_read_batch_[current_component_] < target) {
            int cur_read_size = std::min(batch_size, target - cur_read_batch_[current_component_]);
            ssize_t retval = str_in->read(start + cur_read_batch_[current_component_] * 64 , int(sizeof( short ) * 64), cur_read_size);
            if (retval != cur_read_size) {
                sprintf( errormessage, "unexpected end of file blocks %ld !=  %d", retval, cur_read_size);
                errorlevel = 2;
                return CODING_ERROR;
            }
            cur_read_batch_[current_component_] += cur_read_size;
            colldata->worker_update_cmp_progress(current_component_, cur_read_size);
            if (cur_read_batch_[current_component_] < target) {
                return CODING_PARTIAL;
            }
        }
    }
    if (current_component_ < cmpc) {
        ++current_component_;
        return CODING_PARTIAL;
    } else {
        return CODING_DONE;
    }
}
