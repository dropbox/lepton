#include "boolreader.hh"


class VPXBoolReader
{
private:
    vpx_reader bit_reader;
public:
    void init (const uint8_t *buffer, size_t size) {
        vpx_reader_init(&bit_reader,
                        buffer,
                        size);
    }
    VPXBoolReader() {
    }
    VPXBoolReader(const uint8_t *buffer, size_t size) {
        init(buffer, size);
    }
    __attribute__((always_inline))
    bool get(Branch &branch) {
        bool retval = vpx_read(&bit_reader, branch.prob());
        branch.record_obs_and_update(retval);
        return retval;
    }
};

