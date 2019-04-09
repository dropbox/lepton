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
#if defined (__linux__) || defined (__APPLE__) || defined(BSD)
#define SIRIKATA_FUNCTION_EXPORT __attribute__ ((visibility("default")))
#define SIRIKATA_EXPORT __attribute__ ((visibility("default")))
#define SIRIKATA_PLUGIN_EXPORT __attribute__ ((visibility("default")))
#else
#define SIRIKATA_FUNCTION_EXPORT
#define SIRIKATA_EXPORT
#define SIRIKATA_PLUGIN_EXPORT
#define __builtin_expect(x, y) x
#endif
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <algorithm>
#include "../vp8/util/memory.hh"
#define USE_MMAP
namespace Sirikata{

typedef int64_t int64;
typedef uint64_t uint64;
typedef int32_t int32;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint8_t uint8;
typedef uint8_t byte;
typedef int8_t int8;

}
#ifndef DECODER_PLATFORM_HH_
#define DECODER_PLATFORM_HH_
#ifdef _WIN32
#include <io.h>
inline int write(int fd, const void*data, unsigned int length) {
    return _write(fd, data, length);
}
inline int read(int fd, void*data, unsigned int length) {
    return _read(fd, data, length);
}
inline int close(int fd) {
    return _close(fd);
}
typedef int ssize_t;
#endif


#endif
