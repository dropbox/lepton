#ifndef SIRIKIATA_IO_UTIL_HH_
#define SIRIKIATA_IO_UTIL_HH_
#ifndef _WIN32
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#include "../vp8/util/nd_array.hh"
#include "MuxReader.hh"
namespace Sirikata {
class DecoderReader;
class DecoderWriter;
}
namespace IOUtil {
//#ifdef _WIN32
//    typedef void* HANDLE_or_fd;
//#else
    typedef int HANDLE_or_fd;
//#endif

inline Sirikata::uint32 ReadFull(Sirikata::DecoderReader * reader, void * vdata, Sirikata::uint32 size) {
    using namespace Sirikata;
    unsigned char * data = reinterpret_cast<unsigned char*>(vdata);
    uint32 copied = 0;
    while (copied < size) {
        std::pair<Sirikata::uint32, Sirikata::JpegError> status = reader->Read(data + copied, size - copied);
        copied += status.first;
        if (status.second != JpegError::nil() || status.first == 0) {
            return copied;
        }
    }
    return copied;
}


class FileReader : public Sirikata::DecoderReader {
    int fp;
    uint32_t total_read;
    uint32_t max_read;
    bool is_fd_socket;
public:
    FileReader(int ff, int max_read_allowed, bool is_socket) {
        fp = ff;
        this->is_fd_socket = is_socket;
        total_read = 0;
        max_read = max_read_allowed;
    }
    bool is_socket()const {
        return is_fd_socket;
    }
    std::pair<Sirikata::uint32, Sirikata::JpegError> Read(Sirikata::uint8*data, unsigned int size) {
        if (max_read && total_read + size > max_read) {
            size = max_read - total_read;
            if (size == 0) {
                return std::pair<Sirikata::uint32,
                                 Sirikata::JpegError>(0, Sirikata::JpegError::errEOF());
            }
        }
        using namespace Sirikata;
        do {
            signed long nread = read(fp, data, size);
            if (nread <= 0) {
                if (errno == EINTR) {
                    continue;
                }
                return std::pair<Sirikata::uint32, JpegError>(0, MakeJpegError("Short read"));
            }
            total_read += nread;
            return std::pair<Sirikata::uint32, JpegError>(nread, JpegError::nil());
        } while(true); // while not EINTR
    }
    unsigned int bound() const {
        return max_read;
    }
    size_t length() {
        return total_read;
    }
    size_t getsize() {
        return total_read;
    }
    int get_fd() const {
        return fp;
    }
    void mark_some_bytes_already_read(uint32_t num_bytes) {
        total_read += num_bytes;
    }
};
class FileWriter : public Sirikata::DecoderWriter {
    int fp;
    int total_written;
    bool close_stream;
    bool is_fd_socket;
public:
    FileWriter(int ff, bool do_close_stream, bool is_fd_socket) {
        this->is_fd_socket = is_fd_socket;
        fp = ff;
        total_written = 0;
        close_stream = do_close_stream;
    }
    bool is_socket() const {
        return is_fd_socket;
    }
    void Close() {
        if (close_stream) {
          while (close(fp) == -1 && errno == EINTR){}
          // not always useful (eg during SECCOMP)
        }
        fp = -1;
    }
    std::pair<Sirikata::uint32, Sirikata::JpegError> Write(const Sirikata::uint8*data, unsigned int size) {
        using namespace Sirikata;
                size_t data_written = 0;
        while (data_written < size) {
            signed long nwritten = write(fp, data + data_written, size - data_written);
            if (nwritten <= 0) {
                if (errno == EINTR) {
                    continue;
                }
                //        The size_t -> Sirikata::uint32 cast is safe because sizeof(size) is <= sizeof(Sirikata::uint32)
                return std::pair<Sirikata::uint32, JpegError>(static_cast<Sirikata::uint32>(data_written), JpegError::errShortHuffmanData());
            }
            data_written += nwritten;
        }
        total_written += size;
        return std::pair<Sirikata::uint32, JpegError>(size, JpegError::nil());
    }
    size_t getsize() {
        return total_written;
    }
    int get_fd() const {
        return fp;
    }

};

//SIRIKATA_FUNCTION_EXPORT FileReader * OpenFileOrPipe(const char * filename, int is_pipe, int max_size_read);
//SIRIKATA_FUNCTION_EXPORT FileWriter * OpenWriteFileOrPipe(const char * filename, int is_pipe);

SIRIKATA_FUNCTION_EXPORT FileReader * BindFdToReader(int fd, uint32_t max_size_read, bool is_socket);
SIRIKATA_FUNCTION_EXPORT FileWriter * BindFdToWriter(int fd, bool is_socket);


Sirikata::Array1d<uint8_t, 16> send_and_md5_result(const uint8_t *data,
                                                   size_t data_size,
                                                    HANDLE_or_fd send_to_subprocess,
                                                    HANDLE_or_fd recv_from_subprocess,
                                                   size_t *output_size);

// returns the md5sum of the input and tee'd input stores the output in the ResizableByteBuffer
Sirikata::Array1d<uint8_t, 16> transfer_and_md5(Sirikata::Array1d<uint8_t, 2> header,
                                                size_t start_byte,
                                                size_t end_byte,
                                                bool send_header,
                                                int input, HANDLE_or_fd input_tee,
                                                HANDLE_or_fd output, size_t *input_size,
                                                Sirikata::MuxReader::ResizableByteBuffer *stored_outpt,
                                                std::vector<uint8_t>*optional_original_input_return,
                                                bool is_socket);

struct SubprocessConnection {
    HANDLE_or_fd pipe_stdin;
    HANDLE_or_fd pipe_stdout;
    HANDLE_or_fd pipe_stderr;
    int sub_pid;
};
SubprocessConnection start_subprocess(int argc, const char **argv, bool pipe_stder, bool stderr_to_nul=false);
}
#endif
