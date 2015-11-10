#include "../../vp8/util/memory.hh"
#include <string.h>
#include "Reader.hh"
#include "ioutil.hh"
namespace IOUtil {

FileReader * OpenFileOrPipe(const char * filename, int is_pipe, int max_file_size) {
    int fp = 0;
    if (!is_pipe) {
        fp = open(filename, O_RDONLY);
    }
    if (fp >= 0) {
        return new FileReader(fp, max_file_size);
    }
    return NULL;
}
FileWriter * OpenWriteFileOrPipe(const char * filename, int is_pipe) {
    int fp = 1;
    if (!is_pipe) {
        fp = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR | S_IRUSR);
    }
    if (fp >= 0) {
        return new FileWriter(fp, !g_use_seccomp);
    }
    return NULL;
}

}
