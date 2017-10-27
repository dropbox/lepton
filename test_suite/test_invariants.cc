#include <stdio.h>
#include <assert.h>
#include <cstdint>
#include <cstddef>
#include "../src/vp8/util/nd_array.hh"
#include "../src/vp8/model/numeric.hh"
#include "../src/vp8/model/branch.hh"
#include "../src/io/MuxReader.hh"
#include "../src/io/MemReadWriter.hh"
#include "../src/lepton/thread_handoff.hh"
#ifndef ENABLE_ANS_EXPERIMENTAL
#define ENABLE_ANS_EXPERIMENTAL
#endif
#include "../src/vp8/decoder/ans_bool_reader.hh"
#include "../src/vp8/encoder/ans_bool_writer.hh"
#undef ENABLE_ANS_EXPERIMENTAL
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
    MuxWriter writer(&rw, alloc, 2);
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
        always_assert(r.first > 0);
        always_assert(r.second == JpegError::nil());
        offset += r.first;
        if (!r.first) {
            return; // ERROR IN TEST;
        }
    }
    always_assert(memcmp(testx, &testData[0][0], sizeof(testx)) == 0);
    offset = 0;
    std::pair<uint32_t, JpegError> expectEof(0, JpegError::nil());
    while(offset<sizeof(testx)) {
        expectEof = reader.Read(0, testx + offset, sizeof(testx) - offset);
        offset += expectEof.first;
        if (expectEof.second != JpegError::nil()) {
            break;
        }
    }
    always_assert(offset == 258);
    always_assert(expectEof.second == JpegError::errEOF());
    
    uint8_t testy[maxtesty];
    offset = 0;
    while(offset<sizeof(testy)) {
        std::pair<uint32_t, JpegError> r = reader.Read(1, testy + offset, sizeof(testy) - offset);
        always_assert(r.first > 0);
        always_assert(r.second == JpegError::nil());
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
    always_assert(!res);
    
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
    MuxWriter writer(&rw, alloc, 2);
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
                    always_assert(err == JpegError::nil());
                }
                allDone = false;
            }
        }
    }while(!allDone);
    writer.Close();
    for(size_t j = 0; j < sizeof(testData) / sizeof(testData[0]); ++j) {
        always_assert(progress[j] == testData[j].size());
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
                        always_assert(roundTrip[j][progress[j] + r]
                               == testData[j][progress[j] + r]);
                    }
                    progress[j] += ret.first;
                    always_assert(ret.second == JpegError::nil());
                }
                allDone = false;
            }
        }
    }while(!allDone);
    for(size_t j = 0; j < sizeof(testData) / sizeof(testData[0]); ++j) {
        always_assert(roundTrip[j] ==
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

void handoff_compare(const std::vector<ThreadHandoff> &a,
                     const std::vector<ThreadHandoff> &b) {
    if (a.size() != b.size()) {
        exit(1);
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (memcmp(&a[i], &b[i], sizeof(ThreadHandoff)) != 0) {
            exit(1);
        }
    }
}
void test_thread_handoff() {
    NUM_THREADS=MAX_NUM_THREADS;
    std::vector<ThreadHandoff> x = ThreadHandoff::make_rand(NUM_THREADS);

    Sirikata::Array1d<ThreadHandoff, MAX_NUM_THREADS> random_handoffs;
    for (size_t i = 0; i < MAX_NUM_THREADS;++i) {
        random_handoffs[i] = x[i];
    }
    std::vector<unsigned char> data = ThreadHandoff::serialize(&random_handoffs[0],
                                                               MAX_NUM_THREADS);
    std::vector<ThreadHandoff> roundtrip = ThreadHandoff::deserialize(&data[0], data.size());
    handoff_compare(x, roundtrip);
    std::vector<ThreadHandoff> test8;
    {
        ThreadHandoff item0 = {17767, 22714,
                               846930886, 105,
                               3, {{-23633, 5194, 7977}}};
        ThreadHandoff item1 = {22714, 8987, 1350490027,
                               242, 3, {{10723, 124, 2132}}};

        ThreadHandoff item2 = {8987, 50377,
                               521595368, 231,
                               5, {{-3958, -31022, -16947}}};
        ThreadHandoff item3 = {
            50377, 24869, 1801979802,
            102, 2, {{20493,
                      14897, 12451}}};
        ThreadHandoff item4 = {24869, 53453,
                               1653377373, 5,
                               7, {{-10328, 1118, 15787}}};
        ThreadHandoff item5 = {
            53453, 62753, 184803526,
            155, 4, {{
                    -9300, -17422, -30836}}};
        ThreadHandoff item6 = {62753, 44540,
                               2084420925, 220,
                               7, {{6768, 18494, 16961}}};
        ThreadHandoff item7 = {
            44540, 0, 84353895,
            62, 1, {{18046, 14826, -21355}}};
        test8.push_back(item0);
        test8.push_back(item1);
        test8.push_back(item2);
        test8.push_back(item3);
        test8.push_back(item4);
        test8.push_back(item5);
        test8.push_back(item6);
        test8.push_back(item7);
    }
    const unsigned char data8[] = {
        0x48,0x08,0x67,0x45,0xc6,0x23,0x7b,0x32,0x69,0x03,0xaf,0xa3,0x4a,0x14,0x29,0x1f,0x00,0x00,
        0xba,0x58,0xab,0xd7,0x7e,0x50,0xf2,0x03,0xe3,0x29,0x7c,0x00,0x54,0x08,0x00,0x00,0x1b,0x23,
        0xe8,0xe9,0x16,0x1f,0xe7,0x05,0x8a,0xf0,0xd2,0x86,0xcd,0xbd,0x00,0x00,0xc9,0xc4,0x9a,0x07,
        0x68,0x6b,0x66,0x02,0x0d,0x50,0x31,0x3a,0xa3,0x30,0x00,0x00,0x25,0x61,0x5d,0x89,0x8c,0x62,
        0x05,0x07,0xa8,0xd7,0x5e,0x04,0xab,0x3d,0x00,0x00,0xcd,0xd0,0xc6,0xe0,0x03,0x0b,0x9b,0x04,
        0xac,0xdb,0xf2,0xbb,0x8c,0x87,0x00,0x00,0x21,0xf5,0x3d,0xbd,0x3d,0x7c,0xdc,0x07,0x70,0x1a,
        0x3e,0x48,0x41,0x42,0x00,0x00,0xfc,0xad,0x67,0x23,0x07,0x05,0x3e,0x01,0x7e,0x46,0xea,0x39,
        0x95,0xac,0x00,0x00
    };
    std::vector<ThreadHandoff> roundtrip8 = ThreadHandoff::deserialize(data8, sizeof(data8));
    handoff_compare(test8, roundtrip8);
}

void test_ans_coding() {
    ANSBoolWriter writer;
    Branch p50_50 = Branch::identity();
    Branch p66_44 = Branch::identity();
    p66_44.record_obs_and_update(true);
    p66_44.record_obs_and_update(false);
    p66_44.record_obs_and_update(false);
    Branch p90_10 = Branch::identity();
    p90_10.record_obs_and_update(false);
    p90_10.record_obs_and_update(false);
    p90_10.record_obs_and_update(false);
    p90_10.record_obs_and_update(false);
    p90_10.record_obs_and_update(false);
    p90_10.record_obs_and_update(false);
    p90_10.record_obs_and_update(false);
    p90_10.record_obs_and_update(false);
    Branch p10_90 = Branch::identity();
    p10_90.record_obs_and_update(true);
    p10_90.record_obs_and_update(true);
    p10_90.record_obs_and_update(true);
    p10_90.record_obs_and_update(true);
    p10_90.record_obs_and_update(true);
    p10_90.record_obs_and_update(true);
    p10_90.record_obs_and_update(true);
    p10_90.record_obs_and_update(true);
    Branch p44_66 = Branch::identity();
    p66_44.record_obs_and_update(true);
    p66_44.record_obs_and_update(true);
    p66_44.record_obs_and_update(false);
    Branch p20_80 = Branch::identity();
    p20_80.record_obs_and_update(true);
    p20_80.record_obs_and_update(true);
    p20_80.record_obs_and_update(true);
    const Branch probs[16] = {
        p50_50,
        p66_44,
        p90_10,
        p10_90,
        p44_66,
        p20_80,
        p66_44,
        p66_44,
        p66_44,
        p66_44,
        p66_44,
        p66_44,
        p10_90,
        p10_90,
        p10_90,
        p10_90
    };
    Branch enc_probs[16];
    Branch dec_probs[16];
    memcpy(enc_probs, probs, sizeof(probs));
    memcpy(dec_probs, probs, sizeof(probs));
    bool vals[16] = {
        true,
        false,
        false,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
        true,
        true,
        true,
    };
    std::vector<Branch> prob_stream;
    for (int j =0;j < 64; ++j) {
        for (int i =0;i < 16; ++i) {
            prob_stream.push_back(enc_probs[i]);
            writer.put(vals[i], enc_probs[i], Billing::HEADER);
        }
    }
    Sirikata::MuxReader::ResizableByteBuffer final_buffer;
    writer.finish(final_buffer);
    ANSBoolReader reader(final_buffer.data(), final_buffer.size());
    size_t prob_index = 0;
    for (int j = 0; j < 64; ++j) {
        for (int i =0;i < 16; ++i) {
            int prob_similarity_res = memcmp(&dec_probs[i], &prob_stream[prob_index++], sizeof(Branch));
            if (prob_similarity_res) {
                fprintf(stderr, "pass %d %d) [%d %d %d] != [%d %d %d]\n",
                        j, i,
                        dec_probs[i].false_count(),dec_probs[i].true_count(),(int)dec_probs[i].prob(),
                        prob_stream[prob_index - 1].false_count(),prob_stream[prob_index-1].true_count(),(int)prob_stream[prob_index -1].prob());
            }
            always_assert(prob_similarity_res == 0);
            bool val = reader.get(dec_probs[i], Billing::HEADER);
            if (val != vals[i]) {
                for (int inner =0;inner <= i; ++inner) {
                    fprintf(stderr, "pass:%d %d) [prob=%d] val=%d (should be %d)\n",
                            j, inner, probs[i].prob(), inner == i ? val:vals[i], vals[i]);
                }
            }
            always_assert(val == vals[i]);
        }
    }
    
}
int main() {
    Sirikata::memmgr_init(32 * 1024 * 1024,
                          16 * 1024 * 1024,
                          3,
                          256);
    test_ans_coding();
    test_thread_handoff();
    for (size_t i = 0; i < karray.size(); ++i) {
        always_assert(karray[i] == i);
    }
    using namespace Sirikata;
    AlignedArray7d<unsigned char, 1,3,2,5,4,6,16> aligned7d;
    uint8_t* d =&aligned7d.at(0, 2, 1, 3, 2, 1, 0);
    *d = 4;
    size_t offset = d - (uint8_t*)nullptr;
    always_assert(0 == (offset & 15) && "Must have alignment");
    always_assert(aligned7d.at(0, 2, 1, 3, 2, 1, 0) == 4);
    Array7d<unsigned char, 1,3,2,5,3,3,16> a7;
    uint8_t* d2 =&a7.at(0, 2, 1, 3, 2, 1, 0);
    *d2 = 5;
    offset = d2 - (uint8_t*)nullptr;
    if (offset & 15) {
        fprintf(stderr, "Array7d array doesn't require alignment");
    }
    always_assert(a7.at(0, 2, 1, 3, 2, 1, 0) == 5);
    a7.at(0, 2, 1, 3, 2, 1, 1) = 8;
    always_assert(a7.at(0, 2, 1, 3, 2, 1, 1) == 8);
    Array1d<unsigned char, 16>::Slice s = a7.at(0, 2, 1, 3, 2, 1);
    s.at(1) = 16;
    always_assert(a7.at(0, 2, 1, 3, 2, 1, 1) == 16);
    s.at(0) = 6;
    always_assert(a7.at(0, 2, 1, 3, 2, 1, 0) == 6);
    {
        a7.at(0,0,0,0,0,0,0) = 16;
        always_assert(a7.at(0,0,0,0,0,0,0) == 16);
        auto x = a7.at(0);
        auto y = x.at(0);
        auto z = y.at(0);
        auto w = z.at(0);
        auto a = w.at(0);
        auto b = a.at(0);
        b.at(0) = 47;
        always_assert(a7.at(0,0,0,0,0,0,0) == 47);
    }
    podtest(4, a7);
    testEof();
    testRoundtrip();
    for (int i = 0; i < 65536; ++i) {
        always_assert(bit_length((uint16_t)i) == uint16bit_length(i));
        if (i > 0) {
            always_assert(local_log2((uint16_t)i) == uint16log2(i));
        }
    }
    for (int denom = 1; denom < 1026; ++denom) {
        for (int num = 256; num < 262144; num += 256) {
            always_assert(slow_divide18bit_by_10bit(num, denom) == (unsigned int)num/denom);
            always_assert(fast_divide18bit_by_10bit(num, denom) == (unsigned int)num / denom);
            if (num < 16384) {
                always_assert(fast_divide16bit(num, denom) == (unsigned int)num / denom);
                if (denom == 5) {
                    always_assert(templ_divide16bit<5>(num) == (unsigned int)num / denom);
#ifndef USE_SCALAR
                    __m128i retval = divide16bit_vec_signed<5>(_mm_set_epi32(num,(int)-num, -num-1, -num + 1));
                    int ret[4];
                    _mm_storeu_si128((__m128i*)(char *)ret, retval);
                    always_assert(ret[3] == num / denom);
                    always_assert(ret[2] == -num / denom);
                    always_assert(ret[1] == (-num-1)/denom);
                    always_assert(ret[0] == (-num + 1) / denom);
#endif
                }
#ifndef USE_SCALAR
                if (denom == 767) {
                    __m128i retval = divide16bit_vec<767>(_mm_set_epi32(num,num + 1, 0, num * 2));
                    int ret[4];
                    _mm_storeu_si128((__m128i*)(char *)ret, retval);
                    always_assert(ret[3] == num / denom);
                    always_assert(ret[2] == (1 + num) / denom);
                    always_assert(ret[1] == 0);
                    always_assert(ret[0] == (num * 2) / denom);
                }
#endif
            }
        }
    }
    printf("OK\n");
}

