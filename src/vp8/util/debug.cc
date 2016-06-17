#include <sys/types.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/fcntl.h>
#endif
#include <errno.h>
#include "debug.hh"
#include "memory.hh"

namespace LeptonDebug{
int med_err;
int amd_err;
int avg_err;
int ori_err;
int loc_err;
int luma_debug_width;
int luma_debug_height;

int chroma_debug_width;
int chroma_debug_height;
int getDebugWidth(int color){
    return color == 0 ? luma_debug_width : chroma_debug_width;
}
int getDebugHeight(int color){
    return color == 0 ? luma_debug_height : chroma_debug_height;
}

#if defined(DUMP_RAW_IMAGE)
int load_raw_fd_output(const char *fname) {
    return open(fname, O_CREAT|O_TRUNC|O_WRONLY, S_IWUSR | S_IRUSR);
}
char * serialize_unsigned_int(unsigned int value, char *output, bool term = true) {
    int counter = value;
    char *end = output;
    do {
        ++end;
        counter /= 10;
    } while(counter);
    if (term) {
        *end = 0;
    }
    char *retval = end;
    do {
        *--end = '0' + value % 10;
        value /= 10;
    }while(value);
    return retval;
}
static ptrdiff_t write_full(int fd, unsigned char * data, size_t size) {
    size_t total_written = 0;
    ptrdiff_t written = 0;
    do {
        written = write(fd, data + total_written, size - total_written);
        if (written <= 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total_written += written;
    } while(total_written < size);
    return total_written;
}
void dumpDebugFile(int fd, int width, int height, unsigned char *data) {
    char header[256] = "P5\n";
    char * width_end = serialize_unsigned_int(width, header + 3);
    *width_end = ' ';
    ++width_end;
    width_end = serialize_unsigned_int(height, width_end);
    *width_end = ' ';
    ++width_end;
    width_end = serialize_unsigned_int(255, width_end);
    *width_end = '\n';
    ++width_end;
    *width_end = '\0';
    write_full(fd, (unsigned char*)header, width_end - header);
    write_full(fd, data, width * height);
}
void dumpDebugData() {
    dumpDebugFile(raw_decoded_fp_Y, luma_debug_width, luma_debug_height, raw_YCbCr[0]);
    dumpDebugFile(raw_decoded_fp_Cb, chroma_debug_width, chroma_debug_height, raw_YCbCr[1]);
    dumpDebugFile(raw_decoded_fp_Cr, chroma_debug_width, chroma_debug_height, raw_YCbCr[2]);
}
void setupDebugData(int lumaWidth, int lumaHeight,
                    int chromaWidth, int chromaHeight) {
    raw_YCbCr[0] = (unsigned char*)custom_calloc(lumaWidth * lumaHeight);
    raw_YCbCr[1] = (unsigned char*)custom_calloc(chromaWidth * chromaHeight);
    raw_YCbCr[2] = (unsigned char*)custom_calloc(chromaWidth * chromaHeight);
    luma_debug_width = lumaWidth;
    luma_debug_height = lumaHeight;
    chroma_debug_width = chromaWidth;
    chroma_debug_height = chromaHeight;
}
#else

int load_raw_fd_output(const char * fname) {
    return -1;
}
void dumpDebugData(){
}
void setupDebugData(int lumaWidth, int lumaHeight,
                    int chromaWidth, int chromaHeight){
}

#endif
int raw_decoded_fp_Y = load_raw_fd_output("/tmp/raw_Y.pgm");
int raw_decoded_fp_Cb = load_raw_fd_output("/tmp/raw_Cb.pgm");
int raw_decoded_fp_Cr = load_raw_fd_output("/tmp/raw_Cr.pgm");
unsigned char *raw_YCbCr[4] = {nullptr, nullptr, nullptr, nullptr};

}
