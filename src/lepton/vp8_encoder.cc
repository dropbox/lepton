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
#include "../vp8/encoder/encoder.hh"

using namespace std;
void printContext(FILE * fp) {
    for (int cm= 0;cm< 3;++cm) {
        for (int y = 0;y < Context::H/8; ++y) {
            for (int x = 0;x < Context::W/8; ++x) {
                for (int by = 0; by < 8; ++by){
                    for (int bx = 0; bx < 8; ++bx) {
                        for (int ctx = 0;ctx < NUMCONTEXT;++ctx) {
                            for (int dim = 0; dim < 3; ++dim) {
                                int val = 0;
#ifdef ANNOTATION_ENABLED
                                val = gctx->p[cm][y][x][by][bx][ctx][dim];
                                const char *nam = "UNKNOWN";
                                switch (ctx) {
                                  case ZDSTSCAN:nam = "ZDSTSCAN";break;
                                  case ZEROS7x7:nam = "ZEROS7x7";break;
                                  case EXPDC:nam = "EXPDC";break;
                                  case RESDC:nam = "RESDC";break;
                                  case SIGNDC:nam = "SIGNDC";break;
                                  case EXP7x7:nam = "EXP7x7";break;
                                  case RES7x7:nam = "RES7x7";break;
                                  case SIGN7x7:nam = "SIGN7x7";break;
                                  case ZEROS1x8:nam = "ZEROS1x8";break;
                                  case ZEROS8x1:nam = "ZEROS8x1";break;
                                  case EXP8:nam = "EXP8";break;
                                  case THRESH8: nam = "THRESH8"; break;
                                  case RES8:nam = "RES8";break;
                                  case SIGN8:nam = "SIGN8";break;
                                  default:break;
                                }
                                if (val != -1 && ctx != ZDSTSCAN) {
                                    fprintf(fp, "col[%02d] y[%02d]x[%02d] by[%02d]x[%02d] [%s][%d] = %d\n",
                                            cm, y, x, by, bx, nam, dim, val);
                                }
#endif
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
    using namespace Sirikata;
    /* construct 8x8 "VP8" blocks to hold 8x8 JPEG blocks */
    Array1d<BlockBasedImage, (uint32_t)ColorChannel::NumBlockTypes>  vp8_blocks;
    Array1d<VContext, (uint32_t)ColorChannel::NumBlockTypes> context;
    for (size_t i = 0; i < vp8_blocks.size(); ++i) {
        vp8_blocks.at(i).init( colldata->block_width( i ), colldata->block_height( i ),
                               colldata->block_width( i ) * colldata->block_height( i ) );
    }
    for (size_t i = 0; i < context.size(); ++i) {
        context[i].context = vp8_blocks[i].begin();
        context[i].y = 0;
    }
    str_out->EnableCompression();

    /* read in probability table coeff probs */
    ProbabilityTables probability_tables = ProbabilityTables::get_probability_tables();

    /* get ready to serialize the blocks */
    BoolEncoder bool_encoder;
    int component = 0;
    while(colldata->get_next_component(context, &component)) {
        int curr_y = context.at(component).y;
        int block_width = colldata->block_width( component );
        for ( int jpeg_x = 0; jpeg_x < block_width; jpeg_x++ ) {
            BlockContext block_context = context.at(component).context;
            AlignedBlock &block = block_context.here();
            for ( int coeff = 0; coeff < 64; coeff++ ) {
                block.mutable_coefficients().raster( jpeg_zigzag.at( coeff ) )
                    = colldata->at_nosync( component, coeff, curr_y * block_width + jpeg_x);
            }
#ifdef ANNOTATION_ENABLED
            gctx->cur_cmp = component; // for debug purposes only, not to be used in production
            gctx->cur_jpeg_x = jpeg_x;
            gctx->cur_jpeg_y = curr_y;
#endif
            block.recalculate_coded_length();
            probability_tables.set_quantization_table( colldata->get_quantization_tables(component));
            serialize_tokens(block_context,
                             get_color_context_blocks(colldata->get_color_context(jpeg_x,
                                                                                  context,
                                                                                  component),
                                                      vp8_blocks),
                             bool_encoder,
                             probability_tables);

            context.at(component).context = vp8_blocks.at(component).next(block_context);
        }
        ++context.at(component).y;
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
