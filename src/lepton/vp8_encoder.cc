/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "../../vp8/util/memory.hh"
#include <string>
#include <cassert>
#include <iostream>
#include <fstream>
#ifdef _WIN32
#include <fcntl.h>
#endif
#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_encoder.hh"

#include "bool_encoder.hh"
#include "model.hh"
#include "numeric.hh"

#include "../vp8/model/model.hh"
#include "../vp8/encoder/encoder.hh"
#include "../io/MuxReader.hh"
#include "lepton_codec.hh"
extern unsigned char ujgversion;
using namespace std;
typedef Sirikata::MuxReader::ResizableByteBuffer ResizableByteBuffer;
void printContext(FILE * fp) {
#ifdef ANNOTATION_ENABLED
    for (int cm= 0;cm< 3;++cm) {
        for (int y = 0;y < Context::H/8; ++y) {
            for (int x = 0;x < Context::W/8; ++x) {
                for (int by = 0; by < 8; ++by){
                    for (int bx = 0; bx < 8; ++bx) {
                        for (int ctx = 0;ctx < NUMCONTEXT;++ctx) {
                            for (int dim = 0; dim < 3; ++dim) {
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
                            }
                        }
                    }
                }
            }
        }
    }
#endif
}

template <class ArithmeticCoder>
VP8ComponentEncoder<ArithmeticCoder>::VP8ComponentEncoder(bool do_threading, bool use_ans_encoder)
    : LeptonCodec<ArithmeticCoder>(do_threading){
    this->mUseAnsEncoder = use_ans_encoder;
}
template <class ArithmeticCoder>
CodingReturnValue VP8ComponentEncoder<ArithmeticCoder>::encode_chunk(const UncompressedComponents *input,
                                                                     IOUtil::FileWriter *output,
                                                                     const ThreadHandoff *selected_splits,
                                                                     unsigned int num_selected_splits)
{
    return vp8_full_encoder(input, output, selected_splits, num_selected_splits, this->mUseAnsEncoder);
}
template <class ArithmeticCoder>
template<class Left, class Middle, class Right, class BoolEncoder>
void VP8ComponentEncoder<ArithmeticCoder>::process_row(ProbabilityTablesBase &pt,
                                      Left & left_model,
                                      Middle& middle_model,
                                      Right& right_model,
                                      int curr_y,
                                      const UncompressedComponents * const colldata,
                                      Sirikata::Array1d<ConstBlockContext,
                                              (uint32_t)ColorChannel::NumBlockTypes> &context,
                                      BoolEncoder &bool_encoder) {
    uint32_t block_width = colldata->full_component_nosync((int)middle_model.COLOR).block_width();
    if (block_width > 0) {
        ConstBlockContext state = context.at((int)middle_model.COLOR);
        const AlignedBlock &block = state.here();
#ifdef ANNOTATION_ENABLED
        gctx->cur_cmp = component; // for debug purposes only, not to be used in production
        gctx->cur_jpeg_x = 0;
        gctx->cur_jpeg_y = curr_y;
#endif
        state.num_nonzeros_here->set_num_nonzeros(block.recalculate_coded_length());
        serialize_tokens(state,
                         bool_encoder,
                         left_model,
                         pt);
        uint32_t offset = colldata->full_component_nosync((int)middle_model.COLOR).next(state,
                                                                                        true,
                                                                                        curr_y);
        context.at((int)middle_model.COLOR) = state;
        if (offset >= colldata->component_size_in_blocks(middle_model.COLOR)) {
            return;
        }
        
    }
    for ( unsigned int jpeg_x = 1; jpeg_x + 1 < block_width; jpeg_x++ ) {
        ConstBlockContext state = context.at((int)middle_model.COLOR);
        const AlignedBlock &block = state.here();
#ifdef ANNOTATION_ENABLED
        gctx->cur_cmp = component; // for debug purposes only, not to be used in production
        gctx->cur_jpeg_x = jpeg_x;
        gctx->cur_jpeg_y = curr_y;
#endif
        state.num_nonzeros_here->set_num_nonzeros(block.recalculate_coded_length()); //FIXME set edge pixels too
        serialize_tokens(state,
                         bool_encoder,
                         middle_model,
                         pt);
        uint32_t offset = colldata->full_component_nosync((int)middle_model.COLOR).next(state,
                                                                                        true,
                                                                                        curr_y);
        context.at((int)middle_model.COLOR) = state;
        if (offset >= colldata->component_size_in_blocks(middle_model.COLOR)) {
            return;
        }
    }
    if (block_width > 1) {
        ConstBlockContext state = context.at((int)middle_model.COLOR);
        const AlignedBlock &block = state.here();
#ifdef ANNOTATION_ENABLED
        gctx->cur_cmp = middle_model.COLOR; // for debug purposes only, not to be used in production
        gctx->cur_jpeg_x = block_width - 1;
        gctx->cur_jpeg_y = curr_y;
#endif
        state.num_nonzeros_here->set_num_nonzeros(block.recalculate_coded_length());
        serialize_tokens(state,
                         bool_encoder,
                         right_model,
                         pt);
        colldata->full_component_nosync((int)middle_model.COLOR).next(state, false, curr_y);
        context.at((int)middle_model.COLOR) = state;
    }
}

uint32_t aligned_block_cost_scalar(const AlignedBlock &block) {
    uint32_t scost = 0;
    for (int i = 0; i < 64; ++i) {
        scost += 1 + 2 * uint16bit_length(abs(block.raw_data()[i]));
    }
    return scost;
}

uint32_t aligned_block_cost(const AlignedBlock &block) {
#if defined(__SSE2__) && !defined(USE_SCALAR) /* SSE2 or higher instruction set available { */
    const __m128i zero = _mm_setzero_si128();
     __m128i v_cost;
    for (int i = 0; i < 64; i+= 8) {
        __m128i val = _mm_abs_epi16(_mm_load_si128((const __m128i*)(const char*)(block.raw_data() + i)));
        v_cost = _mm_set1_epi16(0);
#ifndef __SSE4_1__
        while (_mm_movemask_epi8(_mm_cmpeq_epi32(val, zero)) != 0xFFFF)
#else
        while (!_mm_test_all_zeros(val, val))
#endif
        {
            __m128i mask = _mm_cmpgt_epi16(val, zero);
            v_cost = _mm_add_epi16(v_cost, _mm_and_si128(mask, _mm_set1_epi16(2)));
            val = _mm_srli_epi16(val, 1);
        }
        v_cost = _mm_add_epi16(v_cost, _mm_srli_si128(v_cost, 8));
        v_cost = _mm_add_epi16(v_cost ,_mm_srli_si128(v_cost, 4));
        v_cost = _mm_add_epi16(v_cost, _mm_srli_si128(v_cost, 2));
    }
    return 16 + _mm_extract_epi16(v_cost, 0);
#else /* } No SSE2 instructions { */
    return aligned_block_cost_scalar(block);
#endif /* } */
}

#ifdef ALLOW_FOUR_COLORS
#define ProbabilityTablesTuple(left, above, right) \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR0>, \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR1>, \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR2>, \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR3>
#define EACH_BLOCK_TYPE(left, above, right) ProbabilityTables<left&&above&&right, TEMPLATE_ARG_COLOR0>(BlockType::Y, \
                                                                                   left, \
                                                                                   above, \
                                                                                   right), \
                        ProbabilityTables<left&&above&&right, TEMPLATE_ARG_COLOR1>(BlockType::Cb, \
                                                                                   left, \
                                                                                   above, \
                                                                                   right), \
                        ProbabilityTables<left&&above&&right, TEMPLATE_ARG_COLOR2>(BlockType::Cr, \
                                                                                   left, \
                                                                                   above, \
                                                                                   right), \
                        ProbabilityTables<left&&above&&right, TEMPLATE_ARG_COLOR3>(BlockType::Ck, \
                                                                                   left, \
                                                                                   above, \
                                                                                   right)
#else
#define ProbabilityTablesTuple(left, above, right) \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR0>, \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR1>, \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR2>
#define EACH_BLOCK_TYPE(left, above, right) ProbabilityTables<left&&above&&right, TEMPLATE_ARG_COLOR0>(BlockType::Y, \
                                                                                   left, \
                                                                                   above, \
                                                                                   right), \
                        ProbabilityTables<left&&above&&right, TEMPLATE_ARG_COLOR1>(BlockType::Cb, \
                                                                                   left, \
                                                                                   above, \
                                                                                   right), \
                        ProbabilityTables<left&&above&&right, TEMPLATE_ARG_COLOR2>(BlockType::Cr, \
                                                                                   left, \
                                                                                   above, \
                                                                                   right)
#endif

tuple<ProbabilityTablesTuple(false, false, false)> corner(EACH_BLOCK_TYPE(false,false,false));
tuple<ProbabilityTablesTuple(true, false, false)> top(EACH_BLOCK_TYPE(true, false, false));
tuple<ProbabilityTablesTuple(false, true, true)> midleft(EACH_BLOCK_TYPE(false, true, true));
tuple<ProbabilityTablesTuple(true, true, true)> middle(EACH_BLOCK_TYPE(true, true, true));
tuple<ProbabilityTablesTuple(true, true, false)> midright(EACH_BLOCK_TYPE(true, true, false));
tuple<ProbabilityTablesTuple(false, true, false)> width_one(EACH_BLOCK_TYPE(false, true, false));

template <class ArithmeticCoder> template <class BoolEncoder>
void VP8ComponentEncoder<ArithmeticCoder>::process_row_range(unsigned int thread_id,
                                            const UncompressedComponents * const colldata,
                                            int min_y,
                                            int max_y,
                                            ResizableByteBuffer *stream,
                                            BoolEncoder *bool_encoder,
                                            Sirikata::Array1d<std::vector<NeighborSummary>,
                                                              (uint32_t)ColorChannel::NumBlockTypes
                                                              > *num_nonzeros) {

    TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_STARTED] = TimingHarness::get_time_us();
    using namespace Sirikata;
    Array1d<ConstBlockContext, (uint32_t)ColorChannel::NumBlockTypes> context;
    for (size_t i = 0; i < context.size(); ++i) {
        context[i] = colldata->full_component_nosync(i).begin(num_nonzeros->at(i).begin());
    }
    uint8_t is_top_row[(uint32_t)ColorChannel::NumBlockTypes];
    memset(is_top_row, true, sizeof(is_top_row));
    ProbabilityTablesBase *model = nullptr;
    if (this->do_threading_) {
        LeptonCodec<ArithmeticCoder>::reset_thread_model_state(thread_id);
        model = &this->thread_state_[thread_id]->model_;
    } else {
        LeptonCodec<ArithmeticCoder>::reset_thread_model_state(0);
        model = &this->thread_state_[0]->model_;
    }
    KBlockBasedImagePerChannel<false> image_data;
    for (int i = 0; i < colldata->get_num_components(); ++i) {
        image_data[i] = &colldata->full_component_nosync((int)i);
    }
    uint32_t encode_index = 0;
    Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> max_coded_heights = colldata->get_max_coded_heights();
    while(true) {
        LeptonCodec_RowSpec cur_row = LeptonCodec_row_spec_from_index(encode_index++,
                                              image_data,
                                              colldata->get_mcu_count_vertical(),
                                              max_coded_heights);
        if(cur_row.done) {
            break;
        }
        if (cur_row.luma_y >= max_y && thread_id + 1 != NUM_THREADS) {
            break;
        }
        if (cur_row.skip) {
            continue;
        }
        if (cur_row.luma_y < min_y) {
            continue;
        }
        context[cur_row.component]
            = image_data.at(cur_row.component)->off_y(cur_row.curr_y,
                                                      num_nonzeros->at(cur_row.component).begin());
        // DEBUG only fprintf(stderr, "Thread %d min_y %d - max_y %d cmp[%d] y = %d\n", thread_id, min_y, max_y, (int)component, curr_y);
        int block_width = image_data.at(cur_row.component)->block_width();
        if (is_top_row[cur_row.component]) {
            is_top_row[cur_row.component] = false;
            switch((BlockType)cur_row.component) {
                case BlockType::Y:
                    process_row(*model,
                            std::get<(int)BlockType::Y>(corner),
                            std::get<(int)BlockType::Y>(top),
                            std::get<(int)BlockType::Y>(top),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
                case BlockType::Cb:
                    process_row(*model,
                            std::get<(int)BlockType::Cb>(corner),
                            std::get<(int)BlockType::Cb>(top),
                            std::get<(int)BlockType::Cb>(top),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
                case BlockType::Cr:
                    process_row(*model,
                            std::get<(int)BlockType::Cr>(corner),
                            std::get<(int)BlockType::Cr>(top),
                            std::get<(int)BlockType::Cr>(top),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
#ifdef ALLOW_FOUR_COLORS
                case BlockType::Ck:
                    process_row(*model,
                            std::get<(int)BlockType::Ck>(corner),
                            std::get<(int)BlockType::Ck>(top),
                            std::get<(int)BlockType::Ck>(top),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
#endif
            }
        } else if (block_width > 1) {
            switch((BlockType)cur_row.component) {
                case BlockType::Y:
                    process_row(*model,
                            std::get<(int)BlockType::Y>(midleft),
                            std::get<(int)BlockType::Y>(middle),
                            std::get<(int)BlockType::Y>(midright),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
                case BlockType::Cb:
                    process_row(*model,
                            std::get<(int)BlockType::Cb>(midleft),
                            std::get<(int)BlockType::Cb>(middle),
                            std::get<(int)BlockType::Cb>(midright),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
                case BlockType::Cr:
                    process_row(*model,
                            std::get<(int)BlockType::Cr>(midleft),
                            std::get<(int)BlockType::Cr>(middle),
                            std::get<(int)BlockType::Cr>(midright),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
#ifdef ALLOW_FOUR_COLORS
                case BlockType::Ck:
                    process_row(*model,
                            std::get<(int)BlockType::Ck>(midleft),
                            std::get<(int)BlockType::Ck>(middle),
                            std::get<(int)BlockType::Ck>(midright),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
#endif
            }
        } else {
            always_assert(block_width == 1);
            switch((BlockType)cur_row.component) {
                case BlockType::Y:
                    process_row(*model,
                            std::get<(int)BlockType::Y>(width_one),
                            std::get<(int)BlockType::Y>(width_one),
                            std::get<(int)BlockType::Y>(width_one),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
                case BlockType::Cb:
                    process_row(*model,
                            std::get<(int)BlockType::Cb>(width_one),
                            std::get<(int)BlockType::Cb>(width_one),
                            std::get<(int)BlockType::Cb>(width_one),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                break;
                case BlockType::Cr:
                    process_row(*model,
                            std::get<(int)BlockType::Cr>(width_one),
                            std::get<(int)BlockType::Cr>(width_one),
                            std::get<(int)BlockType::Cr>(width_one),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
#ifdef ALLOW_FOUR_COLORS
                case BlockType::Ck:
                    process_row(*model,
                            std::get<(int)BlockType::Ck>(width_one),
                            std::get<(int)BlockType::Ck>(width_one),
                            std::get<(int)BlockType::Ck>(width_one),
                            cur_row.curr_y,
                            colldata,
                            context,
                            *bool_encoder);
                    break;
#endif
            }
        }
    }
    LeptonCodec_RowSpec test = ::LeptonCodec_row_spec_from_index(encode_index,
                                       image_data,
                                       colldata->get_mcu_count_vertical(),
                                       max_coded_heights);
    
    if (thread_id == NUM_THREADS - 1 && (test.skip == false || test.done == false)) {
        fprintf(stderr, "Row spec test: cmp %d luma %d item %d skip %d done %d\n",
                test.component, test.luma_y, test.curr_y, test.skip, test.done);
        custom_exit(ExitCode::ASSERTION_FAILURE);
    }
    bool_encoder->finish(*stream);
    TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_FINISHED] = TimingHarness::get_time_us();
}

int load_model_file_fd_output() {
    const char * out_model_name = getenv( "LEPTON_COMPRESSION_MODEL_OUT" );
    if (!out_model_name) {
        return -1;
    }
    return open(out_model_name, O_CREAT|O_TRUNC|O_WRONLY, 0
#ifndef _WIN32
        |S_IWUSR | S_IRUSR
#endif
    );
}
int model_file_fd = load_model_file_fd_output();

template <class BoolDecoder>
template<class BoolEncoder> void VP8ComponentEncoder<BoolDecoder>::threaded_encode_inner(const UncompressedComponents * const colldata,
                                                                               IOUtil::FileWriter *str_out,
                                                                               const ThreadHandoff * selected_splits,
                                                                               unsigned int num_selected_splits,
                                                                               BoolEncoder bool_encoder[MAX_NUM_THREADS],
                                                                               ResizableByteBuffer stream[Sirikata::MuxReader::MAX_STREAM_ID]) {
    using namespace Sirikata;
    Array1d<std::vector<NeighborSummary>,
            (uint32_t)ColorChannel::NumBlockTypes> num_nonzeros[MAX_NUM_THREADS];
    for (unsigned int thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
        bool_encoder[thread_id].init();
        for (size_t i = 0; i < num_nonzeros[thread_id].size(); ++i) {
            num_nonzeros[thread_id].at(i).resize(colldata->block_width(i) << 1);
        }
    }
    
    if (this->do_threading()) {
        for (unsigned int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            this->spin_workers_[thread_id - 1].work
                = std::bind(&VP8ComponentEncoder<BoolDecoder>::process_row_range<BoolEncoder>, this,
                            thread_id,
                            colldata,
                            selected_splits[thread_id].luma_y_start,
                            selected_splits[thread_id].luma_y_end,
                            &stream[thread_id],
                            &bool_encoder[thread_id],
                            &num_nonzeros[thread_id]);
            this->spin_workers_[thread_id - 1].activate_work();
        }
    }
    process_row_range(0,
                          colldata,
                      selected_splits[0].luma_y_start,
                      selected_splits[0].luma_y_end,
                      &stream[0],
                      &bool_encoder[0],
                      &num_nonzeros[0]);
    if(!this->do_threading()) { // single threading
        for (unsigned int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            process_row_range(thread_id,
                              colldata,
                              selected_splits[thread_id].luma_y_start,
                              selected_splits[thread_id].luma_y_end,
                              &stream[thread_id],
                                  &bool_encoder[thread_id],
                              &num_nonzeros[thread_id]);
        }
    }
    static_assert(MAX_NUM_THREADS * SIMD_WIDTH <= MuxReader::MAX_STREAM_ID,
                  "Need to have enough mux streams for all threads and simd width");
    
    if (this->do_threading()) {
        for (unsigned int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            TimingHarness::timing[thread_id][TimingHarness::TS_THREAD_WAIT_STARTED] = TimingHarness::get_time_us();
            this->spin_workers_[thread_id - 1].main_wait_for_done();
            TimingHarness::timing[thread_id][TimingHarness::TS_THREAD_WAIT_FINISHED] = TimingHarness::get_time_us();
        }
    }
}

template<class BoolDecoder>
CodingReturnValue VP8ComponentEncoder<BoolDecoder>::vp8_full_encoder(const UncompressedComponents * const colldata,
                                                                     IOUtil::FileWriter *str_out,
                                                                     const ThreadHandoff * selected_splits,
                                                                     unsigned int num_selected_splits,
                                                                     bool use_ans_encoder)
{
    /* cmpc is a global variable with the component count */
    using namespace Sirikata;
    /* get ready to serialize the blocks */
    if (colldata->get_num_components() > (int)BlockType::Y) {
        ProbabilityTablesBase::set_quantization_table(BlockType::Y,
                                                      colldata->get_quantization_tables(BlockType::Y));
    }
    if (colldata->get_num_components() > (int)BlockType::Cb) {
        ProbabilityTablesBase::set_quantization_table(BlockType::Cb,
                                                      colldata->get_quantization_tables(BlockType::Cb));
    }
    if (colldata->get_num_components() > (int)BlockType::Cr) {
        ProbabilityTablesBase::set_quantization_table(BlockType::Cr,
                                                      colldata->get_quantization_tables(BlockType::Cr));
    }
#ifdef ALLOW_FOUR_COLORS
    if (colldata->get_num_components() > (int)BlockType::Ck) {
        ProbabilityTablesBase::set_quantization_table(BlockType::Ck,
                                                      colldata->get_quantization_tables(BlockType::Ck));
    }
#endif
    
    ResizableByteBuffer stream[MuxReader::MAX_STREAM_ID];
    if (use_ans_encoder) {
#ifdef ENABLE_ANS_EXPERIMENTAL
        ANSBoolWriter bool_encoder[MAX_NUM_THREADS];
        this->threaded_encode_inner(colldata,
                                    str_out,
                                    selected_splits,
                                    num_selected_splits,
                                    bool_encoder,
                                    stream);
#else
        always_assert(false && "Need to enable ANS compile flag to include ANS");
#endif
    } else {
    
        VPXBoolWriter bool_encoder[MAX_NUM_THREADS];
        this->threaded_encode_inner(colldata,
                                    str_out,
                                    selected_splits,
                                    num_selected_splits,
                                    bool_encoder,
                                    stream);
    }
    TimingHarness::timing[0][TimingHarness::TS_STREAM_MULTIPLEX_STARTED] = TimingHarness::get_time_us();

    Sirikata::MuxWriter mux_writer(str_out, JpegAllocator<uint8_t>(), ujgversion);
    size_t stream_data_offset[MuxReader::MAX_STREAM_ID] = {0};
    bool any_written = true;
    while (any_written) {
        any_written = false;
        for (int i = 0; i < MuxReader::MAX_STREAM_ID; ++i) {
            if (stream[i].size() > stream_data_offset[i]) {
                any_written = true;
                size_t max_written = 65536;
                if (stream_data_offset[i] == 0) {
                    max_written = 256;
                } else if (stream_data_offset[i] == 256) {
                    max_written = 4096;
                }
                auto to_write = std::min(max_written, stream[i].size() - stream_data_offset[i]);
                stream_data_offset[i] += mux_writer.Write(i, &(stream[i])[stream_data_offset[i]], to_write).first;
            }
        }
    }
    mux_writer.Close();
    write_byte_bill(Billing::DELIMITERS, true, mux_writer.getOverhead());
    // we can probably exit(0) here
    TimingHarness::timing[0][TimingHarness::TS_STREAM_MULTIPLEX_FINISHED] =
        TimingHarness::timing[0][TimingHarness::TS_STREAM_FLUSH_STARTED] = TimingHarness::get_time_us();
    check_decompression_memory_bound_ok(); // this has to happen before last
    // bytes are written
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
        write_byte_bill(Billing::HEADER, true, sizeof(out_file_size));
        (void)file_size;
        always_assert(str_out->getsize() == file_size);
    }
    
    if ( model_file_fd >= 0 ) {
        const char * msg = "Writing new compression model...\n";
        while (write(2, msg, strlen(msg)) < 0 && errno == EINTR){}

        std::get<(int)BlockType::Y>(middle).optimize(this->thread_state_[0]->model_);
        std::get<(int)BlockType::Y>(middle).serialize(this->thread_state_[0]->model_, model_file_fd );
    }
#ifdef ANNOTATION_ENABLED
    {
        FILE * fp = fopen("/tmp/lepton.ctx","w");
        printContext(fp);
        fclose(fp);
    }
#endif
    TimingHarness::timing[0][TimingHarness::TS_STREAM_FLUSH_FINISHED] = TimingHarness::get_time_us();
    return CODING_DONE;
}
template class VP8ComponentEncoder<VPXBoolReader>;
#ifdef ENABLE_ANS_EXPERIMENTAL
template class VP8ComponentEncoder<ANSBoolReader>;
#endif
