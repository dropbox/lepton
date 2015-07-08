/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <algorithm>
#include <cassert>
#include <iostream>

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_decoder.hh"

#include "mmap.hh"

#include "../io/SwitchableCompression.hh"
using namespace std;

void VP8ComponentDecoder::initialize( Sirikata::
                                      SwitchableDecompressionReader<Sirikata::SwitchableXZBase> *input)
{
    str_in = input;
}


void VP8ComponentDecoder::vp8_continuous_decoder( UncompressedComponents * const colldata,
                                                  Sirikata::
                                                  SwitchableDecompressionReader<Sirikata
                                                                                ::SwitchableXZBase> *str_in )
{
    colldata->worker_wait_for_begin_signal();

    VP8ComponentDecoder scd;
    scd.initialize(str_in);
    while(scd.decode_chunk(colldata) == CODING_PARTIAL) {
        
    }
}

VP8ComponentDecoder::VP8ComponentDecoder()
    : probability_tables_( ProbabilityTables::get_probability_tables() ),
      bool_decoder_()
{}

CodingReturnValue VP8ComponentDecoder::decode_chunk(UncompressedComponents * const colldata)
{
    /* cmpc is a global variable with the component count */
    /* verify JFIF is Y'CbCr only (no grayscale images allowed) */
    if ( colldata->get_num_components() != 3 ) {
        std::cerr << "JPEG/JFIF was not a three-component Y'CbCr image" << std::endl;
        return CODING_ERROR;
    }

    /* construct 4x4 VP8 blocks to hold 8x8 JPEG blocks */
    if ( vp8_blocks_.empty() ) {
        /* first call */
        str_in->EnableCompression();

        /* read and verify "x" mark */
        unsigned char mark {};
        const bool ok = str_in->Read( &mark, 1 ).second == Sirikata::JpegError::nil();
        if ( mark != 'x' ) {
            cerr << "CMPx marker not found " << ok << endl;
            return CODING_ERROR;
        }
        str_in->DisableCompression();

        /* populate the VP8 blocks */
        vp8_blocks_.emplace_back( colldata->block_width( 0 ), colldata->block_height( 0 ), Y );
        vp8_blocks_.emplace_back( colldata->block_width( 1 ), colldata->block_height( 1 ), Cb );
        vp8_blocks_.emplace_back( colldata->block_width( 2 ), colldata->block_height( 2 ), Cr );

        /* read length */
        uint32_t length_big_endian;
        if ( 4 != IOUtil::ReadFull( str_in, &length_big_endian, sizeof( uint32_t ) ) ) {
            return CODING_ERROR;
        }

        /* allocate memory for compressed frame */
        file_.resize( be32toh( length_big_endian ) );

        /* read entire chunk into memory */
        if ( file_.size()
             != IOUtil::ReadFull( str_in, file_.data(), file_.size() ) ) {
            return CODING_ERROR;
        }

        /* initialize the bool decoder */
        bool_decoder_.initialize( Slice( file_.data(), file_.size() ) );
        memset(jpeg_y_, 0, sizeof(jpeg_y_));
    }
        
    /* deserialize each block in planar order */
    int component = 0;
    while(colldata->get_next_component(jpeg_y_, &component)) {
        int curr_y = jpeg_y_[component];
        int block_width = colldata->block_width( component );
        for ( int jpeg_x = 0; jpeg_x < block_width; jpeg_x++ ) {
            auto & block = vp8_blocks_.at( component ).at( jpeg_x, curr_y );
            block.parse_tokens( bool_decoder_.get(), probability_tables_ );
            for ( int coeff = 0; coeff < 64; coeff++ ) {
                colldata->set( component, coeff, curr_y * block_width + jpeg_x )
                    = block.coefficients().at( jpeg_zigzag.at( coeff ) );
            }
        }
        colldata->worker_update_cmp_progress( component,
                                              block_width );
        ++jpeg_y_[component];
        return CODING_PARTIAL;
    }

    colldata->worker_update_coefficient_position_progress( 64 );
    colldata->worker_update_bit_progress( 16 );
    return CODING_DONE;
}
