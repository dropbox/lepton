#include "DecoderPlatform.hh"
#include "Allocator.hh"
#include "Error.hh"
#include "BrotliWrapper.hh"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "brotli/enc/streams.h"
#include "brotli/dec/streams.h"

#include "brotli/dec/decode.h"
#include "brotli/enc/encode.h"
#pragma GCC diagnostic pop
namespace Sirikata {
struct BrotliResizableMemOutput {
    std::vector<uint8_t,
                JpegAllocator<uint8_t> > buffer;
};

// Adapter class to make BrotliOut objects from a string.
class BrotliResizableMemOut : public brotli::BrotliOut {
 public:
  // Create a writer that appends its data to buf.
  // buf->size() will grow to at most max_size
  // buf is expected to be empty when constructing BrotliStringOut.
  BrotliResizableMemOut(std::vector<uint8_t, JpegAllocator<uint8_t> >* buf, size_t max_size);

  void Reset(std::vector<uint8_t, JpegAllocator<uint8_t> >* buf, size_t max_len);

  bool Write(const void* buf, size_t n);

 private:
    std::vector<uint8_t, JpegAllocator<uint8_t> >* buf_;  // start of output buffer
  size_t max_size_;  // max length of output
};


int BrotliResizableMemOutputFunction(void * data, const uint8_t* buf, size_t count) {
    BrotliResizableMemOutput * output = (BrotliResizableMemOutput *)data;
    output->buffer.insert(output->buffer.end(), buf, buf + count);
    return count;
}
std::pair<std::vector<uint8_t,
                     JpegAllocator<uint8_t> >,
          JpegError> brotli_full_decompress(const uint8_t *buffer,
                                            size_t size,
                                            const JpegAllocator<uint8_t> &alloc) {
    std::pair<std::vector<uint8_t, JpegAllocator<uint8_t> >,
              JpegError> retval(std::vector<uint8_t, JpegAllocator<uint8_t> >(alloc),
                                JpegError::nil());
    size_t decoded_size = 0;
    if(!BrotliDecompressedSize(size, buffer,
                               &decoded_size)) {
        decoded_size = size * 2;
    }
    //guess (or exact) of the output size
    retval.first.reserve(decoded_size);
    

    BrotliResizableMemOutput output;
    output.buffer.swap(retval.first);
    BrotliOutput interf;
    interf.cb_ = &BrotliResizableMemOutputFunction;
    interf.data_ = &output;
    BrotliMemInput input = {buffer, size, 0};
    BrotliInput ininterf = BrotliInitMemInput(buffer, size, &input);
    BrotliResult res = BrotliDecompress(ininterf, interf);
    if (res == 1) {
        retval.first.swap(output.buffer);
    } else {
        retval.second = JpegError::errMissingFF00();
    }
    return retval;
}
std::vector<uint8_t,
            JpegAllocator<uint8_t> > brotli_full_compress(const uint8_t *buffer,
                                                          size_t size,
                                                          const JpegAllocator<uint8_t> &alloc){
    std::vector<uint8_t,
                JpegAllocator<uint8_t> > retval(alloc);
    retval.reserve(size);
    brotli::BrotliParams params;
    params.lgblock = 1;
    {
        size_t tmp = size;
        while(tmp >>= 1) {
            ++params.lgblock; // log of the size + 1 should be the max input block size
        }
        if (params.lgblock < 16) {
            params.lgblock = 16;
        }
        if (params.lgblock > 24) {
            params.lgblock = 24;
        }
    }
    brotli::BrotliMemIn ininterf(buffer,size);
    BrotliResizableMemOut interf(&retval, 1024 * 1024 * 64); // no more than 64 meg header allowed
    brotli::BrotliCompress(params, &ininterf, &interf);
    
    return retval;
}

BrotliResizableMemOut::BrotliResizableMemOut(std::vector<uint8_t, JpegAllocator<uint8_t> >* buf,
                                             size_t max_size)
    : buf_(buf),
      max_size_(max_size) {
  assert(buf->empty());
}

void BrotliResizableMemOut::Reset(std::vector<uint8_t, JpegAllocator<uint8_t> >* buf, size_t max_len) {
  buf_ = buf;
  max_size_ = max_len;
}

// Brotli output routine: add n bytes to a string.
bool BrotliResizableMemOut::Write(const void *buf, size_t n) {
  if (buf_->size() + n > max_size_)
    return false;
  buf_->insert(buf_->end(),
               static_cast<const uint8_t*>(buf),
               static_cast<const uint8_t*>(buf) + n);
  return true;
}


}
