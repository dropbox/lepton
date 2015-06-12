/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <algorithm>
#include <cassert>
#include <iostream>

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_decoder.hh"

using namespace std;

void VP8ComponentDecoder::initialize( ::iostream *input ) {
    str_in = input;
}

CodingReturnValue VP8ComponentDecoder::decode_chunk(UncompressedComponents *dst) {
    return vp8_decoder(dst, str_in);
}

void VP8ComponentDecoder::vp8_continuous_decoder( UncompressedComponents * const colldata,
                                                  ::iostream * const str_in )
{
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

bool & VP8ComponentDecoder::mutable_started_scan()
{
    return started_scan_.at( current_component_ );
}

int VP8ComponentDecoder::cur_read_batch() const
{
    return cur_read_batch_.at( current_component_ );
}

int & VP8ComponentDecoder::mutable_cur_read_batch()
{
    return cur_read_batch_.at( current_component_ );
}

CodingReturnValue VP8ComponentDecoder::vp8_decoder( UncompressedComponents * const colldata,
                                                    ::iostream * const str_in )
{
    /* verify JFIF is Y' or Y'CbCr */
    assert( cmpc >= 1 );
    assert( cmpc <= 3 );
    assert( current_component_ < cmpc );

    int batch_size = 1600;
    colldata->worker_update_band_progress( 64 ); // we are optimizing for baseline only atm
    colldata->worker_update_bit_progress( 16 ); // we are optimizing for baseline only atm

    /* read and verify mark */
    if ( not started_scan() ) {
        mutable_started_scan() = true;

        array<char, 4> expected_mark {{ 'C', 'M', 'P', '_' }}, mark {{}};
        expected_mark.at( 3 ) = current_component_ + 0x30;

        if ( current_component_ == 0 ) {
            mark = {{ 'C', 'M', 'P', '_' }};
            str_in->read( &mark.at( 3 ), 1, 1 );
        } else {
            str_in->read( &mark.at( 0 ), 1, 4 );
        }

        if ( mark != expected_mark ) {
            cerr << "CMP_ marker not found" << endl;
            errorlevel = 2;
            return CODING_ERROR;
        }
    }

    // read coefficient data from file
    signed short * const start = colldata->full_component_write( current_component_ );
    const int target = colldata->component_size_in_blocks( current_component_ );

    while ( cur_read_batch() < target) {
        const int cur_read_size = min( batch_size,
                                       target - cur_read_batch() );

        ssize_t retval = str_in->read( start + cur_read_batch() * 64 ,
                                       sizeof( short ) * 64, cur_read_size );

        if ( retval != cur_read_size ) {
            cerr << "unexpected end of file" << endl;
            errorlevel = 2;
            return CODING_ERROR;
        }

        mutable_cur_read_batch() += cur_read_size;

        colldata->worker_update_cmp_progress( current_component_, cur_read_size );
        if ( cur_read_batch() < target) {
            return CODING_PARTIAL;
        }
    }

    current_component_++;
    return current_component_ >= cmpc ? CODING_DONE : CODING_PARTIAL;
}
