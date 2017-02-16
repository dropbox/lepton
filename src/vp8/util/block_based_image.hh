#ifndef _BLOCK_BASED_IMAGE_HH_
#define _BLOCK_BASED_IMAGE_HH_
#include "memory.hh"
#include "aligned_block.hh"
#include "block_context.hh"
#include <atomic>
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
    BlockBasedImageBase(uint32_t width, uint32_t height, uint32_t nblocks, bool mem_optimized, uint8_t *custom_storage, size_t custom_storage_byte_size)
      : memory_optimized_image_(force_memory_optimization) {
        image_ = nullptr;
        storage_ = nullptr;
        width_ = 0;
        nblocks_ = 0;
        theoretical_component_height_ = 0;
        init(width, height, nblocks, mem_optimized, custom_storage, custom_storage_byte_size);
    }
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
    void init(uint32_t width, uint32_t height, uint32_t nblocks, bool memory_optimized_image, uint8_t *custom_storage = nullptr, size_t custom_storage_byte_size=0) {
        size_t alignment_padding = 31;
        always_assert(custom_storage == nullptr ||
                      custom_storage_byte_size == alignment_padding + nblocks * sizeof(Block));
        theoretical_component_height_ = height;
        if (force_memory_optimization) {
            always_assert(memory_optimized_image && "MemoryOptimized must match template");
        }
        memory_optimized_image_ = force_memory_optimization || memory_optimized_image;
        always_assert(nblocks <= width * height);
        width_ = width;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
#error "CURRENTLY NO LONGER SUPPORTED"
            nblocks = width * 4;
#else
            nblocks = width * 2;
#endif
        }
        nblocks_ = nblocks;
        if (custom_storage) {
          storage_ = custom_storage;
        } else {
          storage_ = (uint8_t*)custom_calloc(nblocks * sizeof(Block) + alignment_padding);
        }
        size_t offset = storage_ - (uint8_t*)nullptr;
        if (offset & 31) { //needs alignment adjustment
            image_ = (Block*)(storage_ + (alignment_padding + 1) - (offset & alignment_padding));
        } else { // already aligned
            image_ = (Block*)storage_;
        }
        if (custom_storage) {
          storage_ = nullptr;
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
        it.num_nonzeros_here += step;
        it.num_nonzeros_above += step;
        /*
        if (!has_left) {
            bool cur_row_first = (it.num_nonzeros_here < it.num_nonzeros_above);
            if (cur_row_first) {
                it.num_nonzeros_above -= width_;
                it.num_nonzeros_above -= width_;
            } else {
                it.num_nonzeros_here -= width_;
                it.num_nonzeros_here -= width_;
            }
            }*/
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
            dev_assert(offset <= nblocks_ && "we mod offset by width_: it is < nblocks_");
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
            dev_assert(offset <= nblocks_ && "we mod offset by width_: it is < nblocks_");
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
    BlockBasedImage(uint32_t width, uint32_t height, uint32_t nblocks, bool mem_optimized, uint8_t *custom_storage, size_t custom_storage_byte_size)
      : BlockBasedImageBase(width, height, nblocks, mem_optimized,
                            custom_storage, custom_storage_byte_size) {}

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
extern std::atomic<std::vector<NeighborSummary> *>gNopNeighbor;
extern uint8_t custom_nop_storage[sizeof(AlignedBlock) * 6 + 31];
enum {
  // if this is true, then luma is used to seed priors for the color channels instead
  // of vice versa
  REVERSE_CMP = 0
};
enum {
  NUM_PRIORS = 2
};
template<class BlockContextType> class ChannelContext {
public:
  Sirikata::Array1d<BlockContextType, NUM_PRIORS + 1> context;
  uint32_t jpeg_x;
  uint32_t jpeg_y;
  BlockContextType& at(uint32_t i) {
    return context.at(i);
  }
  const BlockContextType& at(uint32_t i) const {
    return context.at(i);
  }
};
template<class BlockBasedImage, class BlockContextType> class MultiChannelBlockContext {
  ChannelContext<BlockContextType> context_;
  Sirikata::Array2d<uint32_t, 2, NUM_PRIORS + 1 > eostep;
  Sirikata::Array1d<BlockBasedImage*,
                    (uint32_t)NUM_PRIORS + 1> image_data_;
  BlockBasedImage nop_image;
public:
  ChannelContext<BlockContextType>& getContext() {
    return context_;
  }
  void print(int curr_x, int curr_y) const {
      return;
      for (int i = 1; i <= NUM_PRIORS; ++i) {
          auto ctx = context_.at(i);
          auto block = ctx.here();
          auto nz = *ctx.num_nonzeros_here;
          fprintf(stderr, "(%d,%d) NZ: %d/%d [%d %d %d %d...] ft[%d %d %d %d]\n",
                  curr_x, curr_y,
                  (int) nz.num_nonzeros_,
                  (int)block.recalculate_coded_length(),
                  (int)nz.edge_pixels[0], (int)nz.edge_pixels[1], (int)nz.edge_pixels[8], (int)nz.edge_pixels[9],
                  (int)block.dc(), (int)block.coefficients_raster(1),(int)block.coefficients_raster(8),(int)block.coefficients_raster(9));
          
      }
  }
  template<class ColorChan> MultiChannelBlockContext(
                          int curr_y,
                          ColorChan original_color,
                            Sirikata::Array1d<BlockBasedImage*, (uint32_t)ColorChannel::NumBlockTypes> &in_image_data,
                          Sirikata::Array1d<std::vector<NeighborSummary>,
                                             (size_t)ColorChannel::NumBlockTypes> &num_nonzeros_
      ) : nop_image(3, 2, 3 * 2, true, custom_nop_storage, sizeof(custom_nop_storage)) {
    context_.jpeg_x = 0;
    context_.jpeg_y = curr_y;
    std::vector<NeighborSummary> * nopNeighbor= gNopNeighbor.load();
    if(!nopNeighbor) {
        size_t size = sizeof(custom_nop_storage)/sizeof(AlignedBlock);
        auto * tmp = new std::vector<NeighborSummary>(size);
        memset(&(*tmp)[0], 0, size * sizeof(NeighborSummary));
        gNopNeighbor.store(tmp);
        nopNeighbor= gNopNeighbor.load();
        always_assert(nopNeighbor != nullptr);
    }
    static_assert(NUM_PRIORS + 1 <= (int)ColorChannel::NumBlockTypes,
                  "We need to have room for at least as many priors in max number of channels we support");
    for (int i = 0; i <= NUM_PRIORS; ++i) {
      eostep.at(0, i) = 0;
      eostep.at(1, i) = 0;
      image_data_.at(i) = &nop_image;
      context_.at(i) = image_data_.at(i)->off_y(1, nopNeighbor->begin());
      image_data_.at(i)->next(context_.at(i), false, 1, 1);
    }
    for (size_t col = 0 ; col < (size_t)ColorChannel::NumBlockTypes; ++col) {
      BlockBasedImage*cur = nullptr;
      std::vector<NeighborSummary>::iterator neighborNonzeros;
      size_t out_index = 0;
      if (((REVERSE_CMP && col <= (size_t)original_color)
           ||
           (REVERSE_CMP ==0 && col >= (size_t)original_color)
              ) && in_image_data.at(col) && in_image_data.at(col)->blocks_allocated()) {
          cur = in_image_data.at(col);
          neighborNonzeros = num_nonzeros_[col].begin();
          if (original_color == 0) {
              out_index = col;
          } else if (col == 0) {
              out_index = 1; // make sure luma comes first
          } else if (col == original_color) {
              out_index = 0;
          } else {
              out_index = 2;
          }
      } else {
          continue;
      }
      if (out_index > NUM_PRIORS) {
          continue; //no need to record a 3rd prior if we have 4 channels
      }
      image_data_.at(out_index) = cur;
      size_t vertical_count = cur->original_height();
      size_t orig_vertical_count = in_image_data.at(original_color)->original_height();
      size_t vratio = vertical_count / orig_vertical_count;
      size_t voffset = vratio;
      if (vratio) {
          voffset -= 1; // one less than the edge of this block
      }
      uint32_t adjusted_curr_y = (curr_y * vertical_count)/orig_vertical_count + voffset;
      context_.at(out_index)
        = cur->off_y(adjusted_curr_y,// if we need to fallback to zero, we don't want to use the big index
                     neighborNonzeros);
      size_t horizontal_count = cur->block_width();
      size_t orig_horizontal_count = in_image_data.at((size_t)original_color)->block_width();
      size_t hratio = horizontal_count / orig_horizontal_count;
      if (hratio == 0) {
        eostep.at(0, out_index) = 0;
        if (2 * vertical_count == orig_vertical_count) {
          eostep.at(1, out_index) = 1;
        } else {
          eostep.at(1, out_index) = 0; // avoid prior if we have > 2x ratio of Y to Cb or Cr
          // this can happen for the nop image or for a 3:1 ratio
        }
      } else {
        eostep.at(0, out_index) = hratio;
        eostep.at(1, out_index) = hratio;
        for(size_t off = 1; off < hratio; ++off) { // lets advance to the bottom right edge
          cur->next(context_.at(out_index), true, adjusted_curr_y, 1);
        }
      }
    }
  }
  int next(int curr_x, int curr_y) {
    context_.jpeg_x = curr_x;
    context_.jpeg_y = curr_y;
    int oddness = (curr_x & 1);
    int retval = 0;
    for (int i = NUM_PRIORS; i >= 0; --i) { // we have NUM_PRIORS + 1 entries here
      //fprintf(stderr,"Prior %d stepping %d\n", i, eostep.at(oddness, i));
      retval = image_data_[i]->next(context_.at(i), true, curr_y, eostep.at(oddness, i));
    }
    return retval;
  }
};
#endif
