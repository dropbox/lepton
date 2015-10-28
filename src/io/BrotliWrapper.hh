namespace Sirikata {
std::pair<std::vector<uint8_t,
                     JpegAllocator<uint8_t> >,
          JpegError> brotli_full_decompress(const uint8_t *buffer,
                                            size_t size,
                                            const JpegAllocator<uint8_t> &alloc
                                            = JpegAllocator<uint8_t>());
std::vector<uint8_t,
            JpegAllocator<uint8_t> > brotli_full_compress(const uint8_t *buffer,
                                                          size_t size,
                                                          const JpegAllocator<uint8_t> &alloc
                                                          = JpegAllocator<uint8_t>());
}
