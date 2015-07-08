/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <string>
#include <cassert>
#include <iostream>
#include <fstream>

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_encoder.hh"

#include "block.hh"
#include "bool_encoder.hh"
#include "model.hh"
#include "numeric.hh"

#include "encoder.cc"
#include "../io/SwitchableCompression.hh"

using namespace std;

CodingReturnValue VP8ComponentEncoder::encode_chunk(const UncompressedComponents *input,
                                                    Sirikata::
                                                    SwitchableCompressionWriter<Sirikata::
                                                                                DecoderCompressionWriter> *output) {
    return vp8_full_encoder(input, output);
}

CodingReturnValue VP8ComponentEncoder::vp8_full_encoder( const UncompressedComponents * const colldata,
                                            Sirikata::
                                            SwitchableCompressionWriter<Sirikata::
                                                                        DecoderCompressionWriter> *str_out)
{
    /* cmpc is a global variable with the component count */
    /* verify JFIF is Y'CbCr only (no grayscale images allowed) */
    if ( cmpc != 3 ) {
        std::cerr << "JPEG/JFIF was not a three-component Y'CbCr image" << std::endl;
        return CODING_ERROR;
    }

    /* construct 8x8 "VP8" blocks to hold 8x8 JPEG blocks */
    vector<Plane<Block>> vp8_blocks;

    vp8_blocks.emplace_back( colldata->block_width( 0 ), colldata->block_height( 0 ), Y );
    vp8_blocks.emplace_back( colldata->block_width( 1 ), colldata->block_height( 1 ), Cb );
    vp8_blocks.emplace_back( colldata->block_width( 2 ), colldata->block_height( 2 ), Cr );
    str_out->EnableCompression();

    /* read in probability table coeff probs */
    ProbabilityTables probability_tables = ProbabilityTables::get_probability_tables();

    /* get ready to serialize the blocks */
    BoolEncoder bool_encoder;
    int jpeg_y[4] = {0};
    int component = 0;
    while(colldata->get_next_component(jpeg_y, &component)) {
        int curr_y = jpeg_y[component];
        int block_width = colldata->block_width( component );
        for ( int jpeg_x = 0; jpeg_x < block_width; jpeg_x++ ) {
            auto & block = vp8_blocks.at( component ).at( jpeg_x, curr_y );

            for ( int coeff = 0; coeff < 64; coeff++ ) {
                block.mutable_coefficients().at( jpeg_zigzag.at( coeff ) )
                    = colldata->at_nosync( component, coeff, curr_y * block_width + jpeg_x);
            }

            block.recalculate_coded_length();
            block.serialize_tokens( bool_encoder, probability_tables );
        }
        ++jpeg_y[component];
    }

    /* get coded output */
    const auto stream = bool_encoder.finish();

    /* write block header */
    str_out->Write( reinterpret_cast<const unsigned char*>("x"), 1 );
    str_out->DisableCompression();
    
    /* write length */
    const uint32_t length_big_endian =
        htobe32( stream.size() );
    str_out->Write( reinterpret_cast<const unsigned char*>(&length_big_endian), sizeof( uint32_t ));

    /* write coded octet stream */
    str_out->Write( &stream.at( 0 ), stream.size() );

    /* possibly write out new probability model */
    const char * out_model_name = getenv( "LEPTON_COMPRESSION_MODEL_OUT" );
    if ( out_model_name ) {
        cerr << "Writing new compression model..." << endl;

        std::ofstream model_file { out_model_name };
        if ( not model_file.good() ) {
            std::cerr << "error writing to " + string( out_model_name ) << std::endl;
            return CODING_ERROR;
        }

        probability_tables.optimize();
        probability_tables.serialize( model_file );
    }
    return CODING_DONE;
}
