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

void VP8ComponentDecoder::initialize( Sirikata::
                                      SwitchableDecompressionReader<Sirikata::SwitchableXZBase> *input)
{
    str_in = input;
    mux_reader_.init(input);
}


void VP8ComponentDecoder::vp8_continuous_decoder( UncompressedComponents * const colldata,
                                                  Sirikata::
                                                  SwitchableDecompressionReader<Sirikata
                                                                                ::SwitchableXZBase> *str_in )
{
    colldata->worker_wait_for_begin_signal();

    VP8ComponentDecoder scd;
    scd.initialize(str_in);
    while(scd.decode_chunk(colldata) == CODING_PARTIAL) {
    }
}

VP8ComponentDecoder::VP8ComponentDecoder() : mux_reader_(Sirikata::JpegAllocator<uint8_t>()) {
    ProbabilityTablesBase::load_probability_tables();
}


template<class Left, class Middle, class Right>
void VP8ComponentDecoder::process_row(Left & left_model,
                                       Middle& middle_model,
                                       Right& right_model,
                                       int block_width,
                                       UncompressedComponents * const colldata) {
    if (block_width > 0) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_.get(),
                     left_model); //FIXME
        context_.at((int)middle_model.COLOR).context = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_.at((int)middle_model.COLOR).context, true);
    }
    for (int jpeg_x = 1; jpeg_x + 1 < block_width; jpeg_x++) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_.get(),
                     middle_model); //FIXME
        context_.at((int)middle_model.COLOR).context = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_.at((int)middle_model.COLOR).context, true);
    }
    if (block_width > 1) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_.get(),
                     right_model);
        context_.at((int)middle_model.COLOR).context = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_.at((int)middle_model.COLOR).context, false);
    }
}


CodingReturnValue VP8ComponentDecoder::decode_chunk(UncompressedComponents * const colldata)
{
    /* cmpc is a global variable with the component count */


    /* construct 4x4 VP8 blocks to hold 8x8 JPEG blocks */
    if ( context_[0].context.isNil() ) {
        /* first call */
        str_in->EnableCompression();

        /* read and verify "x" mark */
        unsigned char mark {};
        const bool ok = str_in->Read( &mark, 1 ).second == Sirikata::JpegError::nil();
        if ( mark != 'x' ) {
            cerr << "CMPx marker not found " << ok << endl;
            return CODING_ERROR;
        }
        str_in->DisableCompression();

        for (int i = 0; i < colldata->get_num_components(); ++i) {
            context_.at(i).context = colldata->full_component_write((BlockType)i).begin();
            context_.at(i).y = 0;
        }
        std::pair <std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >::const_iterator,
        std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >::const_iterator> streams[Sirikata::MuxReader::MAX_STREAM_ID];
        /* read entire chunk into memory */
        mux_reader_.fillBufferEntirely(streams);
        /* initialize the bool decoder */
            bool_decoder_.initialize( Slice( streams[0].first != streams[0].second
                                        ? &*streams[0].first : nullptr,
                                        streams[0].second - streams[0].first ) );

    }
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

    while(colldata->get_next_component(context_, &component)) {
        int curr_y = context_.at((int)component).y;
        int block_width = colldata->block_width((int)component);
        if (curr_y == 0) {
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
        colldata->worker_update_cmp_progress( component,
                                              block_width );
        ++context_.at((int)component).y;
        return CODING_PARTIAL;
    }

    colldata->worker_update_coefficient_position_progress( 64 );
    colldata->worker_update_bit_progress( 16 );
    return CODING_DONE;
}
