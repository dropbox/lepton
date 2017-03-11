/**
    # $FreeBSD$
	#       @(#)COPYRIGHT   8.2 (Berkeley) 3/21/94

	The compilation of software known as the FreeBSD Ports Collection is
	distributed under the following terms:

	Copyright (C) 1994-2016 The FreeBSD Project. All rights reserved.

    Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:
	1. Redistributions of source code must retain the above copyright
	   notice, this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
	OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
	SUCH DAMAGE.
*/

#if !defined(_WIN32) && defined(__SSE2__) && !defined(__SSE4_1__) && !defined(MM_MULLO_EPI32_H)
#define MM_MULLO_EPI32_H

#include <immintrin.h>
// See:	http://stackoverflow.com/questions/10500766/sse-multiplication-of-4-32-bit-integers
// and	https://software.intel.com/en-us/forums/intel-c-compiler/topic/288768
static inline __m128i
fallback_mm_mullo_epi32(const __m128i &a, const __m128i &b)
{
	__m128i tmp1 = _mm_mul_epu32(a,b); /* mul 2,0*/
	__m128i tmp2 = _mm_mul_epu32(_mm_srli_si128(a,4),
	    _mm_srli_si128(b,4)); /* mul 3,1 */
	return _mm_unpacklo_epi32( /* shuffle results to [63..0] and pack */
	    _mm_shuffle_epi32(tmp1, _MM_SHUFFLE (0,0,2,0)),
	    _mm_shuffle_epi32(tmp2, _MM_SHUFFLE (0,0,2,0)));
}
#define _mm_mullo_epi32 fallback_mm_mullo_epi32

#endif
