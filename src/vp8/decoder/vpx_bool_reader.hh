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
    bool get(Branch &branch) {
        bool retval = vpx_read(&bit_reader, branch.prob());
        if (retval) {
            branch.record_true_and_update();
        } else {
            branch.record_false_and_update();
        }
        return retval;
    }
};

