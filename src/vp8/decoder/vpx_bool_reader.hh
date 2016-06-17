#include "boolreader.hh"


class VPXBoolReader
{
private:
    vpx_reader bit_reader;
#ifdef DEBUG_ARICODER
    bool any_read;
#endif
public:
    void init (const uint8_t *buffer, size_t size) {
        vpx_reader_init(&bit_reader,
                        buffer,
                        size);
    }
    VPXBoolReader() {
#ifdef DEBUG_ARICODER
        any_read = false;
#endif

    }
    VPXBoolReader(const uint8_t *buffer, size_t size) {
#ifdef DEBUG_ARICODER
        any_read = false;
#endif
        init(buffer, size);
    }
#ifndef _WIN32
    __attribute__((always_inline))
#endif
    bool get(Branch &branch, Billing bill=Billing::RESERVED) {
#ifdef DEBUG_ARICODER
        if (!any_read) {
               any_read = true;
               static int count=0;
               r_bitcount = count * 500000000;
	       count++;
        }
#endif
        bool retval = vpx_read(&bit_reader, branch.prob(), bill);
        branch.record_obs_and_update(retval);
        return retval;
    }
};

