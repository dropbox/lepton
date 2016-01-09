#include "lepton_codec.hh"
#include "uncompressed_components.hh"
#include "../vp8/decoder/decoder.hh"
template<class Left, class Middle, class Right>
void LeptonCodec::ThreadState::decode_row(Left & left_model,
                                                   Middle& middle_model,
                                                   Right& right_model,
                                                   int block_width,
                                                   UncompressedComponents * const colldata) {
    uint32_t component_size_in_block = colldata->component_size_in_blocks(middle_model.COLOR);
    if (block_width > 0) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_,
                     left_model,
                     model_); //FIXME
        uint32_t offset = colldata->full_component_write((BlockType)middle_model.COLOR).next(context_.at((int)middle_model.COLOR), true);
        if (offset >= component_size_in_block) {
            return;
        }
    }
    for (int jpeg_x = 1; jpeg_x + 1 < block_width; jpeg_x++) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_,
                     middle_model,
                     model_); //FIXME
        BlockBasedImage * channel_image = &colldata->full_component_write((BlockType)middle_model.COLOR);
        uint32_t offset = channel_image->next(context_.at((int)middle_model.COLOR),
                                              true);
        if (offset >= component_size_in_block) {
            return;
        }
    }
    if (block_width > 1) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_,
                     right_model,
                     model_);
        colldata->full_component_write((BlockType)middle_model.COLOR).next(context_.at((int)middle_model.COLOR), false);
    }
}

#ifdef ALLOW_FOUR_COLORS
#define ProbabilityTablesTuple(left, above, right) \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR0>, \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR1>, \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR2>, \
    ProbabilityTables<left && above && right, TEMPLATE_ARG_COLOR3>
#define EACH_BLOCK_TYPE ProbabilityTables<left&&above&&right, TEMPLATE_ARG_COLOR0>(BlockType::Y, \
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

CodingReturnValue LeptonCodec::ThreadState::vp8_decode_thread(int thread_id,
                                                                      UncompressedComponents *const colldata) {
    /* deserialize each block in planar order */
    using namespace std;
    BlockType component = BlockType::Y;
    tuple<ProbabilityTablesTuple(false, false, false)> corner(EACH_BLOCK_TYPE(false,false,false));
    tuple<ProbabilityTablesTuple(true, false, false)> top(EACH_BLOCK_TYPE(true,false,false));
    tuple<ProbabilityTablesTuple(false, true, true)> midleft(EACH_BLOCK_TYPE(false, true, true));
    tuple<ProbabilityTablesTuple(true, true, true)> middle(EACH_BLOCK_TYPE(true,true,true));
    tuple<ProbabilityTablesTuple(true, true, false)> midright(EACH_BLOCK_TYPE(true, true, false));
    tuple<ProbabilityTablesTuple(false, true, false)> width_one(EACH_BLOCK_TYPE(false, true, false));
    assert(luma_splits_.size() == 2); // not ready to do multiple work items on a thread yet
    int min_y = luma_splits_[0];
    int max_y = luma_splits_[1];
    int luma_y = 0;
    while(colldata->get_next_component(context_, &component, &luma_y)) {
        int curr_y = context_.at((int)component).y;
        if (luma_y >= min_y) {
            is_valid_range_ = true;
        }
        if (luma_y >= max_y) {
            break; // coding done
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
                    decode_row(std::get<(int)BlockType::Y>(corner),
                                std::get<(int)BlockType::Y>(top),
                                std::get<(int)BlockType::Y>(top),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    decode_row(std::get<(int)BlockType::Cb>(corner),
                                std::get<(int)BlockType::Cb>(top),
                                std::get<(int)BlockType::Cb>(top),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    decode_row(std::get<(int)BlockType::Cr>(corner),
                                std::get<(int)BlockType::Cr>(top),
                                std::get<(int)BlockType::Cr>(top),
                                block_width,
                                colldata);
                    break;
#ifdef ALLOW_FOUR_COLORS
              case BlockType::Ck:
                    decode_row(std::get<(int)BlockType::Ck>(corner),
                                std::get<(int)BlockType::Ck>(top),
                                std::get<(int)BlockType::Ck>(top),
                                block_width,
                                colldata);
                    break;
#endif
            }
        } else if (block_width > 1) {
            assert(curr_y); // just a sanity check that the zeroth row took the first branch
            switch(component) {
                case BlockType::Y:
                    decode_row(std::get<(int)BlockType::Y>(midleft),
                                std::get<(int)BlockType::Y>(middle),
                                std::get<(int)BlockType::Y>(midright),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    decode_row(std::get<(int)BlockType::Cb>(midleft),
                                std::get<(int)BlockType::Cb>(middle),
                                std::get<(int)BlockType::Cb>(midright),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    decode_row(std::get<(int)BlockType::Cr>(midleft),
                                std::get<(int)BlockType::Cr>(middle),
                                std::get<(int)BlockType::Cr>(midright),
                                block_width,
                                colldata);
                    break;
#ifdef ALLOW_FOUR_COLORS
              case BlockType::Ck:
                    decode_row(std::get<(int)BlockType::Ck>(midleft),
                                std::get<(int)BlockType::Ck>(middle),
                                std::get<(int)BlockType::Ck>(midright),
                                block_width,
                                colldata);
                    break;
#endif
            }
        } else {
            assert(curr_y); // just a sanity check that the zeroth row took the first branch
            assert(block_width == 1);
            switch(component) {
                case BlockType::Y:
                    decode_row(std::get<(int)BlockType::Y>(width_one),
                                std::get<(int)BlockType::Y>(width_one),
                                std::get<(int)BlockType::Y>(width_one),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cb:
                    decode_row(std::get<(int)BlockType::Cb>(width_one),
                                std::get<(int)BlockType::Cb>(width_one),
                                std::get<(int)BlockType::Cb>(width_one),
                                block_width,
                                colldata);
                    break;
                case BlockType::Cr:
                    decode_row(std::get<(int)BlockType::Cr>(width_one),
                                std::get<(int)BlockType::Cr>(width_one),
                                std::get<(int)BlockType::Cr>(width_one),
                                block_width,
                                colldata);
                    break;
#ifdef ALLOW_FOUR_COLORS
                case BlockType::Ck:
                    decode_row(std::get<(int)BlockType::Ck>(width_one),
                                std::get<(int)BlockType::Ck>(width_one),
                                std::get<(int)BlockType::Ck>(width_one),
                                block_width,
                                colldata);
                    break;
#endif
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
