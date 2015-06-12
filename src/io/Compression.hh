/*  Sirikata Jpeg Texture Transfer -- Texture Transfer management system
 *  main.cpp
 *
 *  Copyright (c) 2015, Daniel Reiter Horn
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "Reader.hh"
#include <lzma.h>
namespace Sirikata {
enum {
    MAX_COMPRESSION_THREADS = 16  
};
class SIRIKATA_EXPORT MagicNumberReplacementReader : public DecoderReader {
    uint32 mMagicNumbersReplaced;
    DecoderReader *mBase;
    std::vector<uint8_t, JpegAllocator<uint8_t> > mOriginalMagic;
    std::vector<uint8_t, JpegAllocator<uint8_t> > mNewMagic;
public:
    MagicNumberReplacementReader(DecoderReader *r,
                                 const std::vector<uint8_t, JpegAllocator<uint8_t> >& originalMagic,
                                 const std::vector<uint8_t, JpegAllocator<uint8_t> >& newMagic);
    virtual std::pair<uint32, JpegError> Read(uint8*data, unsigned int size);
    ~MagicNumberReplacementReader();
};
#define LZHAM0_HEADER_SIZE (5 + sizeof(uint64_t))

class SIRIKATA_EXPORT MagicNumberReplacementWriter : public DecoderWriter {
    uint32 mMagicNumbersReplaced;
    DecoderWriter *mBase;
    std::vector<uint8_t, JpegAllocator<uint8_t> > mOriginalMagic;
    std::vector<uint8_t, JpegAllocator<uint8_t> > mNewMagic;
public:
    MagicNumberReplacementWriter(DecoderWriter *w,
                                 const std::vector<uint8_t, JpegAllocator<uint8_t> >& originalMagic,
                                 const std::vector<uint8_t, JpegAllocator<uint8_t> >& newMagic);
    virtual std::pair<uint32, JpegError> Write(const uint8*data, unsigned int size) ;
    virtual ~MagicNumberReplacementWriter();
    virtual void Close();
};

class SIRIKATA_EXPORT LZHAMDecompressionReader : public DecoderReader {
 protected:
    JpegAllocator<uint8_t> mAlloc;
    unsigned char mReadBuffer[4 * 65536];
    unsigned char *mReadOffset;
    size_t mAvailIn;
    void *mLzham;
    DecoderReader *mBase;
    uint8_t mHeader[LZHAM0_HEADER_SIZE];
    unsigned int mHeaderBytesRead;
public:
    LZHAMDecompressionReader(DecoderReader *r, const JpegAllocator<uint8_t> &alloc);
    virtual std::pair<uint32, JpegError> Read(uint8*data, unsigned int size);
    virtual ~LZHAMDecompressionReader();
};

class SIRIKATA_EXPORT LZHAMCompressionWriter : public DecoderWriter {
    std::vector<uint8_t, JpegAllocator<uint8_t> > mWriteBuffer;
    size_t mAvailOut;
    void *mLzham;
    DecoderWriter *mBase;
    size_t mBytesWritten;
    uint8_t mDictSizeLog2;    
    bool mClosed;
public:
    // compresison level should be a value: 1 through 9
    LZHAMCompressionWriter(DecoderWriter *w, uint8_t compression_level, const JpegAllocator<uint8_t> &alloc);
    virtual std::pair<uint32, JpegError> Write(const uint8*data, unsigned int size) ;
    virtual ~LZHAMCompressionWriter();
    virtual void Close();
};


class SIRIKATA_EXPORT DecoderDecompressionReader : public DecoderReader {
protected:
    JpegAllocator<uint8_t> mAlloc;
    unsigned char mReadBuffer[4096];
    lzma_allocator mLzmaAllocator;
    lzma_stream mStream;
    DecoderReader *mBase;
    bool mClosed;
    bool mStreamEndEncountered;
public:
    DecoderDecompressionReader(DecoderReader *r, bool concatenated, const JpegAllocator<uint8_t> &alloc);
    virtual std::pair<uint32, JpegError> Read(uint8*data, unsigned int size);
    virtual ~DecoderDecompressionReader();
    void Close();
};

class SIRIKATA_EXPORT DecoderCompressionWriter : public DecoderWriter {
    JpegAllocator<uint8_t> mAlloc;
    lzma_allocator mLzmaAllocator;
    unsigned char mWriteBuffer[4096];
    lzma_stream mStream;
    DecoderWriter *mBase;
    bool mClosed;
public:
    // compresison level should be a value: 1 through 9
    DecoderCompressionWriter(DecoderWriter *w, uint8_t compression_level, const JpegAllocator<uint8_t> &alloc);
    virtual std::pair<uint32, JpegError> Write(const uint8*data, unsigned int size) ;
    virtual ~DecoderCompressionWriter();
    virtual void Close();
};

SIRIKATA_FUNCTION_EXPORT void writeLZHAMHeader(uint8 * output, uint8 dictSize, uint32 fileSize);

SIRIKATA_FUNCTION_EXPORT JpegError CompressAnyto7Z(DecoderReader &r, DecoderWriter &w,
                                                   uint8 compression_level,
                                                   const JpegAllocator<uint8_t> &alloc);
SIRIKATA_FUNCTION_EXPORT JpegError Decompress7ZtoAny(DecoderReader &r, DecoderWriter &w,
                                                     const JpegAllocator<uint8_t> &alloc);

SIRIKATA_FUNCTION_EXPORT JpegError CompressAnytoLZHAM(DecoderReader &r, DecoderWriter &w,
                                                   uint8 compression_level,
                                                   const JpegAllocator<uint8_t> &alloc);
SIRIKATA_FUNCTION_EXPORT JpegError DecompressLZHAMtoAny(DecoderReader &r, DecoderWriter &w,
                                                     const JpegAllocator<uint8_t> &alloc);

}
