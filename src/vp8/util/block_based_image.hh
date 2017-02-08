#ifndef _BLOCK_BASED_IMAGE_HH_
#define _BLOCK_BASED_IMAGE_HH_
#include "memory.hh"
#include "aligned_block.hh"
#include "block_context.hh"
#include <map>
extern bool g_allow_progressive;
template<bool force_memory_optimization=false>
class BlockBasedImageBase {
    typedef AlignedBlock Block;
    Block *image_;
    uint32_t width_;
    uint32_t nblocks_;
    uint8_t *storage_;
    uint32_t theoretical_component_height_;
    // if true, this image only contains 2 rows during decode
    bool memory_optimized_image_;
    BlockBasedImageBase(const BlockBasedImageBase&) = delete;
    BlockBasedImageBase& operator=(const BlockBasedImageBase&) = delete;
public:
    BlockBasedImageBase()
      : memory_optimized_image_(force_memory_optimization) {
        image_ = nullptr;
        storage_ = nullptr;
        width_ = 0;
        nblocks_ = 0;
        theoretical_component_height_ = 0;
    }
    bool is_memory_optimized() const {
        return force_memory_optimization
            || memory_optimized_image_;
    }
    uint32_t block_width() const {
        return width_;
    }
    size_t bytes_allocated() const {
        return 32 + nblocks_ * sizeof(Block);
    }
    size_t blocks_allocated() const {
        return nblocks_;
    }
    size_t original_height() const {
        return theoretical_component_height_;
    }
    void init (uint32_t width, uint32_t height, uint32_t nblocks, bool memory_optimized_image) {
        theoretical_component_height_ = height;
        if (force_memory_optimization) {
            always_assert(memory_optimized_image && "MemoryOptimized must match template");
        }
        memory_optimized_image_ = force_memory_optimization || memory_optimized_image;
        always_assert(nblocks <= width * height);
        width_ = width;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            nblocks = width * 4;
#else
            nblocks = width * 2;
#endif
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
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            return {image_ + width_ * (y & 3),
                    image_ + ((y + 3) & 3) * width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
#else
            return {(y & 1) ? image_ + width_ : image_,
                    (y & 1) ? image_ : image_ + width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
#endif
        }
        return {image_ + width_ * y,
                (y != 0) ? image_ + width_ * (y - 1) : nullptr,
                (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
    }
    ConstBlockContext off_y(int y,
                            std::vector<NeighborSummary>::iterator num_nonzeros_begin) const {
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            return {image_ + width_ * (y & 3),
                    image_ + ((y + 3) & 3) * width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
#else
            return {(y & 1) ? image_ + width_ : image_,
                    (y & 1) ? image_ : image_ + width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
#endif
        }
        return {image_ + width_ * y,
                (y != 0) ? image_ + width_ * (y - 1) : nullptr,
                (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
    }
    template <class BlockContext> uint32_t next(BlockContext& it, bool has_left, int component_y, int step) const {
        it.cur += step;
        ptrdiff_t offset = it.cur - image_;
        uint32_t retval = offset;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            if (__builtin_expect(offset == (width_ << 2), 0)) {
                retval = offset = 0;
                it.cur = image_;
            }
            if (retval >= (width_ << 1)) {
                retval -= (width_ << 1);
            }
            if (retval >= width_) {
                retval -= width_;
            }
            retval += width_ * component_y;
#else
            if (__builtin_expect(offset == (width_ << 1), 0)) {
                retval = offset = 0;
                it.cur = image_;
            }
            if (retval >= width_) {
                retval -= width_;
            }
            retval += width_ * component_y;
#endif
        }
        if (__builtin_expect(offset < width_, 0)) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            it.above = it.cur + 3 * width_;
#else
            it.above = it.cur + width_;
#endif
        } else {
            it.above = it.cur - width_;
        }
        ++it.num_nonzeros_here;
        ++it.num_nonzeros_above;
        if (!has_left) {
            bool cur_row_first = (it.num_nonzeros_here < it.num_nonzeros_above);
            if (cur_row_first) {
                it.num_nonzeros_above -= width_;
                it.num_nonzeros_above -= width_;
            } else {
                it.num_nonzeros_here -= width_;
                it.num_nonzeros_here -= width_;
            }
        }
        return retval;
    }
    AlignedBlock& at(uint32_t y, uint32_t x) {
        uint32_t index;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            index = x + (y & 3) * width_;
#else
            index = (y & 1) ? width_ + x : x;
#endif
            if (__builtin_expect(x >= width_, 0)) {
                custom_exit(ExitCode::OOM);
            }
        } else {
            index = y * width_ + x;
            if (__builtin_expect(index >= nblocks_, 0)) {
                custom_exit(ExitCode::OOM);
            }
        }
        return image_[index];
    }
    const AlignedBlock& at(uint32_t y, uint32_t x) const {
        uint32_t index;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            index = x + (y & 3) * width_;
#else
            index = (y & 1) ? width_ + x : x;
#endif
            if (__builtin_expect(x >= width_, 0)) {
                custom_exit(ExitCode::OOM);
            }
        } else {
            index = y * width_ + x;
            if (__builtin_expect(index >= nblocks_, 0)) {
                custom_exit(ExitCode::OOM);
            }
        }
        return image_[index];
    }


    AlignedBlock& raster(uint32_t offset) {
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            offset = offset % (width_ << 2);
#else
            offset = offset % (width_ << 1);
#endif
            assert(offset <= nblocks_ && "we mod offset by width_: it is < nblocks_");
        } else if (offset >= nblocks_) {
            custom_exit(ExitCode::OOM);
        }
        return image_[offset];
    }
    const AlignedBlock& raster(uint32_t offset) const {
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            offset = offset % (width_ << 2);
#else
            offset = offset % (width_ << 1);
#endif
            assert(offset <= nblocks_ && "we mod offset by width_: it is < nblocks_");
        } else if (__builtin_expect(offset >= nblocks_, 0)) {
            custom_exit(ExitCode::OOM);
        }
        return image_[offset];
    }
};
class BlockBasedImage : public BlockBasedImageBase<false> {
    BlockBasedImage(const BlockBasedImage&) = delete;
    BlockBasedImage& operator=(const BlockBasedImage&) = delete;
public:
    BlockBasedImage() {}
};
template<bool force_memory_optimization=false> class BlockBasedImagePerChannel :
    public Sirikata::Array1d<BlockBasedImageBase<force_memory_optimization> *,
                             (uint32_t)ColorChannel::NumBlockTypes> {
public:
    BlockBasedImagePerChannel() {
        this->memset(0);
    }
};

template<bool force_memory_optimization=false> class KBlockBasedImagePerChannel :
    public Sirikata::Array1d<const BlockBasedImageBase<force_memory_optimization> *,
                             (uint32_t)ColorChannel::NumBlockTypes> {
public:
    KBlockBasedImagePerChannel() {
        this->memset(0);
    }
};
template<class BlockBasedImage, class BlockContextType> class MultiChannelBlockContext {
public:
  Sirikata::Array1d<BlockContextType, (size_t)ColorChannel::NumBlockTypes > context_;
  Sirikata::Array2d<size_t, 2, (size_t)ColorChannel::NumBlockTypes > eostep;
  Sirikata::Array1d<BlockBasedImage*,
		    (uint32_t)ColorChannel::NumBlockTypes> image_data_;
  template<class ColorChan> MultiChannelBlockContext(
			  int curr_y,
			  ColorChan original_color,
  			  Sirikata::Array1d<BlockBasedImage*, (uint32_t)ColorChannel::NumBlockTypes> &in_image_data,
			  Sirikata::Array1d<std::vector<NeighborSummary>,
					     (size_t)ColorChannel::NumBlockTypes> &num_nonzeros_
			   ) {
    for (size_t col = 0 ; col < (size_t)ColorChannel::NumBlockTypes; ++col) {
      size_t fixed_index = col;
      if (!in_image_data.at(col)) { // if we don't have this channel, fall back to zero
	fixed_index = 0;
      }
      image_data_.at(col) = in_image_data.at(fixed_index);
      size_t vertical_count = image_data_.at(col)->original_height();
      size_t orig_vertical_count = in_image_data.at(original_color)->original_height();
      size_t vratio = vertical_count / orig_vertical_count;
      size_t voffset = vratio;
      if (vratio) {
	voffset -= 1; // one less than the edge of this block
      }
      uint32_t adjusted_curr_y = (curr_y * vertical_count + voffset)/ orig_vertical_count;
      context_.at(col)
	= image_data_[col]->off_y(adjusted_curr_y,// if we need to fallback to zero, we don't want to use the big index
				 num_nonzeros_.at((int)fixed_index).begin());
      size_t horizontal_count = image_data_.at(col)->block_width();
      size_t orig_horizontal_count = in_image_data.at((size_t)original_color)->block_width();
      size_t hratio = horizontal_count / orig_horizontal_count;
      if (hratio == 0) {
	eostep.at(0, col) = 0;
	if (2 * vertical_count == orig_vertical_count) {
	  eostep.at(1, col) = 1;
	} else {
	  eostep.at(1, col) = 0; // avoid prior if we have > 2x ratio of Y to Cb or Cr
	}
      } else {
	eostep.at(0, col) = hratio;
	eostep.at(1, col) = hratio;
	for(size_t off = 1; off < hratio; ++off) { // lets advance to the bottom right edge
	  image_data_[col]->next(context_.at(col), true, adjusted_curr_y, 1);
	}
      }
    }
  }
  template<class ColorChan> int next(int curr_y ,ColorChan original_color) {
    int retval = 0;
    for (int i = 0; i < (int)ColorChannel::NumBlockTypes; ++i) {
      int ret =image_data_[i]->next(context_.at(i), true, curr_y, eostep.at(0, (int)i));
      if (i == (int)original_color) {
	retval = ret;
      }
    }
    return retval;
  }
};
#endif
