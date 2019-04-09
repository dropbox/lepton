/*  Sirikata Jpeg Texture Transfer -- Texture Transfer management system
 *  ZlibCompression.cpp
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

#include "ZlibCompression.hh"

namespace Sirikata {
JpegAllocator<uint8_t> alloc;

void * custom_zallocator(void * opaque2, unsigned int nmemb, unsigned int size) {
    const JpegAllocator<uint8_t>*sub_opaque = (const JpegAllocator<uint8_t>*)opaque2;
    return sub_opaque->get_custom_allocate()(sub_opaque->get_custom_state(), nmemb, size);
}
void custom_zdeallocator(void * opaque2, void * data) {
    const JpegAllocator<uint8_t>*sub_opaque = (const JpegAllocator<uint8_t>*)opaque2;
    sub_opaque->get_custom_deallocate()(sub_opaque->get_custom_state(), data);
}
std::vector<uint8_t,
            JpegAllocator<uint8_t> > ZlibDecoderCompressionWriter::Compress(const uint8_t *buffer,
                                                                        size_t size,
                                                                        const JpegAllocator<uint8_t> &alloc) {
    z_stream strm;
    memset(&strm, 0, sizeof(z_stream));
    JpegAllocator<uint8_t> local_alloc;
    strm.zalloc = &custom_zallocator;
    strm.zfree = &custom_zdeallocator;
    strm.opaque = &local_alloc;
    strm.next_in = (Bytef*)buffer;
    int ret = deflateInit(&strm, 9);
    if (ret != Z_OK) {
        always_assert(false && "LZMA Incorrectly installed");
        exit(1); // lzma not installed properly
    }
    strm.avail_in = size;
    std::vector<uint8_t, JpegAllocator<uint8_t> > retval(alloc);
    retval.resize(compressBound(size));
    strm.next_out = retval.data();
    strm.avail_out = retval.size();
    ret = deflate(&strm, Z_NO_FLUSH);
    while(true) {
        if (ret == Z_STREAM_END) {
            retval.resize(retval.size() - strm.avail_out);
            deflateEnd(&strm);
            break;
        }
        ret = deflate(&strm, Z_FINISH);
    }
    deflateEnd(&strm);
    return retval;
}
std::pair<std::vector<uint8_t, JpegAllocator<uint8_t> >,
          JpegError > ZlibDecoderDecompressionReader::Decompress(const uint8_t *buffer, size_t size, const JpegAllocator<uint8_t> &alloc,
                                                                 size_t max_file_size) {
    z_stream strm;
    memset(&strm, 0, sizeof(z_stream));
    JpegAllocator<uint8_t> local_alloc;
    strm.zalloc = &custom_zallocator;
    strm.zfree = &custom_zdeallocator;
    strm.opaque = &local_alloc;
    std::pair<std::vector<uint8_t, JpegAllocator<uint8_t> >, JpegError> retval (std::vector<uint8_t, JpegAllocator<uint8_t> >(alloc),
                                                      JpegError::nil());
    retval.first.resize(size * 2);
    size_t retval_size  = 0;
    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        retval.second = JpegError::errShortHuffmanData();
        retval.first.clear();
    } else {
        size_t avail_bytes = retval.first.size();
        strm.next_in = (Bytef*)buffer;
        strm.avail_in = size;
        strm.next_out = retval.first.data();
        strm.avail_out = avail_bytes;
        while(true) {
            ret = inflate(&strm, strm.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH);
            if (ret == Z_STREAM_END) {
                retval_size += avail_bytes - strm.avail_out;
                if (strm.avail_in != 0) {
                    retval.second = JpegError::errShortHuffmanData();
                    break;
                }
                retval.first.resize(retval_size);
                inflateEnd(&strm);
                break;
            }
            if (ret != Z_OK) {
                retval.second = JpegError::errShortHuffmanData();
                break;
            }
            if (strm.avail_out == 0) {
                retval_size += avail_bytes - strm.avail_out;
                if (retval.first.size() >= max_file_size) {
                    retval.second = JpegError::errShortHuffmanData();
                    break;
                }
                retval.first.resize(std::min(retval.first.size() * 2, max_file_size));
                avail_bytes = retval.first.size() - retval_size;

                strm.next_out = retval.first.data() + retval_size;
                strm.avail_out = avail_bytes;
            }
        }
    }
    return retval;
}
ZlibDecoderDecompressionReader::ZlibDecoderDecompressionReader(DecoderReader *r,
                                                       bool concatenated,
                                                       const JpegAllocator<uint8_t> &alloc)
        : mAlloc(alloc) {
    mClosed = false;
    mStreamEndEncountered = false;
    mBase = r;
    z_stream tmp;
    memset(&tmp, 0, sizeof(z_stream));
    mStream = tmp;
    mStream.zalloc = &custom_zallocator;
    mStream.zfree = &custom_zdeallocator;
    mStream.opaque = &mAlloc;

    int ret = inflateInit(&mStream);
	mStream.next_in = NULL;
	mStream.avail_in = 0;
    if (ret != Z_OK) {
        switch(ret) {
          case Z_MEM_ERROR:
            always_assert(ret == Z_OK && "the stream decoder had insufficient memory");
            break;
          default:
            always_assert(ret == Z_OK && "the stream decoder was not initialized properly");
        }
    }
}

std::pair<uint32, JpegError> ZlibDecoderDecompressionReader::Read(uint8*data,
                                                              unsigned int size){
    mStream.next_out = data;
    mStream.avail_out = size;
    while(true) {
        JpegError err = JpegError::nil();
        int action = Z_NO_FLUSH;
        if (mStream.avail_in == 0) {
            mStream.next_in = mReadBuffer;
            std::pair<uint32, JpegError> bytesRead = mBase->Read(mReadBuffer, sizeof(mReadBuffer));
            mStream.avail_in = bytesRead.first;
            err = bytesRead.second;
            if (bytesRead.first == 0) {
                action = Z_FINISH;
            }
        }
        int ret = inflate(&mStream, action);
        if (mStream.avail_out == 0 || ret == Z_STREAM_END) {
            if (ret == Z_STREAM_END) {
                mStreamEndEncountered = true;
            }
            unsigned int write_size = size - mStream.avail_out;
            return std::pair<uint32, JpegError>(write_size,JpegError::nil());
/*                                                (ret == LZMA_STREAM_END
                                                 || (ret == LZMA_OK &&write_size > 0))
                                                 ? JpegError::nil() : err);*/
        }
        if (ret != Z_OK) {
            switch(ret) {
              case Z_STREAM_ERROR:
                return std::pair<uint32, JpegError>(0, MakeJpegError("Invalid XZ magic number"));
              case Z_DATA_ERROR:
              case Z_BUF_ERROR:
                return std::pair<uint32, JpegError>(size - mStream.avail_out,
                                                    MakeJpegError("Corrupt xz file"));
              case Z_MEM_ERROR:
                always_assert(false && "Memory allocation failed");
                break;
              default:
                always_assert(false && "Unknown LZMA error code");
            }
        }
    }
    return std::pair<uint32, JpegError>(0, MakeJpegError("Unreachable"));
}
void ZlibDecoderDecompressionReader::Close() {
    if (!mClosed) {
        inflateEnd(&mStream);
    }
    mClosed = true;
}
ZlibDecoderDecompressionReader::~ZlibDecoderDecompressionReader() {
    Close();
}


ZlibDecoderCompressionWriter::ZlibDecoderCompressionWriter(DecoderWriter *w,
                                                   uint8_t compression_level,
                                                   const JpegAllocator<uint8_t> &alloc)
        : mAlloc(alloc) {
    mClosed = false;
    mBase = w;
    z_stream tmp;
    memset(&tmp, 0, sizeof(z_stream));
    mStream = tmp;
    mStream.zalloc = &custom_zallocator;
    mStream.zfree = &custom_zdeallocator;
    mStream.opaque = &mAlloc;
    int ret = deflateInit(&mStream, compression_level);
	mStream.next_in = NULL;
	mStream.avail_in = 0;
    if (ret != Z_OK) {
        switch(ret) {
          case Z_MEM_ERROR:
            always_assert(ret == Z_OK && "the stream decoder had insufficient memory");
            break;
          case Z_STREAM_ERROR:
            always_assert(ret == Z_OK && "Specified integrity check but not supported");
          default:
            always_assert(ret == Z_OK && "the stream decoder was not initialized properly");
        }
    }
}


void ZlibDecoderCompressionWriter::Close(){
    always_assert(!mClosed);
    mClosed = true;
    while(true) {
        int ret = deflate(&mStream, Z_FINISH);
        if (mStream.avail_out == 0 || ret == Z_STREAM_END) {
            size_t write_size = sizeof(mWriteBuffer) - mStream.avail_out;
            if (write_size > 0) {
                std::pair<uint32, JpegError> r = mBase->Write(mWriteBuffer, write_size);
                if (r.second != JpegError::nil()) {
                    return;
                }
                mStream.avail_out = sizeof(mWriteBuffer);
                mStream.next_out = mWriteBuffer;
            }
        }
        if (ret == Z_STREAM_END) {
            return;
        }
    }
}

std::pair<uint32, JpegError> ZlibDecoderCompressionWriter::Write(const uint8*data,
                                                             unsigned int size){
    mStream.next_out = mWriteBuffer;
    mStream.avail_out = sizeof(mWriteBuffer);
    mStream.avail_in = size;
    mStream.next_in = (Bytef*)data;
    std::pair<uint32, JpegError> retval (0, JpegError::nil());
    while(mStream.avail_in > 0) {
        int ret = deflate(&mStream, Z_NO_FLUSH);
        if (mStream.avail_in == 0 || mStream.avail_out == 0 || ret == Z_STREAM_END) {
            size_t write_size = sizeof(mWriteBuffer) - mStream.avail_out;
            if (write_size > 0) {
                std::pair<uint32, JpegError> r = mBase->Write(mWriteBuffer, write_size);
                mStream.avail_out = sizeof(mWriteBuffer);
                mStream.next_out = mWriteBuffer;
                retval.first += r.first;
                if (r.second != JpegError::nil()) {
                    retval.second = r.second;
                    return retval;
                }
            }
        }
    }
    return retval;
}

ZlibDecoderCompressionWriter::~ZlibDecoderCompressionWriter() {
    if (!mClosed) {
        Close();
    }
    always_assert(mClosed);
}



}
