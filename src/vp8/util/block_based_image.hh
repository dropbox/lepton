#ifndef _BLOCK_BASED_IMAGE_HH_
#define _BLOCK_BASED_IMAGE_HH_
#include "memory.hh"
#include "aligned_block.hh"
#include "block_context.hh"
#include <map>
extern bool g_allow_progressive;
class BlockBasedImage {
    typedef AlignedBlock Block;
    Block *image_;
    uint32_t width_;
    uint32_t nblocks_;
    uint8_t *storage_;
    // if true, this image only contains 2 rows during decode
    bool memory_optimized_image_;
    BlockBasedImage(const BlockBasedImage&) = delete;
    BlockBasedImage& operator=(const BlockBasedImage&) = delete;
public:
    BlockBasedImage() : memory_optimized_image_(false){
        image_ = nullptr;
        storage_ = nullptr;
        width_ = 0;
        nblocks_ = 0;
    }

    void init (uint32_t width, uint32_t height, uint32_t nblocks, bool memory_optimized_image) {
        memory_optimized_image_ = memory_optimized_image;
        assert(nblocks <= width * height);
        width_ = width;
        if (memory_optimized_image_) {
            nblocks = width * 4;
        }
        nblocks_ = nblocks;
        storage_ = (uint8_t*)custom_calloc(nblocks * sizeof(Block) + 31);
        size_t offset = storage_ - (uint8_t*)nullptr;
        if (offset & 31) { //needs alignment adjustment
            image_ = (Block*)(storage_ + 32 - (offset & 31));
        } else { // already aligned
            image_ = (Block*)storage_;
        }
    }
    BlockContext begin(std::vector<NeighborSummary>::iterator num_nonzeros_begin) {
        return {image_, nullptr, num_nonzeros_begin, num_nonzeros_begin + width_};
    }
    ConstBlockContext begin(std::vector<NeighborSummary>::iterator num_nonzeros_begin) const {
        return {image_, nullptr, num_nonzeros_begin, num_nonzeros_begin + width_};
    }
    BlockContext off_y(int y,
                       std::vector<NeighborSummary>::iterator num_nonzeros_begin) {
        if (memory_optimized_image_) {
            return {image_ + width_ * (y & 3),
                    image_ + ((y + 3) & 3) * width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
        }
        return {image_ + width_ * y,
                (y != 0) ? image_ + width_ * (y - 1) : nullptr,
                (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
    }
    ConstBlockContext off_y(int y,
                            std::vector<NeighborSummary>::iterator num_nonzeros_begin) const {
        if (memory_optimized_image_) {
            return {image_ + width_ * (y & 3),
                    image_ + ((y + 3) & 3) * width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
        }
        return {image_ + width_ * y,
                (y != 0) ? image_ + width_ * (y - 1) : nullptr,
                (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
    }
    template <class BlockContext> uint32_t next(BlockContext& it, bool has_left) const {
        it.context.cur += 1;
        ptrdiff_t offset = it.context.cur - image_;
        uint32_t retval = offset;
        if (memory_optimized_image_) {
            if (__builtin_expect(offset == (width_ << 2), 0)) {
                retval = offset = 0;
                it.context.cur = image_;
            }
            if (retval >= (width_ << 1)) {
                retval -= (width_ << 1);
            }
            if (retval >= width_) {
                retval -= width_;
            }
            retval += width_ * it.y;
        }
        if (__builtin_expect(offset < width_, 0)) {
            it.context.above = it.context.cur + 3 * width_;
	} else {
            it.context.above = it.context.cur - width_;
        }
        ++it.context.num_nonzeros_here;
        ++it.context.num_nonzeros_above;
        if (!has_left) {
            bool cur_row_first = (it.context.num_nonzeros_here < it.context.num_nonzeros_above);
            if (cur_row_first) {
                it.context.num_nonzeros_above -= width_;
                it.context.num_nonzeros_above -= width_;
            } else {
                it.context.num_nonzeros_here -= width_;
                it.context.num_nonzeros_here -= width_;
            }
        }
        return retval;
    }
    AlignedBlock& at(uint32_t y, uint32_t x) {
        uint32_t index = x + (y & 3) * width_;
        if (!memory_optimized_image_) {
            index = y * width_ + x;
        }
        if (__builtin_expect(index >= nblocks_, 0)) {
            custom_exit(ExitCode::OOM);
        }
        return image_[index];
    }
    const AlignedBlock& at(uint32_t y, uint32_t x) const {
        uint32_t index = x + (y & 3) * width_;
        if (!memory_optimized_image_) {
            index = y * width_ + x;
        }
        if (__builtin_expect(index >= nblocks_, 0)) {
            custom_exit(ExitCode::OOM);
        }
        return image_[index];
    }


    AlignedBlock& raster(uint32_t offset) {
        if (memory_optimized_image_) {
            offset = offset % (width_ << 2);
        }
        if (offset >= nblocks_) {
            custom_exit(ExitCode::OOM);
        }
        return image_[offset];
    }
    const AlignedBlock& raster(uint32_t offset) const {
        if (memory_optimized_image_) {
            offset = offset % (width_ << 2);
        }
        if (__builtin_expect(offset >= nblocks_, 0)) {
            custom_exit(ExitCode::OOM);
        }
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
    (void)indices;
    (void)jpeg;
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
