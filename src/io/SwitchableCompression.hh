#ifndef _SIRIKATA_SWITCHABLE_COMPRESSION_HH_
#define _SIRIKATA_SWITCHABLE_COMPRESSION_HH_
#include <assert.h>
#include "Compression.hh"

namespace Sirikata {

class SIRIKATA_EXPORT SwitchableXZBase : public DecoderDecompressionReader {
  public:
    SwitchableXZBase(DecoderReader *r,
                     const JpegAllocator<uint8_t> &alloc) : DecoderDecompressionReader(r, false, alloc) {
    }
    const unsigned char * bufferNextIn() {
        return mStream.next_in;
    }
    unsigned int bufferAvailIn() {
        return mStream.avail_in;
    }
    JpegError bufferFill() {
        if (mStream.avail_in == 0) {
            std::pair<uint32, JpegError> result = mBase->Read(mReadBuffer,
                                                              sizeof(mReadBuffer));
            mStream.next_in = mReadBuffer;
            mStream.avail_in = result.first;
            return result.second;
        } else {
            return JpegError::nil();
        }
    }
    void switchToUncompressed() {
        memmove(mReadBuffer, mStream.next_in, mStream.avail_in);
        mStream.next_in = mReadBuffer;
        while (mStream.avail_in < sizeof(mReadBuffer)) {
            // need to guarantee have a full buffer at this time
            std::pair<uint32, JpegError> read = mBase->Read(mReadBuffer + mStream.avail_in,
                                                            sizeof(mReadBuffer) - mStream.avail_in);
            mStream.avail_in += read.first;
            if (read.first == 0 || read.second != JpegError::nil()) {
                break;
            }
        }
        if (!mStreamEndEncountered) {
            unsigned char nop[1];
            std::pair<uint32, JpegError> fake_read = Read(nop, 1);
            assert(fake_read.first == 0);
        }
        Close();
    }
    void bufferConsume(unsigned int amount) {
        assert(amount <= mStream.avail_in && "We are trying to consume more than avail");
        mStream.avail_in -= amount;
        mStream.next_in += amount;
    }
};

class SIRIKATA_EXPORT SwitchableLZHAMBase : public LZHAMDecompressionReader {
    SwitchableLZHAMBase(DecoderReader *r, const JpegAllocator<uint8_t> &alloc)
        : LZHAMDecompressionReader(r, alloc) {}
    unsigned char * bufferNextIn() {
        return mReadOffset;
    }
    unsigned int bufferAvailIn() {
        return mAvailIn;
    }
    JpegError bufferFill() {
        if (mAvailIn == 0) {
            std::pair<uint32, JpegError> result = mBase->Read(mReadBuffer,
                                                              sizeof(mReadBuffer));
            mReadOffset = mReadBuffer;
            mAvailIn = result.first;
            return result.second;
        } else {
            return JpegError::nil();
        }
    }
    void switchToUncompressed() {
        //noop
    }
    void bufferConsume(unsigned int amount) {
        assert(amount <= mAvailIn && "We are trying to consume more than avail");
        mAvailIn -= amount;
        mReadOffset += amount;
    }
};

class SIRIKATA_EXPORT UncloseableWriterWrapper : public Sirikata::DecoderWriter {
    DecoderWriter *mBase;
    bool mBaseClosed;
  public:
    UncloseableWriterWrapper(DecoderWriter *base) {
        mBase = base;
        mBaseClosed = false;
    }
    std::pair<unsigned int, JpegError> Write(const Sirikata::uint8*data, unsigned int size) {
        if (mBaseClosed) {
            return std::pair<unsigned int, JpegError>(size, JpegError::nil());
        }
        return mBase->Write(data, size);
    }
    void Close() {
        mBaseClosed = true;
    }
    void ResetBase() {
        mBaseClosed = false;
    }
    DecoderWriter* writer() {
        return mBase;
    }
};

template<class VarDecompressionWriter> class SwitchableCompressionWriter : public DecoderWriter {
    UncloseableWriterWrapper mBase;
    VarDecompressionWriter mCompressBase;
    JpegAllocator<uint8_t> mAllocator;
    uint8_t mLevel;
    bool compressing;
    bool has_compressed;
public:
    SwitchableCompressionWriter(DecoderWriter *base, uint8_t compression_level, const JpegAllocator<uint8_t> &alloc) :
        mBase(base),
        mCompressBase(&mBase, compression_level, alloc), mAllocator(alloc) {
        mLevel = compression_level;
        compressing = false;
        has_compressed = false;
    }
    void EnableCompression() {
        mBase.ResetBase();
        if (has_compressed && !compressing) {
            mCompressBase.~VarDecompressionWriter();
            new(&mCompressBase)VarDecompressionWriter(&mBase, mLevel, mAllocator);
        }
        has_compressed = true;
        compressing = true;
    }
    void DisableCompression() {
        mCompressBase.Close();
        compressing = false;
    }
    DecoderWriter *getBase() {
        return mBase.writer();
    }
    std::pair<unsigned int, JpegError> Write(const Sirikata::uint8*data, unsigned int size) {
        if (compressing) {
            //static unsigned int cumulative_count = 0;
            //fprintf(stderr, "Writing %d compressed bytes (%d so far)\n", size, cumulative_count += size);
            return mCompressBase.Write(data, size);
        } else {
            //static unsigned int ucumulative_count = 0;
            //fprintf(stderr, "Writing %d uncompressed bytes (%d so far)\n", size, ucumulative_count += size);
            //fflush(stderr);
            //fwrite(data, size, 1, stderr);
            return mBase.writer()->Write(data, size);
        }
    }
    void Close() {
        if (compressing) {
            mCompressBase.Close();
        }
        compressing = false;
        mBase.Close();
        mBase.writer()->Close();
    }
};
template<class VarDecompressionReader> class SwitchableDecompressionReader : public DecoderReader {
    VarDecompressionReader mCompressBase;
    bool decompressing;
public:
    SwitchableDecompressionReader(DecoderReader *base, const JpegAllocator<uint8_t> &alloc) :
        mCompressBase(base, alloc) {
        decompressing = false;
    }
    void EnableCompression() {
        decompressing = true;
        
    }
    void DisableCompression() {
        if (decompressing) {
            mCompressBase.switchToUncompressed();
        }
        decompressing = false;
    }
    std::pair<unsigned int, JpegError> Read(Sirikata::uint8*data, unsigned int size) {
        if (decompressing) {
            auto retval = mCompressBase.Read(data, size);
            //static unsigned int cumulative_count = 0;
            //fprintf(stderr, "Reading %d compressed bytes (%d so far)\n", retval.first, cumulative_count += retval.first);
            return retval;
        }
        unsigned int read_so_far = 0;
        while (read_so_far < size) {
            if (!mCompressBase.bufferAvailIn()) {
                JpegError err = mCompressBase.bufferFill();
                if (err != JpegError::nil()) {
                    return std::pair<unsigned int, JpegError>(read_so_far, err);
                }
            }
            unsigned int amount_to_read = std::min(size - read_so_far, mCompressBase.bufferAvailIn());
            memcpy(data + read_so_far, mCompressBase.bufferNextIn(), amount_to_read);
            mCompressBase.bufferConsume(amount_to_read);
            read_so_far += amount_to_read;
        }
        //static unsigned int ucumulative_count = 0;
        //fprintf(stderr, "Reading %d uncompressed bytes (%d so far)\n", read_so_far, ucumulative_count += read_so_far);
        //fflush(stderr);
        //fwrite(data, read_so_far, 1, stderr);
        return std::pair<unsigned int, JpegError>(read_so_far, JpegError::nil());
    }
};
}
#endif
