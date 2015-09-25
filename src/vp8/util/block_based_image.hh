#ifndef _BLOCK_BASED_IMAGE_HH_
#define _BLOCK_BASED_IMAGE_HH_
#include "aligned_block.hh"
#include "block_context.hh"

class BlockBasedImage {
    typedef AlignedBlock Block;
    Block *image_;
    uint32_t width_;
    uint32_t nblocks_;
    //the last 2 rows of the image
    mutable std::vector<uint8_t> num_nonzeros_;// 0 is cur 1 is prev
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
        num_nonzeros_.resize(width << 1);
        width_ = width;
        nblocks_ = nblocks;
        storage_ = (uint8_t*)calloc(nblocks * sizeof(Block) + 15, 1);
        size_t offset = storage_ - (uint8_t*)nullptr;
        if (offset & 15) { //needs alignment adjustment
            image_ = (Block*)(storage_ + 16 - offset);
        } else { // already aligned
            image_ = (Block*)storage_;
        }
    }
    BlockContext begin() {
        return {image_, nullptr, num_nonzeros_.begin(), num_nonzeros_.begin() + width_};
    }
    ConstBlockContext begin() const {
        return {image_, nullptr, num_nonzeros_.begin(), num_nonzeros_.begin() + width_};
    }
    template <class BlockContext> BlockContext next(BlockContext it, bool has_left) const {
        it.cur += 1;
        ptrdiff_t offset = it.cur - image_;
        if (offset >= width_) {
            it.above = it.cur - width_;
        } else {
            it.above = nullptr;
        }
        if (!has_left) {
            bool cur_row_first = (it.num_nonzeros_here < it.num_nonzeros_above);
            it.num_nonzeros_here = num_nonzeros_.begin();
            it.num_nonzeros_above = num_nonzeros_.begin();
            if (cur_row_first) {
                it.num_nonzeros_here += width_;
            } else {
                it.num_nonzeros_above += width_;
            }
        } else {
            ++it.num_nonzeros_here;
            ++it.num_nonzeros_above;
        }
        return it;
    }
    AlignedBlock& at(uint32_t y, uint32_t x) {
        return image_[width_ * y + x];
    }
    const AlignedBlock& at(uint32_t y, uint32_t x) const {
        return image_[width_ * y + x];
    }


    AlignedBlock& raster(uint32_t offset) {
        return image_[offset];
    }
    const AlignedBlock& raster(uint32_t offset) const {
        return image_[offset];
    }
};

inline
BlockColorContext get_color_context_blocks(
                                        const BlockColorContextIndices & indices,
                                           const Sirikata::Array1d<BlockBasedImage,
                                                                   (uint32_t)ColorChannel::NumBlockTypes> &jpeg,
                                           uint8_t component) {
    BlockColorContext retval = {(uint8_t)component};
    retval.color = component;
#ifdef USE_COLOR_VALUES
    for (size_t i = 0; i < sizeof(indices.luminanceIndex)/sizeof(indices.luminanceIndex[0]); ++i) {
        for (size_t j = 0; j < sizeof(indices.luminanceIndex[0])/sizeof(indices.luminanceIndex[0][0]); ++j) {
            if (indices.luminanceIndex[i][j].initialized()) {
                retval.luminance[i][j] = &jpeg[0].at(indices.luminanceIndex[i][j].get().second,indices.luminanceIndex[i][j].get().first);
            }
        }
    }
    if (indices.chromaIndex.initialized()) {
        retval.chroma = &jpeg[1].at(indices.chromaIndex.get().second,indices.chromaIndex.get().first);
    }
#endif
    return retval;
}
#endif
