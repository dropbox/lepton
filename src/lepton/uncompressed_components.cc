/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "../../vp8/util/memory.hh"
#include <thread>
#include "uncompressed_components.hh"
#include "component_info.hh"

int UncompressedComponents::max_number_of_blocks = 0;

int gcd(int a, int b) {
    while(b) {
        int tmp = a % b;
        a = b;
        b = tmp;
    }
    return a;
}
int lcm (int a, int b) {
    return a * b / gcd(a, b);
}
int UncompressedComponents::min_vertical_luma_multiple() const {
    return min_vertical_cmp_multiple(0);
}
int UncompressedComponents::min_vertical_cmp_multiple(int cmp) const {
    return min_vertical_extcmp_multiple(&header_[cmp]);
}
int UncompressedComponents::min_vertical_extcmp_multiple(const ExtendedComponentInfo *cmpinfo) const{
    int luma_height = cmpinfo->info_.bcv;
    /*
    int overall_gcd = luma_height;
    for (int i = 1; i< cmpc_; ++i) {
        int cur_height = header_[i].info_.bcv;
        overall_gcd = gcd(overall_gcd, cur_height);
        }*/
    return luma_height / mcuv_;//luma_height / overall_gcd;
}
