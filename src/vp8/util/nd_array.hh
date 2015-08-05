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

#ifndef _SIRIKATA_ARRAY_ND_HPP_
#define _SIRIKATA_ARRAY_ND_HPP_
#if __GXX_EXPERIMENTAL_CXX0X__ || __cplusplus > 199711L
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#else
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define constexpr
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

template<class T, uint32_t s0> struct ArrayBaseType1d {
    typedef T Array[s0];
};
template<class T, uint32_t s0, uint32_t s1> struct ArrayBaseType2d {
    typedef T Array[s0][s1];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2> struct ArrayBaseType3d {
    typedef T Array[s0][s1][s2];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3> struct ArrayBaseType4d {
    typedef T Array[s0][s1][s2][s3];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4> struct ArrayBaseType5d {
    typedef T Array[s0][s1][s2][s3][s4];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2,
         uint32_t s3, uint32_t s4, uint32_t s5> struct ArrayBaseType6d {
    typedef T Array[s0][s1][s2][s3][s4][s5];
};
template<class T, uint32_t s0, uint32_t s1, uint32_t s2,
         uint32_t s3, uint32_t s4, uint32_t s5, uint32_t s6> struct ArrayBaseType7d {
    typedef T Array[s0][s1][s2][s3][s4][s5][s6];
};

template <class T,
          uint32_t s0, class IsReferenceType=DirectType<ArrayBaseType1d<T,s0> > > struct Array1d {
    typedef typename ArrayBaseType1d<T,s0>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array1d<T, s0, ReferenceType<ArrayBaseType1d<T, s0> > > Slice;
    enum Sizes{
        size0 = s0
    };
    static constexpr uint32_t size() {
        return s0;
    }
    static constexpr uint32_t dimension() {
        return 1;
    }
    T& at(uint32_t i0) {
        assert(i0 < s0);
        return IsReference::dereference(data)[i0];
    }
    const T& at(uint32_t i0) const {
        assert(i0 < s0);
        return IsReference::dereference(data)[i0];
    }
    T& operator[](uint32_t i0) {
        assert(i0 < s0);
        return IsReference::dereference(data)[i0];
    }
    const T& operator[](uint32_t i0) const {
        assert(i0 < s0);
        return IsReference::dereference(data)[i0];
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
};

template <class T,
          uint32_t s0, uint32_t s1,
          class IsReferenceType=DirectType<ArrayBaseType2d<T,s0,s1> > > struct Array2d {
    typedef typename ArrayBaseType2d<T,s0,s1>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array2d<T, s0, s1, ReferenceType<ArrayBaseType2d<T, s0, s1> > > Slice;
    enum Sizes{
        size0 = s0,
        size1 = s1
    };
    static constexpr Array1d<uint32_t, 2> size() {
        return {{s0, s1}};
    }
    static uint32_t dimension() {
        return 2;
    }
    const T& raster(uint32_t offset) const {
        assert(offset < s0 * s1);
        return reinterpret_cast<const T*>(&data)[offset];
    }
    T& raster(uint32_t offset) {
        assert(offset < s0 * s1);
        return reinterpret_cast<T*>(&data)[offset];
    }
    T& at(uint32_t i0,
                   uint32_t i1) {
        assert(i0 < s0);
        assert(i1 < s1);
        return IsReference::dereference(data)[i0][i1];
    }
    const T& at(uint32_t i0,
                         uint32_t i1) const {
        assert(i0 < s0);
        assert(i1 < s1);
        return IsReference::dereference(data)[i0][i1];
    }
    typename Array1d<T, s1>::Slice at(uint32_t i0) {
        return {&IsReference::dereference(data)[i0]};
    }
    const typename Array1d<T, s1>::Slice at(uint32_t i0) const {
        return {&IsReference::dereference(data)[i0]};
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
};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          class IsReferenceType=DirectType<ArrayBaseType3d<T,s0,s1,s2> > > struct Array3d {
    typedef typename ArrayBaseType3d<T,s0,s1,s2>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array3d<T, s0, s1, s2, ReferenceType<ArrayBaseType3d<T, s0, s1, s2> > > Slice;

    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2
    };
    static constexpr Array1d<uint32_t, 3> size() {
        return {{s0,s1,s2}};
    }
    static uint32_t dimension() {
        return 3;
    }
    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return IsReference::dereference(data)[i0][i1][i2];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return IsReference::dereference(data)[i0][i1][i2];
    }
    typename Array1d<T, s2>::Slice at(uint32_t i0,
                   uint32_t i1) {
        assert(i0 < s0);
        assert(i1 < s1);
        return {&IsReference::dereference(data)[i0][i1]};
    }
    const typename Array1d<T, s2>::Slice at(uint32_t i0,
                   uint32_t i1) const {
        assert(i0 < s0);
        assert(i1 < s1);
        return {&IsReference::dereference(data)[i0][i1]};
    }
    typename Array2d<T, s1, s2>::Slice at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
    }
    const typename Array2d<T, s1, s2>::Slice at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
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
};




template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3,
          class IsReferenceType=DirectType<ArrayBaseType4d<T,s0,s1,s2,s3> > > struct Array4d {
    typedef typename ArrayBaseType4d<T,s0,s1,s2,s3>::Array Array;
    typedef Array4d<T, s0, s1, s2, s3, ReferenceType<ArrayBaseType4d<T, s0, s1, s2, s3> > > Slice;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2,
        size3 = s3
    };

    static constexpr Array1d<uint32_t, 4> size() {
        return {{s0,s1,s2,s3}};
    }
    static uint32_t dimension() {
        return 4;
    }
    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return IsReference::dereference(data)[i0][i1][i2][i3];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return IsReference::dereference(data)[i0][i1][i2][i3];
    }
    typename Array1d<T, s3>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return {&IsReference::dereference(data)[i0][i1][i2]};
    }
    const typename Array1d<T, s3>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return {&IsReference::dereference(data)[i0][i1][i2]};
    }
    typename Array2d<T, s2, s3>::Slice at(uint32_t i0,
                   uint32_t i1) {
        assert(i0 < s0);
        assert(i1 < s1);
        return {&IsReference::dereference(data)[i0][i1]};
    }
    const typename Array2d<T, s2, s3>::Slice at(uint32_t i0,
                   uint32_t i1) const {
        assert(i0 < s0);
        assert(i1 < s1);
        return {&IsReference::dereference(data)[i0][i1]};
    }

    typename Array3d<T, s1, s2, s3>::Slice at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
    }
    const typename Array3d<T, s1, s2, s3>::Slice at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
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
};




template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4,
          class IsReferenceType=DirectType<ArrayBaseType5d<T,s0,s1,s2,s3,s4> > > struct Array5d {
    typedef typename ArrayBaseType5d<T,s0,s1,s2,s3,s4>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;


    typedef Array5d<T, s0, s1, s2, s3, s4, ReferenceType<ArrayBaseType5d<T, s0, s1, s2, s3, s4> > > Slice;
    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2,
        size3 = s3,
        size4 = s4
    };
    static constexpr Array1d<uint32_t, 5> size() {
        return {{s0,s1,s2,s3,s4}};
    }
    static uint32_t dimension() {
        return 5;
    }
    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3,
                         uint32_t i4) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4];
    }
    typename Array1d<T, s4>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return {&IsReference::dereference(data)[i0][i1][i2][i3]};
    }
    const typename Array1d<T, s4>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return {&IsReference::dereference(data)[i0][i1][i2][i3]};
    }
    typename Array2d<T, s3, s4>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return {&IsReference::dereference(data)[i0][i1][i2]};
    }
    const typename Array2d<T, s3, s4>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return {&IsReference::dereference(data)[i0][i1][i2]};
    }

    typename Array4d<T, s1, s2, s3, s4>::Slice at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
    }
    const typename Array4d<T, s1, s2, s3, s4>::Slice at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
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
};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5,
          class IsReferenceType=DirectType<ArrayBaseType6d<T,s0,s1,s2,s3,s4,s5> > > struct Array6d {
    typedef typename ArrayBaseType6d<T,s0,s1,s2,s3,s4,s5>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array6d<T, s0, s1, s2, s3, s4, s5,
                    ReferenceType<ArrayBaseType6d<T, s0, s1, s2, s3, s4, s5> > > Slice;

    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2,
        size3 = s3,
        size4 = s4,
        size5 = s5,
    };
    static constexpr Array1d<uint32_t, 6> size() {
        return {{s0,s1,s2,s3,s4,s5}};
    }

    static uint32_t dimension() {
        return 6;
    }
    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4,
                   uint32_t i5) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        assert(i5 < s5);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4][i5];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3,
                         uint32_t i4,
                         uint32_t i5) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        assert(i5 < s5);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4][i5];
    }
    typename Array1d<T, s5>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return {&IsReference::dereference(data)[i0][i1][i2][i3][i4]};
    }
    const typename Array1d<T, s5>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return {&IsReference::dereference(data)[i0][i1][i2][i3][i4]};
    }
    typename Array2d<T, s4, s5>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return {&IsReference::dereference(data)[i0][i1][i2][i3]};
    }
    const typename Array2d<T, s4, s5>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return {&IsReference::dereference(data)[i0][i1][i2][i3]};
    }

    typename Array5d<T, s1, s2, s3, s4, s5>::Slice at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
    }
    const typename Array5d<T, s1, s2, s3, s4, s5>::Slice at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
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
};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5,
          uint32_t s6, class IsReferenceType=DirectType<ArrayBaseType7d<T,s0,s1,s2,s3,s4,s5,s6> > > struct Array7d {
    typedef typename ArrayBaseType7d<T,s0,s1,s2,s3,s4,s5,s6>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType data;
    typedef Array7d<T, s0, s1, s2, s3, s4, s5, s6,
        ReferenceType<ArrayBaseType7d<T, s0, s1, s2,
                                      s3, s4, s5, s6> > > Slice;
    enum Sizes{
        size0 = s0,
        size1 = s1,
        size2 = s2,
        size3 = s3,
        size4 = s4,
        size5 = s5,
        size6 = s6,
    };
    static constexpr Array1d<uint32_t, 7> size() {
        return {{s0,s1,s2,s3,s4,s5,s6}};
    }
    static uint32_t dimension() {
        return 7;
    }
    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4,
                   uint32_t i5,
                   uint32_t i6) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        assert(i5 < s5);
        assert(i6 < s6);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4][i5][i6];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3,
                         uint32_t i4,
                         uint32_t i5,
                         uint32_t i6) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        assert(i5 < s5);
        assert(i6 < s6);
        return IsReference::dereference(data)[i0][i1][i2][i3][i4][i5][i6];
    }
    typename Array1d<T, s6>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4,
                   uint32_t i5) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        assert(i5 < s5);
        return {&IsReference::dereference(data)[i0][i1][i2][i3][i4][i5]};
    }
    const typename Array1d<T, s6>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4,
                   uint32_t i5) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        assert(i5 < s5);
        return {&IsReference::dereference(data)[i0][i1][i2][i3][i4][i5]};
    }
    typename Array2d<T, s5, s6>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return {&IsReference::dereference(data)[i0][i1][i2][i3][i4]};
    }
    const typename Array2d<T, s5, s6>::Slice at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return {&IsReference::dereference(data)[i0][i1][i2][i3][i4]};
    }


    typename Array6d<T, s1, s2, s3, s4, s5, s6>::Slice at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
    }
    const typename Array6d<T, s1, s2, s3, s4, s5, s6>::Slice at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(data)[i0]};
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

template <class T,
          uint32_t s0>
struct AlignedArray1d : AlignedArrayNd<typename Array1d<T, s0>::Slice > {};

template <class T,
          uint32_t s0, uint32_t s1>
struct AlignedArray2d : AlignedArrayNd<typename Array2d<T, s0, s1>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2>
struct AlignedArray3d : AlignedArrayNd<typename Array3d<T, s0, s1, s2>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3>
struct AlignedArray4d : AlignedArrayNd<typename Array4d<T, s0, s1, s2, s3>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4>
struct AlignedArray5d : AlignedArrayNd<typename Array5d<T, s0, s1, s2, s3, s4>::Slice > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5>
struct AlignedArray6d : AlignedArrayNd<typename Array6d<T, s0, s1, s2, s3, s4, s5>::Slice > {};

template <class T,
uint32_t s0, uint32_t s1, uint32_t s2,
uint32_t s3, uint32_t s4, uint32_t s5,
          uint32_t s6> struct AlignedArray7d : AlignedArrayNd<typename Array7d<T, s0, s1, s2, s3, s4, s5, s6>::Slice > {};
}


#endif //_SIRIKATA_ARRAY_ND_HPP_
