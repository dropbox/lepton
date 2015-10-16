#include "Reader.hh"

namespace Sirikata {

template<uint32_t bufferSize> class BufferedReader : public DecoderReader {
    uint8_t *mOffset;
    DecoderReader *mBase;
    uint8_t mBuffer[bufferSize];
    uint8_t *end() {
        return mBuffer + bufferSize;
    }
public:
    BufferedReader(DecoderReader *base) {
        mBase = base;
        mOffset = end();
    }
    void init(DecoderReader *base) {
        mBase = base;
    }
    std::pair<uint32, JpegError> Read(uint8*data, unsigned int size) {
        uint32_t remaining = end() - mOffset;
        if (!remaining) {
            if (size >= (bufferSize >> 1)) {
                return mBase->Read(data, size); //buffering won't help much here
            }
            std::pair<uint32, JpegError> hasRead = mBase->Read(mBuffer, bufferSize);
            if (!hasRead.first) {
                return hasRead;
            }
            mOffset = end() - hasRead.first;
            if (hasRead.first < bufferSize) {
                memmove(mOffset, mBuffer, hasRead.first);
            }
            remaining = hasRead.first;
            if (hasRead.second) {
                hasRead.first = 0;
                return hasRead;
            }
        }
        uint32_t toRead = std::min(size, remaining);
        memcpy(data, mOffset, toRead);
        mOffset += toRead;
        return std::pair<uint32, JpegError>(toRead, JpegError::nil());
    }
    ~BufferedReader(){}
};
template<uint32_t bufferSize>class BufferedWriter {
    uint8_t *mOffset;
    uint8_t mBuffer[bufferSize];
    DecoderWriter *mBase;
    // writers are guaranteed to consume full data or error
    std::pair<uint32, JpegError> WriteFull(const uint8*data, unsigned int size) {
        return mBase->Write(data, size);
    }
public:
    BufferedWriter(DecoderWriter *base) {
        mBase = base;
        mOffset = mBuffer;
    }
    void init(DecoderWriter *base) {
        mBase = base;
    }
    std::pair<uint32, JpegError> Write(const uint8*data, unsigned int size) {
        if (size > bufferSize) {
            std::pair<uint32, JpegError> retval = WriteFull(mBuffer, mOffset - mBuffer);
            mOffset = mBuffer;
            if (retval.second != JpegError::nil()) {
                return std::pair<uint32, JpegError>(0, retval.second);
            }
            return WriteFull(data, size);
        }
        uint32 origSize = size;
        uint32 bytesLeft = bufferSize - (mOffset - mBuffer);
        uint32 toWrite = std::min(size, bytesLeft);
        memcpy(mOffset, data, toWrite);
        mOffset += toWrite;
        if (toWrite == bytesLeft) {
            std::pair<uint32, JpegError> retval = WriteFull(mBuffer, bufferSize);
            if (retval.second != JpegError::nil()) {
                if (retval.first > bytesLeft) {
                    retval.first = retval.first - bytesLeft;
                    return retval;
                }
                retval.first = 0;
                return retval;
            }
            mOffset = mBuffer;
        }
        data += toWrite;
        size -= toWrite;
        if (size) {
            memcpy(mOffset, data, size);
            mOffset += size;
        }
        return std::pair<uint32, JpegError>(origSize, JpegError::nil()); 
    }
    virtual void Close(){
        if (mOffset != mBuffer) {
            std::pair<uint32, JpegError> retval = WriteFull(mBuffer, mOffset - mBuffer);
            (void)retval;
            mOffset = mBuffer;
        }
    }
    virtual ~BufferedWriter(){
        Close();
    }
};




}
