/*  Sirikata Utilities -- Sirikata Array Utilities
 *  ArrayNd.hpp
 *
 *  Copyright (c) 2009, Daniel Reiter Horn
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
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef SIRIKATA_ARRAY_ND_HPP_
#define SIRIKATA_ARRAY_ND_HPP_
#include <assert.h>
#include <cstddef>
#include <cstring>
#if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus > 199711L || defined(_WIN32)
#include <cstdint>
#else
#include <assert.h>
#include <stdint.h>
#define constexpr
#define NOCONSTEXPR
#endif
#ifdef AVOID_ARRAY_BOUNDS_CHECKS
#define ARRAY_BOUNDS_ASSERT(x) assert(x)
#else
#include "memory.hh"
#define ARRAY_BOUNDS_ASSERT(x) always_assert(x)
#endif

namespace Sirikata {

template<class T> class ReferenceType {
public:
    typedef typename T::Array BaseArrayType;
    typedef typename T::Array * ArrayType;
    static BaseArrayType& dereference(BaseArrayType*a) {
        return *a;
    }
    static const BaseArrayType& dereference(const BaseArrayType*a) {
        return *a;
    }
};
template<class T> class DirectType {
public:
    typedef typename T::Array BaseArrayType;
    typedef typename T::Array ArrayType;
    static BaseArrayType& dereference(BaseArrayType &a) {
        return a;
    }
    static const BaseArrayType& dereference(const BaseArrayType&a) {
        return a;
    }
};
class RoundToPow2 { public:
    enum {
        SHOULD_ROUND_POW2 = 1
    };
};
class DontRoundPow2 { public:
    enum {
        SHOULD_ROUND_POW2 = 0
    };
};
template <uint32_t v, class ShouldRound> class RoundP2 {public:
    enum RoundingResult {
        value = (ShouldRound::SHOULD_ROUND_POW2 && (v & (v - 1)) ? (1 + (v | (v >> 1) | (v >> 2) | (v >> 4) | (v >> 8) | (v >> 16))) : v)
    };
    typedef char ARRAY_BOUNDS_ASSERT_power_of_two_constraint[(value & (value - 1)) == 0
                                                || !ShouldRound::SHOULD_ROUND_POW2 ? 1 : -1];
};
template<class T, uint32_t s0, class ShouldRoundPow2> struct ArrayBaseType1d {
    typedef T Array[RoundP2<s0 ? s0 : 1, ShouldRoundPow2>::value];
};
template<class T, uint32_t s0, uint32_t s1, class ShouldRoundPow2> struct ArrayBaseType2d {
    typedef T Array[RoundP2<s0, ShouldRoundPow2>::value][RoundP2<s1, ShouldRoundPow2>::value];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2,
         class ShouldRoundPow2> struct ArrayBaseType3d {
    typedef T Array[RoundP2<s0, ShouldRoundPow2>::value][RoundP2<s1, ShouldRoundPow2>::value][RoundP2<s2, ShouldRoundPow2>::value];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3,
         class ShouldRoundPow2> struct ArrayBaseType4d {
    typedef T Array[RoundP2<s0, ShouldRoundPow2>::value][RoundP2<s1, ShouldRoundPow2>::value][RoundP2<s2, ShouldRoundPow2>::value][RoundP2<s3, ShouldRoundPow2>::value];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4,
         class ShouldRoundPow2> struct ArrayBaseType5d {
    typedef T Array[RoundP2<s0, ShouldRoundPow2>::value][RoundP2<s1, ShouldRoundPow2>::value][RoundP2<s2, ShouldRoundPow2>::value][RoundP2<s3, ShouldRoundPow2>::value][RoundP2<s4, ShouldRoundPow2>::value];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2,
         uint32_t s3, uint32_t s4, uint32_t s5, class ShouldRoundPow2> struct ArrayBaseType6d {
    typedef T Array[RoundP2<s0, ShouldRoundPow2>::value][RoundP2<s1, ShouldRoundPow2>::value][RoundP2<s2, ShouldRoundPow2>::value][RoundP2<s3, ShouldRoundPow2>::value][RoundP2<s4, ShouldRoundPow2>::value][RoundP2<s5, ShouldRoundPow2>::value];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2,
         uint32_t s3, uint32_t s4, uint32_t s5, uint32_t s6,
         class ShouldRoundPow2> struct ArrayBaseType7d {
    typedef T Array[RoundP2<s0, ShouldRoundPow2>::value][RoundP2<s1, ShouldRoundPow2>::value][RoundP2<s2, ShouldRoundPow2>::value][RoundP2<s3, ShouldRoundPow2>::value][RoundP2<s4, ShouldRoundPow2>::value][RoundP2<s5, ShouldRoundPow2>::value][RoundP2<s6, ShouldRoundPow2>::value];
};

template <class T,
          uint32_t s0, class ShouldRoundPow2 = DontRoundPow2,
          class IsReferenceType=DirectType<ArrayBaseType1d<T, s0, ShouldRoundPow2> >
          > struct Array1d {
    typedef typename ArrayBaseType1d<T,s0,ShouldRoundPow2>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array1d<T,
                    s0,
                    ShouldRoundPow2,
                    ReferenceType<ArrayBaseType1d<T, s0, ShouldRoundPow2> > > Slice;
    enum Sizes{
        size0 = s0
    };
    static constexpr uint32_t size() {
        return s0;
    }
    static constexpr uint32_t dimsize() {
        return s0;
    }
    static constexpr uint32_t dimension() {
        return 1;
    }
    T& at(uint32_t i0) {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        return IsReference::dereference(data)[i0];
    }
    template<int index> constexpr T kat() const {
        static_assert(index < s0, "template argument must be within bound");
        return data[index];
    }
    const T& at(uint32_t i0) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        return IsReference::dereference(data)[i0];
    }
    T& operator[](uint32_t i0) {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        return IsReference::dereference(data)[i0];
    }
    const T& operator[](uint32_t i0) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        return IsReference::dereference(data)[i0];
    }
    template <class StartEnd> typename Array1d<T,
                                               StartEnd::END - StartEnd::START,
                                               ShouldRoundPow2>::Slice slice(const StartEnd&range) {
        return slice<StartEnd::START, StartEnd::END>();
    }
    template <class StartEnd> typename Array1d<T,
                                               StartEnd::END - StartEnd::START,
                                               ShouldRoundPow2>::Slice slice(const StartEnd&range) const {
        return slice<StartEnd::START, StartEnd::END>();
    }
    template <uint32_t kstart, uint32_t kend> typename Array1d<T, kend - kstart,
                                                           ShouldRoundPow2>::Slice slice() {
            uint8_t ARRAY_BOUNDS_ASSERT_slice_legal[kend > s0 ? -1 : 1];
            uint8_t ARRAY_BOUNDS_ASSERT_slice_start_legal[kend < kstart ? -1 : 1];
            (void)ARRAY_BOUNDS_ASSERT_slice_legal;
            (void)ARRAY_BOUNDS_ASSERT_slice_start_legal;
            const typename Array1d<T, kend-kstart, ShouldRoundPow2>::Slice retval = {(typename Array1d<T, kend-kstart, ShouldRoundPow2>::Slice::IsReference::ArrayType)
                                                                  &IsReference::dereference(data)[kstart]};
            return retval;
    }
    template <uint32_t start, uint32_t end> const typename Array1d<T, end - start, ShouldRoundPow2>::Slice slice() const {
        uint8_t ARRAY_BOUNDS_ASSERT_slice_legal[end > s0 ? -1 : 1];
        uint8_t ARRAY_BOUNDS_ASSERT_slice_start_legal[end < start ? -1 : 1];
        (void)ARRAY_BOUNDS_ASSERT_slice_legal;
        (void)ARRAY_BOUNDS_ASSERT_slice_start_legal;
        const typename Array1d<T, end-start, ShouldRoundPow2>::Slice retval = {(typename Array1d<T, end-start, ShouldRoundPow2>::Slice::IsReference::ArrayType)
                                                              &IsReference::dereference(data)[start]};
        return retval;
    }

    template <uint32_t new_size> typename Array1d<T, new_size,
        ShouldRoundPow2>::Slice dynslice(uint32_t start) {
        uint8_t ARRAY_BOUNDS_ASSERT_slice_size_legal[new_size > s0 ? -1 : 1];
        (void)ARRAY_BOUNDS_ASSERT_slice_size_legal;
        ARRAY_BOUNDS_ASSERT(start + new_size <= s0 && "slice must fit within original array");
        const typename Array1d<T, new_size, ShouldRoundPow2>::Slice retval
            = {(typename Array1d<T, new_size, ShouldRoundPow2>::Slice::IsReference::ArrayType)
                &IsReference::dereference(data)[start]};
        return retval;
    }
          
    void memset(uint8_t val) {
        std::memset(data, val, sizeof(Array));
    }
    template<class F> void foreach(const F& f){
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            f(IsReference::dereference(data)[i0]);
        }
    }
    template<class F> void foreach(const F& f) const{
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            f(IsReference::dereference(data)[i0]);
        }
    }
    T* begin() {
        return (T*)data;
    }
    const T* begin() const {
        return (const T*)data;
    }
    T* end() {
        return (T*)data + s0;
    }
    const T* end() const {
        return (const T*)data + s0;
    }
};

template <class T,
          uint32_t s0, uint32_t s1, class ShouldRoundPow2 = DontRoundPow2,
          class IsReferenceType=DirectType<ArrayBaseType2d<T,
                                                           s0,
                                                           s1,
                                                           ShouldRoundPow2> > > struct Array2d {
    typedef typename ArrayBaseType2d<T,s0,s1,ShouldRoundPow2>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array2d<T,
                    s0,
                    s1,
                    ShouldRoundPow2,
                    ReferenceType<ArrayBaseType2d<T, s0, s1, ShouldRoundPow2> > > Slice;
    enum Sizes{
        size0 = s0,
        size1 = s1
    };
    static constexpr Array1d<uint32_t, 2, ShouldRoundPow2> size() {
#ifdef NOCONSTEXPR
        Array1d<uint32_t, 2, ShouldRoundPow2> retval = {{s0, s1}};
        return retval;
#else
        return {{s0, s1}};
#endif
    }
    static uint32_t dimension() {
        return 2;
    }
    static constexpr uint32_t dimsize() {
        return s0;
    }
    T& raster(uint32_t index) {
        ARRAY_BOUNDS_ASSERT(index < s0 * s1);
        return (&IsReference::dereference(data)[0][0])[index];
    }
    const T& raster(uint32_t index) const {
        ARRAY_BOUNDS_ASSERT(index < s0 * s1);
        return (&IsReference::dereference(data)[0][0])[index];
    }
    T& at(uint32_t i0,
                   uint32_t i1) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1);
        return IsReference::dereference(data)[i0][i1];
    }
    const T& at(uint32_t i0,
                         uint32_t i1) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1);
        return IsReference::dereference(data)[i0][i1];
    }
    typename Array1d<T, s1,ShouldRoundPow2>::Slice at(uint32_t i0) {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        typename Array1d<T, s1,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0]};
        return retval;
    }
    const typename Array1d<T, s1,ShouldRoundPow2>::Slice at(uint32_t i0) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        const typename Array1d<T, s1,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0]};
        return retval;
    }
    void memset(uint8_t val) {
        std::memset(data, val, sizeof(Array));
    }
    template<class F> void foreach(const F& f){
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                f(IsReference::dereference(data)[i0][i1]);
            }
        }
    }
    template<class F> void foreach(const F& f) const{
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                f(IsReference::dereference(data)[i0][i1]);
            }
        }
    }
    T* begin() {
        return (T*)data;
    }
    const T* begin() const {
        return (const T*)data;
    }
    T* end() {
        return (T*)data + s0 * s1;
    }
    const T* end() const {
        return (const T*)data + s0 * s1;
    }
};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          class ShouldRoundPow2 = DontRoundPow2,
          class IsReferenceType=DirectType<ArrayBaseType3d<T,s0,s1,s2,ShouldRoundPow2> > > struct Array3d {
    typedef typename ArrayBaseType3d<T,s0,s1,s2,ShouldRoundPow2>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array3d<T, s0, s1, s2,ShouldRoundPow2, ReferenceType<ArrayBaseType3d<T, s0, s1, s2 ,ShouldRoundPow2> > > Slice;

    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2
    };
    static constexpr Array1d<uint32_t, 3, DontRoundPow2> size() {
#ifdef NOCONSTEXPR
        Array1d<uint32_t, 3, DontRoundPow2> retval = {{s0,s1,s2}};
        return retval;
#else
        return {{s0,s1,s2}};
#endif
    }
    static uint32_t dimension() {
        return 3;
    }
    static constexpr uint32_t dimsize() {
        return s0;
    }

    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2);
        return IsReference::dereference(data)[i0][i1][i2];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2);
        return IsReference::dereference(data)[i0][i1][i2];
    }
    typename Array1d<T, s2,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1);
        typename Array1d<T, s2,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0][i1]};
        return retval;
    }
    const typename Array1d<T, s2,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1);
        const typename Array1d<T, s2,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0][i1]};
        return retval;
    }
    typename Array2d<T, s1, s2,ShouldRoundPow2>::Slice at(uint32_t i0) {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        typename Array2d<T, s1, s2,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0]};
        return retval;
    }
    const typename Array2d<T, s1, s2,ShouldRoundPow2>::Slice at(uint32_t i0) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        const typename Array2d<T, s1, s2,ShouldRoundPow2>::Slice retval = {(typename Array2d<T, s1, s2, ShouldRoundPow2>::IsReference::ArrayType*)
                                                           &IsReference::dereference(data)[i0]};
        return retval;
    }
    void memset(uint8_t val) {
        memset(data, val, sizeof(Array));
    }
    template<class F> void foreach(const F& f){
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    f(IsReference::dereference(data)[i0][i1][i2]);
                }
            }
        }
    }
    template<class F> void foreach(const F& f) const{
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    f(IsReference::dereference(data)[i0][i1][i2]);
                }
            }
        }
    }
    T* begin() {
        return (T*)data;
    }
    const T* begin() const {
        return (const T*)data;
    }
    T* end() {
        return (T*)data + s0 * s1 * s2;
    }
    const T* end() const {
        return (const T*)data + s0 * s1 * s2;
    }
};




template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3,
          class ShouldRoundPow2 = DontRoundPow2,
          class IsReferenceType=DirectType<ArrayBaseType4d<T,s0,s1,s2,s3,ShouldRoundPow2> > > struct Array4d {
    typedef typename ArrayBaseType4d<T,s0,s1,s2,s3,ShouldRoundPow2>::Array Array;
    typedef Array4d<T, s0, s1, s2, s3,ShouldRoundPow2, ReferenceType<ArrayBaseType4d<T, s0, s1, s2, s3,ShouldRoundPow2> > > Slice;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2,
        size3 = s3
    };

    static constexpr Array1d<uint32_t, 4, ShouldRoundPow2> size() {
#ifdef NOCONSTEXPR
        Array1d<uint32_t, 4, ShouldRoundPow2> retval = {{s0,s1,s2,s3}};
        return retval;
#else
        return {{s0,s1,s2,s3}};
#endif
    }
    static uint32_t dimension() {
        return 4;
    }
    static constexpr uint32_t dimsize() {
        return s0;
    }

    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3);
        return IsReference::dereference(data)[i0][i1][i2][i3];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3);
        return IsReference::dereference(data)[i0][i1][i2][i3];
    }
    typename Array1d<T, s3,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2);
        typename Array1d<T, s3,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0][i1][i2]};
        return retval;
    }
    const typename Array1d<T, s3,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2);
        const typename Array1d<T, s3,ShouldRoundPow2>::Slice retval ={&IsReference::dereference(data)[i0][i1][i2]};
        return retval;
    }
    typename Array2d<T, s2, s3,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1);
        typename Array2d<T, s2, s3,ShouldRoundPow2>::Slice retval ={&IsReference::dereference(data)[i0][i1]};
        return retval;
    }
    const typename Array2d<T, s2, s3,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1);
        const typename Array2d<T, s2, s3,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0][i1]};
        return retval;
    }

    typename Array3d<T, s1, s2, s3,ShouldRoundPow2>::Slice at(uint32_t i0) {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        typename Array3d<T, s1, s2, s3,ShouldRoundPow2>::Slice retval ={&IsReference::dereference(data)[i0]};
        return retval;
    }
    const typename Array3d<T, s1, s2, s3,ShouldRoundPow2>::Slice at(uint32_t i0) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        const typename Array3d<T, s1, s2, s3,ShouldRoundPow2>::Slice retval ={(typename Array3d<T, s1, s2, s3,ShouldRoundPow2>::IsReference::ArrayType*)
                                                              (&IsReference::dereference(data)[i0])};
        return retval;
    }
    void memset(uint8_t val) {
        memset(data, val, sizeof(Array));
    }
    template<class F> void foreach(const F& f){
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    for (uint32_t i3 = 0; i3 < s3; ++i3) {
                        f(IsReference::dereference(data)[i0][i1][i2][i3]);
                    }
                }
            }
        }
    }
    template<class F> void foreach(const F& f) const{
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    for (uint32_t i3 = 0; i3 < s3; ++i3) {
                        f(IsReference::dereference(data)[i0][i1][i2][i3]);
                    }
                }
            }
        }
    }
    T* begin() {
      return (T*)data;
    }
    const T* begin() const {
      return (const T*)data;
    }
    T* end() {
      return (T*)data + s0 * s1 * s2 * s3;
    }
    const T* end() const {
      return (const T*)data + s0 * s1 * s2 * s3;
    }

};




template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4,
          class ShouldRoundPow2 = DontRoundPow2,
          class IsReferenceType=DirectType<ArrayBaseType5d<T,s0,s1,s2,s3,s4,ShouldRoundPow2> > > struct Array5d {
    typedef typename ArrayBaseType5d<T,s0,s1,s2,s3,s4,ShouldRoundPow2>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;


    typedef Array5d<T, s0, s1, s2, s3, s4,ShouldRoundPow2,
                    ReferenceType<ArrayBaseType5d<T, s0, s1, s2, s3, s4,ShouldRoundPow2> > > Slice;
    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2,
        size3 = s3,
        size4 = s4
    };
    static constexpr Array1d<uint32_t, 5, DontRoundPow2> size() {
#ifdef NOCONSTEXPR
        Array1d<uint32_t, 5, DontRoundPow2> retval = {{s0,s1,s2,s3,s4}};
        return retval;
#else
        return {{s0,s1,s2,s3,s4}};
#endif
    }
    static uint32_t dimension() {
        return 5;
    }
    static constexpr uint32_t dimsize() {
        return s0;
    }

    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3,
                         uint32_t i4) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4];
    }
    typename Array1d<T, s4,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3);
        typename Array1d<T, s4,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0][i1][i2][i3]};
        return retval;
    }
    const typename Array1d<T, s4,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3);
        const typename Array1d<T, s4,ShouldRoundPow2>::Slice retval ={&IsReference::dereference(data)[i0][i1][i2][i3]};
        return retval;
    }
    typename Array2d<T, s3, s4,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2);
        typename Array2d<T, s3, s4,ShouldRoundPow2>::Slice retval ={&IsReference::dereference(data)[i0][i1][i2]};
        return retval;
    }
    const typename Array2d<T, s3, s4,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2);
        const typename Array2d<T, s3, s4,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0][i1][i2]};
        return retval;
    }

    typename Array4d<T, s1, s2, s3, s4,ShouldRoundPow2>::Slice at(uint32_t i0) {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        typename Array4d<T, s1, s2, s3, s4,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0]};
        return retval;
    }
    const typename Array4d<T, s1, s2, s3, s4,ShouldRoundPow2>::Slice at(uint32_t i0) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        const typename Array4d<T, s1, s2, s3, s4,ShouldRoundPow2>::Slice retval ={(typename Sirikata::ReferenceType<typename Sirikata::ArrayBaseType4d<T, s1, s2, s3, s4,ShouldRoundPow2> >::ArrayType )&IsReference::dereference(data)[i0]};
        return retval;
    }
    void memset(uint8_t val) {
        memset(data, val, sizeof(Array));
    }
    template<class F> void foreach(const F& f){
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    for (uint32_t i3 = 0; i3 < s3; ++i3) {
                        for (uint32_t i4 = 0; i4 < s4; ++i4) {
                            f(IsReference::dereference(data)[i0][i1][i2][i3][i4]);
                        }
                    }
                }
            }
        }
    }
    template<class F> void foreach(const F& f) const{
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    for (uint32_t i3 = 0; i3 < s3; ++i3) {
                        for (uint32_t i4 = 0; i4 < s4; ++i4) {
                            f(IsReference::dereference(data)[i0][i1][i2][i3][i4]);
                        }
                    }
                }
            }
        }
    }
    T* begin() {
      return (T*)data;
    }
    const T* begin() const {
      return (const T*)data;
    }
    T* end() {
      return (T*)data + s0 * s1 * s2 * s3 * s4;
    }
    const T* end() const {
      return (const T*)data + s0 * s1 * s2 * s3 * s4;
    }

};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5,class ShouldRoundPow2 = DontRoundPow2,
          class IsReferenceType=DirectType<ArrayBaseType6d<T,s0,s1,s2,s3,s4,s5,ShouldRoundPow2> > > struct Array6d {
    typedef typename ArrayBaseType6d<T,s0,s1,s2,s3,s4,s5,ShouldRoundPow2>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array6d<T, s0, s1, s2, s3, s4, s5,
                    ShouldRoundPow2,
                    ReferenceType<ArrayBaseType6d<T, s0, s1, s2, s3, s4, s5,
                                                  ShouldRoundPow2> > > Slice;

    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2,
        size3 = s3,
        size4 = s4,
        size5 = s5,
    };
    static constexpr Array1d<uint32_t, 6, DontRoundPow2> size() {
#ifdef NOCONSTEXPR
        Array1d<uint32_t, 6, DontRoundPow2> retval = {{s0,s1,s2,s3,s4,s5}};
        return retval;
#else
        return {{s0,s1,s2,s3,s4,s5}};
#endif
    }

    static uint32_t dimension() {
        return 6;
    }
    static constexpr uint32_t dimsize() {
        return s0;
    }

    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4,
                   uint32_t i5) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4 && i5 < s5);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4][i5];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3,
                         uint32_t i4,
                         uint32_t i5) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4 && i5 < s5);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4][i5];
    }
    typename Array1d<T, s5,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4);
        typename Array1d<T, s5,ShouldRoundPow2>::Slice retval = {
            &IsReference::dereference(data)[i0][i1][i2][i3][i4]};
        return retval;
    }
    const typename Array1d<T, s5,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4);
        const typename Array1d<T, s5,ShouldRoundPow2>::Slice retval = {&IsReference::dereference(data)[i0][i1][i2][i3][i4]};
        return retval;
    }
    typename Array2d<T, s4, s5,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3);
        typename Array2d<T, s4, s5,ShouldRoundPow2>::Slice retval ={
            &IsReference::dereference(data)[i0][i1][i2][i3]};
        return retval;
    }
    const typename Array2d<T, s4, s5,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3);
        const typename Array2d<T, s4, s5,ShouldRoundPow2>::Slice retval ={
            &IsReference::dereference(data)[i0][i1][i2][i3]};
        return retval;
    }

    typename Array5d<T, s1, s2, s3, s4, s5,ShouldRoundPow2>::Slice at(uint32_t i0) {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        typename Array5d<T, s1, s2, s3, s4, s5,ShouldRoundPow2>::Slice retval = {
            &IsReference::dereference(data)[i0]};
        return retval;
    }
    const typename Array5d<T, s1, s2, s3, s4, s5,ShouldRoundPow2>::Slice at(uint32_t i0) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        const typename Array5d<T, s1, s2, s3, s4, s5,ShouldRoundPow2>::Slice retval = {
            (typename Sirikata::ReferenceType<typename Sirikata::ArrayBaseType5d<T,
                                                                                 s1,
                                                                                 s2,
                                                                                 s3,
                                                                                 s4,
                                                                                 s5,
                                                                                 ShouldRoundPow2> >
             ::ArrayType )&IsReference::dereference(data)[i0]};
        return retval;
    }
    void memset(uint8_t val) {
        memset(data, val, sizeof(Array));
    }
    template<class F> void foreach(const F& f){
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    for (uint32_t i3 = 0; i3 < s3; ++i3) {
                        for (uint32_t i4 = 0; i4 < s4; ++i4) {
                            for (uint32_t i5 = 0; i5 < s5; ++i5) {
                                f(IsReference::dereference(data)[i0][i1][i2][i3][i4][i5]);
                            }
                        }
                    }
                }
            }
        }
    }
    template<class F> void foreach(const F& f) const{
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    for (uint32_t i3 = 0; i3 < s3; ++i3) {
                        for (uint32_t i4 = 0; i4 < s4; ++i4) {
                            for (uint32_t i5 = 0; i5 < s5; ++i5) {
                                f(IsReference::dereference(data)[i0][i1][i2][i3][i4][i5]);
                            }
                        }
                    }
                }
            }
        }
    }
    T* begin() {
      return (T*)data;
    }
    const T* begin() const {
      return (const T*)data;
    }
    T* end() {
      return (T*)data + s0 * s1 * s2 * s3 * s4 * s5;
    }
    const T* end() const {
      return (const T*)data + s0 * s1 * s2 * s3 * s4 * s5;
    }

};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5,
          uint32_t s6, class ShouldRoundPow2 = DontRoundPow2,
          class IsReferenceType=DirectType<ArrayBaseType7d<T,s0,s1,s2,s3,s4,s5,s6,
                                                           ShouldRoundPow2> > > struct Array7d {
    typedef typename ArrayBaseType7d<T,s0,s1,s2,s3,s4,s5,s6,ShouldRoundPow2>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array7d<T, s0, s1, s2, s3, s4, s5, s6, ShouldRoundPow2,
        ReferenceType<ArrayBaseType7d<T, s0, s1, s2,
                                      s3, s4, s5, s6, ShouldRoundPow2> > > Slice;
    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2,
        size3 = s3,
        size4 = s4,
        size5 = s5,
        size6 = s6,
    };
    static constexpr Array1d<uint32_t, 7, DontRoundPow2> size() {
#ifdef NOCONSTEXPR
        Array1d<uint32_t, 7, DontRoundPow2> retval = {{s0,s1,s2,s3,s4,s5,s6}};
        return retval;
#else
        return {{s0,s1,s2,s3,s4,s5,s6}};
#endif
    }
    static uint32_t dimension() {
        return 7;
    }
    static constexpr uint32_t dimsize() {
        return s0;
    }

    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4,
                   uint32_t i5,
                   uint32_t i6) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4 && i5 < s5 && i6 < s6);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4][i5][i6];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3,
                         uint32_t i4,
                         uint32_t i5,
                         uint32_t i6) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4 && i5 < s5 && i6 < s6);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4][i5][i6];
    }
    typename Array1d<T, s6,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4,
                   uint32_t i5) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4 && i5 < s5);
        typename Array1d<T, s6,ShouldRoundPow2>::Slice retval =
            {&IsReference::dereference(data)[i0][i1][i2][i3][i4][i5]};
        return retval;
    }
    const typename Array1d<T, s6,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4,
                   uint32_t i5) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4 && i5 < s5);
        const typename Array1d<T, s6,ShouldRoundPow2>::Slice retval =
            {&IsReference::dereference(data)[i0][i1][i2][i3][i4][i5]};
        return retval;
    }
    typename Array2d<T, s5, s6,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4);
        typename Array2d<T, s5, s6,ShouldRoundPow2>::Slice retval =
            {&IsReference::dereference(data)[i0][i1][i2][i3][i4]};
        return retval;
    }
    const typename Array2d<T, s5, s6,ShouldRoundPow2>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0 && i1 < s1 && i2 < s2 && i3 < s3 && i4 < s4);
        const typename Array2d<T, s5, s6,ShouldRoundPow2>::Slice retval =
            {&IsReference::dereference(data)[i0][i1][i2][i3][i4]};
        return retval;
    }


    typename Array6d<T, s1, s2, s3, s4, s5, s6,ShouldRoundPow2>::Slice at(uint32_t i0) {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        typename Array6d<T, s1, s2, s3, s4, s5, s6,ShouldRoundPow2>::Slice retval =
            {&IsReference::dereference(data)[i0]};
        return retval;
    }
    const typename Array6d<T, s1, s2, s3, s4, s5, s6,
                           ShouldRoundPow2>::Slice at(uint32_t i0) const {
        ARRAY_BOUNDS_ASSERT(i0 < s0);
        const typename Array6d<T, s1, s2, s3, s4, s5, s6, ShouldRoundPow2>::Slice retval =
            {&IsReference::dereference(data)[i0]};
        return retval;
    }
    void memset(uint8_t val) {
        std::memset(data, val, sizeof(Array));
    }
    template<class F> void foreach(const F& f){
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    for (uint32_t i3 = 0; i3 < s3; ++i3) {
                        for (uint32_t i4 = 0; i4 < s4; ++i4) {
                            for (uint32_t i5 = 0; i5 < s5; ++i5) {
                                for (uint32_t i6 = 0; i6 < s6; ++i6) {
                                    f(IsReference::dereference(data)[i0][i1][i2][i3][i4][i5][i6]);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    template<class F> void foreach(const F &f) const{
        for (uint32_t i0 = 0; i0 < s0; ++i0) {
            for (uint32_t i1 = 0; i1 < s1; ++i1) {
                for (uint32_t i2 = 0; i2 < s2; ++i2) {
                    for (uint32_t i3 = 0; i3 < s3; ++i3) {
                        for (uint32_t i4 = 0; i4 < s4; ++i4) {
                            for (uint32_t i5 = 0; i5 < s5; ++i5) {
                                for (uint32_t i6 = 0; i6 < s6; ++i6) {
                                    f(IsReference::dereference(data)[i0][i1][i2][i3][i4][i5][i6]);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    T* begin() {
       return (T*)data;
    }
    const T* begin() const {
       return (const T*)data;
    }
    T* end() {
       return (T*)data + s0 * s1 * s2 * s3 * s4 * s5 * s6;
    }
    const T* end() const {
       return (const T*)data + s0 * s1 * s2 * s3 * s4 * s5 * s6;
    }

};


template<class Slice> struct AlignedArrayNd : public Slice {
    uint8_t backingStore[sizeof(typename Slice::Array) + 15];
    AlignedArrayNd() {
        init();
    }
    AlignedArrayNd(const AlignedArrayNd&other){
        // need to memcpy around to the aligned areas
        init();
        *this = other;
    }
    AlignedArrayNd& operator=(const AlignedArrayNd& other) {
        memcpy(this->data, other.data, sizeof(typename Slice::Array));
        return *this;
    }
private:
    void init() {
        uint8_t* begin = NULL;
        size_t offset = ((backingStore - begin) & 15);
        if (offset == 0) {
            this->data = (typename Slice::Array*)backingStore;
        } else {
            this->data = (typename Slice::Array*)(backingStore + 16 - offset);
        }

    }

};


template<class Slice> struct Aligned256ArrayNd : public Slice {
    uint8_t backingStore[sizeof(typename Slice::Array) + 31];
    Aligned256ArrayNd() {
        init();
    }
    Aligned256ArrayNd(const Aligned256ArrayNd&other){
        // need to memcpy around to the aligned areas
        init();
        *this = other;
    }
    Aligned256ArrayNd& operator=(const Aligned256ArrayNd& other) {
        memcpy(this->data, other.data, sizeof(typename Slice::Array));
        return *this;
    }
private:
    void init() {
        uint8_t* begin = NULL;
        size_t offset = ((backingStore - begin) & 31);
        if (offset == 0) {
            this->data = (typename Slice::Array*)backingStore;
        } else {
            this->data = (typename Slice::Array*)(backingStore + 32 - offset);
        }

    }

};

template <class T,
          uint32_t s0>
struct AlignedArray1d : AlignedArrayNd<typename Array1d<T, s0, RoundToPow2>::Slice > {};

template <class T,
          uint32_t s0, uint32_t s1>
struct AlignedArray2d : AlignedArrayNd<typename Array2d<T, s0, s1, RoundToPow2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2>
struct AlignedArray3d : AlignedArrayNd<typename Array3d<T, s0, s1, s2, RoundToPow2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3>
struct AlignedArray4d : AlignedArrayNd<typename Array4d<T, s0, s1, s2, s3, RoundToPow2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4>
struct AlignedArray5d : AlignedArrayNd<typename Array5d<T, s0, s1, s2, s3, s4, RoundToPow2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5>
struct AlignedArray6d : AlignedArrayNd<typename Array6d<T, s0, s1, s2, s3, s4, s5, RoundToPow2>::Slice > {};

template <class T,
uint32_t s0, uint32_t s1, uint32_t s2,
uint32_t s3, uint32_t s4, uint32_t s5,
          uint32_t s6> struct AlignedArray7d : AlignedArrayNd<typename Array7d<T, s0, s1, s2, s3, s4, s5, s6, RoundToPow2>::Slice > {};






template <class T,
          uint32_t s0>
struct Aligned256Array1d : Aligned256ArrayNd<typename Array1d<T, s0, RoundToPow2>::Slice > {};

template <class T,
          uint32_t s0, uint32_t s1>
struct Aligned256Array2d : Aligned256ArrayNd<typename Array2d<T, s0, s1, RoundToPow2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2>
struct Aligned256Array3d : Aligned256ArrayNd<typename Array3d<T, s0, s1, s2, RoundToPow2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3>
struct Aligned256Array4d : Aligned256ArrayNd<typename Array4d<T, s0, s1, s2, s3, RoundToPow2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4>
struct Aligned256Array5d : Aligned256ArrayNd<typename Array5d<T, s0, s1, s2, s3, s4, RoundToPow2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5>
struct Aligned256Array6d : Aligned256ArrayNd<typename Array6d<T, s0, s1, s2, s3, s4, s5, RoundToPow2>::Slice > {};

template <class T,
uint32_t s0, uint32_t s1, uint32_t s2,
uint32_t s3, uint32_t s4, uint32_t s5,
          uint32_t s6> struct Aligned256Array7d : Aligned256ArrayNd<typename Array7d<T, s0, s1, s2, s3, s4, s5, s6, RoundToPow2>::Slice > {};
}


#if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus > 199711L
#undef constexpr
#undef NOCONSTEXPR
#endif
#endif //_SIRIKATA_ARRAY_ND_HPP_
