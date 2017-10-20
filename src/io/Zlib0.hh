/*  Sirikata Jpeg Texture Transfer -- Zlib implementation
 *  Zlib0.hpp
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
namespace Sirikata {
#define ZLIB_CHUNK_HEADER_LEN 5
/**
 * Writes a zlib compression stream given an input
 * Currently only supports nop mode
 */
class SIRIKATA_EXPORT Zlib0Writer : public DecoderWriter {
    DecoderWriter *mBase;
    // currently the system only works for a preconceived filesize
    uint32_t mAdler32; // adler32 sum
    bool mClosed;
    uint16_t mBilledBytesLeft;
    std::pair<uint32, JpegError> writeHeader();

    uint8_t mBuffer[(1<<16) + ZLIB_CHUNK_HEADER_LEN + 4];
    size_t mBufferPos;
    size_t mWritten;
  public:
    Zlib0Writer(DecoderWriter * stream, int level);
    virtual std::pair<uint32, JpegError> Write(const uint8*data, unsigned int size);
    virtual ~Zlib0Writer();
    /// writes the adler32 sum
    virtual void Close();
    static size_t getCompressedSize(size_t originalSize);
};

}
