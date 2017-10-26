#include "jpgcoder.hh"
#include "model.hh"
#include "bool_decoder.hh"
#include "base_coders.hh"
#include "../../io/MuxReader.hh"


struct ResizableByteBufferListNode : public Sirikata::MuxReader::ResizableByteBuffer {
    uint8_t stream_id;
    ResizableByteBufferListNode *next;
    ResizableByteBufferListNode(){
        stream_id = 0;
        next = NULL;
    }
};
struct ResizableByteBufferList {
    ResizableByteBufferListNode * head;
    ResizableByteBufferListNode * tail;
    ResizableByteBufferList() {
        head = NULL;
        tail = NULL;
    }
    void push(ResizableByteBufferListNode * item) {
        always_assert(item);
        if (!tail) {
            always_assert(!head);
            head = tail = item;
        } else {
            always_assert(!tail->next);
            tail->next = item;
            tail = item;
        }
    }
    bool empty()const {
        return head == NULL && tail == NULL;
    }
    bool size_gt_1()const {
        return head != tail;
    }
    ResizableByteBufferListNode * front() {
        return head;
    }
    const ResizableByteBufferListNode * front() const{
        return head;
    }
    ResizableByteBufferListNode * pop() {
        always_assert(!empty());
        auto retval = head;
        if (tail == head) {
            always_assert(tail->next == NULL);
            head = NULL;
            tail = NULL;
            return retval;
        }
        head = head->next;
        return retval;
    }
};


class VP8ComponentDecoder_SendToVirtualThread {
    ResizableByteBufferList vbuffers[Sirikata::MuxReader::MAX_STREAM_ID];
    
    GenericWorker *all_workers;
    bool eof;
    void set_eof();
public:
    int8_t thread_target[Sirikata::MuxReader::MAX_STREAM_ID]; // 0 is the current thread
    VP8ComponentDecoder_SendToVirtualThread();
    void init(GenericWorker *generic_workers);
    
    void bind_thread(uint8_t virtual_thread_id, int8_t physical_thread_id) {
        thread_target[virtual_thread_id] = physical_thread_id;
    }
    void send(ResizableByteBufferListNode *data);
    void drain(Sirikata::MuxReader&reader);
    
    ResizableByteBufferListNode* read(Sirikata::MuxReader&reader, uint8_t stream_id);
    void read_all(Sirikata::MuxReader&reader);
};
class VP8ComponentDecoder_SendToActualThread {
public:
    ResizableByteBufferList vbuffers[Sirikata::MuxReader::MAX_STREAM_ID];
};

class ActualThreadPacketReader : public PacketReader{
    GenericWorker *worker;
    VP8ComponentDecoder_SendToActualThread *base;
    uint8_t stream_id;
    ResizableByteBufferListNode* last;
public:
    ActualThreadPacketReader(uint8_t stream_id, GenericWorker*worker, VP8ComponentDecoder_SendToActualThread*base) {
        this->worker = worker;
        this->stream_id = stream_id;
        this->base = base;
        this->last = NULL;
    }
    // returns a buffer with at least sizeof(BD_VALUE) before it
    virtual ROBuffer getNext() {
        if (!base->vbuffers[stream_id].empty()) {
            auto retval = base->vbuffers[stream_id].front();
            if (!retval->empty()) {
                base->vbuffers[stream_id].pop();
            }
            if (retval->empty()) {
                isEof = true;
                return {NULL, NULL};
            }
            return {retval->data(), retval->data() + retval->size()};
        }
        while(!isEof) {
            auto dat = worker->batch_recv_data();
            for (unsigned int i = 0; i < dat.count; ++i) {
                ResizableByteBufferListNode* lnode = (ResizableByteBufferListNode*) dat.data[i];
                if (dat.count == 1 && lnode->stream_id == stream_id && lnode && lnode->size()) {
                    assert(stream_id == lnode->stream_id);
                    last = lnode;
                    return {lnode->data(), lnode->data() + lnode->size()};
                } else {
                    base->vbuffers[lnode->stream_id].push(lnode);
                }
            }
            if (!base->vbuffers[stream_id].empty()) {
                return getNext(); // recursive call, 1 deep
            }
            if (dat.return_code < 0) {
                isEof = true; // hmm... should we bail here?
                always_assert(false);
            }
        }
        return {NULL, NULL};
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
    virtual ~ActualThreadPacketReader(){}
};
