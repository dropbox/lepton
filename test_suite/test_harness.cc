/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "../src/vp8/util/memory.hh"
#include <vector>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
extern int test_file(int argc, char **argv, bool use_lepton, bool jailed, int inject_syscall_level,
                     const std::vector<const char *> &filenames,
                     bool expect_encoder_failure, bool expect_decoder_failure);
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
#ifdef EXPECT_FAILURE
    expect_failure = true;
#endif
#if defined(EXPECT_LINUX_FAILURE) && defined(__linux)
    expect_failure = true;
#endif
#if defined(EXPECT_LINUX_DECODE_FAILURE) && defined(__linux)
    expect_decode_failure = true;
#endif
#ifdef USE_LEPTON
    use_lepton = true;
#endif
    std::vector<const char *> filenames;
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
    return test_file(argc, argv, use_lepton, IS_JAILED, INJECT_SYSCALL, filenames,
                     expect_failure, expect_decode_failure);
}
