/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "simple_decoder.hh"

#include <algorithm>
SimpleComponentDecoder::SimpleComponentDecoder() {
    str_in = NULL;
    batch_size = 0;
    for (unsigned int i = 0; i < sizeof(cur_read_batch) / sizeof(cur_read_batch[0]); ++i) {
        cur_read_batch[i] = 0;
        target[i] = 0;
        started_scan[i] = false;
    }
}
void SimpleComponentDecoder::initialize(Sirikata::DecoderReader *i,
                                        const std::vector<ThreadHandoff>& thread_transition_info) {
    this->str_in = i;
    this->thread_handoffs_ = thread_transition_info;
}

void SimpleComponentDecoder::decode_row(int thread_state_id,
                                        BlockBasedImagePerChannel<true>& image_data, // FIXME: set image_data to true
                                        Sirikata::Array1d<uint32_t,
                                                          (uint32_t)ColorChannel::
                                                          NumBlockTypes> component_size_in_blocks,
                                        int component,
                                        int curr_y) {
    custom_exit(ExitCode::ASSERTION_FAILURE);
}
BlockType bt_get_cmp(int cur_read_batch[3], int target[3]) {
    BlockType cmp = BlockType::Y;
    double cmp_progress = cur_read_batch[(int)cmp]/(double)target[(int)cmp];
    for (unsigned int icmp = 1; icmp < 3; ++icmp) {
        if (target[(int)cmp] && cur_read_batch[icmp] != target[icmp]) {
            double cprogress = cur_read_batch[icmp]/(double)target[icmp];
            if (cprogress < cmp_progress) {
                cmp = (BlockType)icmp;
                cmp_progress = cprogress;
            }
        }
    }
    return cmp;
}

CodingReturnValue SimpleComponentDecoder::decode_chunk(UncompressedComponents* colldata) {
    colldata->worker_update_coefficient_position_progress(64); // we are optimizing for baseline only atm
    colldata->worker_update_bit_progress(16); // we are optimizing for baseline only atm
	// read actual decompressed coefficient data from file
    char zero[sizeof(target)] = {0};
    if (memcmp(target, zero, sizeof(target)) == 0) {
        unsigned char bs[4] = {0};
        IOUtil::ReadFull(str_in, bs, sizeof(bs));
        batch_size = bs[3];
        batch_size <<= 8;
        batch_size |= bs[2];
        batch_size <<= 8;
        batch_size |= bs[1];
        batch_size <<= 8;
        batch_size |= bs[0];
        for (unsigned int cmp = 0; cmp < 3; ++cmp) {
            target[cmp] = colldata->component_size_in_blocks(cmp);
        }
    }
    BlockType cmp = bt_get_cmp(cur_read_batch, target);
    if ((size_t)cmp == sizeof(cur_read_batch)/sizeof(cur_read_batch[0]) || cur_read_batch[(size_t)cmp] == target[(size_t)cmp]) {
        return CODING_DONE;
    }
    // read coefficient data from file
    BlockBasedImage &start = colldata->full_component_write( cmp );
    while (cur_read_batch[(int)cmp] < target[(int)cmp]) {
        int cur_read_size = std::min((int)batch_size, target[(int)cmp] - cur_read_batch[(int)cmp]);
        for (int i = 0;i < cur_read_size; ++i) {
            size_t retval = IOUtil::ReadFull(str_in, &start.raster(cur_read_batch[(int)cmp] + i), sizeof(short) * 64);
            if (retval != sizeof( short) * 64) {
                errormessage = "Unexpected end of file blocks";
                errorlevel = 2;
                return CODING_ERROR;
            }
        }
        cur_read_batch[(int)cmp] += cur_read_size;
        colldata->worker_update_cmp_progress(cmp, cur_read_size);
        
        return CODING_PARTIAL;
    }
    dev_assert(false && "UNREACHABLE");
    return CODING_PARTIAL;
}
SimpleComponentDecoder::~SimpleComponentDecoder() {

}
