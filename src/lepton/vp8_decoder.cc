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

VP8ComponentDecoder::VP8ComponentDecoder() {
    ProbabilityTablesBase::load_probability_tables();
}

void copy_coef_to_zigzag(UncompressedComponents * const colldata, AlignedBlock& block, BlockType component, int coord) {
    for ( int coeff = 0; coeff < 64; coeff++ ) {
        colldata->set(component, coeff, coord )
        = block.coefficients().raster( jpeg_zigzag.at( coeff ) );
    }
}

template<class Left, class Middle, class Right>
void VP8ComponentDecoder::process_row(Left & left_model,
                                       Middle& middle_model,
                                       Right& right_model,
                                       int block_width,
                                       UncompressedComponents * const colldata) {
    int curr_y = context_.at((int)Middle::COLOR).y;
    if (block_width > 0) {
        BlockContext context = context_.at((int)Middle::COLOR).context;
        parse_tokens(context,
                     bool_decoder_.get(),
                     left_model); //FIXME
        copy_coef_to_zigzag(colldata, context.here(), (BlockType)Middle::COLOR, curr_y * block_width + 0);
        context_.at(Middle::COLOR).context = vp8_blocks_.at(Middle::COLOR).next(context_.at(Middle::COLOR).context);
    }
    for (int jpeg_x = 1; jpeg_x + 1 < block_width; jpeg_x++) {
        BlockContext context = context_.at((int)Middle::COLOR).context;
        parse_tokens(context,
                     bool_decoder_.get(),
                     middle_model); //FIXME
        copy_coef_to_zigzag(colldata, context.here(), (BlockType)Middle::COLOR, curr_y * block_width + jpeg_x);
        context_.at(Middle::COLOR).context = vp8_blocks_.at(Middle::COLOR).next(context_.at(Middle::COLOR).context);
    }
    if (block_width > 1) {
        int curr_y = context_.at((int)Middle::COLOR).y;
        BlockContext context = context_.at((int)Middle::COLOR).context;
        parse_tokens(context,
                     bool_decoder_.get(),
                     right_model);
        copy_coef_to_zigzag(colldata, context.here(), (BlockType)Middle::COLOR, curr_y * block_width + block_width - 1);
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

        /* populate the VP8 blocks */
        for (size_t i = 0; i < sizeof(context_)/sizeof(context_[0]); ++i) {
             // FIXME: we may be able to truncate here if we are faced with a truncated image
            vp8_blocks_.at(i).init(colldata->block_width( i ), colldata->block_height( i ),
                                colldata->block_width( i ) * colldata->block_height( i ) );
        }
        for (size_t i = 0; i < sizeof(context_)/sizeof(context_[0]); ++i) {
            context_.at(i).context = vp8_blocks_.at(i).begin();
            context_.at(i).y = 0;
        }

        /* read length */
        uint32_t length_big_endian;
        if ( 4 != IOUtil::ReadFull( str_in, &length_big_endian, sizeof( uint32_t ) ) ) {
            return CODING_ERROR;
        }

        /* allocate memory for compressed frame */
        file_.resize( be32toh( length_big_endian ) );

        /* read entire chunk into memory */
        if ( file_.size()
             != IOUtil::ReadFull( str_in, file_.data(), file_.size() ) ) {
            return CODING_ERROR;
        }

        /* initialize the bool decoder */
        bool_decoder_.initialize( Slice( file_.data(), file_.size() ) );

    }
    /* deserialize each block in planar order */
    using namespace std;
    BlockType component = BlockType::Y;
    ProbabilityTablesBase::set_quantization_table(component, colldata->get_quantization_tables(BlockType::Y));
    ProbabilityTablesBase::set_quantization_table(component, colldata->get_quantization_tables(BlockType::Cb));
    ProbabilityTablesBase::set_quantization_table(component, colldata->get_quantization_tables(BlockType::Cr));
    tuple<ProbabilityTables<false, false, false, BlockType::Y>,
          ProbabilityTables<false, false, false, BlockType::Cb>,
          ProbabilityTables<false, false, false, BlockType::Cr> > corner;

    tuple<ProbabilityTables<true, false, false, BlockType::Y>,
          ProbabilityTables<true, false, false, BlockType::Cb>,
          ProbabilityTables<true, false, false, BlockType::Cr> > top;

    tuple<ProbabilityTables<false, true, true, BlockType::Y>,
          ProbabilityTables<false, true, true, BlockType::Cb>,
          ProbabilityTables<false, true, true, BlockType::Cr> > midleft;

    tuple<ProbabilityTables<true, true, true, BlockType::Y>,
          ProbabilityTables<true, true, true, BlockType::Cb>,
          ProbabilityTables<true, true, true, BlockType::Cr> > middle;

    tuple<ProbabilityTables<true, true, false, BlockType::Y>,
          ProbabilityTables<true, true, false, BlockType::Cb>,
          ProbabilityTables<true, true, false, BlockType::Cr> > midright;

    tuple<ProbabilityTables<false, true, false, BlockType::Y>,
          ProbabilityTables<false, true, false, BlockType::Cb>,
          ProbabilityTables<false, true, false, BlockType::Cr> > width_one;

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
