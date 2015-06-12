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
    FILE * fp;
    unsigned int magicread;
public:
    FileReader(FILE * ff) {
        fp = ff;
    }
    std::pair<Sirikata::uint32, Sirikata::JpegError> Read(Sirikata::uint8*data, unsigned int size) {
        using namespace Sirikata;
        signed long nread = fread(data, 1, size, fp);
        //fprintf(stderr, "%d READ %02x%02x%02x%02x - %02x%02x%02x%02x\n", (int)nread, data[0], data[1],data[2], data[3],
        //        data[nread-4],data[nread-3],data[nread-2],data[nread-1]);
        if (nread <= 0) {
            return std::pair<Sirikata::uint32, JpegError>(0, MakeJpegError("Short read"));
        }
        return std::pair<Sirikata::uint32, JpegError>(nread, JpegError::nil());
    }
    size_t length() {
        size_t where = ftell(fp);
        fseek(fp, 0, SEEK_END);
        size_t retval = ftell(fp);
        fseek(fp, where, SEEK_SET);
        return retval;
    }
};
class FileWriter : public Sirikata::DecoderWriter {
    FILE * fp;
public:
    FileWriter(FILE * ff) {
        fp = ff;
    }
    void Close() {
        fclose(fp);
        fp = NULL;
    }
    std::pair<Sirikata::uint32, Sirikata::JpegError> Write(const Sirikata::uint8*data, unsigned int size) {
        using namespace Sirikata;
        signed long nwritten = fwrite(data, size, 1, fp);
        if (nwritten == 0) {
            return std::pair<Sirikata::uint32, JpegError>(0, JpegError::errShortHuffmanData());
        }
        return std::pair<Sirikata::uint32, JpegError>(size, JpegError::nil());
    }
    size_t getsize() {
        return ftell(fp);
    }
};

SIRIKATA_FUNCTION_EXPORT FileReader * OpenFileOrPipe(char * filename, int is_pipe, int, int);
SIRIKATA_FUNCTION_EXPORT FileWriter * OpenWriteFileOrPipe(char * filename, int is_pipe, int, int);

}
