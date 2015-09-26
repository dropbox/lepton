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

#include "mmap.hh"

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
    for (int i = 0; i < NUM_THREADS; ++i) {
        model_[i].load_probability_tables();
    }
}


template<class Left, class Middle, class Right>
void VP8ComponentDecoder::process_row(int thread_id,
                                      ProbabilityTablesBase&pt,
                                      Left & left_model,
                                       Middle& middle_model,
                                       Right& right_model,
                                       int block_width,
                                       UncompressedComponents * const colldata) {
    auto bool_decoders = bool_decoder_.dynslice<SIMD_WIDTH>(thread_id * SIMD_WIDTH);
    if (block_width > 0) {
        BlockContext context = context_[thread_id].at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoders,
                     left_model,
                     pt); //FIXME
        context_[thread_id].at((int)middle_model.COLOR).context = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_[thread_id].at((int)middle_model.COLOR).context, true);
    }
    for (int jpeg_x = 1; jpeg_x + 1 < block_width; jpeg_x++) {
        BlockContext context = context_[thread_id].at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoders,
                     middle_model,
                     pt); //FIXME
        context_[thread_id].at((int)middle_model.COLOR).context
            = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_[thread_id].at((int)middle_model.COLOR).context, true);
    }
    if (block_width > 1) {
        BlockContext context = context_[thread_id].at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoders,
                     right_model,
                     pt);
        context_[thread_id].at((int)middle_model.COLOR).context
            = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_[thread_id].at((int)middle_model.COLOR).context, false);
    }
}

CodingReturnValue VP8ComponentDecoder::vp8_decode_thread(int thread_id,
                                                         UncompressedComponents *const colldata) {
    /* deserialize each block in planar order */
    using namespace std;
    BlockType component = BlockType::Y;
    ProbabilityTablesBase::set_quantization_table(BlockType::Y, colldata->get_quantization_tables(BlockType::Y));
    ProbabilityTablesBase::set_quantization_table(BlockType::Cb, colldata->get_quantization_tables(BlockType::Cb));
    ProbabilityTablesBase::set_quantization_table(BlockType::Cr, colldata->get_quantization_tables(BlockType::Cr));
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
    int min_y = thread_id == 0 ? 0 : luma_splits_[thread_id - 1];
    int max_y = luma_splits_[thread_id];
    while(colldata->get_next_component(context_[thread_id], &component)) {
        int curr_y = context_[thread_id].at((int)component).y;
        if (component == BlockType::Y) {
            if (curr_y >= min_y) {
                is_valid_range_[thread_id] = true;
            }
            if (curr_y >= max_y) {
                break; // coding done
            }
        }
        if (!is_valid_range_[thread_id]) {
            ++context_[thread_id].at((int)component).y;
            continue;
        }
        context_[thread_id].at((int)component).context = colldata->full_component_write(component).off_y(curr_y);
        int block_width = colldata->block_width((int)component);
        if (is_top_row_.at(thread_id, (int)component)) {
            is_top_row_.at(thread_id, (int)component) = false;
            switch(component) {
                case BlockType::Y:
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Y>(corner),
                                std::get<(int)BlockType::Y>(top),
                                std::get<(int)BlockType::Y>(top),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Cb>(corner),
                                std::get<(int)BlockType::Cb>(top),
                                std::get<(int)BlockType::Cb>(top),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Cr>(corner),
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
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Y>(midleft),
                                std::get<(int)BlockType::Y>(middle),
                                std::get<(int)BlockType::Y>(midright),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Cb>(midleft),
                                std::get<(int)BlockType::Cb>(middle),
                                std::get<(int)BlockType::Cb>(midright),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Cr>(midleft),
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
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Y>(width_one),
                                std::get<(int)BlockType::Y>(width_one),
                                std::get<(int)BlockType::Y>(width_one),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Cb>(width_one),
                                std::get<(int)BlockType::Cb>(width_one),
                                std::get<(int)BlockType::Cb>(width_one),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    process_row(thread_id,
                                model_[thread_id],
                                std::get<(int)BlockType::Cr>(width_one),
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
        ++context_[thread_id].at((int)component).y;
        return CODING_PARTIAL;
    }
    return CODING_DONE;
}
extern void pick_luma_splits(const UncompressedComponents *colldata, int luma_splits[NUM_THREADS]);
CodingReturnValue VP8ComponentDecoder::decode_chunk(UncompressedComponents * const colldata)
{
    /* cmpc is a global variable with the component count */


    /* construct 4x4 VP8 blocks to hold 8x8 JPEG blocks */
    if ( context_[0][0].context.isNil() ) {
        /* first call */
        /* read and verify "x" mark */
        unsigned char mark {};
        const bool ok = str_in->Read( &mark, 1 ).second == Sirikata::JpegError::nil();
        if ( mark != 'x' ) {
            cerr << "CMPx marker not found " << ok << endl;
            return CODING_ERROR;
        }
        for (int thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
            for (int i = 0; i < colldata->get_num_components(); ++i) {
                context_[thread_id].at(i).context
                    = colldata->full_component_write((BlockType)i).begin();
                context_[thread_id].at(i).y = 0;
            }
        }
        std::pair <std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >::const_iterator,
        std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >::const_iterator> streams[Sirikata::MuxReader::MAX_STREAM_ID];
        /* read entire chunk into memory */
        mux_reader_.fillBufferEntirely(streams);
        /* initialize the bool decoder */
        for (int i = 0; i < Sirikata::MuxReader::MAX_STREAM_ID; ++i) {
            bool_decoder_[i].init(streams[i].first != streams[i].second
                                  ? &*streams[i].first : nullptr,
                                  streams[i].second - streams[i].first );
        }
        for (int i = 0; i < NUM_THREADS; ++i) {
            for (int j   = 0 ; j < (int)ColorChannel::NumBlockTypes; ++j) {
                is_top_row_.at(i,j) = true;
            }
            is_valid_range_[i] = false;
        }
        pick_luma_splits(colldata, luma_splits_); //FIXME <-- this needs to be deserialized

    }
    CodingReturnValue ret = vp8_decode_thread(0, colldata);
    if (ret == CODING_PARTIAL) {
        return ret;
    }
    // wait for "threads"
    for (int thread_id = 1; thread_id < NUM_THREADS; ++thread_id) {
        while (vp8_decode_thread(thread_id,colldata) == CODING_PARTIAL) {
            
        }
    }
    // join on all threads
    for (int component = 0; component < colldata->get_num_components(); ++component) {
        colldata->worker_mark_cmp_finished((BlockType)component);
    }

    colldata->worker_update_coefficient_position_progress( 64 );
    colldata->worker_update_bit_progress( 16 );
    return CODING_DONE;
}
