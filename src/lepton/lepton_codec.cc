#include "lepton_codec.hh"
#include "uncompressed_components.hh"
#include "../vp8/decoder/decoder.hh"



template<class BoolDecoder> template<class Left, class Middle, class Right, bool force_memory_optimization>
void LeptonCodec<BoolDecoder>::ThreadState::decode_row(Left & left_model,
                                          Middle& middle_model,
                                          Right& right_model,
                                          int curr_y,
                                          BlockBasedImagePerChannel<force_memory_optimization>& image_data,
                                          int component_size_in_block) {
    uint32_t block_width = image_data[(int)middle_model.COLOR]->block_width();
    if (block_width > 0) {
        BlockContext context = context_.at((int)middle_model.COLOR);
        parse_tokens(context,
                     bool_decoder_,
                     left_model,
                     model_); //FIXME
        int offset = image_data[middle_model.COLOR]->next(context_.at((int)middle_model.COLOR), true, curr_y);
        if (offset >= component_size_in_block) {
            return;
        }
    }
    for (unsigned int jpeg_x = 1; jpeg_x + 1 < block_width; jpeg_x++) {
        BlockContext context = context_.at((int)middle_model.COLOR);
        parse_tokens(context,
                     bool_decoder_,
                     middle_model,
                     model_); //FIXME
        int offset = image_data[middle_model.COLOR]->next(context_.at((int)middle_model.COLOR),
							  true,
							  curr_y);
        if (offset >= component_size_in_block) {
            return;
        }
    }
    if (block_width > 1) {
        BlockContext context = context_.at((int)middle_model.COLOR);
        parse_tokens(context,
                     bool_decoder_,
                     right_model,
                     model_);
        image_data[middle_model.COLOR]->next(context_.at((int)middle_model.COLOR), false, curr_y);
    }
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




template <class BoolDecoder>
void LeptonCodec<BoolDecoder>::ThreadState::decode_row_wrapper(BlockBasedImagePerChannel<true>& image_data,
                                          Sirikata::Array1d<uint32_t,
                                                            (uint32_t)ColorChannel::
                                                            NumBlockTypes> component_size_in_blocks,
                                          int component,
                                          int curr_y) {
    return decode_rowt(image_data, component_size_in_blocks, component, curr_y);
}
template <class BoolDecoder>
void LeptonCodec<BoolDecoder>::ThreadState::decode_rowf(BlockBasedImagePerChannel<false>& image_data,
                                          Sirikata::Array1d<uint32_t,
                                                            (uint32_t)ColorChannel::
                                                            NumBlockTypes> component_size_in_blocks,
                                          int component,
                                          int curr_y) {
    decode_row_internal(image_data, component_size_in_blocks,component,curr_y);
}
template <class BoolDecoder>
void LeptonCodec<BoolDecoder>::ThreadState::decode_rowt(BlockBasedImagePerChannel<true>& image_data,
                                          Sirikata::Array1d<uint32_t,
                                                            (uint32_t)ColorChannel::
                                                            NumBlockTypes> component_size_in_blocks,
                                          int component,
                                          int curr_y) {
    decode_row_internal(image_data, component_size_in_blocks,component,curr_y);
}
template <class BoolDecoder> template<bool force_memory_optimization>
void LeptonCodec<BoolDecoder>::ThreadState::decode_row_internal(BlockBasedImagePerChannel<force_memory_optimization>& image_data,
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
    context_.at(component)
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
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            break;
          case BlockType::Cb:
            decode_row(std::get<(int)BlockType::Cb>(corner),
                       std::get<(int)BlockType::Cb>(top),
                       std::get<(int)BlockType::Cb>(top),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cr:
            decode_row(std::get<(int)BlockType::Cr>(corner),
                       std::get<(int)BlockType::Cr>(top),
                       std::get<(int)BlockType::Cr>(top),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#ifdef ALLOW_FOUR_COLORS
          case BlockType::Ck:
            decode_row(std::get<(int)BlockType::Ck>(corner),
                       std::get<(int)BlockType::Ck>(top),
                       std::get<(int)BlockType::Ck>(top),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#endif
        }
    } else if (block_width > 1) {
        dev_assert(curr_y); // just a sanity check that the zeroth row took the first branch
        switch((BlockType)component) {
          case BlockType::Y:
            decode_row(std::get<(int)BlockType::Y>(midleft),
                       std::get<(int)BlockType::Y>(middle),
                       std::get<(int)BlockType::Y>(midright),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cb:
            decode_row(std::get<(int)BlockType::Cb>(midleft),
                       std::get<(int)BlockType::Cb>(middle),
                       std::get<(int)BlockType::Cb>(midright),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cr:
            decode_row(std::get<(int)BlockType::Cr>(midleft),
                       std::get<(int)BlockType::Cr>(middle),
                       std::get<(int)BlockType::Cr>(midright),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#ifdef ALLOW_FOUR_COLORS
          case BlockType::Ck:
            decode_row(std::get<(int)BlockType::Ck>(midleft),
                       std::get<(int)BlockType::Ck>(middle),
                       std::get<(int)BlockType::Ck>(midright),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#endif
        }
    } else {
        dev_assert(curr_y); // just a sanity check that the zeroth row took the first branch
        dev_assert(block_width == 1);
        switch((BlockType)component) {
          case BlockType::Y:
            decode_row(std::get<(int)BlockType::Y>(width_one),
                       std::get<(int)BlockType::Y>(width_one),
                       std::get<(int)BlockType::Y>(width_one),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cb:
            decode_row(std::get<(int)BlockType::Cb>(width_one),
                       std::get<(int)BlockType::Cb>(width_one),
                       std::get<(int)BlockType::Cb>(width_one),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
          case BlockType::Cr:
            decode_row(std::get<(int)BlockType::Cr>(width_one),
                       std::get<(int)BlockType::Cr>(width_one),
                       std::get<(int)BlockType::Cr>(width_one),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#ifdef ALLOW_FOUR_COLORS
          case BlockType::Ck:
            decode_row(std::get<(int)BlockType::Ck>(width_one),
                       std::get<(int)BlockType::Ck>(width_one),
                       std::get<(int)BlockType::Ck>(width_one),
                       curr_y,
                       image_data,
                       component_size_in_blocks[component]);
            
            break;
#endif
        }
    }
}
template <class BoolDecoder>
CodingReturnValue LeptonCodec<BoolDecoder>::ThreadState::vp8_decode_thread(unsigned int thread_id,
                                                              UncompressedComponents *const colldata) {
    Sirikata::Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks;
    BlockBasedImagePerChannel<false> image_data;
    for (int i = 0; i < colldata->get_num_components(); ++i) {
        component_size_in_blocks[i] = colldata->component_size_in_blocks(i);
        image_data[i] = &colldata->full_component_write((BlockType)i);
    }
    Sirikata::Array1d<uint32_t,
                      (size_t)ColorChannel::NumBlockTypes> max_coded_heights
        = colldata->get_max_coded_heights();
    /* deserialize each block in planar order */

    dev_assert(luma_splits_.size() == 2); // not ready to do multiple work items on a thread yet
    always_assert(luma_splits_.size() >= 2);
    int min_y = luma_splits_[0];
    int max_y = luma_splits_[1];
    while(true) {
        LeptonCodec_RowSpec cur_row = LeptonCodec_row_spec_from_index(decode_index_++, image_data, colldata->get_mcu_count_vertical(), max_coded_heights);
        if (cur_row.done) {
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
        decode_rowf(image_data,
                   component_size_in_blocks,
                   cur_row.component,
                   cur_row.curr_y);
        if (thread_id == 0) {
            colldata->worker_update_cmp_progress((BlockType)cur_row.component,
                                                 image_data[cur_row.component]->block_width() );
        }
        return CODING_PARTIAL;
    }
    return CODING_DONE;
}


template<class BoolDecoder> void LeptonCodec<BoolDecoder>::worker_thread(ThreadState *ts, int thread_id, UncompressedComponents * const colldata,
                                        int8_t thread_target[Sirikata::MuxReader::MAX_STREAM_ID],
                                        GenericWorker *worker,
                                        VP8ComponentDecoder_SendToActualThread *send_to_actual_thread_state) {
    TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_STARTED] = TimingHarness::get_time_us();
    for (uint8_t i = 0; i < Sirikata::MuxReader::MAX_STREAM_ID; ++i) {
        if (thread_target[i] == int8_t(thread_id)) {
            ts->bool_decoder_.init(new ActualThreadPacketReader(i,worker, send_to_actual_thread_state));
        }
    }
    while (ts->vp8_decode_thread(thread_id, colldata) == CODING_PARTIAL) {
    }
    TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_FINISHED] = TimingHarness::get_time_us();
}
template class LeptonCodec<VPXBoolReader>;
#ifdef ENABLE_ANS_EXPERIMENTAL
template class LeptonCodec<ANSBoolReader>;
#endif
