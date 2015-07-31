#include <assert.h>
#include <cstdint>
#include <cstddef>
#include "../vp8/util/nd_array.hh"
#include <stdio.h>
struct Data {
    unsigned char prob;
    unsigned short trueCount;
    unsigned short falseCount;
};
void podtest(int hellote,...) {

}
int main() {
    using namespace Sirikata;
    AlignedArray7d<unsigned char, 1,3,2,5,4,6,16> aligned7d;
    uint8_t* d =&aligned7d.at(0, 2, 1, 3, 2, 1, 0);
    *d = 4;
    size_t offset = d - (uint8_t*)nullptr;
    assert(0 == (offset & 15) && "Must have alignment");
    assert(aligned7d.at(0, 2, 1, 3, 2, 1, 0) == 4);
    Array7d<unsigned char, 1,3,2,5,3,3,16> a7;
    uint8_t* d2 =&a7.at(0, 2, 1, 3, 2, 1, 0);
    *d2 = 5;
    offset = d2 - (uint8_t*)nullptr;
    if (offset & 15) {
        fprintf(stderr, "Array7d array doesn't require alignment");
    }
    assert(a7.at(0, 2, 1, 3, 2, 1, 0) == 5);
    a7.at(0, 2, 1, 3, 2, 1, 1) = 8;
    assert(a7.at(0, 2, 1, 3, 2, 1, 1) == 8);
    Slice1d<unsigned char, 16> s = a7.at(0, 2, 1, 3, 2, 1);
    s.at(1) = 16;
    assert(a7.at(0, 2, 1, 3, 2, 1, 1) == 16);
    s.at(0) = 6;
    assert(a7.at(0, 2, 1, 3, 2, 1, 0) == 6);
    {
        a7.at(0,0,0,0,0,0,0) = 16;
        assert(a7.at(0,0,0,0,0,0,0) == 16);
        auto x = a7.at(0);
        auto y = x.at(0);
        auto z = y.at(0);
        auto w = z.at(0);
        auto a = w.at(0);
        auto b = a.at(0);
        b.at(0) = 47;
        assert(a7.at(0,0,0,0,0,0,0) == 47);
    }
    podtest(4, a7);
}

