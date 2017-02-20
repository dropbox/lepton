#include "thread_handoff.hh"
#include "../vp8/util/memory.hh"

std::vector<ThreadHandoff> ThreadHandoff::deserialize(const unsigned char *data, size_t max_size) {
    if (max_size < 2 || data[0] != 'H') {
        custom_exit(ExitCode::VERSION_UNSUPPORTED);
    }
    ++data; --max_size;
    int num_threads = data[0];
    ++data; --max_size;
    std::vector<ThreadHandoff> retval;
    if (int( max_size ) < BYTES_PER_HANDOFF * num_threads) {
        custom_exit(ExitCode::VERSION_UNSUPPORTED);
    }
    for (int i = 0; i< num_threads; ++i) {
        ThreadHandoff th = ThreadHandoff::zero();
        th.luma_y_start = data[0] + data[1] * 0x100;
        th.segment_size = data[2] + data[3] * 0x100U + data[4] * 0x10000UL + data[5] * 0x1000000UL;
        th.overhang_byte = data[6];
        th.num_overhang_bits = data[7];
        int biggest_value = 7;
        for (size_t i = 0; i < 4; ++i) {
            int32_t dc = data[8 + 2 * i] + data[biggest_value = 9 + 2 * i] * 0x100;
            if (dc >= 32768) {
                dc -= 65536;
            }
            if (i < sizeof(th.last_dc)/sizeof(th.last_dc[0])) { // we store 4 values even if this file isn't 4 channels
                th.last_dc[i] = dc;
            }
        }
        dev_assert(BYTES_PER_HANDOFF == biggest_value + 1);
        retval.push_back(th);
        data += BYTES_PER_HANDOFF;
    }
    for (size_t i = 1; i < retval.size(); ++i) {
        retval[i - 1].luma_y_end = retval[i].luma_y_start;
    }
    return retval;
}
size_t ThreadHandoff::get_remaining_data_size_from_two_bytes(unsigned char input[2]) {
    if (input[0] != 'H') {
        custom_exit(ExitCode::VERSION_UNSUPPORTED);
    }
    return input[1] * ThreadHandoff::BYTES_PER_HANDOFF;
}
std::vector<unsigned char> ThreadHandoff::serialize(const ThreadHandoff * data,
                                                    unsigned int num_threads) {
    always_assert(num_threads == NUM_THREADS);
    std::vector<unsigned char> retval;
    retval.reserve(NUM_THREADS * BYTES_PER_HANDOFF + 2);
    retval.push_back('H');
    retval.push_back(num_threads);
    for (unsigned int i = 0; i < num_threads; ++i) {
        ThreadHandoff th = data[i];
        retval.push_back(th.luma_y_start & 255);
        retval.push_back((th.luma_y_start >> 8) & 255);
        retval.push_back(th.segment_size & 255);
        retval.push_back((th.segment_size >> 8) & 255);
        retval.push_back((th.segment_size >> 16) & 255);
        retval.push_back((th.segment_size >> 24) & 255);
        retval.push_back(th.overhang_byte);
        retval.push_back(th.num_overhang_bits);
        unsigned int dc_values = 0;
        for (unsigned int i = 0; i < sizeof(th.last_dc)/sizeof(th.last_dc[0]); ++i) {
            uint16_t dc = th.last_dc[i]; // this will cast to unsigned
            retval.push_back(dc & 255);
            retval.push_back((dc >> 8) & 255);
            ++dc_values;
        }
        for(; dc_values < 4; ++dc_values) {
            retval.push_back(0);
            retval.push_back(0);
        }
    }
    return retval;
}
std::vector<ThreadHandoff> ThreadHandoff::make_rand(int num) {
    std::vector<ThreadHandoff> retval(num);
    for (int i = 0; i < num; ++i) {
        retval[i].luma_y_start = rand() & 65535;
        retval[i].segment_size = rand();
        retval[i].overhang_byte = rand() & 255;
        retval[i].num_overhang_bits = rand() & 7;
        for (uint32_t j = 0; j < (uint32_t)ColorChannel::NumBlockTypes; ++j) {
            retval[i].last_dc[j] = rand() & 32767;
            if (rand() < RAND_MAX/2) {
                retval[i].last_dc[j] = -retval[i].last_dc[j];
            }
        }
    }
    retval[num - 1].luma_y_end = 0;
    for (size_t i = 1; i < retval.size(); ++i) {
        retval[i - 1].luma_y_end = retval[i].luma_y_start;
    }
    return retval;
}
/* combine two ThreadHandoff objects into a range, starting with the initialization
   of the thread represented by the first object, and continuing until the end
   of the second object */
ThreadHandoff ThreadHandoff::operator-( const ThreadHandoff & other ) const
{
  ThreadHandoff ret = other;
  ret.luma_y_end = luma_y_start;
  ret.segment_size = segment_size - other.segment_size;
  return ret;
}
