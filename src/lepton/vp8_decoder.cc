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



VP8ComponentDecoder::VP8ComponentDecoder() : mux_reader_(Sirikata::JpegAllocator<uint8_t>(),
                                                         4,
                                                         1024 * 1024 + 262144) {
}
VP8ComponentDecoder::VP8ComponentDecoder(Sirikata::Array1d<GenericWorker,
                                                           (NUM_THREADS - 1)>::Slice workers)
    : VP8ComponentEncoder(workers),
      mux_reader_(Sirikata::JpegAllocator<uint8_t>(),
                  4,
                  1024 * 1024 + 262144) {
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
    while (ts->vp8_decode_thread(thread_id, colldata) == CODING_PARTIAL) {
    }
}
CodingReturnValue VP8ComponentDecoder::decode_chunk(UncompressedComponents * const colldata)
{
    /* cmpc is a global variable with the component count */


    /* construct 4x4 VP8 blocks to hold 8x8 JPEG blocks */
    if ( thread_state_[0] == nullptr || thread_state_[0]->context_[0].context.isNil() ) {
        /* first call */
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
            return CODING_ERROR;
        }
        if ( mark > NUM_THREADS || mark == 0) {
            cerr << " unsupported NUM_THREADS " << (int)mark << endl;
            return CODING_ERROR;
        }
        file_luma_splits_.insert(file_luma_splits_.end(), NUM_THREADS, colldata->block_height(0));

        std::vector<uint16_t> luma_splits_tmp(mark - 1);
        IOUtil::ReadFull(str_in, luma_splits_tmp.data(), sizeof(uint16_t) * (mark - 1));
        for (int i = 0; i + 1 < mark; ++i) {
            file_luma_splits_[i] = htole16(luma_splits_tmp[i]);
        }
        for (int thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
            fprintf(stderr,"Luma splits %d\n", (int)file_luma_splits_[thread_id]);
            for (int i = 0; i < colldata->get_num_components(); ++i) {
                thread_state_[thread_id]->context_.at(i).context
                    = colldata->full_component_write((BlockType)i).begin(thread_state_[thread_id]->num_nonzeros_.at(i).begin());
                thread_state_[thread_id]->context_.at(i).y = 0;
            }
        }
        std::pair <Sirikata::MuxReader::ResizableByteBuffer::const_iterator,
        Sirikata::MuxReader::ResizableByteBuffer::const_iterator> streams[Sirikata::MuxReader::MAX_STREAM_ID];
        /* read entire chunk into memory */
        mux_reader_.fillBufferEntirely(streams);
        /* initialize the bool decoder */
        for (int thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
            int index = thread_id;
            thread_state_[thread_id]->bool_decoder_.init(streams[index].first != streams[index].second
                                                         ? &*streams[index].first : nullptr,
                                                         streams[index].second - streams[index].first );
        }
        for (int thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
            for (int j   = 0 ; j < (int)ColorChannel::NumBlockTypes; ++j) {
                thread_state_[thread_id]->is_top_row_.at(j) = true;
                thread_state_[thread_id]->num_nonzeros_.at(j).resize(colldata->block_width(j) << 1);
            }
            thread_state_[thread_id]->is_valid_range_ = false;
            thread_state_[thread_id]->luma_splits_.resize(2);
            thread_state_[thread_id]->luma_splits_[0] = thread_id != 0 ? file_luma_splits_[thread_id - 1] : 0;
            thread_state_[thread_id]->luma_splits_[1] = file_luma_splits_[thread_id];
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
    CodingReturnValue ret = thread_state_[0]->vp8_decode_thread(0, colldata);
    if (ret == CODING_PARTIAL) {
        return ret;
    }
    if (do_threading_) {
        for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            if (dospin) {
                spin_workers_.at(thread_id - 1).main_wait_for_done();
            } else {
                workers[thread_id]->join();// for now maybe we want to use atomics instead
                delete workers[thread_id];
            }
        }
        // join on all threads
    } else {
        // wait for "threads"
        for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
            while (thread_state_[thread_id]->vp8_decode_thread(thread_id,
                                                               colldata) == CODING_PARTIAL) {

            }
        }
    }
    for (int component = 0; component < colldata->get_num_components(); ++component) {
        colldata->worker_mark_cmp_finished((BlockType)component);
    }

    colldata->worker_update_coefficient_position_progress( 64 );
    colldata->worker_update_bit_progress( 16 );
    return CODING_DONE;
}
