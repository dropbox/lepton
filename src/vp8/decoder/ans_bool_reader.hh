#include "billing.hh"
#include "../model/numeric.hh"
#include "boolreader.hh"
#include "../../ans/rans64.hh"
class ANSBoolReader {
    Rans64State r0;
    Rans64State r1;
    uint32_t *mPptr;
    PacketReader *mReader;
    uint32_t mBuffer[32];
    const uint8_t *mLastPacketReadPtr;
    ROBuffer mLastPacket;
    void fill() {
        assert(mPptr == &mBuffer[sizeof(mBuffer)/sizeof(mBuffer[0])]);
        uint8_t local_buffer[sizeof(mBuffer)];
        uint8_t *write_ptr = &local_buffer[0];
        uint8_t *end_ptr = &local_buffer[sizeof(mBuffer)];
        while (write_ptr != end_ptr) {
            if(mLastPacketReadPtr == mLastPacket.second) {
                if (mReader->eof()) {
                    break; // done with reading
                }
                if (mLastPacket.first){
                    mReader->setFree(mLastPacket);
                }
                mLastPacket = mReader->getNext();
            }
            size_t last_packet_size = mLastPacket.second - mLastPacketReadPtr;
            size_t to_copy = last_packet_size;
            if (end_ptr - write_ptr < (ptrdiff_t)last_packet_size) {
                to_copy = end_ptr - write_ptr;
            }
            if (to_copy) {
                memcpy(write_ptr, mLastPacketReadPtr, to_copy);
            }
            write_ptr += to_copy;
            mLastPacketReadPtr += to_copy;
        }
        if (__builtin_expect(write_ptr != end_ptr, 0)) {
            memset(write_ptr, 0, end_ptr - write_ptr);
        }
        static_assert(sizeof(mBuffer) == sizeof(local_buffer),
                      "Packet buffer and local buffer must be same size");
        memcpy(mBuffer, local_buffer, sizeof(mBuffer));
    }
public:
    void init (PacketReader *pr) {
        mLastPacket.first = mLastPacket.second = NULL;
        mLastPacketReadPtr = NULL;
        memset(mBuffer,0,sizeof(mBuffer));
        this->mReader = pr;
        mPptr = &mBuffer[sizeof(mBuffer)/sizeof(mBuffer[0])];
        fill();
        static_assert(sizeof(mBuffer) > 2 * sizeof(r0),
                      "Must have sufficient room to hold stream initialization parameters and one decode");
        Rans64DecInit(&r0, &mPptr);
        Rans64DecInit(&r1, &mPptr);
    }
    ANSBoolReader() {
        memset(this, 0, sizeof(*this));
    }
    ANSBoolReader(const uint8_t *buffer, size_t size) {
        init(new TestPacketReader(buffer,
                                  buffer + size));
    }
    ANSBoolReader(PacketReader *pr) {
        init(pr);
    }
#ifndef _WIN32
    __attribute__((always_inline))
#endif
    bool get(Branch &branch, Billing bill=Billing::RESERVED) {
        Rans64State local_state = r0;
        r0 = r1;
        uint32_t cumulative_freq = Rans64DecGet(&local_state, 9);
        uint32_t prob = branch.prob();
        prob <<= 1;
        prob += 1;
        bool retval = cumulative_freq >= prob;
        uint32_t start = prob & (-(int32_t)retval);
        uint32_t freq = retval ? 512 - prob: prob;
        Rans64DecAdvance(&local_state, &mPptr, start, freq, 9);
        r1 = local_state;
        if (__builtin_expect(mPptr == &mBuffer[sizeof(mBuffer)/sizeof(mBuffer[0])], 0)) {
            fill();
        }
        return retval;
    }
};

