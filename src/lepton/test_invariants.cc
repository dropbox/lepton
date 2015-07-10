#include "../vp8/model/numeric.hh"
#include "../vp8/model/model.hh"
#include "../vp8/encoder/bool_encoder.hh"
#include "../vp8/encoder/encoder.hh"
#include "../vp8/model/numeric.hh"
int main() {
    int16_t min = std::numeric_limits<int16_t>::min();
    int16_t max = std::numeric_limits<int16_t>::max();
    int16_t min_for_token_index[(size_t)TokenNodeNot::BaseOffset];
    int16_t max_for_token_index[(size_t)TokenNodeNot::BaseOffset] = {0};
    for (size_t i = 0; i < (size_t)TokenNodeNot::BaseOffset; ++i) {
        min_for_token_index[i] = max;
    }
    min = -2048;
    max = 2048;
    for (int16_t value = min; ; ++value) {
        BitsAndLivenessFromEncoding value_bits;
        put_one_signed_coefficient(value_bits, false, false, value);
        for (size_t i = 0; i < (size_t)TokenNodeNot::BaseOffset; ++i) {
            if (value_bits.liveness() & (1 << i)) {
                min_for_token_index[i]
                    = std::min(min_for_token_index[i], int16_t(abs(value)));
                max_for_token_index[i]
                    = std::max(max_for_token_index[i], int16_t(abs(value)));
            }
        }
        if (value == max) {
            break;
        }
    }
    bool failed = false;
    for (size_t i = 0; i < (size_t)TokenNodeNot::BaseOffset; ++i) {
        if (min_from_entropy_node_index(i) != min_for_token_index[i]) {
            fprintf(stderr, "Invalid min for entropy node index %d  f(%d) != %d\n", (int)i, 
                    min_from_entropy_node_index(i), (int)min_for_token_index[i]);
            failed = true;
        }
        if (max_from_entropy_node_index_inclusive(i) != max_for_token_index[i]) {
            fprintf(stderr, "Invalid max for entropy node index %d  f(%d) != %d\n", (int)i, 
                    max_from_entropy_node_index_inclusive(i), (int)max_for_token_index[i]);
            failed = true;
        }
    }   
    return failed ? 1 : 0;
}
