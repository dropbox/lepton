#include "lepton_codec.hh"
#include "uncompressed_components.hh"
#include "../vp8/decoder/decoder.hh"
template<class Left, class Middle, class Right, bool force_memory_optimization>
void LeptonCodec::ThreadState::decode_row(Left & left_model,
                                          Middle& middle_model,
                                          Right& right_model,
                                          int block_width,
                                          BlockBasedImagePerChannel<force_memory_optimization>& image_data,
                                          int component_size_in_block) {
    if (block_width > 0) {
        BlockContext context = context_.at((int)middle_model.COLOR).context;
        parse_tokens(context,
                     bool_decoder_,
                     left_model,
                     model_); //FIXME
        uint32_t offset = image_data[middle_model.COLOR]->next(context_.at((int)middle_model.COLOR), true);
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
        uint32_t offset = image_data[middle_model.COLOR]->next(context_.at((int)middle_model.COLOR),
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
        image_data[middle_model.COLOR]->next(context_.at((int)middle_model.COLOR), false);
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



template<bool force_memory_optimization>
void LeptonCodec::ThreadState::decode_row(BlockBasedImagePerChannel<force_memory_optimization>& image_data,
                                          Sirikata::Array1d<uint32_t,
                                                            (uint32_t)ColorChannel::
                                                            NumBlockTypes> component_size_in_blocks,
                                          int component,
                                          int curr_y) {
    using std::tuple;
    tuple<ProbabilityTablesTuple(false, false, false)> corner(EACH_BLOCK_TYPE(false,false,false));
    tuple<ProbabilityTablesTuple(true, false, false)> top(EACH_BLOCK_TYPE(true,false,false));
    tuple<ProbabilityTablesTuple(false, true, true)> midleft(EACH_BLOCK_TYPE(false, true, true));
    tuple<ProbabilityTablesTuple(true, true, true)> middle(EACH_BLOCK_TYPE(true,true,true));
    tuple<ProbabilityTablesTuple(true, true, false)> midright(EACH_BLOCK_TYPE(true, true, false));
    tuple<ProbabilityTablesTuple(false, true, false)> width_one(EACH_BLOCK_TYPE(false, true, false));
    assert(context_.at((int)component).y == curr_y); //caller must set this
    context_.at(component).context
        = image_data[component]->off_y(curr_y,
                                       num_nonzeros_.at(component).begin());
    
    int block_width = image_data[component]->block_width();
    if (is_top_row_.at(component)) {
        is_top_row_.at(component) = false;
        switch((BlockType)component) {
          case BlockType::Y:
            decode_row(std::get<(int)BlockType::Y>(corner),
                       std::get<(int)BlockType::Y>(top),
                       std::get<(int)BlockType::Y>(top),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            break;
          case BlockType::Cb:
            decode_row(std::get<(int)BlockType::Cb>(corner),
                       std::get<(int)BlockType::Cb>(top),
                       std::get<(int)BlockType::Cb>(top),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cr:
            decode_row(std::get<(int)BlockType::Cr>(corner),
                       std::get<(int)BlockType::Cr>(top),
                       std::get<(int)BlockType::Cr>(top),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#ifdef ALLOW_FOUR_COLORS
          case BlockType::Ck:
            decode_row(std::get<(int)BlockType::Ck>(corner),
                       std::get<(int)BlockType::Ck>(top),
                       std::get<(int)BlockType::Ck>(top),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#endif
        }
    } else if (block_width > 1) {
        assert(curr_y); // just a sanity check that the zeroth row took the first branch
        switch((BlockType)component) {
          case BlockType::Y:
            decode_row(std::get<(int)BlockType::Y>(midleft),
                       std::get<(int)BlockType::Y>(middle),
                       std::get<(int)BlockType::Y>(midright),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cb:
            decode_row(std::get<(int)BlockType::Cb>(midleft),
                       std::get<(int)BlockType::Cb>(middle),
                       std::get<(int)BlockType::Cb>(midright),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cr:
            decode_row(std::get<(int)BlockType::Cr>(midleft),
                       std::get<(int)BlockType::Cr>(middle),
                       std::get<(int)BlockType::Cr>(midright),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#ifdef ALLOW_FOUR_COLORS
          case BlockType::Ck:
            decode_row(std::get<(int)BlockType::Ck>(midleft),
                       std::get<(int)BlockType::Ck>(middle),
                       std::get<(int)BlockType::Ck>(midright),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#endif
        }
    } else {
        assert(curr_y); // just a sanity check that the zeroth row took the first branch
        assert(block_width == 1);
        switch((BlockType)component) {
          case BlockType::Y:
            decode_row(std::get<(int)BlockType::Y>(width_one),
                       std::get<(int)BlockType::Y>(width_one),
                       std::get<(int)BlockType::Y>(width_one),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cb:
            decode_row(std::get<(int)BlockType::Cb>(width_one),
                       std::get<(int)BlockType::Cb>(width_one),
                       std::get<(int)BlockType::Cb>(width_one),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cr:
            decode_row(std::get<(int)BlockType::Cr>(width_one),
                       std::get<(int)BlockType::Cr>(width_one),
                       std::get<(int)BlockType::Cr>(width_one),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#ifdef ALLOW_FOUR_COLORS
          case BlockType::Ck:
            decode_row(std::get<(int)BlockType::Ck>(width_one),
                       std::get<(int)BlockType::Ck>(width_one),
                       std::get<(int)BlockType::Ck>(width_one),
                       block_width,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#endif
        }
    }
}

CodingReturnValue LeptonCodec::ThreadState::vp8_decode_thread(int thread_id,
                                                              UncompressedComponents *const colldata) {
    Sirikata::Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks;
    BlockBasedImagePerChannel<false> image_data;
    for (int i = 0; i < colldata->get_num_components(); ++i) {
        component_size_in_blocks[i] = colldata->component_size_in_blocks(i);
        image_data[i] = &colldata->full_component_write((BlockType)i);
    }
    /* deserialize each block in planar order */

    BlockType component = BlockType::Y;
    assert(luma_splits_.size() == 2); // not ready to do multiple work items on a thread yet
    int luma_y = 0;
    while(colldata->get_next_component(context_, &component, &luma_y)) {
        int min_y = luma_splits_[0];
        int max_y = luma_splits_[1];
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
        int curr_y = context_.at((int)component).y;
        decode_row(image_data,
                   component_size_in_blocks,
                   (int)component,
                   curr_y);
        ++context_.at((int)component).y;

        if (thread_id == 0) {
            colldata->worker_update_cmp_progress(component,
                                                 image_data[(int)component]->block_width() );
        }
        return CODING_PARTIAL;
    }
    return CODING_DONE;
}
