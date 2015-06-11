/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <thread>
#include "uncompressed_components.hh"
#include "component_info.hh"

void UncompressedComponents::init(componentInfo cmpinfo[ 4 ], int cmpc) {
        allocated_ = 0;
        for (int cmp = 0; cmp < cmpc; cmp++) {
            bch_[cmp] = cmpinfo[cmp].bch;
            bcv_[cmp] = cmpinfo[cmp].bcv;
            allocated_ += cmpinfo[cmp].bc * 64;
        }
        colldata_ = new signed short[allocated_];
        int total = 0;
        for (int cmp = 0; cmp < 4; cmp++) {
            cmpoffset_[cmp] = colldata_ + total;
            if (cmp < cmpc) {
                total += cmpinfo[cmp].bc * 64;
            }
        }
}

void UncompressedComponents::start_decoder_worker_thread(const std::function<void()> &f) {
    std::thread thread(f);
//    thread.detach();
    thread.join();
}
