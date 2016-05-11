#include "Reader.hh"
#include "MuxReader.hh"
namespace Sirikata {
class SIRIKATA_EXPORT BoundedMemWriter : public Sirikata::DecoderWriter {
    MuxReader::ResizableByteBuffer mBuffer;
    size_t mWriteCursor;
    size_t mNumBytesAttemptedToWrite;
  public:
    BoundedMemWriter(const JpegAllocator<uint8_t> &alloc = JpegAllocator<uint8_t>()) : mBuffer(alloc){
        mWriteCursor = 0;
        mNumBytesAttemptedToWrite = 0;
    }
    size_t get_bound () const{
        return mBuffer.size();
    }
    void set_bound(size_t bound) {
        mBuffer.resize(bound);
        mWriteCursor = std::min(bound, mWriteCursor);
    }
    void Reset() {
        mWriteCursor = 0;
        mNumBytesAttemptedToWrite = 0;
    }
    void Close() {
        mWriteCursor = 0;
        mNumBytesAttemptedToWrite = 0;
    }
    virtual std::pair<Sirikata::uint32, Sirikata::JpegError> Write(const Sirikata::uint8*data,
                                                                   unsigned int size) {
        mNumBytesAttemptedToWrite += size;
        unsigned int bounded_size = 0;
        if (mBuffer.size() > mWriteCursor) {
            bounded_size = (unsigned int)std::min((size_t)size,
                                                               mBuffer.size() - mWriteCursor);
            memcpy(&mBuffer[mWriteCursor], data, bounded_size);
        }
        Sirikata::JpegError err = Sirikata::JpegError::nil();
        if (bounded_size != size) {
            err = Sirikata::JpegError::errEOF();
        }
        mWriteCursor += bounded_size;
        return std::pair<Sirikata::uint32, Sirikata::JpegError>(bounded_size,
                                                                err);
    }
    MuxReader::ResizableByteBuffer &buffer() {
        return mBuffer;
    }
    size_t bytes_written() const{
        return mWriteCursor;
    }
    const MuxReader::ResizableByteBuffer &buffer() const{
        return mBuffer;
    }
    bool has_exceeded_bound() const { // equivalent to an EOF...needs a write
        return mBuffer.size() < mNumBytesAttemptedToWrite;
    }
    bool has_reached_bound() const { // equivalent to an EOF...needs a write
        return mBuffer.size() <= mNumBytesAttemptedToWrite;
    }
    void write(const void *data, unsigned int size) {
        Write((const Sirikata::uint8*)data, size);
    }
    void write_byte(Sirikata::uint8 data) {
        Write(&data, 1);
    }
};
}
