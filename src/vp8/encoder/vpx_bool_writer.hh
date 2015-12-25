#include "boolwriter.hh"

class VPXBoolWriter
{
private:
    vpx_writer boolwriter;
    std::vector<uint8_t> output_;
#ifdef DEBUG_ARICODER
    bool any_written;
#endif
public:
    VPXBoolWriter() : output_(4096 * 1024 + 1024) {
        vpx_start_encode(&boolwriter, output_.data());
#ifdef DEBUG_ARICODER
        any_written = false;
#endif
    }
    void put( const bool value, Branch & branch) {
#ifdef DEBUG_ARICODER
        if (!any_written) {
               any_written = true;
               static int count=0;
               w_bitcount = count * 500000000;
	       ++count;
        }
#endif
        vpx_write(&boolwriter, value, branch.prob());
        if (__builtin_expect(boolwriter.pos & 0xffc00000, false)) {
            // check if we're out of buffer space
            if (boolwriter.pos + 128 > output_.size()) {
                output_.resize(output_.size() * 2);
                boolwriter.buffer = &output_[0]; //reset buffer
            }
        }
        branch.record_obs_and_update(value);
    }
    void finish(std::vector<uint8_t> &finish) {
        vpx_stop_encode(&boolwriter);
        output_.resize(boolwriter.pos);
        finish.swap(output_);
    }
};

