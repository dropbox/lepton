#ifndef _BLOCK_BASED_IMAGE_HH_
#define _BLOCK_BASED_IMAGE_HH_
#include "aligned_block.hh"
#include "block_context.hh"

class BlockBasedImage {
    typedef AlignedBlock Block;
    Block *image_;
    uint32_t width_;
    uint32_t nblocks_;
    uint8_t *storage_;
    BlockBasedImage(const BlockBasedImage&) = delete;
    BlockBasedImage& operator=(const BlockBasedImage&) = delete;
public:
    BlockBasedImage() {
        image_ = nullptr;
        storage_ = nullptr;
        width_ = 0;
        nblocks_ = 0;
    }

    void init (uint32_t width, uint32_t height, uint32_t nblocks) {
        assert(nblocks <= width * height);
        width_ = width;
        nblocks_ = nblocks;
        storage_ = (uint8_t*)malloc(nblocks * sizeof(Block) + 15);
        size_t offset = storage_ - (uint8_t*)nullptr;
        if (offset & 15) { //needs alignment adjustment
            image_ = (Block*)(storage_ + 16 - offset);
        } else { // already aligned
            image_ = (Block*)storage_;
        }
    }
    BlockContext begin() {
        return {image_, 0, 0};
    }
    BlockContext next(BlockContext it) {
        it.cur += 1;
        ptrdiff_t offset = it.cur - image_;
        if (offset >= width_) {
            it.up_offset = -width_;
        }
        assert(((offset >= width_ && it.up_offset) ||
               (offset < width_ && it.up_offset == 0)) && "up neighbor must be present after width");
        if (it.is_left_avail == 0) {
            it.is_left_avail = width_;
        }
        --it.is_left_avail;
        return it;
    }
    AlignedBlock& at(uint32_t y, uint32_t x) {
        return image_[width_ * y + x];
    }
    const AlignedBlock& at(uint32_t y, uint32_t x) const {
        return image_[width_ * y + x];
    }
};

inline
BlockColorContext get_color_context_blocks(const BlockColorContextIndices & indices,
                                           const Sirikata::Array1d<BlockBasedImage,
                                                                   (uint32_t)ColorChannel::NumBlockTypes> &jpeg,
                                           int component) {
    BlockColorContext retval = {};
    retval.color = component;
    for (int i = 0; i < sizeof(indices.luminanceIndex)/sizeof(indices.luminanceIndex[0]); ++i) {
        for (int j = 0; j < sizeof(indices.luminanceIndex[0])/sizeof(indices.luminanceIndex[0][0]); ++j) {
            if (indices.luminanceIndex[i][j].initialized()) {
                retval.luminance[i][j] = &jpeg[0].at(indices.luminanceIndex[i][j].get().second,indices.luminanceIndex[i][j].get().first);
            }
        }
    }
    if (indices.chromaIndex.initialized()) {
        retval.chroma = &jpeg[1].at(indices.chromaIndex.get().second,indices.chromaIndex.get().first);
    }
    return retval;
}
#endif
