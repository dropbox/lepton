/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "../src/vp8/util/memory.hh"
#include <vector>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
extern int test_file(int argc, char **argv, bool use_lepton, bool jailed, int inject_syscall_level,
                     int allow_progressive_files, bool multithread,
                     const std::vector<const char *> &filenames,
                     bool expect_encoder_failure, bool expect_decoder_failure,
                     const char* encode_memory, const char * decode_memory, const char * singlethread_memory,
                     const char* thread_memory, bool use_brotli,
                     bool force_no_ans);
#ifdef UNJAILED
#define IS_JAILED false
#else
#define IS_JAILED true
#endif
#ifndef INJECT_SYSCALL
#define INJECT_SYSCALL 0
#endif
int main (int argc, char **argv) {
    bool use_lepton = false;
    bool expect_failure = false;
    bool expect_decode_failure = false;
    int allow_progressive_files = 0;
    bool multithread = true;
#ifdef SINGLETHREAD
    multithread = false;
#endif
#ifdef ALLOW_PROGRESSIVE
    allow_progressive_files = 1;
#endif
#ifdef DISALLOW_PROGRESSIVE
    allow_progressive_files = -1;
#endif
#ifdef EXPECT_FAILURE
    expect_failure = true;
#endif
#if defined(EXPECT_LINUX_FAILURE) && defined(__linux)
    expect_failure = true;
#endif
#if defined(EXPECT_LINUX_DECODE_FAILURE) && defined(__linux)
    expect_decode_failure = true;
#endif
    bool enable_jailing = IS_JAILED;
#ifdef USE_LEPTON
    use_lepton = true;
#endif
    bool use_brotli=true;
#ifdef ZLIB_HEADER
    use_brotli=false;
#endif
    bool force_no_ans = false;
#ifdef FORCE_NO_ANS
    force_no_ans = true;
#endif
    for (int i = 1; i < argc; ++i) {
        if (strstr(argv[i], "-unjailed")) {
            enable_jailing = false;
            for (int j = i; j + 1 < argc; ++j) {
                argv[j] = argv[j + 1];
            }
            --argc;
            break;
        }
    }
    std::vector<const char *> filenames;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            filenames.push_back(argv[i]);
        }
    } else {
#ifdef TEST_FILE
    // we can only test one failure at a time
        filenames.push_back("images/" STRINGIFY(TEST_FILE) ".jpg");
#endif
#ifdef TEST_FILE0
        if (expect_failure != false) {
            return 1;
        }
        filenames.push_back("images/" STRINGIFY(TEST_FILE0) ".jpg");
#endif
#ifdef TEST_FILE1
        if (expect_failure != false) {
            return 1;
        }
        filenames.push_back("images/" STRINGIFY(TEST_FILE1) ".jpg");
#endif
#ifdef TEST_FILE2
        if (expect_failure != false) {
            return 1;
        }
        filenames.push_back("images/" STRINGIFY(TEST_FILE2) ".jpg");
#endif
#ifdef TEST_FILE3
    #error "We only support 4 test files in the same test atm"
#endif
    }
    const char * encode_memory = NULL;
    const char * decode_memory = NULL;
    const char * thread_memory = NULL;
#ifdef MORE_MEMORY
    encode_memory = "-memory=" MORE_MEMORY;
    decode_memory = "-memory=" MORE_MEMORY;
#endif
#ifdef DECODE_MEMORY
    decode_memory = "-memory=" DECODE_MEMORY;
#endif
#ifdef THREAD_MEMORY
    thread_memory = "-threadmemory=" THREAD_MEMORY;
#endif
    const char * singlethread_memory = NULL;
#ifdef SINGLETHREAD_MEMORY
    singlethread_memory = "-recodememory=" SINGLETHREAD_MEMORY;
#endif
    return test_file(argc, argv,
                     use_lepton, enable_jailing, INJECT_SYSCALL, allow_progressive_files,
                     multithread, filenames,
                     expect_failure,
                     expect_decode_failure, encode_memory, decode_memory, singlethread_memory, thread_memory,
                     use_brotli,
                     force_no_ans);
}
