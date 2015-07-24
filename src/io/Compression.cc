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
#include <assert.h>
#include <string.h>

#include "Compression.hh"

#include <lzma.h>
#if __GXX_EXPERIMENTAL_CXX0X__ || __cplusplus > 199711L
#include <thread>
#define USE_BUILTIN_THREADS
#define USE_BUILTIN_ATOMICS
#else
#include <sirikata/core/util/Thread.hpp>
#endif
#ifdef HAS_LZHAM
#include <lzham.h>
#endif

#ifdef __linux
#include <sys/wait.h>
#include <linux/seccomp.h>

#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

#define THREAD_COMMAND_BUFFER_SIZE 5
//#define LZHAMTEST_DEFAULT_DICT_SIZE 28
#define LZHAMTEST_DEFAULT_DICT_SIZE 24


namespace Sirikata {

uint32 LEtoUint32(const uint8*buffer) {
    uint32 retval = buffer[3];
    retval <<=8;
    retval |= buffer[2];
    retval <<= 8;
    retval |= buffer[1];
    retval <<= 8;
    retval |= buffer[0];
    return retval;
}

void uint32toLE(uint32 value, uint8*retval) {
    retval[0] = uint8(value & 0xff);
    retval[1] = uint8((value >> 8) & 0xff);
    retval[2] = uint8((value >> 16) & 0xff);
    retval[3] = uint8((value >> 24) & 0xff);
}

// 4 headers also defined in MultiCompression.cpp
static const unsigned char lzham_fixed_header[] = {'L','Z','H','0'};


void writeLZHAMHeader(uint8 * output, uint8 dictSize, uint32 fileSize) {
    output[0] = lzham_fixed_header[0];
    output[1] = lzham_fixed_header[1];
    output[2] = lzham_fixed_header[2];
    output[3] = lzham_fixed_header[3];
    output[4] = dictSize;
    output[5] = fileSize & 0xff;
    output[6] = (fileSize >> 8) & 0xff;
    output[7] = (fileSize >> 16) & 0xff;
    output[8] = (fileSize >> 24) & 0xff;
    output[9] = 0;
    output[10] = 0;
    output[11] = 0;
    output[12] = 0;
    
}


#ifdef HAS_LZHAM
std::pair<lzham_decompress_params, JpegError> makeLZHAMDecodeParams(const uint8 header[LZHAM0_HEADER_SIZE]) {
    std::pair<lzham_decompress_params, JpegError> retval;
    retval.second = JpegError::nil();
    memset(&retval.first, 0, sizeof(retval.first));
    if (memcmp(header, "LZH0", 4)) {
        retval.second = MakeJpegError("LZHAM Header Error");
    } else {
        retval.first.m_struct_size = sizeof(lzham_decompress_params);
        retval.first.m_dict_size_log2 = header[4];
    }
    return retval;
}
#endif

std::pair<Sirikata::uint32, Sirikata::JpegError> MemReadWriter::Write(const Sirikata::uint8*data, unsigned int size) {
    using namespace Sirikata;
    mBuffer.insert(mBuffer.begin() + mWriteCursor, data, data + size);
    mWriteCursor += size;
    return std::pair<Sirikata::uint32, JpegError>(size, JpegError());
}
std::pair<Sirikata::uint32, Sirikata::JpegError> MemReadWriter::Read(Sirikata::uint8*data, unsigned int size) {
    using namespace Sirikata;
    size_t bytesLeft = mBuffer.size() - mReadCursor;
    size_t actualBytesRead = size;
    if (bytesLeft < size) {
        actualBytesRead = bytesLeft;
    }
    if (actualBytesRead > 0) {
        memcpy(data, &mBuffer[mReadCursor], actualBytesRead);
    }
    mReadCursor += actualBytesRead;
    JpegError err = JpegError();
    if (actualBytesRead == 0) {
        err = JpegError::errEOF();
    }
    //fprintf(stderr, "%d READ %02x%02x%02x%02x - %02x%02x%02x%02x\n", (uint32)actualBytesRead, data[0], data[1],data[2], data[3],
    //        data[actualBytesRead-4],data[actualBytesRead-3],data[actualBytesRead-2],data[actualBytesRead-1]);

    std::pair<Sirikata::uint32, JpegError> retval(actualBytesRead, err);
    return retval;
}



MagicNumberReplacementReader::MagicNumberReplacementReader(DecoderReader *r,
                                                           const std::vector<uint8_t, JpegAllocator<uint8> >& originalMagic,
                                                           const std::vector<uint8_t, JpegAllocator<uint8> >& newMagic)
        : mOriginalMagic(originalMagic), mNewMagic(newMagic) {
    mBase = r;
    mMagicNumbersReplaced = 0;
    assert(mOriginalMagic.size() == mNewMagic.size() && "Magic numbers must be the same length");
}
std::pair<uint32, JpegError> MagicNumberReplacementReader::Read(uint8*data, unsigned int size){
    std::pair<uint32, JpegError> retval = mBase->Read(data, size);
    for (size_t off = 0;
         mMagicNumbersReplaced < mOriginalMagic.size() && off < size;
         ++mMagicNumbersReplaced, ++off) {
        if (memcmp(data + off, mOriginalMagic.data() + mMagicNumbersReplaced, 1) != 0) {
            retval.second = MakeJpegError("Magic Number Mismatch");
        }
        data[off] = mNewMagic[mMagicNumbersReplaced];
    }
    return retval;
}
MagicNumberReplacementReader::~MagicNumberReplacementReader(){

}

MagicNumberReplacementWriter::MagicNumberReplacementWriter(DecoderWriter *w,
                                                           const std::vector<uint8_t, JpegAllocator<uint8> >& originalMagic,
                                                           const std::vector<uint8_t, JpegAllocator<uint8> >& newMagic)
        : mOriginalMagic(originalMagic), mNewMagic(newMagic) {
    mBase = w;
    mMagicNumbersReplaced = 0;
    assert(mOriginalMagic.size() == mNewMagic.size() && "Magic numbers must be the same length");
}
std::pair<uint32, JpegError> MagicNumberReplacementWriter::Write(const uint8*data, unsigned int size) {
    if (mMagicNumbersReplaced < mOriginalMagic.size()) {
        if (size > mOriginalMagic.size() - mMagicNumbersReplaced) {
            std::vector<uint8> replacedMagic(data, data + size);
            for (size_t off = 0;mMagicNumbersReplaced < mOriginalMagic.size(); ++mMagicNumbersReplaced, ++off) {
                assert(memcmp(mOriginalMagic.data() + mMagicNumbersReplaced, &replacedMagic[off], 1) == 0);
                replacedMagic[off] = mNewMagic[mMagicNumbersReplaced];
            }
            return mBase->Write(&replacedMagic[0], size);
        } else {
            assert(memcmp(data, mOriginalMagic.data() + mMagicNumbersReplaced, size) == 0);
            mMagicNumbersReplaced += size;
            return mBase->Write((uint8*)mNewMagic.data() + (mMagicNumbersReplaced - size),
                                size);
        }
    } else {
        return mBase->Write(data, size);
    }
}
MagicNumberReplacementWriter::~MagicNumberReplacementWriter() {

}
void MagicNumberReplacementWriter::Close() {
    mBase->Close();
}


#ifdef HAS_LZHAM
lzham_compress_params makeLZHAMEncodeParams(int level) {
    lzham_compress_params params;
    memset(&params, 0, sizeof(params));

    params.m_struct_size = sizeof(lzham_compress_params);
    params.m_dict_size_log2 = LZHAMTEST_DEFAULT_DICT_SIZE;//LZHAM_MAX_DICT_SIZE_LOG2_X64;
    switch(level) {
      case 0:
        params.m_level = LZHAM_COMP_LEVEL_FASTEST;
        break;
      case 1:
      case 2:
        params.m_level = LZHAM_COMP_LEVEL_FASTER;
        break;
      case 3:
      case 4:
        params.m_level = LZHAM_COMP_LEVEL_DEFAULT;
        break;
      case 9:
      case 8:
      case 7:
        params.m_level = LZHAM_COMP_LEVEL_UBER;
        break;
      case 5:
      case 6:
      default:
        params.m_level = LZHAM_COMP_LEVEL_BETTER;
    }
    params.m_compress_flags = LZHAM_COMP_FLAG_DETERMINISTIC_PARSING;
    params.m_max_helper_threads = 0;
    return params;
}

LZHAMDecompressionReader::LZHAMDecompressionReader(DecoderReader *r,
                                                       const JpegAllocator<uint8_t> &alloc)
        : mAlloc(alloc) {
    mBase = r;
    mHeaderBytesRead = 0;
    mLzham = NULL;
    mAvailIn = 0;
    mReadOffset = mReadBuffer;
    memset(mHeader, 0xff, sizeof(mHeader));
    if (alloc.get_custom_reallocate() && alloc.get_custom_msize()) {
        lzham_set_memory_callbacks(alloc.get_custom_reallocate(),
                                   alloc.get_custom_msize(),
                                   alloc.get_custom_state());
    }
};

std::pair<uint32, JpegError> LZHAMDecompressionReader::Read(uint8*data,
                                                              unsigned int size){
    
    bool inputEof = false;
    bool outputEof = false; // when we reach the end of one (of possibly many) concatted streams
    size_t outAvail = size;
    while(true) {
        JpegError err = JpegError::nil();
        if (inputEof == false  && mAvailIn < sizeof(mReadBuffer) / 8) {
            if (mAvailIn == 0) {
                mReadOffset = mReadBuffer;
            } else if (mReadOffset - mReadBuffer > (int)sizeof(mReadBuffer) * 3 / 4) {
                // guaranteed not to overlap since it will only be at
                // most 1/8 the size of the buffer
                memcpy(mReadBuffer, mReadOffset, mAvailIn);
                mReadOffset = mReadBuffer;
            }
            size_t toRead = sizeof(mReadBuffer) - (mReadOffset + mAvailIn - mReadBuffer);
            assert(toRead > 0);
            std::pair<uint32, JpegError> bytesRead = mBase->Read(mReadOffset + mAvailIn, toRead);
            mAvailIn += bytesRead.first;
            err = bytesRead.second;
            if (bytesRead.first == 0) {
                inputEof = true;
            }
        }
        if (outputEof && !inputEof) {
            // gotta reset the lzham state
            mHeaderBytesRead = 0;
            outputEof = false;
        }
        if (mHeaderBytesRead < LZHAM0_HEADER_SIZE) {
            if (mHeaderBytesRead + mAvailIn <= LZHAM0_HEADER_SIZE) {
                memcpy(mHeader + mHeaderBytesRead, mReadOffset, mAvailIn);
                mHeaderBytesRead += mAvailIn;
                mAvailIn = 0;
            } else {
                memcpy(mHeader + mHeaderBytesRead, mReadOffset, LZHAM0_HEADER_SIZE - mHeaderBytesRead);
                mAvailIn -= LZHAM0_HEADER_SIZE - mHeaderBytesRead;
                mReadOffset += LZHAM0_HEADER_SIZE - mHeaderBytesRead;
                mHeaderBytesRead = LZHAM0_HEADER_SIZE;
            }
            if (mHeaderBytesRead == LZHAM0_HEADER_SIZE) {
                std::pair<lzham_decompress_params, JpegError> p = makeLZHAMDecodeParams(mHeader);
                if (p.second != JpegError::nil()) {
                    return std::pair<uint32, JpegError>(0, p.second);
                }
                if (mLzham == NULL) {
                    mLzham = lzham_decompress_init(&p.first);
                } else {
                    mLzham = lzham_decompress_reinit((lzham_decompress_state_ptr)mLzham, &p.first);
                }
                assert(mLzham && "the stream decoder had insufficient memory");
            }
        }
        size_t nread = mAvailIn;
        size_t nwritten = outAvail;
        lzham_decompress_status_t status = lzham_decompress(
            (lzham_compress_state_ptr)mLzham,
            mReadOffset, &nread,
            data + size - outAvail, &nwritten,
            inputEof);
        mReadOffset += nread;
        mAvailIn -= nread;
        outAvail -= nwritten;
        if (status >= LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE) {
            if (status == LZHAM_DECOMP_STATUS_SUCCESS) {
                outputEof = true;
            }
            if (status >= LZHAM_DECOMP_STATUS_FIRST_FAILURE_CODE) {
                switch(status) {
                  case LZHAM_DECOMP_STATUS_FAILED_INITIALIZING:
                  case LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL:
                  case LZHAM_DECOMP_STATUS_FAILED_EXPECTED_MORE_RAW_BYTES:
                  case LZHAM_DECOMP_STATUS_FAILED_BAD_CODE:
                  case LZHAM_DECOMP_STATUS_FAILED_ADLER32:
                  case LZHAM_DECOMP_STATUS_FAILED_BAD_RAW_BLOCK:
                  case LZHAM_DECOMP_STATUS_FAILED_BAD_COMP_BLOCK_SYNC_CHECK:
                  case LZHAM_DECOMP_STATUS_INVALID_PARAMETER:
                  default:
                    return std::pair<uint32, JpegError>(size - outAvail, MakeJpegError("LZHAM error"));
                }
                break;
            }
        }
        if (outAvail == 0 || (inputEof && outputEof)) {
            unsigned int write_size = size - outAvail;
            return std::pair<uint32, JpegError>(write_size,JpegError::nil());
        }
    }
    assert(false);//FIXME
    unsigned int write_size = size - outAvail;
    return std::pair<uint32, JpegError>(write_size,JpegError::nil());
    //return std::pair<uint32, JpegError>(0, MakeJpegError("Unreachable"));
}

LZHAMDecompressionReader::~LZHAMDecompressionReader() {
    if (mLzham) {
        lzham_decompress_deinit((lzham_decompress_state_ptr)mLzham);
    }
}




LZHAMCompressionWriter::LZHAMCompressionWriter(DecoderWriter *w,
                                               uint8_t compression_level,
                                               const JpegAllocator<uint8_t> &alloc)
    : mWriteBuffer(alloc) {
    if (alloc.get_custom_reallocate() && alloc.get_custom_msize()) {
        lzham_set_memory_callbacks(alloc.get_custom_reallocate(),
                                   alloc.get_custom_msize(),
                                   alloc.get_custom_state());
    }
    mClosed = false;
    mBase = w;
    lzham_compress_params p = makeLZHAMEncodeParams(compression_level);
    mDictSizeLog2 = p.m_dict_size_log2;
    mLzham = lzham_compress_init(&p);
    assert(mLzham && "Problem with initialization");
    size_t defaultStartBufferSize = 1024 - LZHAM0_HEADER_SIZE;
    mAvailOut = defaultStartBufferSize;
    mWriteBuffer.resize(mAvailOut + LZHAM0_HEADER_SIZE);
    mBytesWritten = 0;
}



void LZHAMCompressionWriter::Close(){
    assert(!mClosed);
    mClosed = true;
    //assert((mBytesWritten >> 24) == 0 && "Expect a small item to compress");
    writeLZHAMHeader(&mWriteBuffer[0], mDictSizeLog2, mBytesWritten);
    std::pair<uint32, JpegError> r = mBase->Write(&mWriteBuffer[0], mWriteBuffer.size() - mAvailOut);
    if (r.second != JpegError::nil()) {
        return; // ERROR
    }
    if (mWriteBuffer.size() < 4 * 65536) {
        mWriteBuffer.resize(4 * 65536);
    }
    while(true) {
        size_t zero = 0;
        size_t nwritten = mWriteBuffer.size();
        lzham_compress_status_t ret = lzham_compress((lzham_compress_state_ptr)mLzham,
                                                     NULL, &zero,
                                                     &mWriteBuffer[0], &nwritten,
                                                     true);
        if (ret == LZHAM_COMP_STATUS_HAS_MORE_OUTPUT) {
            assert(nwritten == mWriteBuffer.size());
        }
        if (nwritten > 0) {
            std::pair<uint32, JpegError> r = mBase->Write(&mWriteBuffer[0], nwritten);
            if (r.second != JpegError::nil()) {
                return;
            }
        }
        if (ret == LZHAM_COMP_STATUS_NOT_FINISHED) {
            continue;
        } else if (ret == LZHAM_COMP_STATUS_HAS_MORE_OUTPUT) {
            continue;
        } else if (ret == LZHAM_COMP_STATUS_SUCCESS) {
            return;
        } else {
            assert(ret != LZHAM_COMP_STATUS_FAILED && "Something went wrong");
            assert(ret != LZHAM_COMP_STATUS_INVALID_PARAMETER && "Something went wrong with param");
            assert(ret != LZHAM_COMP_STATUS_FAILED_INITIALIZING && "Something went wrong with init");
            assert(false && "UNREACHABLE");
            return;
        }
    }
}

std::pair<uint32, JpegError> LZHAMCompressionWriter::Write(const uint8*data,
                                                             unsigned int size){
    size_t availIn = size;
    std::pair<uint32, JpegError> retval (0, JpegError::nil());
    mBytesWritten += size;
    while(availIn > 0) {
        if (mAvailOut == 0) {
            mAvailOut += mWriteBuffer.size();
            mWriteBuffer.resize(mWriteBuffer.size() * 2);
        }
        size_t nread = availIn; // this must be temporarily set to to
                                // the bytes avail
        size_t nwritten = mAvailOut;
        lzham_compress_status_t ret = lzham_compress((lzham_compress_state_ptr)mLzham,
                                                     data + size - availIn, &nread,
                                                     &mWriteBuffer[0] + mWriteBuffer.size() - mAvailOut, &nwritten,
                                                     false);
        mAvailOut -= nwritten;
        availIn -= nread;
        if (ret == LZHAM_COMP_STATUS_NEEDS_MORE_INPUT) {
            assert(availIn == 0);
        }
        if (ret >= LZHAM_COMP_STATUS_FIRST_FAILURE_CODE) {
            assert(false && "LZHAM COMPRESSION FAILED");
        }
    }
    return std::pair<uint32, JpegError>(size - availIn, JpegError::nil());
}

LZHAMCompressionWriter::~LZHAMCompressionWriter() {
    if (!mClosed) {
        Close();
    }
    assert(mClosed);
    lzham_compress_deinit((lzham_compress_state_ptr)mLzham);
}
#endif


DecoderDecompressionReader::DecoderDecompressionReader(DecoderReader *r,
                                                       bool concatenated,
                                                       const JpegAllocator<uint8_t> &alloc)
        : mAlloc(alloc) {
    mClosed = false;
    mStreamEndEncountered = false;
    mBase = r;
    mStream = LZMA_STREAM_INIT;
    mLzmaAllocator.alloc = mAlloc.get_custom_allocate();
    mLzmaAllocator.free = mAlloc.get_custom_deallocate();
    mLzmaAllocator.opaque = mAlloc.get_custom_state();
    mStream.allocator = &mLzmaAllocator;

    lzma_ret ret = lzma_stream_decoder(
        &mStream, UINT64_MAX, concatenated ? LZMA_CONCATENATED : 0);
	mStream.next_in = NULL;
	mStream.avail_in = 0;
    if (ret != LZMA_OK) {
        switch(ret) {
          case LZMA_MEM_ERROR:
            assert(ret == LZMA_OK && "the stream decoder had insufficient memory");
            break;
          case LZMA_OPTIONS_ERROR:
            assert(ret == LZMA_OK && "the stream decoder had incorrect options for the system version");
            break;
          default:
            assert(ret == LZMA_OK && "the stream decoder was not initialized properly");
        }
    }
}

std::pair<uint32, JpegError> DecoderDecompressionReader::Read(uint8*data,
                                                              unsigned int size){
    mStream.next_out = data;
    mStream.avail_out = size;
    while(true) {
        JpegError err = JpegError::nil();
        lzma_action action = LZMA_RUN;
        if (mStream.avail_in == 0) {
            mStream.next_in = mReadBuffer;
            std::pair<uint32, JpegError> bytesRead = mBase->Read(mReadBuffer, sizeof(mReadBuffer));
            mStream.avail_in = bytesRead.first;
            err = bytesRead.second;
            if (bytesRead.first == 0) {
                action = LZMA_FINISH;
            }
        }
        lzma_ret ret = lzma_code(&mStream, action);
        if (mStream.avail_out == 0 || ret == LZMA_STREAM_END) {
            if (ret == LZMA_STREAM_END) {
                mStreamEndEncountered = true;
            }
            unsigned int write_size = size - mStream.avail_out;
            return std::pair<uint32, JpegError>(write_size,JpegError::nil());
/*                                                (ret == LZMA_STREAM_END
                                                 || (ret == LZMA_OK &&write_size > 0))
                                                 ? JpegError::nil() : err);*/
        }
        if (ret != LZMA_OK) {
            switch(ret) {
              case LZMA_FORMAT_ERROR:
                return std::pair<uint32, JpegError>(0, MakeJpegError("Invalid XZ magic number"));
              case LZMA_DATA_ERROR:
              case LZMA_BUF_ERROR:
                return std::pair<uint32, JpegError>(size - mStream.avail_out,
                                                    MakeJpegError("Corrupt xz file"));
              case LZMA_MEM_ERROR:
                assert(false && "Memory allocation failed");
                break;
              default:
                assert(false && "Unknown LZMA error code");
            }
        }
    }
    return std::pair<uint32, JpegError>(0, MakeJpegError("Unreachable"));
}
void DecoderDecompressionReader::Close() {
    if (!mClosed) {
        lzma_end(&mStream);
    }
    mClosed = true;
}
DecoderDecompressionReader::~DecoderDecompressionReader() {
    Close();
}


DecoderCompressionWriter::DecoderCompressionWriter(DecoderWriter *w,
                                                   uint8_t compression_level,
                                                   const JpegAllocator<uint8_t> &alloc)
        : mAlloc(alloc) {
    mClosed = false;
    mBase = w;
    mStream = LZMA_STREAM_INIT;
    mLzmaAllocator.alloc = mAlloc.get_custom_allocate();
    mLzmaAllocator.free = mAlloc.get_custom_deallocate();
    mLzmaAllocator.opaque = mAlloc.get_custom_state();
    mStream.allocator = &mLzmaAllocator;
    lzma_ret ret = lzma_easy_encoder(&mStream, compression_level, LZMA_CHECK_NONE);
	mStream.next_in = NULL;
	mStream.avail_in = 0;
    if (ret != LZMA_OK) {
        switch(ret) {
          case LZMA_MEM_ERROR:
            assert(ret == LZMA_OK && "the stream decoder had insufficient memory");
            break;
          case LZMA_OPTIONS_ERROR:
            assert(ret == LZMA_OK && "the stream decoder had incorrect options for the system version");
            break;            
          case LZMA_UNSUPPORTED_CHECK:
            assert(ret == LZMA_OK && "Specified integrity check but not supported");
          default:
            assert(ret == LZMA_OK && "the stream decoder was not initialized properly");
        }
    }
}


void DecoderCompressionWriter::Close(){
    assert(!mClosed);
    mClosed = true;
    while(true) {
        lzma_ret ret = lzma_code(&mStream, LZMA_FINISH);
        if (mStream.avail_out == 0 || ret == LZMA_STREAM_END) {
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
        if (ret == LZMA_STREAM_END) {
            return;
        }
    }
}

std::pair<uint32, JpegError> DecoderCompressionWriter::Write(const uint8*data,
                                                             unsigned int size){
    mStream.next_out = mWriteBuffer;
    mStream.avail_out = sizeof(mWriteBuffer);
    mStream.avail_in = size;
    mStream.next_in = data;
    std::pair<uint32, JpegError> retval (0, JpegError::nil());
    while(mStream.avail_in > 0) {
        lzma_ret ret = lzma_code(&mStream, LZMA_RUN);
        if (mStream.avail_in == 0 || mStream.avail_out == 0 || ret == LZMA_STREAM_END) {
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

DecoderCompressionWriter::~DecoderCompressionWriter() {
    if (!mClosed) {
        Close();
    }
    assert(mClosed);
}



JpegError Copy(DecoderReader &r, DecoderWriter &w, const JpegAllocator<uint8> &alloc) {
    std::vector<uint8, JpegAllocator<uint8> > buffer(alloc);
    size_t bufferSize = 16384;
    buffer.resize(bufferSize);
    std::pair<uint32, JpegError> ret;
    while (true) {
        ret = r.Read(&buffer[0], bufferSize);
        if (ret.first == 0) {
            w.Close();
            return JpegError::nil();
        }
        uint32 offset = 0;
        std::pair<uint32, JpegError> wret = w.Write(&buffer[offset], ret.first - offset);
        offset += wret.first;
        if (wret.second != JpegError::nil()) {
            w.Close();
            return wret.second;
        }
        if (ret.second != JpegError::nil()) {
            w.Close();
            if (ret.second == JpegError::errEOF()) {
                return JpegError::nil();
            }
            return ret.second;
        }
    }
}


// Decode reads a JPEG image from r and returns it as an image.Image.
JpegError CompressAnyto7Z(DecoderReader &r, DecoderWriter &w, uint8 compression_level, const JpegAllocator<uint8> &alloc) {
    DecoderCompressionWriter cw(&w, compression_level, alloc);
    return Copy(r, cw, alloc);
}
// Decode reads a JPEG image from r and returns it as an image.Image.
JpegError Decompress7ZtoAny(DecoderReader &r, DecoderWriter &w, const JpegAllocator<uint8> &alloc) {
    DecoderDecompressionReader cr(&r, true, alloc);
    return Copy(cr, w, alloc);
}

#if 0
// Decode reads a JPEG image from r and returns it as an image.Image.
JpegError CompressAnytoLZHAM(DecoderReader &r, DecoderWriter &w, uint8 compression_level, const JpegAllocator<uint8> &alloc) {
    LZHAMCompressionWriter cw(&w, compression_level, alloc);
    return Copy(r, cw, alloc);
}
// Decode reads a JPEG image from r and returns it as an image.Image.
JpegError DecompressLZHAMtoAny(DecoderReader &r, DecoderWriter &w, const JpegAllocator<uint8> &alloc) {
    LZHAMDecompressionReader cr(&r, alloc);
    return Copy(cr, w, alloc);
}
#endif
}
