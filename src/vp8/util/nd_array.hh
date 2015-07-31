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
#include <cstddef>
#include <cstdint>
#else
#include <stdint.h>
#include <stddef.h>
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
          uint32_t s0, class IsReferenceType=ReferenceType<ArrayBaseType1d<T,s0> > > struct Slice1d {
    typedef typename ArrayBaseType1d<T,s0>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType ptr;
    
    T& at(uint32_t i0) {
        assert(i0 < s0);
        return IsReference::dereference(ptr)[i0];
    }
    const T& at(uint32_t i0) const {
        assert(i0 < s0);
        return IsReference::dereference(ptr)[i0];
    }
};

template <class T,
          uint32_t s0, uint32_t s1,
          class IsReferenceType=ReferenceType<ArrayBaseType2d<T,s0,s1> > > struct Slice2d {
    typedef typename ArrayBaseType2d<T,s0,s1>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType ptr;
    
    T& at(uint32_t i0,
                   uint32_t i1) {
        assert(i0 < s0);
        assert(i1 < s1);
        return IsReference::dereference(ptr)[i0][i1];
    }
    const T& at(uint32_t i0,
                         uint32_t i1) const {
        assert(i0 < s0);
        assert(i1 < s1);
        return IsReference::dereference(ptr)[i0][i1];
    }
    Slice1d<T, s1> at(uint32_t i0) {
        return {&IsReference::dereference(ptr)[i0]};
    }
    const Slice1d<T, s1> at(uint32_t i0) const {
        return {&IsReference::dereference(ptr)[i0]};
    }
};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          class IsReferenceType=ReferenceType<ArrayBaseType3d<T,s0,s1,s2> > > struct Slice3d {
    typedef typename ArrayBaseType3d<T,s0,s1,s2>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType ptr;
    
    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return IsReference::dereference(ptr)[i0][i1][i2];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return IsReference::dereference(ptr)[i0][i1][i2];
    }
    Slice1d<T, s2> at(uint32_t i0,
                   uint32_t i1) {
        assert(i0 < s0);
        assert(i1 < s1);
        return {&IsReference::dereference(ptr)[i0][i1]};
    }
    const Slice1d<T, s2> at(uint32_t i0,
                   uint32_t i1) const {
        assert(i0 < s0);
        assert(i1 < s1);
        return {&IsReference::dereference(ptr)[i0][i1]};
    }
    Slice2d<T, s1, s2> at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
    const Slice2d<T, s1, s2> at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }

};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3,
          class IsReferenceType=ReferenceType<ArrayBaseType4d<T,s0,s1,s2,s3> > > struct Slice4d {
    typedef typename ArrayBaseType4d<T,s0,s1,s2,s3>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType ptr;
    
    T& at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return IsReference::dereference(ptr)[i0][i1][i2][i3];
    }
    const T& at(uint32_t i0,
                         uint32_t i1,
                         uint32_t i2,
                         uint32_t i3) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return IsReference::dereference(ptr)[i0][i1][i2][i3];
    }
    Slice1d<T, s3> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return {&IsReference::dereference(ptr)[i0][i1][i2]};
    }
    const Slice1d<T, s3> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return {&IsReference::dereference(ptr)[i0][i1][i2]};
    }
    Slice2d<T, s2, s3> at(uint32_t i0,
                   uint32_t i1) {
        assert(i0 < s0);
        assert(i1 < s1);
        return {&IsReference::dereference(ptr)[i0][i1]};
    }
    const Slice2d<T, s2, s3> at(uint32_t i0,
                   uint32_t i1) const {
        assert(i0 < s0);
        assert(i1 < s1);
        return {&IsReference::dereference(ptr)[i0][i1]};
    }

    Slice3d<T, s1, s2, s3> at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
    const Slice3d<T, s1, s2, s3> at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4,
          class IsReferenceType=ReferenceType<ArrayBaseType5d<T,s0,s1,s2,s3,s4> > > struct Slice5d {
    typedef typename ArrayBaseType5d<T,s0,s1,s2,s3,s4>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType ptr;
    
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
        return IsReference::dereference(ptr)[i0][i1][i2][i3][i4];
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
        return IsReference::dereference(ptr)[i0][i1][i2][i3][i4];
    }
    Slice1d<T, s4> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3]};
    }
    const Slice1d<T, s4> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3]};
    }
    Slice2d<T, s3, s4> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return {&IsReference::dereference(ptr)[i0][i1][i2]};
    }
    const Slice2d<T, s3, s4> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        return {&IsReference::dereference(ptr)[i0][i1][i2]};
    }

    Slice4d<T, s1, s2, s3, s4> at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
    const Slice4d<T, s1, s2, s3, s4> at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5,
          class IsReferenceType=ReferenceType<ArrayBaseType6d<T,s0,s1,s2,s3,s4,s5> > > struct Slice6d {
    typedef typename ArrayBaseType6d<T,s0,s1,s2,s3,s4,s5>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType ptr;
    
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
        return IsReference::dereference(ptr)[i0][i1][i2][i3][i4][i5];
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
        return IsReference::dereference(ptr)[i0][i1][i2][i3][i4][i5];
    }
    Slice1d<T, s5> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3][i4]};
    }
    const Slice1d<T, s5> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3][i4]};
    }
    Slice2d<T, s4, s5> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3]};
    }
    const Slice2d<T, s4, s5> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3]};
    }

    Slice5d<T, s1, s2, s3, s4, s5> at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
    const Slice5d<T, s1, s2, s3, s4, s5> at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5,
          uint32_t s6, class IsReferenceType=ReferenceType<ArrayBaseType7d<T,s0,s1,s2,s3,s4,s5,s6> > > struct Slice7d {
    typedef typename ArrayBaseType7d<T,s0,s1,s2,s3,s4,s5,s6>::Array Array;
    typedef IsReferenceType IsReference;
    typename IsReference::ArrayType ptr;
    
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
        return IsReference::dereference(ptr)[i0][i1][i2][i3][i4][i5][i6];
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
        return IsReference::dereference(ptr)[i0][i1][i2][i3][i4][i5][i6];
    }
    Slice1d<T, s6> at(uint32_t i0,
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
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3][i4][i5]};
    }
    const Slice1d<T, s6> at(uint32_t i0,
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
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3][i4][i5]};
    }
    Slice2d<T, s5, s6> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3][i4]};
    }
    const Slice2d<T, s5, s6> at(uint32_t i0,
                   uint32_t i1,
                   uint32_t i2,
                   uint32_t i3,
                   uint32_t i4) const {
        assert(i0 < s0);
        assert(i1 < s1);
        assert(i2 < s2);
        assert(i3 < s3);
        assert(i4 < s4);
        return {&IsReference::dereference(ptr)[i0][i1][i2][i3][i4]};
    }


    Slice6d<T, s1, s2, s3, s4, s5, s6> at(uint32_t i0) {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
    const Slice6d<T, s1, s2, s3, s4, s5, s6> at(uint32_t i0) const {
        assert(i0 < s0);
        return {&IsReference::dereference(ptr)[i0]};
    }
};

template <class T,uint32_t s0>
struct Array1d : Slice1d<T, s0, DirectType<ArrayBaseType1d<T, s0> > > {};

template <class T,
uint32_t s0, uint32_t s1>
struct Array2d : Slice2d<T, s0, s1, DirectType<ArrayBaseType2d<T, s0, s1> > > {};

template <class T,
uint32_t s0, uint32_t s1, uint32_t s2>
struct Array3d : Slice3d<T, s0, s1, s2, DirectType<ArrayBaseType3d<T, s0, s1, s2> > > {};

template <class T, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3>
struct Array4d : Slice4d<T, s0, s1, s2, s3, DirectType<ArrayBaseType4d<T, s0, s1, s2, s3> > > {};

template <class T, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4>
struct Array5d : Slice5d<T, s0, s1, s2, s3, s4, DirectType<ArrayBaseType5d<T, s0, s1, s2, s3, s4> > > {};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5>
struct Array6d : Slice6d<T, s0, s1, s2, s3, s4, s5,
                         DirectType<ArrayBaseType6d<T, s0, s1, s2, s3, s4, s5> > > {};

template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5,
          uint32_t s6>
struct Array7d : Slice7d<T, s0, s1, s2, s3, s4, s5, s6,
                         DirectType<ArrayBaseType7d<T, s0, s1, s2,
                                                    s3, s4, s5, s6> > > {
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
        memcpy(this->ptr, other.ptr, sizeof(typename Slice::Array));
        return *this;
    }
private:
    void init() {
        uint8_t* begin = NULL;
        size_t offset = ((backingStore - begin) & 15);
        if (offset == 0) {
            this->ptr = (typename Slice::Array*)backingStore;
        } else {
            this->ptr = (typename Slice::Array*)(backingStore + 16 - offset);
        }

    }

};

template <class T,
          uint32_t s0>
struct AlignedArray1d : AlignedArrayNd<Slice1d<T, s0> > {};

template <class T,
          uint32_t s0, uint32_t s1>
struct AlignedArray2d : AlignedArrayNd<Slice2d<T, s0, s1> > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2>
struct AlignedArray3d : AlignedArrayNd<Slice3d<T, s0, s1, s2> > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3>
struct AlignedArray4d : AlignedArrayNd<Slice4d<T, s0, s1, s2, s3> > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4>
struct AlignedArray5d : AlignedArrayNd<Slice5d<T, s0, s1, s2, s3, s4> > {};


template <class T,
          uint32_t s0, uint32_t s1, uint32_t s2,
          uint32_t s3, uint32_t s4, uint32_t s5>
struct AlignedArray6d : AlignedArrayNd<Slice6d<T, s0, s1, s2, s3, s4, s5> > {};

template <class T,
uint32_t s0, uint32_t s1, uint32_t s2,
uint32_t s3, uint32_t s4, uint32_t s5,
uint32_t s6> struct AlignedArray7d : AlignedArrayNd<Slice7d<T, s0, s1, s2, s3, s4, s5, s6> > {};
}


#endif //_SIRIKATA_ARRAY_ND_HPP_
