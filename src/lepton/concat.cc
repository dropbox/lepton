#include <fcntl.h>
#include "../vp8/util/memory.hh"
#include "../io/BrotliCompression.hh"
#include "../io/ioutil.hh"
#include "../io/Seccomp.hh"

namespace {
uint32_t LEtoUint32(const uint8_t*buffer) {
    uint32_t retval = buffer[3];
    retval <<=8;
    retval |= buffer[2];
    retval <<= 8;
    retval |= buffer[1];
    retval <<= 8;
    retval |= buffer[0];
    return retval;
}

void uint32toLE(uint32_t value, uint8_t *retval) {
    retval[0] = uint8_t(value & 0xff);
    retval[1] = uint8_t((value >> 8) & 0xff);
    retval[2] = uint8_t((value >> 16) & 0xff);
    retval[3] = uint8_t((value >> 24) & 0xff);
}
}

extern const char** filelist;
void concatenate_files(int fdint, int fdout) {
    using namespace Sirikata;
    std::vector<IOUtil::FileReader*,
                JpegAllocator<IOUtil::FileReader*>> files_to_concatenate;
    files_to_concatenate.push_back(new IOUtil::FileReader(fdint, 0x7fffffff, false));
    if (fdout == -1) {
        fdout = 1; // push to stdout
    }
    IOUtil::FileWriter writer(fdout, false, false);
    for (const char ** file = filelist + 1; *file != NULL; ++file) {
        int cur_file = open(*file, O_RDONLY
#ifdef _WIN32
                            |O_BINARY
#endif
            );
        
        files_to_concatenate.push_back(new IOUtil::FileReader(cur_file, 0x7fffffff, false));
    }
    if (g_use_seccomp) {
        Sirikata::installStrictSyscallFilter(true);
    }
    std::vector<std::vector<uint8_t, JpegAllocator<uint8_t> >,
                JpegAllocator<std::vector<uint8_t, JpegAllocator<uint8_t> > > > lepton_headers_28;
    lepton_headers_28.resize(files_to_concatenate.size());
    for (size_t i = 0; i < files_to_concatenate.size(); ++i) {
        lepton_headers_28[i].resize(28);
        if (i == 0) {
            lepton_headers_28[i][0] = 0xcf;
            lepton_headers_28[i][1] = 0x84;
            if (ReadFull(files_to_concatenate[i], &lepton_headers_28[i][2], 26) != 26) {
                custom_exit(ExitCode::SHORT_READ);
            }
        }else {
            if (ReadFull(files_to_concatenate[i], &lepton_headers_28[i][0], 28) != 28) {
                custom_exit(ExitCode::SHORT_READ);
            }
        }
    }
    std::vector<uint8_t, JpegAllocator<uint8_t> > mega_header;
    std::vector<uint8_t, JpegAllocator<uint8_t> > compressed_header;
    for (size_t i = 0; i < files_to_concatenate.size(); ++i) {
        uint32_t header_size = LEtoUint32(&lepton_headers_28[i][24]);
        uint32toLE(0, &lepton_headers_28[i][24]);
        compressed_header.resize(header_size);
        if (ReadFull(files_to_concatenate[i], &compressed_header[0], header_size) != header_size) {
            custom_exit(ExitCode::STREAM_INCONSISTENT);
        }
        std::pair<std::vector<uint8_t,
                              Sirikata::JpegAllocator<uint8_t> >,
                  JpegError> uncompressed_header_buffer(
                      Sirikata::BrotliCodec::Decompress(compressed_header.data(),
                                                        compressed_header.size(),
                                                        JpegAllocator<uint8_t>(),
                                                        0xffffffff));
        always_assert(uncompressed_header_buffer.second == JpegError::nil() && "must be able to read from buffer ");
        
        if (i != 0) {
            if(memcmp(&mega_header[mega_header.size() - 3],
                      "CMP", 3) == 0) {
                mega_header[mega_header.size() - 3] = 'C';
                mega_header[mega_header.size() - 2] = 'N';
                mega_header[mega_header.size() - 1] = 'T'; // set continuation
            } else {
                mega_header.push_back('C');
                mega_header.push_back('N');
                mega_header.push_back('T'); // add this to break from header loop
            }
        }
        mega_header.insert(mega_header.end(), uncompressed_header_buffer.first.begin(), uncompressed_header_buffer.first.end());
    }
    uint8_t desired_thread_count = lepton_headers_28[0][4];
    for (size_t i = 0; i < files_to_concatenate.size(); ++i) {
        always_assert(lepton_headers_28[i][4] == desired_thread_count && "All thread counts must match in lepton");
        always_assert(lepton_headers_28[i][2] >= 2 && "Only version 2 supported for file concatenation");
    }
    std::vector<uint8_t, JpegAllocator<uint8_t> > compressed_mega_header =
        BrotliCodec::Compress(mega_header.data(),
                              mega_header.size(),
                              JpegAllocator<uint8_t>(),
                              11);
    std::vector<uint8_t, JpegAllocator<uint8_t> > body;    
    uint32toLE(compressed_mega_header.size(), &lepton_headers_28[0][24]);
    for (size_t i = 0; i < files_to_concatenate.size(); ++i) {
        uint32_t accumulated_size = 0;
        std::pair<size_t, JpegError> ret = writer.Write(lepton_headers_28[i].data(),lepton_headers_28[i].size());
        accumulated_size += ret.first;
        always_assert(ret.second == JpegError::nil());
        if (i == 0) {
            ret = writer.Write(&compressed_mega_header[0],compressed_mega_header.size());
            accumulated_size += ret.first;
            always_assert(ret.second == JpegError::nil());
        }
        unsigned char buffer[65536];
        body.resize(0);
        while (true) {
            ret = files_to_concatenate[i]->Read(buffer, sizeof(buffer));
            if (ret.first ==0 ){
                break;
            }
            body.insert(body.end(), buffer, buffer + ret.first);
            if (ret.second != JpegError::nil()) {
                break;
            }
        }
        always_assert(body.size() > 4 && "must have enough room for EOF header");
        accumulated_size += body.size();
        uint32toLE(accumulated_size, &body[body.size() - 4]);
        ret = writer.Write(body.data(), body.size());
        always_assert(ret.second == JpegError::nil());
    }
    custom_exit(ExitCode::SUCCESS);
}
