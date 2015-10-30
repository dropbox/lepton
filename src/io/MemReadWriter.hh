#include "Reader.hh"
namespace Sirikata {
class SIRIKATA_EXPORT MemReadWriter : public Sirikata::DecoderWriter, public Sirikata::DecoderReader {
    std::vector<Sirikata::uint8, JpegAllocator<uint8_t> > mBuffer;
    size_t mReadCursor;
    size_t mWriteCursor;
  public:
    MemReadWriter(const JpegAllocator<uint8_t> &alloc) : mBuffer(alloc){
        mReadCursor = 0;
        mWriteCursor = 0;
    }
    void Close() {
        mReadCursor = 0;
        mWriteCursor = 0;
    }
    void SwapIn(std::vector<Sirikata::uint8, JpegAllocator<uint8_t> > &buffer, size_t offset) {
        mReadCursor = offset;
        mWriteCursor = buffer.size();
        buffer.swap(mBuffer);
    }
    void CopyIn(const std::vector<Sirikata::uint8, JpegAllocator<uint8_t> > &buffer, size_t offset) {
        mReadCursor = offset;
        mWriteCursor = buffer.size();
        mBuffer = buffer;
    }
    virtual std::pair<Sirikata::uint32, Sirikata::JpegError> Write(const Sirikata::uint8*data, unsigned int size);
    virtual std::pair<Sirikata::uint32, Sirikata::JpegError> Read(Sirikata::uint8*data, unsigned int size);
    std::vector<Sirikata::uint8, JpegAllocator<uint8_t> > &buffer() {
        return mBuffer;
    }
    const std::vector<Sirikata::uint8, JpegAllocator<uint8_t> > &buffer() const{
        return mBuffer;
    }
};
}
