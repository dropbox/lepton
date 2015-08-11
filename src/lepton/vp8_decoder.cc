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
#include "../vp8/decoder/decoder.hh"
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


    /* construct 4x4 VP8 blocks to hold 8x8 JPEG blocks */
    if ( context_[0].context.isNil() ) {
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
        for (size_t i = 0; i < sizeof(context_)/sizeof(context_[0]); ++i) {
             // FIXME: we may be able to truncate here if we are faced with a truncated image
            vp8_blocks_.at(i).init(colldata->block_width( i ), colldata->block_height( i ),
                                colldata->block_width( i ) * colldata->block_height( i ) );
        }
        for (size_t i = 0; i < sizeof(context_)/sizeof(context_[0]); ++i) {
            context_.at(i).context = vp8_blocks_.at(i).begin();
            context_.at(i).y = 0;
        }

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

    }
    /* deserialize each block in planar order */
    int component = 0;

    while(colldata->get_next_component(context_, &component)) {
        int curr_y = context_.at(component).y;
        int block_width = colldata->block_width( component );
        for ( int jpeg_x = 0; jpeg_x < block_width; jpeg_x++ ) {
            BlockContext context = context_.at(component).context;
            probability_tables_.set_quantization_table( colldata->get_quantization_tables(component));
            parse_tokens(context,
                         get_color_context_blocks(colldata->get_color_context(jpeg_x, context_, component),
                                                  vp8_blocks_, component),
                         bool_decoder_.get(),
                         probability_tables_);
            AlignedBlock &block = context.here();
            for ( int coeff = 0; coeff < 64; coeff++ ) {
                colldata->set( component, coeff, curr_y * block_width + jpeg_x )
                    = block.coefficients().raster( jpeg_zigzag.at( coeff ) );
            }
            context_.at(component).context = vp8_blocks_.at(component).next(context_.at(component).context);
        }
        colldata->worker_update_cmp_progress( component,
                                              block_width );
        ++context_.at(component).y;
        return CODING_PARTIAL;
    }

    colldata->worker_update_coefficient_position_progress( 64 );
    colldata->worker_update_bit_progress( 16 );
    return CODING_DONE;
}
