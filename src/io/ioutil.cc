#include "../../vp8/util/memory.hh"
#include <string.h>
#include <poll.h>
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

FileReader * BindFdToReader(int fd, unsigned int max_file_size) {
    if (fd >= 0) {
        return new FileReader(fd, max_file_size);
    }
    return NULL;
}
FileWriter * BindFdToWriter(int fd) {
    if (fd >= 0) {
        return new FileWriter(fd, !g_use_seccomp);
    }
    return NULL;
}
Sirikata::Array1d<uint8_t, 16> transfer_and_md5(Sirikata::Array1d<uint8_t, 2> header, bool send_header,
                                                int copy_to_input_tee, int input_tee,
                                                int copy_to_storage, size_t *input_size,
                                                Sirikata::MuxReader::ResizableByteBuffer *storage) {
    struct pollfd fds[3];
    size_t num_fds = sizeof(fds)/sizeof(fds[0]);
    uint8_t buffer[65536] = {0};
    static_assert(sizeof(buffer) >= header.size(), "Buffer must be able to hold header");
    uint32_t cursor = 0;
    bool finished;
    while (!finished) {
        num_fds = 0;
        if (copy_to_storage != -1) {
            fds[num_fds].events = POLLIN | POLLERR | POLLHUP;
            fds[num_fds].fd = copy_to_storage;
            ++num_fds;
        }
        if (copy_to_input_tee != -1) {
            if (cursor < sizeof(buffer)) {
                fds[num_fds].events = POLLIN | POLLERR | POLLHUP;
                fds[num_fds].fd = copy_to_input_tee;
                ++num_fds;
            }
        } else {
            // copy to input_tee is closed
            if (input_tee != -1 && cursor == 0) { // we copied everything here
                while (close(input_tee) < 0 && errno == EINTR) {}
                input_tee = -1;
            }
        }
        if (input_tee != -1 && cursor != 0) {
            fds[num_fds].events = POLLOUT | POLLPRI | POLLERR | POLLHUP;
            fds[num_fds].fd = input_tee;
            ++num_fds;
        }
        for (size_t i = 0; i < sizeof(fds) /sizeof(fds[0]); ++i) {
            fds[i].revents = 0;
        }
        int ret = poll(fds, num_fds, -1);
        if (ret == 0 || (ret < 0 && errno == EINTR)) {
            continue;
        }
        for (size_t i = 0; i < num_fds; ++i) {
            bool should_close = false;
            if (fds[i].revents & (POLLERR | POLLHUP)) {
                should_close = true;
            }
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == copy_to_input_tee) {
                    always_assert(cursor < sizeof(buffer)); // precondition to being in the poll
                    ssize_t del = read(fds[i].fd, &buffer[cursor], sizeof(buffer) - cursor);
                    if (del < 0 && errno == EINTR) {
                        continue;
                    }
                    if (del == 0) {
                        should_close = true;
                    }
                    *input_size += del;
                    cursor += del;
                }
                if (fds[i].fd == copy_to_storage) {
                    if (storage->how_much_reserved() < storage->size() + sizeof(buffer)) {
                        storage->reserve(storage->how_much_reserved() * 2);
                    }
                    size_t old_size = storage->size();
                    storage->resize(storage->how_much_reserved());
                    ssize_t del = read(fds[i].fd,
                                       &(*storage)[old_size],
                                       storage->size() - old_size);
                    if (del < 0 && errno == EINTR) {
                        storage->resize(old_size);
                        continue;
                    }
                    if (del == 0) {
                        storage->resize(old_size);
                        finished = true;
                        should_close = true;
                    }
                    storage->resize(old_size + del);
                }
            }
            if (fds[i].revents & POLLOUT) {
                always_assert (cursor != 0);//precondition to being in the pollfd set
                ssize_t del = write(fds[i].fd, buffer, cursor);
                if (del == 0) {
                    should_close = true;
                }
                if (del > 0) {
                    if (del < cursor) {
                        memmove(buffer, buffer + del, cursor - del);
                    }
                    cursor -= del;
                }
            }
            if (should_close) {
                if (fds[i].fd == copy_to_input_tee) {
                    copy_to_input_tee = -1;
                }
                if (fds[i].fd == copy_to_storage) {
                    copy_to_storage = -1;
                    finished = true;
                }
                if (fds[i].fd == input_tee) {
                    input_tee = -1;
                }
                while (close(fds[i].fd) < 0 && errno == EINTR) {}

            }
        }
    }
    Sirikata::Array1d<uint8_t, 16> retval;
    retval.memset(0);
    return retval;
}

}
