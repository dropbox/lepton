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

#include "BrotliCompression.hh"
#include <brotli/encode.h>
#include <brotli/decode.h>

namespace Sirikata {

void * custom_ballocator(void * opaque2, size_t size) {
    const JpegAllocator<uint8_t>*sub_opaque = (const JpegAllocator<uint8_t>*)opaque2;
    return sub_opaque->get_custom_allocate()(sub_opaque->get_custom_state(), 1, size);
}
void custom_bdeallocator(void * opaque2, void * data) {
    const JpegAllocator<uint8_t>*sub_opaque = (const JpegAllocator<uint8_t>*)opaque2;
    sub_opaque->get_custom_deallocate()(sub_opaque->get_custom_state(), data);
}
std::vector<uint8_t,
            JpegAllocator<uint8_t> > BrotliCodec::Compress(const uint8_t *buffer,
                                                                        size_t size,
                                                           const JpegAllocator<uint8_t> &alloc,
                                                           int quality) {
    JpegAllocator<uint8_t> local_alloc(alloc);
    BrotliEncoderState * state = BrotliEncoderCreateInstance(&custom_ballocator,
                                                             &custom_bdeallocator,
                                                             &local_alloc);
    BrotliEncoderSetParameter(state, BROTLI_PARAM_SIZE_HINT, size);
    BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, quality);
    uint32_t lgwin = 1;
    size_t tmp = size;
    while (tmp) {
        tmp >>= 1;
        lgwin += 1;
    }
    if (lgwin < BROTLI_MIN_WINDOW_BITS) {
        lgwin = BROTLI_MIN_WINDOW_BITS;
    }
    if (lgwin > BROTLI_MAX_WINDOW_BITS) {
        lgwin = BROTLI_MAX_WINDOW_BITS;
    }
    BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, lgwin);
    BrotliEncoderSetParameter(state, BROTLI_PARAM_LGBLOCK, lgwin);
    
    std::vector<uint8_t,
                JpegAllocator<uint8_t> > retval(local_alloc);
    retval.resize(BrotliEncoderMaxCompressedSize(size));
    uint8_t*obuffer = &retval[0];
    size_t avail_out = retval.size();
    uint8_t*next_out = obuffer;
    size_t total_out = 0;
    while (true) {
        if (BrotliEncoderCompressStream(
               state,
               size == 0 ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS,
               &size,
               &buffer,
               &avail_out,
               &next_out,
               &total_out)) {
            if (size == 0 && BrotliEncoderIsFinished(state)) {
                break;
            }
        } else {
            custom_exit(ExitCode::MALLOCED_NULL);
        }
    }
    
    
    BrotliEncoderDestroyInstance(state);
    retval.resize(next_out - &retval[0]);
    always_assert(retval.size() == total_out);
    return retval;
}
std::pair<std::vector<uint8_t, JpegAllocator<uint8_t> >,
          JpegError > BrotliCodec::Decompress(const uint8_t *buffer, size_t size, const JpegAllocator<uint8_t> &alloc,
              size_t max_size) {
    JpegAllocator<uint8_t> local_alloc(alloc);
    std::pair<std::vector<uint8_t, JpegAllocator<uint8_t> >, JpegError> retval (std::vector<uint8_t, JpegAllocator<uint8_t> >(alloc),
                                                      JpegError::nil());
    BrotliDecoderState * state = BrotliDecoderCreateInstance(&custom_ballocator,
                                                             &custom_bdeallocator,
                                                             &local_alloc);
    if (!state) {
        retval.second = JpegError::errShortHuffmanData();
        return retval;        
    }
    retval.first.resize(size * 2);
    uint8_t *next_out = retval.first.data();
    size_t avail_out = retval.first.size();
    size_t total_out = 0;
    while(true) {
        BrotliDecoderResult ret = BrotliDecoderDecompressStream(
            state,
            &size,
            &buffer,
            &avail_out,
            &next_out,
            &total_out);
        if (ret == BROTLI_DECODER_RESULT_SUCCESS) {
            if (size != 0) {
                retval.second = JpegError::errShortHuffmanData();
                break;
            }
            retval.first.resize(total_out);
            break;
        }
        if (ret == BROTLI_DECODER_RESULT_ERROR) {
            retval.second = JpegError::errShortHuffmanData();
            break;
        }
        if (ret == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
            size_t new_size = retval.first.size() * 2;
            if (new_size > max_size) {
	      if (retval.first.size() >= max_size) {
                    retval.second = JpegError::errShortHuffmanData();
                    break;
              }
              new_size = std::max(retval.first.size() + 1, max_size);
            }
            retval.first.resize(new_size);
            avail_out = retval.first.size() - total_out;
            next_out = retval.first.data() + total_out;
        }
    }
    BrotliDecoderDestroyInstance(state);
    return retval;

/*    z_stream strm;
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
                retval.first.resize(retval.first.size() * 2);
                avail_bytes = retval.first.size() - retval_size;

                strm.next_out = retval.first.data() + retval_size;
                strm.avail_out = avail_bytes;
            }
        }
    }
    return retval;
*/
    abort();
}

}
