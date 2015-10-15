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

#include "../io/SwitchableCompression.hh"
#include "../vp8/decoder/decoder.hh"
using namespace std;

void VP8ComponentDecoder::initialize( Sirikata::DecoderReader *input)
{
    str_in = input;
    mux_reader_.init(input);
}


void VP8ComponentDecoder::vp8_continuous_decoder( UncompressedComponents * const colldata,
                                                 Sirikata::DecoderReader *str_in )
{
    colldata->worker_wait_for_begin_signal();

    VP8ComponentDecoder scd;
    scd.initialize(str_in);
    while(scd.decode_chunk(colldata) == CODING_PARTIAL) {
    }
}

VP8ComponentDecoder::VP8ComponentDecoder() : mux_reader_(Sirikata::JpegAllocator<uint8_t>()) {
    do_threading_ = false;
    for (int i = 0; i < NUM_THREADS; ++i) {
        thread_state_[i] = new ThreadState;
        thread_state_[i]->model_.load_probability_tables();
    }
}
VP8ComponentDecoder::~VP8ComponentDecoder() {
    for (int i = 0; i < NUM_THREADS; ++i) {
        delete thread_state_[i];
    }
}

template<class Left, class Middle, class Right>
void VP8ComponentDecoder::ThreadState::process_row(Left & left_model,
                                                   Middle& middle_model,
                                                   Right& right_model,
                                                   int block_width,
                                                   UncompressedComponents * const colldata) {
    if (block_width > 0) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_,
                     left_model,
                     model_); //FIXME
        context_.at((int)middle_model.COLOR).context = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_.at((int)middle_model.COLOR).context, true);
    }
    for (int jpeg_x = 1; jpeg_x + 1 < block_width; jpeg_x++) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_,
                     middle_model,
                     model_); //FIXME
        context_.at((int)middle_model.COLOR).context
            = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_.at((int)middle_model.COLOR).context, true);
    }
    if (block_width > 1) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_,
                     right_model,
                     model_);
        context_.at((int)middle_model.COLOR).context
            = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_.at((int)middle_model.COLOR).context, false);
    }
}
const bool dospin = true;

CodingReturnValue VP8ComponentDecoder::ThreadState::vp8_decode_thread(int thread_id,
                                                                      UncompressedComponents *const colldata) {
    /* deserialize each block in planar order */
    using namespace std;
    BlockType component = BlockType::Y;
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

    assert(luma_splits_.size() == 2); // not ready to do multiple work items on a thread yet
    int min_y = luma_splits_[0];
    int max_y = luma_splits_[1];
    while(colldata->get_next_component(context_, &component)) {
        int curr_y = context_.at((int)component).y;
        if (component == BlockType::Y) {
            if (curr_y >= min_y) {
                is_valid_range_ = true;
            }
            if (curr_y >= max_y) {
                break; // coding done
            }
        }
        if (!is_valid_range_) {
            ++context_.at((int)component).y;
            continue;
        }
        context_.at((int)component).context = colldata->full_component_write(component).off_y(curr_y, num_nonzeros_.at((int)component).begin());
        int block_width = colldata->block_width((int)component);
        if (is_top_row_.at((int)component)) {
            is_top_row_.at((int)component) = false;
            switch(component) {
                case BlockType::Y:
                    process_row(std::get<(int)BlockType::Y>(corner),
                                std::get<(int)BlockType::Y>(top),
                                std::get<(int)BlockType::Y>(top),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    process_row(std::get<(int)BlockType::Cb>(corner),
                                std::get<(int)BlockType::Cb>(top),
                                std::get<(int)BlockType::Cb>(top),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    process_row(std::get<(int)BlockType::Cr>(corner),
                                std::get<(int)BlockType::Cr>(top),
                                std::get<(int)BlockType::Cr>(top),
                                block_width,
                                colldata);
                    break;
            }
        } else if (block_width > 1) {
            assert(curr_y); // just a sanity check that the zeroth row took the first branch
            switch(component) {
                case BlockType::Y:
                    process_row(std::get<(int)BlockType::Y>(midleft),
                                std::get<(int)BlockType::Y>(middle),
                                std::get<(int)BlockType::Y>(midright),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    process_row(std::get<(int)BlockType::Cb>(midleft),
                                std::get<(int)BlockType::Cb>(middle),
                                std::get<(int)BlockType::Cb>(midright),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    process_row(std::get<(int)BlockType::Cr>(midleft),
                                std::get<(int)BlockType::Cr>(middle),
                                std::get<(int)BlockType::Cr>(midright),
                                block_width,
                                colldata);
                    break;
            }
        } else {
            assert(curr_y); // just a sanity check that the zeroth row took the first branch
            assert(block_width == 1);
            switch(component) {
                case BlockType::Y:
                    process_row(std::get<(int)BlockType::Y>(width_one),
                                std::get<(int)BlockType::Y>(width_one),
                                std::get<(int)BlockType::Y>(width_one),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    process_row(std::get<(int)BlockType::Cb>(width_one),
                                std::get<(int)BlockType::Cb>(width_one),
                                std::get<(int)BlockType::Cb>(width_one),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    process_row(std::get<(int)BlockType::Cr>(width_one),
                                std::get<(int)BlockType::Cr>(width_one),
                                std::get<(int)BlockType::Cr>(width_one),
                                block_width,
                                colldata);
                    break;
            }
        }
        if (thread_id == 0) {
            colldata->worker_update_cmp_progress( component,
                                                 block_width );
        }
        ++context_.at((int)component).y;
        return CODING_PARTIAL;
    }
    return CODING_DONE;
}
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
        ProbabilityTablesBase::set_quantization_table(BlockType::Y, colldata->get_quantization_tables(BlockType::Y));
        ProbabilityTablesBase::set_quantization_table(BlockType::Cb, colldata->get_quantization_tables(BlockType::Cb));
        ProbabilityTablesBase::set_quantization_table(BlockType::Cr, colldata->get_quantization_tables(BlockType::Cr));
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
        std::pair <std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >::const_iterator,
        std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >::const_iterator> streams[Sirikata::MuxReader::MAX_STREAM_ID];
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
            while (thread_state_[thread_id]->vp8_decode_thread(thread_id,colldata) == CODING_PARTIAL) {
                
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
