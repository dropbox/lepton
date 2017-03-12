#include "../util/options.hh"
#include "boolwriter.hh"
#include "../../io/MuxReader.hh"
class VPXBoolWriter
{
private:
    vpx_writer boolwriter;
    Sirikata::MuxReader::ResizableByteBuffer output_;
#ifdef DEBUG_ARICODER
    bool any_written;
#endif
    enum {
         MIN_SIZE = 1024 * 1024
    };
    enum {
        SIZE_CHECK  = 0xfff00000
    };
public:
  VPXBoolWriter() {
#ifdef DEBUG_ARICODER
        any_written = false;
#endif
        static_assert(MIN_SIZE & SIZE_CHECK,
                      "min size must be caught by the size check, so allocations happen after");
        static_assert(((MIN_SIZE - 1) & SIZE_CHECK) == 0,
                      "min size -1 must not be caught by the size check");
        boolwriter.lowvalue = 0;
        boolwriter.range = 0;
        boolwriter.count = 0;
        boolwriter.pos = 0;
        boolwriter.buffer = NULL;
    }
    void init () {
#ifdef DEBUG_ARICODER
        always_assert(!any_written);
#endif
	output_.resize((std::max((unsigned int)MIN_SIZE,
                                 std::min((unsigned int)4096 * 1024,
                                          (unsigned int)(5120 * 1024 / NUM_THREADS))))
		       + 1024);
        vpx_start_encode(&boolwriter, output_.data());
    }
    void put( const bool value, Branch & branch, Billing bill) {
#ifdef DEBUG_ARICODER
        if (!any_written) {
               any_written = true;
               static int count=0;
               w_bitcount = count * 500000000;
	       ++count;
        }
#endif
        vpx_write(&boolwriter, value, branch.prob(), bill);
        if (__builtin_expect(boolwriter.pos & SIZE_CHECK, false)) {
            // check if we're out of buffer space
            if (boolwriter.pos + 128 > output_.size()) {
                output_.resize(output_.size() * 2);
                boolwriter.buffer = &output_[0]; //reset buffer
            }
        }
        branch.record_obs_and_update(value);
    }
    void finish(Sirikata::MuxReader::ResizableByteBuffer &finish) {
        vpx_stop_encode(&boolwriter);
        output_.resize(boolwriter.pos);
        finish.swap(output_);
    }
};

