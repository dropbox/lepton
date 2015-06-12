/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <vector>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

extern int test_file(int argc, char ** argv, bool use_lepton,
                     const std::vector<const char *> &filenames, bool expect_failure);

int main (int argc, char **argv) {
    bool use_lepton = false;
    bool expect_failure = false;
#ifdef EXPECT_FAILURE
    expect_failure = true;
#endif
#ifdef USE_LEPTON
    use_lepton = true;
#endif
    std::vector<const char *> filenames;
#ifdef TEST_FILE
    // we can only test one failure at a time
    filenames.push_back("../../images/" STRINGIFY(TEST_FILE) ".jpg");
#endif
#ifdef TEST_FILE0
    if (expect_failure != false) {
        return 1;
    }
    filenames.push_back("../../images/" STRINGIFY(TEST_FILE0) ".jpg");
#endif
#ifdef TEST_FILE1
    if (expect_failure != false) {
        return 1;
    }
    filenames.push_back("../../images/" STRINGIFY(TEST_FILE1) ".jpg");
#endif
#ifdef TEST_FILE2
    if (expect_failure != false) {
        return 1;
    }
    filenames.push_back("../../images/" STRINGIFY(TEST_FILE2) ".jpg");
#endif
#ifdef TEST_FILE3
    #error "We only support 4 test files in the same test atm"
#endif
    return test_file(argc, argv, use_lepton, filenames, expect_failure);
}
