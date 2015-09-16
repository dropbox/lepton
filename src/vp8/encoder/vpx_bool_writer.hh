#include "boolwriter.hh"

class VPXBoolWriter
{
private:
    vpx_writer boolwriter;
    std::vector<uint8_t> output_;
public:
    VPXBoolWriter() : output_(4096 * 1024 + 1024) {
        vpx_start_encode(&boolwriter, output_.data());
    }
    void put( const bool value, Branch & branch) {
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
    std::vector<uint8_t> &finish() {
        vpx_stop_encode(&boolwriter);
        output_.resize(boolwriter.pos);
        return output_;
    }
};

