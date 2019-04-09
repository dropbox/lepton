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
template<class BoolEncoder>
void VP8ComponentDecoder<BoolEncoder>::initialize( Sirikata::DecoderReader *input,
                                      const std::vector<ThreadHandoff>& thread_handoff)
{
    str_in = input;
    mux_reader_.init(input);
    thread_handoff_ = thread_handoff;
}
template<class BoolEncoder>
void VP8ComponentDecoder<BoolEncoder>::decode_row(int target_thread_id,
                                     BlockBasedImagePerChannel<true>& image_data, // FIXME: set image_data to true
                                     Sirikata::Array1d<uint32_t,
                                                       (uint32_t)ColorChannel::
                                                       NumBlockTypes> component_size_in_blocks,
                                     int component,
                                     int curr_y) {
    this->thread_state_[target_thread_id]->decode_rowt(image_data,
                                               component_size_in_blocks,
                                               component,
                                               curr_y);
}

 
template<class BoolDecoder>
VP8ComponentDecoder<BoolDecoder>::VP8ComponentDecoder(bool do_threading)
    : VP8ComponentEncoder<BoolDecoder>(do_threading, IsDecoderAns<BoolDecoder>::IS_ANS),
      mux_reader_(Sirikata::JpegAllocator<uint8_t>(),
                  8,
                  0) {
    virtual_thread_id_ = -1;
}
template<class BoolEncoder>
VP8ComponentDecoder<BoolEncoder>::~VP8ComponentDecoder() {

}


#ifdef ALLOW_FOUR_COLORS
#define ProbabilityTablesTuple(left, above, right) \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR0>, \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR1>, \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR2>, \
    ProbabilityTables<left, above, right, TEMPLATE_ARG_COLOR3>
#define EACH_BLOCK_TYPE(left, above, right) BlockType::Y,   \
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

template <class BoolEncoder>
void VP8ComponentDecoder<BoolEncoder>::clear_thread_state(int thread_id, int target_thread_state, BlockBasedImagePerChannel<true>& framebuffer) {


    initialize_thread_id(thread_id, target_thread_state, framebuffer);
    initialize_bool_decoder(thread_id, target_thread_state);
}

class VirtualThreadPacketReader : public PacketReader{
    VP8ComponentDecoder_SendToVirtualThread*base;
    uint8_t stream_id;
    Sirikata::MuxReader*mux_reader_;
    Sirikata::MuxReader::ResizableByteBuffer * last;
public:
    VirtualThreadPacketReader(uint8_t stream_id, Sirikata::MuxReader * mr, VP8ComponentDecoder_SendToVirtualThread*base) {
        this->base = base;
        this->stream_id = stream_id;
        this->mux_reader_ = mr;
        last = NULL;
    }
    // returns a buffer with at least sizeof(BD_VALUE) before it
    virtual ROBuffer getNext() {
        auto retval = base->read(*mux_reader_, stream_id);
        if (retval->size() == 0) {
            isEof = true;
            return {NULL, NULL};
        }
        always_assert(!retval->empty()); // we check this earlier
        return {retval->data(), retval->data() + retval->size()};
    }
    bool eof()const {
        return isEof;
    }
    virtual void setFree(ROBuffer buffer) {// don't even bother
        if (last && last->data() == buffer.first) {
            delete last; // hax
            last = NULL;
        }
    }
    virtual ~VirtualThreadPacketReader(){}
};

template <class BoolDecoder>
void VP8ComponentDecoder<BoolDecoder>::registerWorkers(GenericWorker *workers, unsigned int num_workers) {
        this->VP8ComponentEncoder<BoolDecoder>::registerWorkers(workers, num_workers);
}

template <class BoolDecoder>
void VP8ComponentDecoder<BoolDecoder>::initialize_bool_decoder(int thread_id, int target_thread_state) {
    if (NUM_THREADS > 1 && g_threaded) {
        this->thread_state_[target_thread_state]->bool_decoder_.init(new ActualThreadPacketReader(thread_id,
                                                                                            getWorker(target_thread_state),
                                                                                            &send_to_actual_thread_state));
    } else {
        this->thread_state_[target_thread_state]->bool_decoder_.init(new VirtualThreadPacketReader(thread_id, &mux_reader_, &mux_splicer));
    }
}

template <class BoolDecoder> template <bool force_memory_optimized>
void VP8ComponentDecoder<BoolDecoder>::initialize_thread_id(int thread_id, int target_thread_state,
                                               BlockBasedImagePerChannel<force_memory_optimized>& framebuffer) {
    if (target_thread_state) {
        always_assert(this->spin_workers_);
    }
    TimingHarness::timing[thread_id%NUM_THREADS][TimingHarness::TS_STREAM_MULTIPLEX_STARTED] = TimingHarness::get_time_us();
    //if (thread_id != target_thread_state) {
        this->reset_thread_model_state(target_thread_state);
    //}
    this->thread_state_[target_thread_state]->decode_index_ = 0;
    for (unsigned int i = 0; i < framebuffer.size(); ++i) {
        if (framebuffer[i] != NULL)  {
            this->thread_state_[target_thread_state]->is_top_row_.at(i) = true;
            this->thread_state_[target_thread_state]->num_nonzeros_.at(i).resize(framebuffer[i]->block_width() << 1);
            this->thread_state_[target_thread_state]->context_.at(i)
                = framebuffer[i]->begin(this->thread_state_[target_thread_state]->num_nonzeros_.at(i).begin());
        }
    }
    /* initialize the bool decoder */
    int index = thread_id;
    always_assert((size_t)index < streams_.size());
    
    this->thread_state_[target_thread_state]->is_valid_range_ = false;
    this->thread_state_[target_thread_state]->luma_splits_.resize(2);
    if ((size_t)index < thread_handoff_.size()) {
        this->thread_state_[target_thread_state]->luma_splits_[0] = thread_handoff_[thread_id].luma_y_start;
        this->thread_state_[target_thread_state]->luma_splits_[1] = thread_handoff_[thread_id].luma_y_end;
    } else {
        // we have extra threads that are not in use during this decode.
        // set them to zero sized work (i.e. starting at end and ending at end)
        // since they don't have any rows to decode
        this->thread_state_[target_thread_state]->luma_splits_[0] = thread_handoff_.back().luma_y_end; // <- not a typo
        this->thread_state_[target_thread_state]->luma_splits_[1] = thread_handoff_.back().luma_y_end; // both start and end at end
    }
    //fprintf(stderr, "tid: %d   %d -> %d\n", thread_id, thread_state_[target_thread_state]->luma_splits_[0],
    //        thread_state_[target_thread_state]->luma_splits_[1]);
    TimingHarness::timing[thread_id%NUM_THREADS][TimingHarness::TS_STREAM_MULTIPLEX_FINISHED] = TimingHarness::get_time_us();
}

template <class BoolDecoder> 
std::vector<ThreadHandoff> VP8ComponentDecoder<BoolDecoder>::initialize_baseline_decoder(
    const UncompressedComponents * const colldata,
    Sirikata::Array1d<BlockBasedImagePerChannel<true>,
                      MAX_NUM_THREADS>& framebuffer) {
    mux_splicer.init(this->spin_workers_);
    return initialize_decoder_state(colldata, framebuffer);
}

void VP8ComponentDecoder_SendToVirtualThread::set_eof() {
    using namespace Sirikata;
    if (!eof) {
        for (unsigned int thread_id = 0; thread_id < Sirikata::MuxReader::MAX_STREAM_ID; ++thread_id) {
            for (int i = 0; i < Sirikata::MuxReader::MAX_STREAM_ID; ++i) {
                if (thread_target[i] == int8_t(thread_id)) {
                    
                    auto eof = new ResizableByteBufferListNode;
                    eof->stream_id = i;
                    send(eof); // sends an EOF flag (empty buffer)
                }
            }
        }
    }
    eof = true;
}
VP8ComponentDecoder_SendToVirtualThread::VP8ComponentDecoder_SendToVirtualThread(){
    eof = false;
    for (int i = 0; i < Sirikata::MuxReader::MAX_STREAM_ID; ++i) {
        thread_target[i] = -1;
    }
    this->all_workers = NULL;
}

void VP8ComponentDecoder_SendToVirtualThread::init(GenericWorker * all_workers) {
    this->eof = false;
    for (unsigned int thread_id = 0; thread_id < MAX_NUM_THREADS; ++thread_id) {
        if (!vbuffers[thread_id].empty()) {
            vbuffers[thread_id].pop();
        }
    }
    this->all_workers = all_workers;
}
void VP8ComponentDecoder_SendToVirtualThread::send(ResizableByteBufferListNode *data) {
    always_assert(data);
    always_assert(data->stream_id < sizeof(vbuffers) / sizeof(vbuffers[0]) &&
                  "INVALID SEND STREAM ID");
    if (!g_threaded || NUM_THREADS == 1) {
        /*
    fprintf(stderr, "VSending (%d) %d bytes of data : ptr %p\n",
            (int)data->stream_id, (int)data->size(),
            (void*)data);*/
        vbuffers[data->stream_id].push(data);
        return;
    }
    auto thread_target_id = thread_target[data->stream_id];
    /*
    fprintf(stderr, "Sending (%d) %d bytes of data : ptr %p to %d\n",
            (int)data->stream_id, (int)data->size(),
            (void*)data, thread_target_id);
    */
    if (thread_target_id >= 0) {
        int retval = all_workers[thread_target_id].send_more_data(data);
        always_assert(retval == 0 && "Communication with thread lost");
    }else {
        always_assert(false && "Cannot send to thread that wasn't bound");
    }
}
void VP8ComponentDecoder_SendToVirtualThread::drain(Sirikata::MuxReader&reader) {
    while (!reader.eof) {
        ResizableByteBufferListNode *data = new ResizableByteBufferListNode;
        auto ret = reader.nextDataPacket(*data);
        if (ret.second != Sirikata::JpegError::nil()) {
            set_eof();
            break;
        }
        data->stream_id = ret.first;
        always_assert(data->size()); // the protocol can't store empty runs
        send(data);
    }
    /*
    uint8_t buf[4] = {0};
    reader.getReader()->Read(buf, 4);
    fprintf(stderr, "FINAL BUF %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
    */
}
ResizableByteBufferListNode* VP8ComponentDecoder_SendToVirtualThread::read(Sirikata::MuxReader&reader, uint8_t stream_id) {
    using namespace Sirikata;
    always_assert(stream_id < sizeof(vbuffers) / sizeof(vbuffers[0]) &&
                  "INVALID READ STREAM ID");
    if (!vbuffers[stream_id].empty()) {
        auto retval = vbuffers[stream_id].front();
        if (retval->size() == 0) {
            always_assert(eof);
        } else { // keep this placeholder there
            vbuffers[stream_id].pop();
        }
        return retval;
    }
    if (eof) {
        always_assert(false);
        return NULL;
    }
    while (!eof) {
        ResizableByteBufferListNode *data = new ResizableByteBufferListNode;
        auto ret = reader.nextDataPacket(*data);
        if (ret.second != JpegError::nil()) {
            set_eof();
            break;
        }
        data->stream_id = ret.first;
        bool buffer_it = ret.first != stream_id;
        if (buffer_it) {
            send(data);
        } else {
            return data;
        }
    }
    if (!vbuffers[stream_id].empty()) {
        auto retval = vbuffers[stream_id].front();
        if (retval->size() == 0) {
            always_assert(eof);
        } else { // keep this placeholder there
            vbuffers[stream_id].pop();
        }
        return retval;
    }
    return NULL;
}
void VP8ComponentDecoder_SendToVirtualThread::read_all(Sirikata::MuxReader&reader) {
    using namespace Sirikata;
    while (!eof) {
        ResizableByteBufferListNode *data = new ResizableByteBufferListNode;
        auto ret = reader.nextDataPacket(*data);
        if (ret.second != JpegError::nil()) {
            set_eof();
            break;
        }
        data->stream_id = ret.first;
        always_assert(data->size());
        send(data);
    }
}

template<class BoolDecoder> template <bool force_memory_optimized>
std::vector<ThreadHandoff> VP8ComponentDecoder<BoolDecoder>::initialize_decoder_state(const UncompressedComponents * const colldata,
                                                   Sirikata::Array1d<BlockBasedImagePerChannel<force_memory_optimized>,
                                                                     MAX_NUM_THREADS>& framebuffer) {
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
    if (thread_handoff_.empty()) {
        /* read and verify "x" mark */
        unsigned char mark {};
        const bool ok = str_in->Read( &mark, 1 ).second == Sirikata::JpegError::nil();
        if (!ok) {
            return std::vector<ThreadHandoff>();
        }
        ThreadHandoff th;
        memset(&th, 0, sizeof(th));
        th.num_overhang_bits = ThreadHandoff::LEGACY_OVERHANG_BITS; // to make sure we don't use this value
        th.luma_y_end = colldata->block_height(0);
        thread_handoff_.insert(thread_handoff_.end(), mark, th);
        if (mark == 0) { // must be at least 1 to do splits;
            custom_exit(ExitCode::THREADING_PARTIAL_MCU);
        }
        std::vector<uint16_t> luma_splits_tmp(mark - 1);
        IOUtil::ReadFull(str_in, luma_splits_tmp.data(), sizeof(uint16_t) * (mark - 1));
        int sfv_lcm = colldata->min_vertical_luma_multiple();
        for (int i = 0; i + 1 < mark; ++i) {
            thread_handoff_[i].luma_y_end = htole16(luma_splits_tmp[i]);
            if (thread_handoff_[i].luma_y_end % sfv_lcm) {
                custom_exit(ExitCode::THREADING_PARTIAL_MCU);
            }
        }
        for (int i = 1; i < mark; ++i) {
            thread_handoff_[i].luma_y_start = thread_handoff_[i - 1].luma_y_end;
        }
    }
    /* read entire chunk into memory */
    //initialize_thread_id(0, 0, framebuffer[0]);
    if (thread_handoff_.size()) {
        thread_handoff_.back().luma_y_end = colldata->block_height(0);
    }
    return thread_handoff_;
}
template <class BoolDecoder>
void VP8ComponentDecoder<BoolDecoder>::flush() {
        mux_splicer.drain(mux_reader_);
}
namespace{void nop(){}}

template <class BoolDecoder>
void VP8ComponentDecoder<BoolDecoder>::reset_all_comm_buffers() {
    for (unsigned int thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
        while (!send_to_actual_thread_state.vbuffers[thread_id].empty()) {
            send_to_actual_thread_state.vbuffers[thread_id].pop();
        }
    }
}

template <class BoolDecoder>
CodingReturnValue VP8ComponentDecoder<BoolDecoder>::decode_chunk(UncompressedComponents * const colldata)
{
    mux_splicer.init(this->spin_workers_);
    /* cmpc is a global variable with the component count */


    /* construct 4x4 VP8 blocks to hold 8x8 JPEG blocks */
    if ( this->thread_state_[0] == nullptr || this->thread_state_[0]->context_[0].isNil() ) {
        /* first call */
        BlockBasedImagePerChannel<false> framebuffer;
        framebuffer.memset(0);
        for (size_t i = 0; i < framebuffer.size() && int( i ) < colldata->get_num_components(); ++i) {
            framebuffer[i] = &colldata->full_component_write((BlockType)i);
        }
        Sirikata::Array1d<BlockBasedImagePerChannel<false>, MAX_NUM_THREADS> all_framebuffers;
        for (size_t i = 0; i < all_framebuffers.size(); ++i) {
            all_framebuffers[i] = framebuffer;
        }
        size_t num_threads_needed = initialize_decoder_state(colldata,
                                                             all_framebuffers).size();


        for (size_t i = 0;i < num_threads_needed; ++i) {
            map_logical_thread_to_physical_thread(i, i);
        }
        for (size_t i = 0;i < num_threads_needed; ++i) {
            initialize_thread_id(i, i, framebuffer);
            if (!this->do_threading_) {
                break;
            }
        }
        if (num_threads_needed > NUM_THREADS || num_threads_needed == 0) {
            return CODING_ERROR;
        }
    }
    if (this->do_threading_) {
        reset_all_comm_buffers();
        for (unsigned int physical_thread_id = 0; physical_thread_id < (g_threaded ? getNumWorkers() : 1); ++physical_thread_id) {
            getWorker(physical_thread_id)->work = nop;
        }

        for (unsigned int thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
            unsigned int cur_spin_worker = thread_id;
            if (!this->thread_state_[thread_id]) {
                this->spin_workers_[cur_spin_worker].work
                    = &nop;
            } else {
                this->spin_workers_[cur_spin_worker].work
                    = std::bind(LeptonCodec<BoolDecoder>::worker_thread,
                                this->thread_state_[thread_id],
                                thread_id,
                                colldata,
                                mux_splicer.thread_target,
                                getWorker(cur_spin_worker),
                                &send_to_actual_thread_state);
            }
            this->spin_workers_[cur_spin_worker].activate_work();
        }
        flush();
        for (unsigned int thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
            unsigned int cur_spin_worker = thread_id;
            TimingHarness::timing[thread_id][TimingHarness::TS_THREAD_WAIT_STARTED] = TimingHarness::get_time_us();
            this->spin_workers_[cur_spin_worker].main_wait_for_done();
            TimingHarness::timing[thread_id][TimingHarness::TS_THREAD_WAIT_FINISHED] = TimingHarness::get_time_us();
        }
        // join on all threads
    } else {
        if (virtual_thread_id_ != -1) {
            TimingHarness::timing[0][TimingHarness::TS_ARITH_STARTED] = TimingHarness::get_time_us();
            CodingReturnValue ret = this->thread_state_[0]->vp8_decode_thread(0, colldata);
            if (ret == CODING_PARTIAL) {
                return ret;
            }
            TimingHarness::timing[0][TimingHarness::TS_ARITH_FINISHED] = TimingHarness::get_time_us();
        }
        // wait for "threads"
        virtual_thread_id_ += 1; // first time's a charm
        for (unsigned int thread_id = virtual_thread_id_; thread_id < NUM_THREADS; ++thread_id, ++virtual_thread_id_) {
            BlockBasedImagePerChannel<false> framebuffer;
            framebuffer.memset(0);
            for (size_t i = 0; i < framebuffer.size() && int( i ) < colldata->get_num_components(); ++i) {
                framebuffer[i] = &colldata->full_component_write((BlockType)i);
            }

            initialize_thread_id(thread_id, 0, framebuffer);
            this->thread_state_[0]->bool_decoder_.init(new VirtualThreadPacketReader(thread_id, &mux_reader_, &mux_splicer));
            TimingHarness::timing[thread_id][TimingHarness::TS_ARITH_STARTED] = TimingHarness::get_time_us();
            CodingReturnValue ret;
            if ((ret = this->thread_state_[0]->vp8_decode_thread(0, colldata)) == CODING_PARTIAL) {
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
    write_byte_bill(Billing::DELIMITERS, true, mux_reader_.getOverhead());
    return CODING_DONE;
}

template class VP8ComponentDecoder<VPXBoolReader>;
#ifdef ENABLE_ANS_EXPERIMENTAL
template class VP8ComponentDecoder<ANSBoolReader>;
#endif
