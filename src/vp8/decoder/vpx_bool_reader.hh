#include "boolreader.hh"


class VPXBoolReader
{
private:
    vpx_reader bit_reader;
    Slice slice_;
public:
    VPXBoolReader(const Slice& slice) : slice_(slice) {
        vpx_reader_init(&bit_reader,
                        slice.buffer(),
                        slice.size());
    }
    __attribute__((always_inline))
    bool get(Branch &branch) {
        bool retval = vpx_read(&bit_reader, branch.prob());
        branch.record_obs_and_update(retval);
        return retval;
    }
};

