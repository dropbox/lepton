#include <assert.h>
#include <cstdint>
#include <cstddef>
#include "../vp8/util/nd_array.hh"
#include "../vp8/model/numeric.hh"
#include "../io/MuxReader.hh"
#include "../io/MemReadWriter.hh"
#include <stdio.h>
struct Data {
    unsigned char prob;
    unsigned short trueCount;
    unsigned short falseCount;
};
void podtest(int hellote,...) {
    (void)hellote;
}

class LazyReaderWrapper : public Sirikata::DecoderReader{
    Sirikata::DecoderReader *dr;
    uint32_t mState;
public:
    LazyReaderWrapper(Sirikata::DecoderReader *r) {
        dr = r;
        mState = 0;
    }
    std::pair<uint32_t, Sirikata::JpegError> Read(uint8_t *data, uint32_t size) {
        ++mState;
        if (mState%4 == 0) {
            size = 1;
        } else if (mState%4 == 1) {
            size = std::min(size, 2U);
        } else if (mState %4 == 2) {
            size = std::min(size, 2U);
        } else if (mState %4 == 3) {
            size = std::min(size, 1U);
        }
        return dr->Read(data, size);
    }
};
void setupTestData(std::vector<uint8_t> *testData, size_t num_items) {
    for(size_t j = 0; j < num_items; ++j) {
        for (size_t i = 0; i + 3 < testData[j].size(); i += 4) {
            switch(j) {
              case 0:
                testData[j][i] = i & 0xff;
                testData[j][i+1] = (1 ^ (i >> 8)) & 0xff;
                testData[j][i+2] = (2 ^ (i >> 16)) & 0xff;
                testData[j][i+3] = (3 ^ (i >> 24)) & 0xff;
                break;
              case 1:
                testData[j][i] = testData[0][i%testData[0].size()]
                    ^ testData[0][(i+1)%testData[0].size()];
                testData[j][i + 1] = testData[0][(i + 1)%testData[0].size()]
                    ^ testData[0][(i+2)%testData[0].size()];
                testData[j][i + 2 ] = testData[0][(i + 2)%testData[0].size()]
                    ^ testData[0][(i+3)%testData[0].size()];
                testData[j][i + 3 ] = testData[0][(i + 3)%testData[0].size()]
                    ^ testData[0][(i+0)%testData[0].size()];
                break;
              default:
                testData[j][i] = (j ^ (i + 1) ^ 0xf89721) & 0xff;
                testData[j][i] = ((j ^ (i + 1) ^ 0xf89721) >> 8) & 0xff;
                testData[j][i] = ((j ^ (i + 1) ^ 0xf89721) >> 16) & 0xff;
                testData[j][i] = ((j ^ (i + 1) ^ 0xf89721) >> 24) & 0xff;
            }
        }
    }
}
void EofHelper(bool useLazyWrapper) {
    std::vector<uint8_t> testData[4];
    const uint32_t a0 = 2, a1 = 257, b0 = 2, b1 = 4097,
        c0 = 256, c1 = 16385, d0 = 2, d1 = 32768, e0 = 256, e1 = 65537, f1 = 65536;
    const uint32_t maxtesty = a1 + b1 + c1 + d1 + e1 + f1;
    testData[0].resize(260 + 258);
    testData[1].resize(maxtesty);
    setupTestData(testData, sizeof(testData)/sizeof(testData[0]));
    using namespace Sirikata;
    JpegAllocator<uint8_t> alloc;
    MemReadWriter rw(alloc);
    LazyReaderWrapper lrw(&rw);
    MuxReader reader(alloc, 4, 65536, useLazyWrapper ? (DecoderReader*)&lrw : (DecoderReader*)&rw);
    MuxWriter writer(&rw, alloc);
    writer.Write(1, &testData[1][0], a1);
    writer.Write(0, &testData[0][0], a0);
    writer.Write(1, &testData[1][a1], b1);
    writer.Write(0, &testData[0][a0], b0);
    writer.Write(1, &testData[1][a1 + b1], c1);
    writer.Write(0, &testData[0][a0 + b0], c0);        
    writer.Write(1, &testData[1][a1 + b1 + c1], d1);
    writer.Write(0, &testData[0][a0 + b0 + c0], d0);
    writer.Write(1, &testData[1][a1 + b1 + c1 + d1], e1);
    writer.Write(0, &testData[0][a0 + b0 + c0 + d0], e0);
    writer.Write(1, &testData[1][a1 + b1 + c1 + d1+ e1 ], f1);
    writer.Close();
    uint8_t testx[260];
    uint32_t offset = 0;
    while(offset<sizeof(testx)) {
        std::pair<uint32_t, JpegError> r = reader.Read(0, testx + offset, sizeof(testx) - offset);
        assert(r.first > 0);
        assert(r.second == JpegError::nil());
        offset += r.first;
        if (!r.first) {
            return; // ERROR IN TEST;
        }
    }
    assert(memcmp(testx, &testData[0][0], sizeof(testx)) == 0);
    offset = 0;
    std::pair<uint32_t, JpegError> expectEof(0, JpegError::nil());
    while(offset<sizeof(testx)) {
        expectEof = reader.Read(0, testx + offset, sizeof(testx) - offset);
        offset += expectEof.first;
        if (expectEof.second != JpegError::nil()) {
            break;
        }
    }
    assert(offset == 258);
    assert(expectEof.second == JpegError::errEOF());
    
    uint8_t testy[maxtesty];
    offset = 0;
    while(offset<sizeof(testy)) {
        std::pair<uint32_t, JpegError> r = reader.Read(1, testy + offset, sizeof(testy) - offset);
        assert(r.first > 0);
        assert(r.second == JpegError::nil());
        offset += r.first;
        if (r.second != JpegError::nil() || !r.first) {
            return; // ERROR IN TEST;
        }
    }
    int res = memcmp(testy, &testData[1][0], sizeof(testy));
    if (res) {
        for (size_t i = 0; i < sizeof(testy); ++i) {
            if (testy[i] != testData[1][i]) {
                fprintf(stderr, "data[%d]:: %x != %x\n", (int)i, testy[i], testData[1][i]);
                }
        }
    }
    assert(!res);
    
}
void testEof() {
    EofHelper(true);
    EofHelper(false);
}
void RoundtripHelper(bool useLazyWrapper) {
    std::vector<uint8_t> testData[4];
    std::vector<uint8_t> roundTrip[sizeof(testData) / sizeof(testData[0])];
    uint32_t progress[sizeof(testData) / sizeof(testData[0])] = {0};
    int invProb[sizeof(testData) / sizeof(testData[0])];
    for (size_t j = 0; j < sizeof(testData) / sizeof(testData[0]); ++j) {
        testData[j].resize(131072 * 4 + 1);
        invProb[j] = j + 2;
    }
    testData[3].resize(testData[3].size() + 1);
    memset(&testData[3][0], 0xef, testData[3].size());
    testData[3].back() = 0x47;
    
    setupTestData(testData, sizeof(testData)/ sizeof(testData[0]));
    using namespace Sirikata;
    JpegAllocator<uint8_t> alloc;
    MemReadWriter rw(alloc);
    LazyReaderWrapper lrw(&rw);
    MuxReader reader(alloc, 4, 65536, useLazyWrapper ? (DecoderReader*)&lrw : (DecoderReader*)&rw);
    MuxWriter writer(&rw, alloc);
    srand(1023);
    
    bool allDone;
    do {
        allDone = true;
        for(size_t j = 0; j < sizeof(testData) / sizeof(testData[0]); ++j) {
            if (progress[j] < testData[j].size()) {
                if (rand() < RAND_MAX / invProb[j]) {
                    size_t desiredWrite = 1;
                    if (rand() < rand()/3) {
                        desiredWrite = rand()%5;
                    }
                    if (rand() < rand()/8) {
                        desiredWrite = rand()%256;
                    }
                    
                    if (j >= 1 && rand() < rand()/32) {
                            desiredWrite = 4096;
                    }
                    if (j >= 2 && rand() < rand()/64) {
                        desiredWrite = 16384;
                    }
                    
                    if (j == 3 && rand() < rand()/128) {
                        desiredWrite = 131073;
                    }
                    uint32_t amountToWrite = std::min(desiredWrite,
                                                      testData[j].size() - progress[j]);
                    
                    JpegError err = writer.Write(j,
                                                 &testData[j][progress[j]],
                                                 amountToWrite).second;
                    (void)err;
                    progress[j] += amountToWrite;
                    assert(err == JpegError::nil());
                }
                allDone = false;
            }
        }
    }while(!allDone);
    writer.Close();
    for(size_t j = 0; j < sizeof(testData) / sizeof(testData[0]); ++j) {
        assert(progress[j] == testData[j].size());
        progress[j] = 0;
        roundTrip[j].resize(testData[j].size());
    }
    do {
        allDone = true;
        for(size_t j = 0; j < sizeof(testData) / sizeof(testData[0]); ++j) {
            if (progress[j] < testData[j].size()) {
                if (rand() < RAND_MAX / invProb[j]) {
                    size_t desiredRead = 1;
                    if (rand() < rand()/3) {
                        desiredRead = rand()%5;
                    }
                    if (rand() < rand()/8) {
                        desiredRead = rand()%256;
                    }
                    
                    if (j >= 1 && rand() < rand()/32) {
                        desiredRead = 4096;
                    }
                    if (j >= 2 && rand() < rand()/64) {
                        desiredRead = 16384;
                    }
                    
                    if (j == 3 && rand() < rand()/128) {
                        desiredRead = 131073;
                    }
                    uint32_t amountToRead = std::min(desiredRead,
                                                     testData[j].size() - progress[j]);
                    
                    std::pair<uint32_t,JpegError> ret = reader.Read(j,
                                                                    &roundTrip[j][progress[j]],
                                                                    amountToRead);
                    for (uint32_t r = 0; r < ret.first; ++r) {
                        assert(roundTrip[j][progress[j] + r]
                               == testData[j][progress[j] + r]);
                    }
                    progress[j] += ret.first;
                    assert(ret.second == JpegError::nil());
                }
                allDone = false;
            }
        }
    }while(!allDone);
    for(size_t j = 0; j < sizeof(testData) / sizeof(testData[0]); ++j) {
        assert(roundTrip[j] ==
                         testData[j]);
    }
}
void testRoundtrip() {
    RoundtripHelper(true);
    RoundtripHelper(false);
}
//constexpr Sirikata::AlignedArray1d<uint8_t, 16> karray
constexpr Sirikata::Array1d<uint8_t, 16 > karray
    = {{0,1,2,3,4,5,6,7,8,9,0xa, 0xb, 0xc, 0xd, 0xe, 0xf}};
int main() {
    Sirikata::memmgr_init(768 * 1024 * 1024,
                          64 * 1024 * 1024,
                          3,
                          256);

    for (size_t i = 0; i < karray.size(); ++i) {
        assert(karray[i] == i);
    }
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
    Array1d<unsigned char, 16>::Slice s = a7.at(0, 2, 1, 3, 2, 1);
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
    testEof();
    testRoundtrip();
    for (int i = 0; i < 65536; ++i) {
        assert(bit_length((uint16_t)i) == uint16bit_length(i));
        if (i > 0) {
            assert(log2((uint16_t)i) == uint16log2(i));
        }
        
    }
    for (int denom = 1; denom < 1026; ++denom) {
        for (int num = 256; num < 262144; num += 256) {
            assert(slow_divide18bit_by_10bit(num, denom) == (unsigned int)num/denom);
            assert(fast_divide18bit_by_10bit(num, denom) == (unsigned int)num / denom);
            if (num < 16384) {
                assert(fast_divide16bit(num, denom) == (unsigned int)num / denom);
                if (denom == 5) {
                    assert(templ_divide16bit<5>(num) == (unsigned int)num / denom);
                    __m128i retval = divide16bit_vec_signed<5>(_mm_set_epi32(num,(int)-num, -num-1, -num + 1));
                    int ret[4];
                    _mm_storeu_si128((__m128i*)(char *)ret, retval);
                    assert(ret[3] == num / denom);
                    assert(ret[2] == -num / denom);
                    assert(ret[1] == (-num-1)/denom);
                    assert(ret[0] == (-num + 1) / denom);
                }
                if (denom == 767) {
                    __m128i retval = divide16bit_vec<767>(_mm_set_epi32(num,num + 1, 0, num * 2));
                    int ret[4];
                    _mm_storeu_si128((__m128i*)(char *)ret, retval);
                    assert(ret[3] == num / denom);
                    assert(ret[2] == (1 + num) / denom);
                    assert(ret[1] == 0);
                    assert(ret[0] == (num * 2) / denom);

                }
            }
        }
    }
    printf("OK\n");
}

