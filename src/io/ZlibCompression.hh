/*  Sirikata Jpeg Texture Transfer -- Texture Transfer management system
 *  ZlibCompression.hpp
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
#ifdef USE_SYSTEM_DEPENDENCIES
#include <zlib.h>
#else
#include "../../dependencies/zlib/zlib.h"
#endif
namespace Sirikata{
class SIRIKATA_EXPORT ZlibDecoderDecompressionReader : public DecoderReader {
protected:
    JpegAllocator<uint8_t> mAlloc;
    unsigned char mReadBuffer[4096];
    z_stream mStream;
    DecoderReader *mBase;
    bool mClosed;
    bool mStreamEndEncountered;
public:
    static std::pair<std::vector<uint8_t,
                                 JpegAllocator<uint8_t> >,
                     JpegError> Decompress(const uint8_t *buffer,
                                             size_t size,
                                           const JpegAllocator<uint8_t> &alloc,
                         size_t max_size);
    ZlibDecoderDecompressionReader(DecoderReader *r, bool concatenated, const JpegAllocator<uint8_t> &alloc);
    virtual std::pair<uint32, JpegError> Read(uint8*data, unsigned int size);
    virtual ~ZlibDecoderDecompressionReader();
    void Close();
};

class SIRIKATA_EXPORT ZlibDecoderCompressionWriter : public DecoderWriter {
    JpegAllocator<uint8_t> mAlloc;
    unsigned char mWriteBuffer[4096];
    z_stream mStream;
    DecoderWriter *mBase;
    bool mClosed;
public:
    static std::vector<uint8_t, JpegAllocator<uint8_t> > Compress(const uint8_t *buffer,
                                                                  size_t size, const JpegAllocator<uint8_t> &alloc);
    // compresison level should be a value: 1 through 9
    ZlibDecoderCompressionWriter(DecoderWriter *w, uint8_t compression_level, const JpegAllocator<uint8_t> &alloc);
    virtual std::pair<uint32, JpegError> Write(const uint8*data, unsigned int size) ;
    virtual ~ZlibDecoderCompressionWriter();
    virtual void Close();
};

}
