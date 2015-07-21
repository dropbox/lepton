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
#include "slice.hh"
#include "../io/SwitchableCompression.hh"
#include "../vp8/model/model.hh"

using namespace std;
void printContext(FILE * fp) {
    for (int cm= 0;cm< 3;++cm) {
        for (int y = 0;y < Context::H/8; ++y) {
            for (int x = 0;x < Context::W/8; ++x) {
                for (int by = 0; by < 8; ++by){
                    for (int bx = 0; bx < 8; ++bx) {
                        for (int ctx = 0;ctx < NUMCONTEXT;++ctx) {
                            for (int dim = 0; dim < 3; ++dim) {
                                int val = gctx->p[cm][y][x][by][bx][ctx][dim];
                                const char *nam = "UNKNOWN";
                                switch (ctx) {
                                  case ZDSTSCAN:nam = "ZDSTSCAN";break;
                                  case ZEROS7x7:nam = "ZEROS7x7";break;
                                  case EXPDC:nam = "EXPDC";break;
                                  case RESDC:nam = "RESDC";break;
                                  case EXP7x7:nam = "EXP7x7";break;
                                  case RES7x7:nam = "RES7x7";break;
                                  case ZEROS1x8:nam = "ZEROS1x8";break;
                                  case ZEROS8x1:nam = "ZEROS8x1";break;
                                  case EXP8:nam = "EXP8";break;
                                  case THRESH8: nam = "THRESH8"; break;
                                  case RES8:nam = "RES8";break;
                                  default:break;
                                }
                                if (val != -1 && ctx != ZDSTSCAN) {
                                    fprintf(fp, "col[%02d] y[%02d]x[%02d] by[%02d]x[%02d] [%s][%d] = %d\n",
                                            cm, y, x, by, bx, nam, dim, val);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

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
            gctx->cur_cmp = component;
            gctx->cur_jpeg_x = jpeg_x;
            gctx->cur_jpeg_y = curr_y;
            if (curr_y == 5 && jpeg_x == 2) {
                fprintf(stderr, "2042\n");
            }
            block.recalculate_coded_length();
            probability_tables.set_quantization_table( colldata->get_quantization_tables(component));
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
#ifdef ANNOTATION_ENABLED
    {
        FILE * fp = fopen("/tmp/lepton.ctx","w");
        printContext(fp);
        fclose(fp);
    }
#endif
    return CODING_DONE;
}
