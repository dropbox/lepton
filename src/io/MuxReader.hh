/*  Sirikata Jpeg Reader -- Texture Transfer management system
 *  MuxReader.hpp
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
#ifndef _SIRIKATA_MUX_READER_HPP_
#define _SIRIKATA_MUX_READER_HPP_

#include "Allocator.hh"
#include "Error.hh"
#include "Reader.hh"
namespace Sirikata {
class SIRIKATA_EXPORT MuxReader {
    typedef Sirikata::DecoderReader Reader;
    Reader *mReader;
    static JpegError ReadFull(Reader* r, uint8_t *buffer, uint32_t len) {
        while (len != 0) {
            std::pair<uint32, JpegError> ret = r->Read(buffer, len);
            if (ret.first == 0) {
                assert(ret.second != JpegError::nil() && "Read of 0 bytes == error");
                return ret.second; // must have error
            }
            buffer += ret.first;
            len -= ret.first;
        }
        return JpegError::nil();
    }
 public:
    enum {MAX_STREAM_ID = 4};
    std::vector<uint8_t, JpegAllocator<uint8_t> > mBuffer[MAX_STREAM_ID];
    uint32_t mOffset[MAX_STREAM_ID];
    bool eof;
    MuxReader(Reader* reader, const JpegAllocator<uint8_t> &alloc,
                                          int num_stream_hint = 4)
        : mReader(reader) {
        eof = false;
        for (int i = 0; i < MAX_STREAM_ID; ++i) { // assign a better allocator
            mBuffer[i] = std::vector<uint8_t, JpegAllocator<uint8_t> > (alloc);
            if (i < num_stream_hint) {
                mBuffer[i].reserve(65536); // prime some of the vectors
            }
            mOffset[i] = 0;
        }
    }
    JpegError fillBufferUntil(uint8_t desired_stream_id) {
        if (eof) {
            return JpegError::errEOF();
        }
        assert(mOffset[desired_stream_id] == mBuffer[desired_stream_id].size());
        mOffset[desired_stream_id] = 0;
        std::vector<uint8_t, JpegAllocator<uint8_t> > incomingBuffer(mBuffer[desired_stream_id].get_allocator());
        incomingBuffer.swap(mBuffer[desired_stream_id]);
        do {
            {
                incomingBuffer.resize(16385);
                std::pair<uint32, JpegError> ret = mReader->Read(&incomingBuffer[0], 16385);
                if (!ret.first) {
                    return ret.second;
                }
                incomingBuffer.resize(ret.first);
            }
            uint8_t stream_id;
            std::vector<uint8_t, JpegAllocator<uint8_t> >::iterator first = incomingBuffer.begin(), last = incomingBuffer.end();
            do {
                stream_id = 0xf & *first;
                if (stream_id >= MAX_STREAM_ID) {
                    return JpegError::errShortHuffmanData();
                }
                uint8_t flags = 0xf & (*first >> 4);
                ++first;
                uint32_t len = 0;
                switch (flags & 0x3) {
                case 0:
                    if (first == last || first + 1 == last) {
                        uint8_t len_buf[2]={0};
                        JpegError ret = ReadFull(mReader, len_buf, first == last ? 2 : 1);
                        if (ret != JpegError::nil()) {
                            return ret;
                        }
                        if (first == last) {
                            len = len_buf[0] + 0x100 * (uint16_t)len_buf[1];
                        } else {
                            len = (*first) + 0x100 * (uint16_t)len_buf[0];
                        }
                        first = last;
                        ++len;
                    } else {
                        len = *first;
                        ++first;
                        len += 0x100 * (*first);
                        ++first;
                        ++len;
                    }
                    break;
                case 1:
                    len = 256;
                    break;
                case 2:
                    len = 4096;
                    break;
                case 3:
                    len = 16384;
                    break;
                }
                size_t bodyBytesRead = last - first;
                if (bodyBytesRead >= len) {
                    if (mOffset[stream_id] == mBuffer[stream_id].size()) {
                        mOffset[stream_id] = first - incomingBuffer.begin();
                        mBuffer[stream_id].swap(incomingBuffer);
                        incomingBuffer.clear();
                        incomingBuffer.insert(incomingBuffer.end(),
                                              mBuffer[stream_id].begin() + mOffset[stream_id] + len,
                                              mBuffer[stream_id].end());
                        mBuffer[stream_id].resize(mOffset[stream_id] + len);
                        first = incomingBuffer.begin();
                        last = incomingBuffer.end();
                    } else {
                        mBuffer[stream_id].insert(mBuffer[stream_id].end(), first, first + len);
                        first += len;
                    }
                } else {
                    size_t remainingBytes = len - bodyBytesRead;
                    ptrdiff_t nonzero = mBuffer[stream_id].size();
                    if (nonzero<0) {
                        assert(false&&"EMPTY");
                    }
                    if (mOffset[stream_id] == mBuffer[stream_id].size()) {
                        mOffset[stream_id] = first - incomingBuffer.begin();
                        mBuffer[stream_id].swap(incomingBuffer);
                    } else {
                        mBuffer[stream_id].insert(mBuffer[stream_id].end(),
                                                 first,
                                                 last);
                    }
                    mBuffer[stream_id].resize(mBuffer[stream_id].size() + remainingBytes);
                    JpegError err;
                    err = ReadFull(mReader,
                                  &mBuffer[stream_id][mBuffer[stream_id].size() - remainingBytes],
                                  remainingBytes);
                    if (err) {
                        return err;
                    }
                    incomingBuffer.clear();
                    first = incomingBuffer.begin();
                    last = incomingBuffer.end();
                }
            } while(first != last);
        } while(mOffset[desired_stream_id] == mBuffer[desired_stream_id].size());
        return JpegError::nil();
    }
    std::pair<uint32, JpegError> Read(uint8_t stream_id, uint8*data, unsigned int size) {
        assert(stream_id < MAX_STREAM_ID && "Invalid stream Id; must be less than 16");
        std::pair<uint32, JpegError> retval(0, JpegError::nil());
        bool bytes_available = mOffset[stream_id] != mBuffer[stream_id].size();
        if (bytes_available || (retval.second = fillBufferUntil(stream_id)) == JpegError::nil()) {
            retval.first = std::min((uint32_t)mBuffer[stream_id].size() - mOffset[stream_id],
                                    size);
            std::memcpy(data, &mBuffer[stream_id][mOffset[stream_id]], retval.first);
            mOffset[stream_id] += retval.first;
        }
        return retval;
    }
    ~MuxReader(){}
};

class SIRIKATA_EXPORT MuxWriter {
    typedef Sirikata::DecoderWriter Writer;
    Writer *mWriter;
public:
    enum {MAX_STREAM_ID = MuxReader::MAX_STREAM_ID};
    enum {MIN_OFFSET = 3};
    enum {MAX_BUFFER_LAG = 65537};
    std::vector<uint8_t, JpegAllocator<uint8_t> > mBuffer[MAX_STREAM_ID];
    uint32_t mOffset[MAX_STREAM_ID];
    uint32_t mFlushed[MAX_STREAM_ID];
    uint32_t mTotalWritten;
    uint32_t mLowWaterMark[MAX_STREAM_ID];
    MuxWriter(Writer* writer, const JpegAllocator<uint8_t> &alloc)
        : mWriter(writer) {
        for (uint8_t i = 0; i < MAX_STREAM_ID; ++i) { // assign a better allocator
            mBuffer[i] = std::vector<uint8_t, JpegAllocator<uint8_t> >(alloc);
            mOffset[i] = 0;
            mFlushed[i] = 0;
            mLowWaterMark[i] = 0;
        }
        mTotalWritten = 0;
    }
    uint32_t highWaterMark(uint32_t flushed) {
        if (flushed & 0xffffc000) {
            return 16384;
        }
        if (flushed & 0xffffff00) {        
            return 4096;
        }
        return 256;
    }

    JpegError flushFull(uint8_t stream_id, uint32_t toBeFlushed) {
        if (toBeFlushed ==0) {
            return JpegError::nil();
        }
        assert(toBeFlushed + mOffset[stream_id] == mBuffer[stream_id].size());
        std::pair<uint32_t, JpegError> retval(0, JpegError::nil());
        do{
            uint32_t offset = mOffset[stream_id];
            assert(offset >= MIN_OFFSET);
            uint32_t toWrite = std::min(toBeFlushed, (uint32_t)65536U);
            mBuffer[stream_id][offset - MIN_OFFSET] = stream_id;
            mBuffer[stream_id][offset - MIN_OFFSET + 1] = ((toWrite - 1) & 0xff);
            mBuffer[stream_id][offset - MIN_OFFSET + 2] = (((toWrite - 1) >> 8) & 0xff);
            retval = mWriter->Write(&mBuffer[stream_id][offset - MIN_OFFSET],
                                   toWrite + MIN_OFFSET);
            assert((retval.first == toWrite + MIN_OFFSET || retval.second != JpegError::nil())
                   && "Writers must write full");
            if (retval.second == JpegError::nil()) {
                mTotalWritten += toWrite;
                mFlushed[stream_id] += toWrite;
                mOffset[stream_id] += toWrite;
                toBeFlushed -= toWrite;
            } else {
                break;
            }
        }while(toBeFlushed > 0);
        mOffset[stream_id] = MIN_OFFSET;
        mBuffer[stream_id].resize(MIN_OFFSET);
        mLowWaterMark[stream_id] = mTotalWritten;
        return retval.second;
    }

    JpegError flushPartial(uint8_t stream_id, uint32_t toBeFlushed) {
        uint8_t code = stream_id;
        uint32_t len = 0;
        if (toBeFlushed < 256) {
            assert(false && "We shouldn't reach this");
            return flushFull(stream_id, toBeFlushed);
        }
        
        if (toBeFlushed < 4096) {
            if (toBeFlushed > 512) {
                return flushFull(stream_id, toBeFlushed);
            }
            len = 256;
            code |= (1 << 4);
        } else if (toBeFlushed < 16384) {
            if (toBeFlushed > 8192) {
                return flushFull(stream_id, toBeFlushed);
            }
            len = 4096;
            code |= (2 << 4);
        } else {
            if (toBeFlushed > 32768) {
                return flushFull(stream_id, toBeFlushed);
            }
            len = 16384;
            code |= (3 << 4);
        }
        std::pair<uint32_t, JpegError> retval(0, JpegError::nil());
        for (uint32_t toWrite = 0; toWrite + len <= toBeFlushed; toWrite += len) {
            uint32_t offset = mOffset[stream_id];
            if (offset == mBuffer[stream_id].size()) continue;
            assert(offset >= MIN_OFFSET);
            mBuffer[stream_id][offset - 1] = code;
            retval = mWriter->Write(&mBuffer[stream_id][offset - 1],
                                   len + 1);
            if (retval.first != len + 1) {
                return retval.second;
            }
            mTotalWritten += len;
            mFlushed[stream_id] += len;
            mOffset[stream_id] += len;
            if (mOffset[stream_id] > 65539) {
                for (std::vector<uint8_t, JpegAllocator<uint8_t> >::iterator
                         src = mBuffer[stream_id].begin() + mOffset[stream_id],
                         dst = mBuffer[stream_id].begin() + MIN_OFFSET,
                         ed = mBuffer[stream_id].end(); src != ed; ++src, ++dst) {
                    *dst = *src;
                }
                mBuffer[stream_id].resize(MIN_OFFSET + mBuffer[stream_id].size() - mOffset[stream_id]);
                mOffset[stream_id] = MIN_OFFSET;
            }
        }
        uint32_t delta = mBuffer[stream_id].size() - mOffset[stream_id];
        if (delta > mTotalWritten) {
            mLowWaterMark[stream_id] = 0;
        } else {
            // we're already delta behind the ground truth
            mLowWaterMark[stream_id] = mTotalWritten - delta;
        }
        return retval.second;
    }
    JpegError flush(uint8_t stream_id, uint32_t highWaterMark) {
        JpegError retval = JpegError::nil();
        for (uint8_t i= 0; i < MAX_STREAM_ID; ++i) {
            uint32_t toBeFlushed = mBuffer[i].size() - mOffset[i];
            if (i == stream_id || !toBeFlushed) {
                continue;
            }
            bool isUrgent = mTotalWritten - mLowWaterMark[i] > MAX_BUFFER_LAG;
            if (toBeFlushed < 256) {
                if (isUrgent) {
                    // we need to flush what we have
                    retval = flushFull(i, toBeFlushed);
                    assert(mTotalWritten == mLowWaterMark[i]);
                }
            } else {
                if (isUrgent && toBeFlushed < 4096) {
                    retval = flushFull(i, toBeFlushed);
                } else {
                    retval = flushPartial(i, toBeFlushed);
                }
            }
        }
        uint32_t toBeFlushed = mBuffer[stream_id].size() - mOffset[stream_id];
        retval = flushPartial(stream_id, toBeFlushed);
        return retval;
    }
    std::pair<uint32, JpegError> Write(uint8_t stream_id, const uint8*data, unsigned int size) {
        std::pair<uint32, JpegError> retval(size, JpegError::nil());
        size_t bufferSize = mBuffer[stream_id].size();
        if (bufferSize == 0) {
            mBuffer[stream_id].reserve(16387);
            mBuffer[stream_id].resize(MIN_OFFSET);
            mOffset[stream_id] = MIN_OFFSET;
            bufferSize = MIN_OFFSET;
        }
        mBuffer[stream_id].insert(mBuffer[stream_id].end(), data, data + size);
        bufferSize += size;
        uint32_t hwm = highWaterMark(mFlushed[stream_id]);
        if (bufferSize >= mOffset[stream_id] + hwm) {
            retval.second = flush(stream_id, hwm);
        }
        return retval;
    }
    void Close() {
        for (uint8_t i = 0; i < MAX_STREAM_ID; ++i) {
            if(mOffset[i] != mBuffer[i].size()) {
                assert(mBuffer[i].size() - mOffset[i] < 65536);
                flushFull(i, mBuffer[i].size() - mOffset[i]);
            }
        }
    }
    ~MuxWriter(){}
};
}
#endif
