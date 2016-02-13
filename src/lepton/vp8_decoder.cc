/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <algorithm>
#include <cassert>
#include <iostream>
#include <tuple>

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_decoder.hh"

#include "../io/Reader.hh"
#include "../vp8/decoder/decoder.hh"
using namespace std;

void VP8ComponentDecoder::initialize( Sirikata::DecoderReader *input)
{
    str_in = input;
    mux_reader_.init(input);
}



VP8ComponentDecoder::VP8ComponentDecoder(bool do_threading)
    : VP8ComponentEncoder(do_threading),
      mux_reader_(Sirikata::JpegAllocator<uint8_t>(),
                  4,
                  1024 * 1024 + 262144) {
    if (do_threading) {
        virtual_thread_id_ = -1; // only using real threads here
    } else {
        virtual_thread_id_ = 0;
    }
}

VP8ComponentDecoder::~VP8ComponentDecoder() {
}

const bool dospin = true;
#ifdef ALLOW_FOUR_COLORS
#define ProbabilityTablesTuple(left, above, right) \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR0>, \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR1>, \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR2>, \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR3>
#define EACH_BLOCK_TYPE BlockType::Y, \
                        BlockType::Cb, \
                        BlockType::Cr, \
                        BlockType::Ck
#else
#define ProbabilityTablesTuple(left, above, right) \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR0>, \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR1>, \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR2>
#define EACH_BLOCK_TYPE BlockType::Y, \
                        BlockType::Cb, \
                        BlockType::Cr
#endif

void VP8ComponentDecoder::worker_thread(ThreadState *ts, int thread_id, UncompressedComponents * const colldata) {
    TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_STARTED] = TimingHarness::get_time_us();
    while (ts->vp8_decode_thread(thread_id, colldata) == CODING_PARTIAL) {
    }
    TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_FINISHED] = TimingHarness::get_time_us();
}
template <bool force_memory_optimized>
void VP8ComponentDecoder::initialize_thread_id(int thread_id, int target_thread_state,
                                               BlockBasedImagePerChannel<force_memory_optimized>& framebuffer) {
    TimingHarness::timing[thread_id][TimingHarness::TS_STREAM_MULTIPLEX_STARTED] = TimingHarness::get_time_us();
    if (thread_id != target_thread_state) {
        reset_thread_model_state(target_thread_state);
    }
    for (int i = 0; i < framebuffer.size(); ++i) {
        if (framebuffer[i] != NULL)  {
            thread_state_[target_thread_state]->is_top_row_.at(i) = true;
            thread_state_[target_thread_state]->num_nonzeros_.at(i).resize(framebuffer[i]->block_width() << 1);
            thread_state_[target_thread_state]->context_.at(i).context
                = framebuffer[i]->begin(thread_state_[target_thread_state]->num_nonzeros_.at(i).begin());
            thread_state_[target_thread_state]->context_.at(i).y_deprecated = 0;
        }
    }
    /* initialize the bool decoder */
    int index = thread_id;
    thread_state_[target_thread_state]->bool_decoder_.init(streams_[index].first != streams_
[index].second
                                                 ? &*streams_[index].first : nullptr,
                                                 streams_[index].second - streams_[index].first );
    thread_state_[target_thread_state]->is_valid_range_ = false;
    thread_state_[target_thread_state]->luma_splits_.resize(2);
    thread_state_[target_thread_state]->luma_splits_[0] = thread_id != 0 ? file_luma_splits_[thread_id - 1] : 0;
    thread_state_[target_thread_state]->luma_splits_[1] = file_luma_splits_[thread_id];
    TimingHarness::timing[thread_id][TimingHarness::TS_STREAM_MULTIPLEX_FINISHED] = TimingHarness::get_time_us();
}
template <bool force_memory_optimized>
bool VP8ComponentDecoder::initialize_decoder_state(Sirikata::DecoderReader* input,
                                                   const UncompressedComponents * const colldata,
                                                   bool splits_must_preserve_full_mcu_row,
                                                   Sirikata::Array1d<BlockBasedImagePerChannel<force_memory_optimized>,
                                                                     NUM_THREADS>& framebuffer) {
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
    /* read and verify "x" mark */
    unsigned char mark {};
    const bool ok = str_in->Read( &mark, 1 ).second == Sirikata::JpegError::nil();
    if (!ok) {
        return false;
    }
    if ( mark > NUM_THREADS || mark == 0) {
        cerr << " unsupported NUM_THREADS " << (int)mark << endl;
        return false;
    }
    file_luma_splits_.insert(file_luma_splits_.end(), NUM_THREADS, colldata->block_height(0));

    std::vector<uint16_t> luma_splits_tmp(mark - 1);
    IOUtil::ReadFull(str_in, luma_splits_tmp.data(), sizeof(uint16_t) * (mark - 1));
    int sfv_lcm = colldata->min_vertical_luma_multiple();
    
    for (int i = 0; i + 1 < mark; ++i) {
        file_luma_splits_[i] = htole16(luma_splits_tmp[i]);
        if (splits_must_preserve_full_mcu_row) {
            if (file_luma_splits_[i] % sfv_lcm) {
                fprintf(stderr, "File Split %d = %d (remainder %d)\n",
                        i, file_luma_splits_[i], sfv_lcm);
                custom_exit(ExitCode::THREADING_PARTIAL_MCU);
            }
        }
    }
    /* read entire chunk into memory */
    mux_reader_.fillBufferEntirely(streams_.begin());
    initialize_thread_id(0, 0, framebuffer[0]);
    if (do_threading_) {
        for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            initialize_thread_id(thread_id, thread_id, framebuffer[thread_id]);
        }
    }
    return true;
}

CodingReturnValue VP8ComponentDecoder::decode_chunk(UncompressedComponents * const colldata)
{
    /* cmpc is a global variable with the component count */


    /* construct 4x4 VP8 blocks to hold 8x8 JPEG blocks */
    if ( thread_state_[0] == nullptr || thread_state_[0]->context_[0].context.isNil() ) {
        /* first call */
        BlockBasedImagePerChannel<false> framebuffer;
        framebuffer.memset(0);
        for (size_t i = 0; i < framebuffer.size() && i < colldata->get_num_components(); ++i) {
            framebuffer[i] = &colldata->full_component_write((BlockType)i);
        }
        Sirikata::Array1d<BlockBasedImagePerChannel<false>, NUM_THREADS> all_framebuffers;
        for (size_t i = 0; i < all_framebuffers.size(); ++i) {
            all_framebuffers[i] = framebuffer;
        }
        bool ret = initialize_decoder_state(str_in,
                                            colldata,
                                            true,
                                            all_framebuffers);
        if (!ret) {
            return CODING_ERROR;
        }
        if (do_threading_) {
            for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
                if (dospin) {
                    spin_workers_.at(thread_id - 1).work
                        = std::bind(worker_thread,
                                    thread_state_[thread_id],
                                    thread_id,
                                    colldata);
                    spin_workers_.at(thread_id - 1).activate_work();
                } else {
                    workers[thread_id]
                        = new std::thread(std::bind(worker_thread,
                                                    thread_state_[thread_id],
                                                    thread_id,
                                                    colldata));
                }
            }
        }
    }
    TimingHarness::timing[0][TimingHarness::TS_ARITH_STARTED] = TimingHarness::get_time_us();
    CodingReturnValue ret = thread_state_[0]->vp8_decode_thread(0, colldata);
    if (ret == CODING_PARTIAL) {
        return ret;
    }
    TimingHarness::timing[0][TimingHarness::TS_ARITH_FINISHED] = TimingHarness::get_time_us();
    if (do_threading_) {
        for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            TimingHarness::timing[thread_id][TimingHarness::TS_THREAD_WAIT_STARTED] = TimingHarness::get_time_us();
            if (dospin) {
                spin_workers_.at(thread_id - 1).main_wait_for_done();
            } else {
                workers[thread_id]->join();// for now maybe we want to use atomics instead
                delete workers[thread_id];
            }
            TimingHarness::timing[thread_id][TimingHarness::TS_THREAD_WAIT_FINISHED] = TimingHarness::get_time_us();
        }
        // join on all threads
    } else {
        // wait for "threads"
        virtual_thread_id_ += 1;
        for (int thread_id = virtual_thread_id_; thread_id < NUM_THREADS; ++thread_id, ++virtual_thread_id_) {
            BlockBasedImagePerChannel<false> framebuffer;
            framebuffer.memset(0);
            for (size_t i = 0; i < framebuffer.size() && i < colldata->get_num_components(); ++i) {
                framebuffer[i] = &colldata->full_component_write((BlockType)i);
            }

            initialize_thread_id(thread_id, 0, framebuffer);
            TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_STARTED] = TimingHarness::get_time_us();
            if ((ret = thread_state_[0]->vp8_decode_thread(0, colldata)) == CODING_PARTIAL) {
                return ret;
            }
            TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_FINISHED] = TimingHarness::get_time_us();
        }
    }
    TimingHarness::timing[0][TimingHarness::TS_JPEG_RECODE_STARTED] = TimingHarness::get_time_us();
    for (int component = 0; component < colldata->get_num_components(); ++component) {
        colldata->worker_mark_cmp_finished((BlockType)component);
    }
    colldata->worker_update_coefficient_position_progress( 64 );
    colldata->worker_update_bit_progress( 16 );
    return CODING_DONE;
}
