#include <string.h>
#include "Reader.hh"
#include "ioutil.hh"
namespace IOUtil {

FileReader * OpenFileOrPipe(char * filename, int is_pipe, int, int) {
    FILE * fp = stdin;
    if (!is_pipe) {
        fp = fopen(filename, "rb");
    }
    if (fp) {
        return new FileReader(fp);
    }
    return NULL;
}
FileWriter * OpenWriteFileOrPipe(char * filename, int is_pipe, int, int) {
    FILE * fp = stdout;
    if (!is_pipe) {
        fp = fopen(filename, "wb");
    }
    if (fp) {
        return new FileWriter(fp);
    }
    return NULL;
}


/*
class FDWriter : public Sirikata::DecoderWriter {
    int fd;
public:
    FDWriter(int f) {
        fd = f;
    }
    void Close() {
        // nothing, for now
    }
    std::pair<Sirikata::uint32, Sirikata::JpegError> Write(const Sirikata::uint8*data, unsigned int size) {
        size_t written = 0;
        while (written < size) {
            ssize_t status = write(fd, data + written, size - written);
            if (status <= 0) {
                if (status < 0 && errno == EINTR) {
                    continue;
                } else {
                    return std::pair<Sirikata::uint32, Sirikata::JpegError>(written, Sirikata::JpegError::errEOF());
                }
            }
            written += status;
        }
        return std::pair<Sirikata::uint32, Sirikata::JpegError>(size, Sirikata::JpegError::nil());
    }
};
*/
}
