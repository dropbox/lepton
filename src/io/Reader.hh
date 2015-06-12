/*  Sirikata Jpeg Reader -- Texture Transfer management system
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
#ifndef _SIRIKATA_READER_HPP_
#define _SIRIKATA_READER_HPP_

#include "Allocator.hh"
#include "Error.hh"
namespace Sirikata {
class SIRIKATA_EXPORT DecoderReader {
public:
    virtual std::pair<uint32, JpegError> Read(uint8*data, unsigned int size) = 0;
    virtual ~DecoderReader(){}
};
class SIRIKATA_EXPORT DecoderWriter {
public:
    virtual std::pair<uint32, JpegError> Write(const uint8*data, unsigned int size) = 0;
    virtual void Close() = 0;
    virtual ~DecoderWriter(){}
};

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
SIRIKATA_FUNCTION_EXPORT JpegError Copy(DecoderReader &r, DecoderWriter &w, const JpegAllocator<uint8> &alloc);
}
#endif
