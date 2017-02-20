/*  Sirikata Jpeg Texture Transfer -- Zlib implementation
 *  Zlib0.cpp
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
#include "../../vp8/util/memory.hh"
#include <assert.h>
#include <cstring>

#include "Zlib0.hh"
namespace Sirikata {
uint32_t adler32(uint32_t adler, const uint8_t *buf, uint32_t len);

Zlib0Writer::Zlib0Writer(DecoderWriter * stream, int level){
    mBase = stream;
    mBilledBytesLeft = 0;
    mWritten = 0;
    mClosed = false;
    mAdler32 = adler32(0, NULL, 0);
    always_assert(level == 0 && "Only support stored/raw/literal zlib");
    mFileSize = 0;
}

std::pair<uint32, JpegError> Zlib0Writer::Write(const uint8*data, unsigned int size) {
    always_assert(mWritten + size <= mFileSize);
    mAdler32 = adler32(mAdler32, data, size);
    if (mClosed || mWritten == mFileSize) {
        return std::pair<uint32, JpegError>(0, JpegError::errEOF());
    }
    if (mWritten == 0) {
        std::pair<uint32, JpegError> retval = writeHeader();
        if (retval.second != JpegError::nil()) {
            retval.first = 0;
            return retval;
        }
    }
    std::pair<uint32, JpegError> retval(0, JpegError::nil());
    if (mBilledBytesLeft) { // we've already said we'll write these bytes...lets actually write them
        unsigned int toWrite = std::min(size, (unsigned int)mBilledBytesLeft);
        retval = mBase->Write(data, toWrite);
        if (retval.second != JpegError::nil()) {
            mWritten += retval.first;
            return retval;
        }
        mBilledBytesLeft -= toWrite;
        mWritten += toWrite;
        size -= toWrite;
        data += toWrite;
    }
    while (size) {
        uint8_t buffer[(1<<16) + 1 + 4];
        const uint16_t max_size = 65535;
        if (mWritten + max_size >= mFileSize) {
            buffer[0] = 0x1; // last block
            mBilledBytesLeft = mFileSize - mWritten;
        } else {
            buffer[0] = 0x0;
            mBilledBytesLeft = max_size;
        }
        buffer[1] = mBilledBytesLeft & 0xff;
        buffer[2] = (mBilledBytesLeft >> 8) & 0xff;
        buffer[3] = (~buffer[1]) & 0xff;
        buffer[4] = (~buffer[2]) & 0xff;
        uint32_t toSend = 5;
        uint32_t toWrite = std::min((unsigned int)mBilledBytesLeft, size);
        std::memcpy(buffer + toSend, data, toWrite);
        toSend += toWrite;
        std::pair<uint32, JpegError> retval2 = mBase->Write(buffer, toSend);
        if (retval2.second != JpegError::nil()) {
            if (retval2.first > toSend - toWrite) {
                retval.first += retval2.first - (toSend - toWrite);
                mWritten += retval2.first - (toSend - toWrite);
            }
            retval.second = retval2.second;
            return retval;
        }
        mWritten += toWrite;
        mBilledBytesLeft -= toWrite;
        size -= toWrite;
        data += toWrite;
        retval.first += toWrite;
    }
    return retval;
}

Zlib0Writer::~Zlib0Writer() {
    if (!mClosed) {
        Close();
    }
}
const unsigned int desired_checksum = 31;
static const uint8_t zlibHeader[2] = {0x78, (desired_checksum - (0x78 << 8) % desired_checksum)};
std::pair<uint32_t, JpegError> Zlib0Writer::writeHeader() {
    return mBase->Write(zlibHeader, sizeof(zlibHeader));
}
/// writes the adler32 sum
void Zlib0Writer::Close() {
    always_assert(mWritten == mFileSize && "Must have written as much as promised");
    uint8_t adler[4] = {static_cast<uint8_t>((mAdler32 >> 24) & 0xff),
                        static_cast<uint8_t>((mAdler32 >> 16) & 0xff),
                        static_cast<uint8_t>((mAdler32 >> 8) & 0xff),
                        static_cast<uint8_t>(mAdler32 & 0xff)};
    std::pair<uint32, JpegError> retval = mBase->Write(adler, 4);
    if (retval.second != JpegError::nil()) {
        return;
    }
    mClosed = true;
    mBase->Close();
}
size_t Zlib0Writer::getCompressedSize(size_t originalSize) {
    size_t fullSize = sizeof(zlibHeader);
    size_t numPackets = originalSize /= 65535 + (originalSize % 65535 ? 1 : 0);
    fullSize += numPackets * 5;
    fullSize += 4;// adler32
    return fullSize;
}



#define BASE 65521UL    /* largest prime smaller than 65536 */
#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

#define DO1(buf,i)  {adler += (buf)[i]; sum2 += adler;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

/* use NO_DIVIDE if your processor does not do division in hardware */
#ifdef NO_DIVIDE
#  define MOD(a) \
    do { \
        if (a >= (BASE << 16)) a -= (BASE << 16); \
        if (a >= (BASE << 15)) a -= (BASE << 15); \
        if (a >= (BASE << 14)) a -= (BASE << 14); \
        if (a >= (BASE << 13)) a -= (BASE << 13); \
        if (a >= (BASE << 12)) a -= (BASE << 12); \
        if (a >= (BASE << 11)) a -= (BASE << 11); \
        if (a >= (BASE << 10)) a -= (BASE << 10); \
        if (a >= (BASE << 9)) a -= (BASE << 9); \
        if (a >= (BASE << 8)) a -= (BASE << 8); \
        if (a >= (BASE << 7)) a -= (BASE << 7); \
        if (a >= (BASE << 6)) a -= (BASE << 6); \
        if (a >= (BASE << 5)) a -= (BASE << 5); \
        if (a >= (BASE << 4)) a -= (BASE << 4); \
        if (a >= (BASE << 3)) a -= (BASE << 3); \
        if (a >= (BASE << 2)) a -= (BASE << 2); \
        if (a >= (BASE << 1)) a -= (BASE << 1); \
        if (a >= BASE) a -= BASE; \
    } while (0)
#  define MOD4(a) \
    do { \
        if (a >= (BASE << 4)) a -= (BASE << 4); \
        if (a >= (BASE << 3)) a -= (BASE << 3); \
        if (a >= (BASE << 2)) a -= (BASE << 2); \
        if (a >= (BASE << 1)) a -= (BASE << 1); \
        if (a >= BASE) a -= BASE; \
    } while (0)
#else
#  define MOD(a) a %= BASE
#  define MOD4(a) a %= BASE
#endif

uint32_t adler32(uint32_t adler, const uint8_t *buf, uint32_t len) {
    unsigned long sum2;
    unsigned n;

    /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    /* in case user likes doing a byte at a time, keep it fast */
    if (len == 1) {
        adler += buf[0];
        if (adler >= BASE)
            adler -= BASE;
        sum2 += adler;
        if (sum2 >= BASE)
            sum2 -= BASE;
        return adler | (sum2 << 16);
    }

    /* initial Adler-32 value (deferred check for len == 1 speed) */
    if (buf == NULL)
        return 1L;

    /* in case short lengths are provided, keep it somewhat fast */
    if (len < 16) {
        while (len--) {
            adler += *buf++;
            sum2 += adler;
        }
        if (adler >= BASE)
            adler -= BASE;
        MOD4(sum2);             /* only added so many BASE's */
        return adler | (sum2 << 16);
    }

    /* do length NMAX blocks -- requires just one modulo operation */
    while (len >= NMAX) {
        len -= NMAX;
        n = NMAX / 16;          /* NMAX is divisible by 16 */
        do {
            DO16(buf);          /* 16 sums unrolled */
            buf += 16;
        } while (--n);
        MOD(adler);
        MOD(sum2);
    }

    /* do remaining bytes (less than NMAX, still just one modulo) */
    if (len) {                  /* avoid modulos if none remaining */
        while (len >= 16) {
            len -= 16;
            DO16(buf);
            buf += 16;
        }
        while (len--) {
            adler += *buf++;
            sum2 += adler;
        }
        MOD(adler);
        MOD(sum2);
    }

    /* return recombined sums */
    return adler | (sum2 << 16);
}
}
