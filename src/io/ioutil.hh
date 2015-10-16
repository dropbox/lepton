#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
namespace Sirikata {
class DecoderReader;
class DecoderWriter;
}
namespace IOUtil {


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
public:
    FileReader(int ff) {
        fp = ff;
    }
    std::pair<Sirikata::uint32, Sirikata::JpegError> Read(Sirikata::uint8*data, unsigned int size) {
        using namespace Sirikata;
        do {
            signed long nread = read(fp, data, size);
            if (nread <= 0) {
                if (errno == EINTR) {
                    continue;
                }
                return std::pair<Sirikata::uint32, JpegError>(0, MakeJpegError("Short read"));
            }
            return std::pair<Sirikata::uint32, JpegError>(nread, JpegError::nil());
        } while(true); // while not EINTR
    }
    size_t length() {
        size_t where = lseek(fp, 0, SEEK_CUR);
        lseek(fp, 0, SEEK_END);
        size_t retval = lseek(fp, 0, SEEK_CUR);
        lseek(fp, where, SEEK_SET);
        return retval;
    }
};
class FileWriter : public Sirikata::DecoderWriter {
    int fp;
public:
    FileWriter(int ff) {
        fp = ff;
    }
    void Close() {
        close(fp);
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
                return std::pair<Sirikata::uint32, JpegError>(data_written, JpegError::errShortHuffmanData());
            }
            data_written += nwritten;
        }
        return std::pair<Sirikata::uint32, JpegError>(size, JpegError::nil());
    }
    size_t getsize() {
        return lseek(fp, 0, SEEK_CUR);
    }
};

SIRIKATA_FUNCTION_EXPORT FileReader * OpenFileOrPipe(const char * filename, int is_pipe, int, int);
SIRIKATA_FUNCTION_EXPORT FileWriter * OpenWriteFileOrPipe(const char * filename, int is_pipe, int, int);

}
