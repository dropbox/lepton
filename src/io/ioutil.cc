#include "../../vp8/util/memory.hh"
#include <string.h>
#include <poll.h>
#include "Reader.hh"
#include "ioutil.hh"
#include "../../dependencies/md5/md5.h"

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
Sirikata::Array1d<uint8_t, 16> send_and_md5_result(const uint8_t *data,
                                                   size_t data_size,
                                                   int send_to_subprocess,
                                                   int recv_from_subprocess,
                                                   size_t *output_size){
    struct pollfd fds[2];
    size_t num_fds = sizeof(fds)/sizeof(fds[0]);
    MD5_CTX context;
    MD5_Init(&context);
    *output_size = 0;
    int flags = fcntl(send_to_subprocess, F_GETFL, 0);
    fcntl(send_to_subprocess, F_SETFL, flags | O_NONBLOCK);
    size_t send_cursor = 0;
    uint8_t buffer[65536];
    bool finished = false;
    while(!finished) {
        num_fds = 0;
        if (send_to_subprocess != -1) {
            fds[num_fds].events = POLLOUT | POLLPRI | POLLERR | POLLHUP;
            fds[num_fds].fd = send_to_subprocess;
            ++num_fds;
        }
        if (recv_from_subprocess != -1) {
            fds[num_fds].events = POLLIN | POLLERR | POLLHUP;
            fds[num_fds].fd = recv_from_subprocess;
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
            if (fds[i].revents & (POLLERR/* | POLLHUP*/)) {
                should_close = true;
            }
            if ((fds[i].revents)) {
                if (fds[i].fd == recv_from_subprocess) {
                    ssize_t del = read(fds[i].fd, &buffer[0], sizeof(buffer));
                    if (del < 0 && errno == EINTR) {
                        continue;
                    }
                    if (del == 0) {
                        should_close = true;
                        finished = true;
                    }
                    if (del > 0) {
                        MD5_Update(&context, &buffer[0], del);
                        *output_size += del;
                    }
                    //fprintf(stderr,"%d) Reading %ld bytes for total of %ld\n", fds[i].fd, del, *output_size);
                }
                if (fds[i].fd == send_to_subprocess) {
                    ssize_t del = 0;
                    if (send_cursor < data_size) {
                        del = write(fds[i].fd,
                                    &data[send_cursor],
                                    data_size - send_cursor);
                    }
                    //fprintf(stderr, "Want %ld bytes, but sent %ld\n", data_size - send_cursor,  del);
                    if (del < 0 && errno == EINTR) {
                        continue;
                    }
                    if (del > 0) {
                        send_cursor += del;
                        //fprintf(stderr, "len Storage is %ld/%ld\n", send_cursor, data_size);
                    }
                    if (send_cursor == data_size || del == 0) {
                        should_close = true;
                    }
                }
            }
            if (should_close) {
                while (close(fds[i].fd) < 0 && errno == EINTR) {}
                if (fds[i].fd == send_to_subprocess) {
                    send_to_subprocess = -1;
                }
                if (fds[i].fd == recv_from_subprocess) {
                    recv_from_subprocess = -1;
                }
            }
        }
    }
    Sirikata::Array1d<uint8_t, 16> retval;
    MD5_Final(&retval[0], &context);
    return retval;    
}
Sirikata::Array1d<uint8_t, 16> transfer_and_md5(Sirikata::Array1d<uint8_t, 2> header,
                                                size_t start_byte,
                                                size_t end_byte,
                                                bool send_header,
                                                int copy_to_input_tee, int input_tee,
                                                int copy_to_storage, size_t *input_size,
                                                Sirikata::MuxReader::ResizableByteBuffer *storage,
                                                bool close_input) {
    fd_set fds, readfds, writefds, errorfds;
    FD_ZERO(&fds);
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);
    int max_fd = -1;
    int flags = 0;
#ifndef __APPLE__
    flags = fcntl(input_tee, F_GETFL, 0);
#endif
    fcntl(input_tee, F_SETFL, flags | O_NONBLOCK);
#ifndef __APPLE__
    flags = fcntl(copy_to_input_tee, F_GETFL, 0);
#endif
    fcntl(copy_to_input_tee, F_SETFL, flags | O_NONBLOCK);
#ifndef __APPLE__
    flags = fcntl(copy_to_storage, F_GETFL, 0);
#endif
    fcntl(copy_to_storage, F_SETFL, flags | O_NONBLOCK);

    MD5_CTX context;
    MD5_Init(&context);
    if (start_byte < header.size()) {
        MD5_Update(&context, &header[start_byte], header.size() - start_byte);
    }
    *input_size = header.size();
    uint8_t buffer[65536] = {0};
    static_assert(sizeof(buffer) >= header.size(), "Buffer must be able to hold header");
    uint32_t cursor = 0;
    bool finished = false;
    while (!finished) {
    FD_ZERO(&fds);
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);
        fprintf(stderr, "Overarching loop\n");
        max_fd = 0;
        if (copy_to_storage != -1) {
            FD_SET(copy_to_storage, &readfds);
            FD_SET(copy_to_storage, &errorfds);
            if (copy_to_storage + 1 > max_fd) {
                max_fd = copy_to_storage + 1;
            }
        }
        if (copy_to_input_tee != -1) {
            if (cursor < sizeof(buffer)) {
                FD_SET(copy_to_input_tee, &readfds);
                FD_SET(copy_to_input_tee, &errorfds);
                if (copy_to_input_tee + 1 > max_fd) {
                    max_fd = copy_to_input_tee + 1;
                }
            }
        } else {
            // copy to input_tee is closed
            if (input_tee != -1 && cursor == 0) { // we copied everything here
                fprintf(stderr, "CLosing %d\n", input_tee);
                while (close(input_tee) < 0 && errno == EINTR) {}
                input_tee = -1;
            }
        }
        if (input_tee != -1 && cursor != 0) {
            FD_SET(input_tee, &writefds);
            FD_SET(input_tee, &errorfds);
                if (input_tee + 1 > max_fd) {
                    max_fd = input_tee + 1;
                }
        }
        fprintf(stderr, "START POLL %d: (%d %d %d)\n", max_fd, copy_to_input_tee != -1 ? FD_ISSET(copy_to_input_tee, &fds) : -1,
                input_tee != -1 ? FD_ISSET(input_tee, &fds) : -1, copy_to_storage != -1 ? FD_ISSET(copy_to_storage, &fds) : -1);
            int ret = select(max_fd, &readfds, &writefds, &errorfds, NULL);
            fprintf(stderr, "FIN POLL %d\n", ret);
            if (ret == 0 || (ret < 0 && errno == EINTR)) {
                continue;
            }
            /*for (size_t i = 0; i < num_fds; ++i)*/ {
            fprintf(stderr, "loopdy start %d\n", ret);
            bool should_close = false;
            if (true) {
                fprintf(stderr, "Checking ev\n");
                if (copy_to_input_tee != -1 && cursor < sizeof(buffer)) {

                    always_assert(cursor < sizeof(buffer)); // precondition to being in the poll
                    size_t max_to_read = sizeof(buffer) - cursor;
                    if (end_byte != 0 && max_to_read > end_byte - *input_size) {
                        max_to_read = end_byte - *input_size;
                    }
                    ssize_t del = read(copy_to_input_tee, &buffer[cursor], max_to_read);
                    if (del >= 0) {
                    if (del == 0) {
                        should_close = true;
                        if (close_input) {
                fprintf(stderr, "CLosing %d\n", copy_to_input_tee);
                                while (close(copy_to_input_tee) < 0 && errno == EINTR) {}
                                }
                        fprintf(stderr,"input:Should close(%d) size:%ld\n", copy_to_input_tee, *input_size);
                        copy_to_input_tee = -1;

                    }
                    if (del > 0) {
                        if (*input_size + del > start_byte) {
                            if (*input_size >= start_byte) {
                                MD5_Update(&context, &buffer[cursor], del);
                            } else {
                                size_t offset = (start_byte - *input_size);
                                MD5_Update(&context, &buffer[cursor + offset], del - offset);
                            }
                        }
                        *input_size += del;
                        cursor += del;
                        if (end_byte != 0 && *input_size == end_byte) {
                            should_close = true; // read the truncated input
                        }
                    }
                    } else if (!(errno == EINTR || errno == EWOULDBLOCK  || errno == EAGAIN)) {
                        fprintf(stderr,"%d) retry Err\n", copy_to_input_tee);
                    } else {

                        fprintf(stderr, "Error %d\n", errno);
                    }
                    fprintf(stderr,"%d) Reading %ld bytes for total of %ld\n", copy_to_input_tee, del, *input_size);
                }
                if (copy_to_storage != -1) {
                    if (storage->how_much_reserved() < storage->size() + sizeof(buffer)) {
                        storage->reserve(storage->how_much_reserved() * 2);
                    }
                    size_t old_size = storage->size();
                    storage->resize(storage->how_much_reserved());
                    ssize_t del = read(copy_to_storage,
                                       &(*storage)[old_size],
                                       storage->size() - old_size);
                    fprintf(stderr, "Want %ld bytes, but read %ld\n", storage->size() - old_size,  del);
                    if (del < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                        fprintf(stderr, "EAGAIN %d\n", errno);
                        storage->resize(old_size);
                    } else if (del < 0) {
                        fprintf(stderr, "Error %d\n", errno);
                    }else{
                        if (del == 0) {
                                storage->resize(old_size);
                                finished = true;
                                should_close = true;
                                finished = true;
                                fprintf(stderr,"back_copy:Should close(%d):size %ld\n",copy_to_storage,storage->size());
                                fprintf(stderr, "CLosing %d\n", copy_to_storage);
                                while (close(copy_to_storage) < 0 && errno == EINTR) {}
                                copy_to_storage = -1;                        
                         }

                         if (del > 0) {
                                 storage->resize(old_size + del);
                             fprintf(stderr, "len Storage is %ld\n", storage->size());
                         }
                    }
                }
            }
            if (input_tee != -1 && cursor != 0) {
                always_assert (cursor != 0);//precondition to being in the pollfd set
                ssize_t del = write(input_tee, buffer, cursor);
                fprintf(stderr, "fd: %d: Writing %ld data to %d\n", input_tee, del, cursor);
                fprintf(stderr, "A");
                if (del == 0) {
                        fprintf(stderr, "B\n");
                    should_close = true;
                }
                fprintf(stderr, "C\n");
                if (del > 0) {
                fprintf(stderr, "D\n");
                    if (del < cursor) {
                        fprintf(stderr, "E %ld %ld\n", del, cursor - del);
                        memmove(buffer, buffer + del, cursor - del);
                    }
                    cursor -= del;
                }
            }
                if (cursor == 0 && copy_to_input_tee == -1 && input_tee != -1) {
                    fprintf(stderr,"E\n");
                    should_close = true;
                    fprintf(stderr, "CLosing %d\n", input_tee);
                    while (close(input_tee) < 0 && errno == EINTR) {}
                    fprintf(stderr,"output_to_compressor:Should close (%d) \n", input_tee );
                    input_tee = -1;
                }
                fprintf(stderr,"F\n");
            
                fprintf(stderr, "loopdy  end \n");
        }
    }
    *input_size -= start_byte;
    Sirikata::Array1d<uint8_t, 16> retval;
    MD5_Final(&retval[0], &context);
    return retval;
}

}
