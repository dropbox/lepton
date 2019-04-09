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
#ifndef SIRIKATA_JPEG_ARHC_ERROR_HPP_
#define SIRIKATA_JPEG_ARHC_ERROR_HPP_

namespace Sirikata {

class JpegError {
    explicit JpegError(const char * ):mWhat(ERR_MISC) {
    }
public:
    enum ErrorMessage{
        NO_ERROR,
        ERR_EOF,
        ERR_FF00,
        ERR_SHORT_HUFFMAN,
        ERR_MISC
    } mWhat;
    static JpegError MakeFromStringLiteralOnlyCallFromMacro(const char*) {
        return JpegError(ERR_MISC, ERR_MISC);
    }
    explicit JpegError(ErrorMessage err, ErrorMessage ):mWhat(err) {

    }
    JpegError() :mWhat() { // uses default allocator--but it won't allocate, so that's ok
        mWhat = NO_ERROR;
    }
    const char * what() const {
        if (mWhat == NO_ERROR) return "";
        if (mWhat == ERR_EOF) return "EOF";
        if (mWhat == ERR_FF00) return "MissingFF00";
        if (mWhat == ERR_SHORT_HUFFMAN) return "ShortHuffman";
        return "MiscError";
    }
    operator bool() {
        return mWhat != NO_ERROR;
    }
    static JpegError nil() {
        return JpegError();
    }
    static JpegError errEOF() {
        return JpegError(ERR_EOF,ERR_EOF);
    }
    static JpegError errMissingFF00() {
        return JpegError(ERR_FF00, ERR_FF00);
    }
    static JpegError errShortHuffmanData(){
        return JpegError(ERR_SHORT_HUFFMAN, ERR_SHORT_HUFFMAN);
    }
};
#define MakeJpegError(s) JpegError::MakeFromStringLiteralOnlyCallFromMacro("" s)
#define JpegErrorUnsupportedError(s) MakeJpegError("unsupported JPEG feature: " s)
#define JpegErrorFormatError(s) MakeJpegError("unsupported JPEG feature: " s)
}
#endif
