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
#include "../io/MuxReader.hh"

using namespace std;
void printContext(FILE * fp) {
    for (int cm= 0;cm< 3;++cm) {
        for (int y = 0;y < Context::H/8; ++y) {
            for (int x = 0;x < Context::W/8; ++x) {
                for (int by = 0; by < 8; ++by){
                    for (int bx = 0; bx < 8; ++bx) {
                        for (int ctx = 0;ctx < NUMCONTEXT;++ctx) {
                            for (int dim = 0; dim < 3; ++dim) {
#ifdef ANNOTATION_ENABLED
                                int val = 0;
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
                                                    Sirikata::DecoderWriter *output) {
    return vp8_full_encoder(input, output);
}

template<class Left, class Middle, class Right>
void VP8ComponentEncoder::process_row(ProbabilityTablesBase &pt,
                                      Left & left_model,
                                      Middle& middle_model,
                                      Right& right_model,
                                      int block_width,
                                      const UncompressedComponents * const colldata,
                                      Sirikata::Array1d<KVContext,
                                              (uint32_t)ColorChannel::NumBlockTypes> &context,
                                      Sirikata::Array1d<BoolEncoder, SIMD_WIDTH> &bool_encoder) {
    if (block_width > 0) {
        ConstBlockContext block_context = context.at((int)middle_model.COLOR).context;
        const AlignedBlock &block = block_context.here();
#ifdef ANNOTATION_ENABLED
        gctx->cur_cmp = component; // for debug purposes only, not to be used in production
        gctx->cur_jpeg_x = 0;
        gctx->cur_jpeg_y = curr_y;
#endif
        *block_context.num_nonzeros_here = block.recalculate_coded_length();
        serialize_tokens(block_context,
                         bool_encoder,
                         left_model,
                         pt);
        context.at((int)middle_model.COLOR).context = colldata->full_component_nosync((int)middle_model.COLOR).next(block_context, true);
    }
    for ( int jpeg_x = 1; jpeg_x + 1 < block_width; jpeg_x++ ) {
        ConstBlockContext block_context = context.at((int)middle_model.COLOR).context;
        const AlignedBlock &block = block_context.here();
#ifdef ANNOTATION_ENABLED
        gctx->cur_cmp = component; // for debug purposes only, not to be used in production
        gctx->cur_jpeg_x = jpeg_x;
        gctx->cur_jpeg_y = curr_y;
#endif
        *block_context.num_nonzeros_here = block.recalculate_coded_length();
        serialize_tokens(block_context,
                         bool_encoder,
                         middle_model,
                         pt);
        context.at((int)middle_model.COLOR).context = colldata->full_component_nosync((int)middle_model.COLOR).next(block_context, true);
    }
    if (block_width > 1) {
        ConstBlockContext block_context = context.at((int)middle_model.COLOR).context;
        const AlignedBlock &block = block_context.here();
#ifdef ANNOTATION_ENABLED
        gctx->cur_cmp = middle_model.COLOR; // for debug purposes only, not to be used in production
        gctx->cur_jpeg_x = block_width - 1;
        gctx->cur_jpeg_y = curr_y;
#endif
        *block_context.num_nonzeros_here = block.recalculate_coded_length();
        serialize_tokens(block_context,
                         bool_encoder,
                         right_model,
                         pt);
        context.at((int)middle_model.COLOR).context = colldata->full_component_nosync((int)middle_model.COLOR).next(block_context, false);
    }
}
void pick_luma_splits(const UncompressedComponents *colldata,
                      int luma_splits[NUM_THREADS]) {
    int total = colldata->block_height(0);
    for (int i = 0;i < NUM_THREADS; ++i) {
        luma_splits[i] = std::min((total * (i + 1) + NUM_THREADS / 2) / NUM_THREADS,
                                  total);
    }
    luma_splits[NUM_THREADS - 1] = total; // make sure we're ending at exactly the end
}



tuple<ProbabilityTables<false, false, false, TEMPLATE_ARG_COLOR0>,
ProbabilityTables<false, false, false, TEMPLATE_ARG_COLOR1>,
ProbabilityTables<false, false, false, TEMPLATE_ARG_COLOR2> > corner(BlockType::Y,BlockType::Cb,BlockType::Cr);

tuple<ProbabilityTables<true, false, false, TEMPLATE_ARG_COLOR0>,
ProbabilityTables<true, false, false, TEMPLATE_ARG_COLOR1>,
ProbabilityTables<true, false, false, TEMPLATE_ARG_COLOR2> > top(BlockType::Y,BlockType::Cb,BlockType::Cr);

tuple<ProbabilityTables<false, true, true, TEMPLATE_ARG_COLOR0>,
ProbabilityTables<false, true, true, TEMPLATE_ARG_COLOR1>,
ProbabilityTables<false, true, true, TEMPLATE_ARG_COLOR2> > midleft(BlockType::Y,BlockType::Cb,BlockType::Cr);

tuple<ProbabilityTables<true, true, true, TEMPLATE_ARG_COLOR0>,
ProbabilityTables<true, true, true, TEMPLATE_ARG_COLOR1>,
ProbabilityTables<true, true, true, TEMPLATE_ARG_COLOR2> > middle(BlockType::Y,BlockType::Cb,BlockType::Cr);

tuple<ProbabilityTables<true, true, false, TEMPLATE_ARG_COLOR0>,
ProbabilityTables<true, true, false, TEMPLATE_ARG_COLOR1>,
ProbabilityTables<true, true, false, TEMPLATE_ARG_COLOR2> > midright(BlockType::Y,BlockType::Cb,BlockType::Cr);

tuple<ProbabilityTables<false, true, false, TEMPLATE_ARG_COLOR0>,
ProbabilityTables<false, true, false, TEMPLATE_ARG_COLOR1>,
ProbabilityTables<false, true, false, TEMPLATE_ARG_COLOR2> > width_one(BlockType::Y,BlockType::Cb,BlockType::Cr);

void VP8ComponentEncoder::process_row_range(int thread_id,
                                            const UncompressedComponents * const colldata,
                                            Sirikata::
                                            DecoderWriter *str_out,
                                            int min_y,
                                            int max_y,
                                            std::vector<uint8_t> *streams) {
    using namespace Sirikata;
    Array1d<BoolEncoder, SIMD_WIDTH> bool_encoders;
    Array1d<KVContext, (uint32_t)ColorChannel::NumBlockTypes> context;
    for (size_t i = 0; i < context.size(); ++i) {
        context[i].context = colldata->full_component_nosync(i).begin();
        context[i].y = 0;
    }
    BlockType component = BlockType::Y;
    uint8_t is_top_row[(uint32_t)ColorChannel::NumBlockTypes];
    memset(is_top_row, true, sizeof(is_top_row));
    bool valid_range = false;
    for(;colldata->get_next_component(context, &component); ++context.at((int)component).y) {
        int curr_y = context.at((int)component).y;
        context[(int)component].context = colldata->full_component_nosync((int)component).off_y(curr_y);
        if (component == BlockType::Y) {
            if (curr_y >= min_y) {
                valid_range = true;
            }
            if (curr_y >= max_y && thread_id + 1 != NUM_THREADS) {
                break; // we are out of range
            }
        }
        if (!valid_range) {
            continue; // before range for this thread
        }
        fprintf(stderr,"Thread %d processing %d(%d)\n", thread_id, curr_y, (int)component);
        int block_width = colldata->block_width( component );
        if (is_top_row[(int)component]) {
            is_top_row[(int)component] = false;
            switch(component) {
                case BlockType::Y:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Y>(corner),
                            std::get<(int)BlockType::Y>(top),
                            std::get<(int)BlockType::Y>(top),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                    break;
                case BlockType::Cb:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Cb>(corner),
                            std::get<(int)BlockType::Cb>(top),
                            std::get<(int)BlockType::Cb>(top),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                    break;
                case BlockType::Cr:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Cr>(corner),
                            std::get<(int)BlockType::Cr>(top),
                            std::get<(int)BlockType::Cr>(top),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                    break;
            }
        } else if (block_width > 1) {
            switch(component) {
                case BlockType::Y:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Y>(midleft),
                            std::get<(int)BlockType::Y>(middle),
                            std::get<(int)BlockType::Y>(midright),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                    break;
                case BlockType::Cb:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Cb>(midleft),
                            std::get<(int)BlockType::Cb>(middle),
                            std::get<(int)BlockType::Cb>(midright),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                    break;
                case BlockType::Cr:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Cr>(midleft),
                            std::get<(int)BlockType::Cr>(middle),
                            std::get<(int)BlockType::Cr>(midright),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                    break;
            }
        } else {
            assert(block_width == 1);
            switch(component) {
                case BlockType::Y:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Y>(width_one),
                            std::get<(int)BlockType::Y>(width_one),
                            std::get<(int)BlockType::Y>(width_one),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                    break;
                case BlockType::Cb:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Cb>(width_one),
                            std::get<(int)BlockType::Cb>(width_one),
                            std::get<(int)BlockType::Cb>(width_one),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                break;
                case BlockType::Cr:
                    process_row(model_[thread_id],
                            std::get<(int)BlockType::Cr>(width_one),
                            std::get<(int)BlockType::Cr>(width_one),
                            std::get<(int)BlockType::Cr>(width_one),
                            block_width,
                            colldata,
                            context,
                            bool_encoders);
                    break;
            }
        }
        
    }
    for (int i = 0; i < SIMD_WIDTH; ++i) {
        /* get coded output */
        streams[i] = bool_encoders.at(i).finish();
    }
}


CodingReturnValue VP8ComponentEncoder::vp8_full_encoder( const UncompressedComponents * const colldata,
                                            Sirikata::
                                            DecoderWriter *str_out)
{
    /* cmpc is a global variable with the component count */
    using namespace Sirikata;
    /* get ready to serialize the blocks */
    ProbabilityTablesBase::set_quantization_table(BlockType::Y, colldata->get_quantization_tables(BlockType::Y));
    ProbabilityTablesBase::set_quantization_table(BlockType::Cb, colldata->get_quantization_tables(BlockType::Cb));
    ProbabilityTablesBase::set_quantization_table(BlockType::Cr, colldata->get_quantization_tables(BlockType::Cr));
    for (int i = 0; i < NUM_THREADS; ++i) {
        /* read in probability table coeff probs */
        model_[i].load_probability_tables();
    }
    int luma_splits[NUM_THREADS] = {0};
    pick_luma_splits(colldata, luma_splits);
    std::vector<uint8_t> stream[MuxReader::MAX_STREAM_ID];
    process_row_range(0, colldata, str_out, 0, luma_splits[0], stream);


    static_assert(NUM_THREADS * SIMD_WIDTH <= MuxReader::MAX_STREAM_ID,
                  "Need to have enough mux streams for all threads and simd width");
    for (int i = 1; i < NUM_THREADS;++i) {
        process_row_range(i,
                          colldata, str_out, luma_splits[i - 1], luma_splits[i],
                          stream + i * SIMD_WIDTH);
    }

    fprintf(stderr, "Sizes %ld %ld %ld %ld\n", stream[0].size(), stream[1].size(), stream[2].size(), stream[3].size());
    /* write block header */
    str_out->Write( reinterpret_cast<const unsigned char*>("x"), 1 );

    Sirikata::MuxWriter mux_writer(str_out, JpegAllocator<uint8_t>());
    size_t stream_data_offset[MuxReader::MAX_STREAM_ID] = {0};
    bool any_written = true;
    while (any_written) {
        any_written = false;
        for (int i = 0; i < MuxReader::MAX_STREAM_ID; ++i) {
            if (stream[i].size() > stream_data_offset[i]) {
                any_written = true;
                size_t max_written = 65536;
                if (stream_data_offset == 0) {
                    max_written = 256;
                } else if (stream_data_offset[i] == 256) {
                    max_written = 4096;
                }
                auto to_write = std::min(max_written, stream[i].size() - stream_data_offset[i]);
                stream_data_offset[i] += mux_writer.Write(i, &stream[i][stream_data_offset[i]], to_write).first;
            }
        }
    }
    mux_writer.Close();
    /* possibly write out new probability model */
    const char * out_model_name = getenv( "LEPTON_COMPRESSION_MODEL_OUT" );
    if ( out_model_name ) {
        cerr << "Writing new compression model..." << endl;

        std::ofstream model_file { out_model_name };
        if ( not model_file.good() ) {
            std::cerr << "error writing to " + string( out_model_name ) << std::endl;
            return CODING_ERROR;
        }

        std::get<(int)BlockType::Y>(middle).optimize(model_[0]);
        std::get<(int)BlockType::Y>(middle).serialize(model_[0], model_file );
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
