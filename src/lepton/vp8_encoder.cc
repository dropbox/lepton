/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "../../vp8/util/memory.hh"
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
                                  case SIGN8:nam = "SI#include "emmintrin.h"GN8";break;
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
                                                    IOUtil::FileWriter *output) {
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
                                      BoolEncoder &bool_encoder) {
    if (block_width > 0) {
        ConstBlockContext block_context = context.at((int)middle_model.COLOR).context;
        const AlignedBlock &block = block_context.here();
#ifdef ANNOTATION_ENABLED
        gctx->cur_cmp = component; // for debug purposes only, not to be used in production
        gctx->cur_jpeg_x = 0;
        gctx->cur_jpeg_y = curr_y;
#endif
        block_context.num_nonzeros_here->set_num_nonzeros(block.recalculate_coded_length());
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
        block_context.num_nonzeros_here->set_num_nonzeros(block.recalculate_coded_length()); //FIXME set edge pixels too
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
        block_context.num_nonzeros_here->set_num_nonzeros(block.recalculate_coded_length());
        serialize_tokens(block_context,
                         bool_encoder,
                         right_model,
                         pt);
        context.at((int)middle_model.COLOR).context = colldata->full_component_nosync((int)middle_model.COLOR).next(block_context, false);
    }
}
uint32_t aligned_block_cost(const AlignedBlock &block) {
    uint32_t cost = 16; // .25 cost for zeros
    if (VECTORIZE) {
        for (int i = 0; i < 64; i+= 8) {
            __m128i val = _mm_abs_epi16(_mm_load_si128((const __m128i*)(const char*)(block.raw_data() + i)));
            __m128i v_cost = _mm_set1_epi16(0);
            while (!_mm_test_all_zeros(val, val)) {
                __m128i mask = _mm_cmpgt_epi16(val, _mm_setzero_si128());
                v_cost = _mm_add_epi16(v_cost, _mm_and_si128(mask, _mm_set1_epi16(2)));
                val = _mm_srli_epi16(val, 1);
            }
            __m128i sum = _mm_add_epi16(v_cost, _mm_srli_si128(v_cost, 8));
            sum = _mm_add_epi16(sum ,_mm_srli_si128(sum, 4));
            sum = _mm_add_epi16(sum, _mm_srli_si128(sum, 2));
            cost += _mm_extract_epi16(sum, 0);
        }
    } else {
        uint32_t scost = 0;
        for (int i = 0; i < 64; ++i) {
            scost += 1 + 2 * uint16bit_length(abs(block.raw_data()[i]));
        }
        cost = scost;
    }
    return cost;
}

void pick_luma_splits(const UncompressedComponents *colldata,
                      int luma_splits[NUM_THREADS]) {
    int height = colldata->block_height(0);
    int width = colldata->block_width(0);
    int minheight = height;
    for (int cmp = 1; cmp < colldata->get_num_components(); ++cmp) {
        if (colldata->block_height(cmp) < minheight) {
            minheight = colldata->block_height(cmp);
        }
    }
    int mod_by = height / minheight;
    std::vector<uint32_t> row_costs(height);
    const BlockBasedImage &image = colldata->full_component_nosync(0);
    for (int i = 0; i < height; ++i) {
        uint32_t row_cost = 0;
        for (int j = 0; j < width; ++j) {
            row_cost += aligned_block_cost(image.raster(i * width + j));
        }
        row_costs[i] = row_cost;
        if (i) {
            row_costs[i] += row_costs[i-1];
        }
    }
    for (int i = 0; i < NUM_THREADS;++i) {
        auto split = std::lower_bound(row_costs.begin(), row_costs.end(),
                                      row_costs.back() * (i + 1) / NUM_THREADS);
        luma_splits[i] = split - row_costs.begin();
        if (mod_by == 1 && luma_splits[i] < height) {
            ++luma_splits[i];
        }
        while (luma_splits[i] % mod_by && luma_splits[i] < height) {
            ++luma_splits[i];
        }
    }
    /*
    int last = 0;
    for (int i = 0;i < NUM_THREADS; ++i) {
        luma_splits[i] = std::min((height * (i + 1) + NUM_THREADS / 2) / NUM_THREADS,
                                  height);
        last = luma_splits[i];
    }
    */
    luma_splits[NUM_THREADS - 1] = height; // make sure we're ending at exactly the end
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
                                            int min_y,
                                            int max_y,
                                            std::vector<uint8_t> *stream) {
    using namespace Sirikata;
    BoolEncoder bool_encoder;
    Array1d<KVContext, (uint32_t)ColorChannel::NumBlockTypes> context;
    Array1d<std::vector<NeighborSummary>, (uint32_t)ColorChannel::NumBlockTypes> num_nonzeros;
    for (size_t i = 0; i < num_nonzeros.size(); ++i) {
        num_nonzeros.at(i).resize(colldata->block_width(i) << 1);
    }
    for (size_t i = 0; i < context.size(); ++i) {
        context[i].context = colldata->full_component_nosync(i).begin(num_nonzeros.at(i).begin());
        context[i].y = 0;
    }
    BlockType component = BlockType::Y;
    uint8_t is_top_row[(uint32_t)ColorChannel::NumBlockTypes];
    memset(is_top_row, true, sizeof(is_top_row));
    bool valid_range = false;
    int luma_y = 0;
    for(;colldata->get_next_component(context, &component, &luma_y); ++context.at((int)component).y) {
        int curr_y = context.at((int)component).y;
        context[(int)component].context
            = colldata->full_component_nosync((int)component).off_y(curr_y,
                                                                    num_nonzeros.at((int)component).begin());
        if (luma_y >= min_y) {
            valid_range = true;
        }
        if (luma_y >= max_y && thread_id + 1 != NUM_THREADS) {
            break; // we are out of range
        }
        if (!valid_range) {
            continue; // before range for this thread
        }
        // DEBUG only fprintf(stderr, "Thread %d min_y %d - max_y %d cmp[%d] y = %d\n", thread_id, min_y, max_y, (int)component, curr_y);
        int block_width = colldata->block_width( component );
        if (is_top_row[(int)component]) {
            is_top_row[(int)component] = false;
            switch(component) {
                case BlockType::Y:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Y>(corner),
                            std::get<(int)BlockType::Y>(top),
                            std::get<(int)BlockType::Y>(top),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                    break;
                case BlockType::Cb:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Cb>(corner),
                            std::get<(int)BlockType::Cb>(top),
                            std::get<(int)BlockType::Cb>(top),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                    break;
                case BlockType::Cr:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Cr>(corner),
                            std::get<(int)BlockType::Cr>(top),
                            std::get<(int)BlockType::Cr>(top),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                    break;
            }
        } else if (block_width > 1) {
            switch(component) {
                case BlockType::Y:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Y>(midleft),
                            std::get<(int)BlockType::Y>(middle),
                            std::get<(int)BlockType::Y>(midright),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                    break;
                case BlockType::Cb:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Cb>(midleft),
                            std::get<(int)BlockType::Cb>(middle),
                            std::get<(int)BlockType::Cb>(midright),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                    break;
                case BlockType::Cr:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Cr>(midleft),
                            std::get<(int)BlockType::Cr>(middle),
                            std::get<(int)BlockType::Cr>(midright),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                    break;
            }
        } else {
            assert(block_width == 1);
            switch(component) {
                case BlockType::Y:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Y>(width_one),
                            std::get<(int)BlockType::Y>(width_one),
                            std::get<(int)BlockType::Y>(width_one),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                    break;
                case BlockType::Cb:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Cb>(width_one),
                            std::get<(int)BlockType::Cb>(width_one),
                            std::get<(int)BlockType::Cb>(width_one),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                break;
                case BlockType::Cr:
                    process_row(*model_[thread_id],
                            std::get<(int)BlockType::Cr>(width_one),
                            std::get<(int)BlockType::Cr>(width_one),
                            std::get<(int)BlockType::Cr>(width_one),
                            block_width,
                            colldata,
                            context,
                            bool_encoder);
                    break;
            }
        }
        
    }
    bool_encoder.finish(*stream);
}
VP8ComponentEncoder::VP8ComponentEncoder() {
    for (int i = 0; i < NUM_THREADS; ++i) {
        /* read in probability table coeff probs */
        model_[i] = new ProbabilityTablesBase;
        model_[i]->load_probability_tables();
    }
}

int load_model_file_fd_output() {
    const char * out_model_name = getenv( "LEPTON_COMPRESSION_MODEL_OUT" );
    if (!out_model_name) {
        return -1;
    }
    return open(out_model_name, O_CREAT|O_TRUNC|O_WRONLY, S_IWUSR | S_IRUSR);
}
int model_file_fd = load_model_file_fd_output();

const bool dospin = true;
CodingReturnValue VP8ComponentEncoder::vp8_full_encoder( const UncompressedComponents * const colldata,
                                                         IOUtil::FileWriter *str_out)
{
    /* cmpc is a global variable with the component count */
    using namespace Sirikata;
    /* get ready to serialize the blocks */
    ProbabilityTablesBase::set_quantization_table(BlockType::Y, colldata->get_quantization_tables(BlockType::Y));
    ProbabilityTablesBase::set_quantization_table(BlockType::Cb, colldata->get_quantization_tables(BlockType::Cb));
    ProbabilityTablesBase::set_quantization_table(BlockType::Cr, colldata->get_quantization_tables(BlockType::Cr));
    int luma_splits[NUM_THREADS] = {0};
    pick_luma_splits(colldata, luma_splits);
    
    std::vector<uint8_t>* stream[MuxReader::MAX_STREAM_ID];
    for (int i = 0 ; i < MuxReader::MAX_STREAM_ID; ++i) {
        stream[i] = new std::vector<uint8_t>(); // allocate streams as pointers so threads don't modify them inline
    }
    std::thread*workers[4] = {};
    if(!do_threading_) { // single threading
        for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            process_row_range(thread_id,
                              colldata, luma_splits[thread_id - 1], luma_splits[thread_id],
                              stream[thread_id]);
        }
    } else {
        for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            if (dospin) {
                spin_workers_.at(thread_id - 1).work
                    = std::bind(&VP8ComponentEncoder::process_row_range, this,
                                thread_id,
                                colldata,
                                luma_splits[thread_id - 1], luma_splits[thread_id],
                                stream[thread_id]);
                spin_workers_.at(thread_id - 1).activate_work();
            } else {
                workers[thread_id]
                    = new std::thread(std::bind(&VP8ComponentEncoder::process_row_range, this,
                                                thread_id,
                                                colldata,
                                                luma_splits[thread_id - 1], luma_splits[thread_id],
                                                stream[thread_id]));
            }
        }
    }
    process_row_range(0, colldata, 0, luma_splits[0], stream[0]);


    static_assert(NUM_THREADS * SIMD_WIDTH <= MuxReader::MAX_STREAM_ID,
                  "Need to have enough mux streams for all threads and simd width");

    /* write block header */
    uint8_t thread_splits[1 + NUM_THREADS * 2 - 2];
    thread_splits[0] = NUM_THREADS;
    for (int i = 0; i + 1 < NUM_THREADS; ++i) {
        thread_splits[i * 2 + 1] = (luma_splits[i] & 255);
        thread_splits[i * 2 + 2] = (luma_splits[i] >> 8);
        assert((luma_splits[i] >> 16) == 0 && "We only support jpegs 65536 tall or less--which complies with the spec");
    } // the last thread is expected to cover the rest
    str_out->Write(thread_splits, sizeof(thread_splits));
    if (do_threading_) {
        for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            if (dospin) {
                spin_workers_.at(thread_id - 1).main_wait_for_done();
            } else {
                workers[thread_id]->join();
                delete workers[thread_id];
                workers[thread_id] = NULL;
            }
        }
    }
    Sirikata::MuxWriter mux_writer(str_out, JpegAllocator<uint8_t>());
    size_t stream_data_offset[MuxReader::MAX_STREAM_ID] = {0};
    bool any_written = true;
    while (any_written) {
        any_written = false;
        for (int i = 0; i < MuxReader::MAX_STREAM_ID; ++i) {
            if (stream[i]->size() > stream_data_offset[i]) {
                any_written = true;
                size_t max_written = 65536;
                if (stream_data_offset == 0) {
                    max_written = 256;
                } else if (stream_data_offset[i] == 256) {
                    max_written = 4096;
                }
                auto to_write = std::min(max_written, stream[i]->size() - stream_data_offset[i]);
                stream_data_offset[i] += mux_writer.Write(i, &(*stream[i])[stream_data_offset[i]], to_write).first;
            }
        }
    }
    mux_writer.Close();
    // we can probably exit(0) here
    for (int i = 0 ; i < MuxReader::MAX_STREAM_ID; ++i) {
        delete stream[i]; // allocate streams as pointers so threads don't modify them inline
    }
    /* possibly write out new probability model */
    {
        uint32_t out_file_size = str_out->getsize() + 4; // gotta include the final uint32_t
        uint32_t file_size = out_file_size;
        uint8_t out_buffer[sizeof(out_file_size)] = {};
        for (uint8_t i = 0; i < sizeof(out_file_size); ++i) {
            out_buffer[i] = out_file_size & 0xff;
            out_file_size >>= 8;
        }
        str_out->Write(out_buffer, sizeof(out_file_size));
        (void)file_size;
        assert(str_out->getsize() == file_size);
    }
    
    if ( model_file_fd >= 0 ) {
        const char * msg = "Writing new compression model...\n";
        while (write(2, msg, strlen(msg)) < 0 && errno == EINTR){}

        std::get<(int)BlockType::Y>(middle).optimize(*model_[0]);
        std::get<(int)BlockType::Y>(middle).serialize(*model_[0], model_file_fd );
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
