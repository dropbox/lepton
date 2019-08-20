#include "../../vp8/util/memory.hh"
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <thread>
#include <functional>
#define S_IWUSR 0
#define S_IRUSR 0
#else
#include <sys/select.h>
#endif
#include "Reader.hh"
#include "ioutil.hh"
#ifdef USE_SYSTEM_MD5_DEPENDENCY
#include <openssl/md5.h>
#else
#include "../../dependencies/md5/md5.h"
#endif
#ifdef _WIN32
#include <Windows.h>
#include <tchar.h>
#endif
#if 1//def __APPLE__
#include <mutex>
#endif
const size_t MAX_PERMISSIVE_LEPTON_SIZE = 32 * 1024 * 1024;
namespace IOUtil {
/*
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
*//*
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
*/

FileReader * BindFdToReader(int fd, unsigned int max_file_size, bool is_socket) {
    if (fd >= 0) {
        return new FileReader(fd, max_file_size, is_socket);
    }
    return NULL;
}
FileWriter * BindFdToWriter(int fd, bool is_socket) {
    if (fd >= 0) {
        return new FileWriter(fd, !g_use_seccomp, is_socket);
    }
    return NULL;
}
void send_all_and_close(int fd, const uint8_t *data, size_t data_size) {
    while (data_size > 0) {
        auto ret = write(fd, data, data_size);
        if (ret == 0) {
            break;
        }
        if (ret < 0 && errno == EINTR) {
            continue;
        }
        if (ret < 0) {
            auto local_errno = errno;
            fprintf(stderr, "Send err %d\n", local_errno);
            custom_exit(ExitCode::SHORT_READ);
        }
        data += ret;
        data_size -= ret;
    }
    while (close(fd) == -1 && errno == EINTR) {}
}
Sirikata::Array1d<uint8_t, 16> send_and_md5_result(const uint8_t *data,
                                                   size_t data_size,
                                                   int send_to_subprocess,
                                                   int recv_from_subprocess,
                                                   size_t *output_size){
    MD5_CTX context;
    MD5_Init(&context);
    *output_size = 0;
    uint8_t buffer[65536];

#ifdef _WIN32
    std::thread send_all_thread(std::bind(&send_all_and_close, send_to_subprocess, data, data_size));
    FILE * fp = _fdopen(recv_from_subprocess, "rb");
    while (true) {
        auto ret = fread(buffer, 1, sizeof(buffer), fp);
        if (ret == 0) {
            break;
        }
        *output_size += ret;
        MD5_Update(&context, buffer, ret);
    }
    fclose(fp);
    send_all_thread.join();
#else
#ifndef EMSCRIPTEN
    fd_set readfds, writefds, errorfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);
#endif

    int flags;
    while ((flags= fcntl(send_to_subprocess, F_GETFL, 0)) == -1
           && errno == EINTR){}
    while(fcntl(send_to_subprocess, F_SETFL, flags | O_NONBLOCK) == -1
          && errno == EINTR){}
    while ((flags = fcntl(recv_from_subprocess, F_GETFL, 0)) == -1
           && errno == EINTR){}
    while (fcntl(recv_from_subprocess, F_SETFL, flags | O_NONBLOCK) == -1
           && errno == EINTR){}
    size_t send_cursor = 0;
    bool finished = false;
    while(!finished) {
        int max_fd = 0;
        int ret = 1;
#ifndef EMSCRIPTEN
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&errorfds);
        if (send_to_subprocess != -1) {
            FD_SET(send_to_subprocess, &writefds);
            FD_SET(send_to_subprocess, &errorfds);
            if (send_to_subprocess + 1 > max_fd) {
                max_fd = send_to_subprocess + 1;
            }
        }
        if (recv_from_subprocess != -1) {
            FD_SET(recv_from_subprocess, &readfds);
            FD_SET(recv_from_subprocess, &errorfds);
            if (recv_from_subprocess + 1 > max_fd) {
                max_fd = recv_from_subprocess + 1;
            }
        }
        ret = select(max_fd, &readfds, &writefds, &errorfds, NULL);
#endif
        if (ret == 0 || (ret < 0 && errno == EINTR)) {
            continue;
        }
        if (recv_from_subprocess != -1
#ifndef EMSCRIPTEN
            && (FD_ISSET(recv_from_subprocess, &readfds) || FD_ISSET(recv_from_subprocess, &errorfds))
#endif
            ) {
            ssize_t del = read(recv_from_subprocess, &buffer[0], sizeof(buffer));
            if (del < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                // ignore
            } else if (del < 0) {
                break; // non recoverable error
            } else if (del == 0) {
                while (close(recv_from_subprocess) < 0 && errno == EINTR) {}
                recv_from_subprocess = -1;
                finished = true;
            } else if (del > 0) {
                MD5_Update(&context, &buffer[0], del);
                *output_size += del;
            }
        }
        if (send_to_subprocess != -1
#ifndef EMSCRIPTEN
            && (FD_ISSET(send_to_subprocess, &writefds) || FD_ISSET(send_to_subprocess, &errorfds))
#endif
            ) {
            ssize_t del = 0;
            if (send_cursor < data_size) {
                del = write(send_to_subprocess,
                            &data[send_cursor],
                            data_size - send_cursor);
            }
            //fprintf(stderr, "Want %ld bytes, but sent %ld\n", data_size - send_cursor,  del);
            if (del < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            } else if (del < 0) {
                break; // non recoverable error
            } else if (del > 0) {
                send_cursor += del;
                //fprintf(stderr, "len Storage is %ld/%ld\n", send_cursor, data_size);
            }
            if ((send_cursor == data_size || del == 0) && send_to_subprocess != -1) {
                while (close(send_to_subprocess) < 0 && errno == EINTR) {}
                send_to_subprocess = -1;
            }
        }
    }
#endif
    Sirikata::Array1d<uint8_t, 16> retval;
    MD5_Final(&retval[0], &context);
    return retval;
}
void md5_and_copy_to_tee(int copy_to_input_tee, int input_tee, MD5_CTX *context, size_t *input_size, size_t start_byte, size_t end_byte, bool close_input, bool is_socket, std::vector<uint8_t>*byte_return) {
    unsigned char buffer[65536];
    while (true) {
        size_t max_to_read = sizeof(buffer);
        if (end_byte != 0 && max_to_read > end_byte - *input_size) {
            max_to_read = end_byte - *input_size;
        }
        auto del = read(copy_to_input_tee, buffer, max_to_read);
        if (del == 0) {
            if (close_input) {
                while (close(copy_to_input_tee) < 0 && errno == EINTR) {}
            }
            break;
        }
        else if (del > 0) {
            if (end_byte != 0 && *input_size + del > end_byte && end_byte > *input_size) {
                always_assert(false && "UNREACHABLE"); // we limit by max_to_read
                del = end_byte - *input_size;
            }
            if (*input_size + del > start_byte) {
                if (*input_size >= start_byte) {
                    MD5_Update(context, &buffer[0], del);
					if (byte_return) {
						byte_return->insert(byte_return->end(), &buffer[0], &buffer[0] + del);
					}
                } else {
                    size_t offset = (start_byte - *input_size);
                    MD5_Update(context, &buffer[offset], del - offset);
					if (byte_return) {
						byte_return->insert(byte_return->end(), &buffer[offset], &buffer[offset] + (del-offset));
					}
				}
            }
            { // write all to the subprocess
                size_t cursor = 0;
                while (cursor < (size_t)del) {
                    auto wdel = write(input_tee, &buffer[cursor], del - cursor);
                    if (wdel > 0) {
                        cursor += wdel;
                    } else if (wdel < 0) {
                        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                            continue;
                        }
                        int whatis = errno;
                        fprintf(stderr, "ERR %d\n", whatis);
                        fflush(stderr);
                        perror("Subprocess exit");
                        fflush(stdout);
                        fflush(stderr);
                        del = 0;
						if (byte_return) {
							break;
						}
                        custom_exit(ExitCode::SHORT_READ);
                    } else {
						if (byte_return) {
							break;
						}
                        custom_exit(ExitCode::SHORT_READ);
                    }
                }
            }
            *input_size += del;
            if (end_byte != 0 && *input_size == end_byte) {
                if (close_input) {
                    while (close(copy_to_input_tee) < 0 && errno == EINTR) {}
                }
                copy_to_input_tee = -1;
                break;
            }
        }
        else if (!(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
            //fprintf(stderr,"%d) retry Err\n", copy_to_input_tee);
        }
        else {
            //fprintf(stderr, "Error %d\n", errno);
            break;
        }
    }
    while (close(input_tee) == -1 && errno == EINTR) {}
}
Sirikata::Array1d<uint8_t, 16> transfer_and_md5(Sirikata::Array1d<uint8_t, 2> header,
                                                size_t start_byte,
                                                size_t end_byte,
                                                bool send_header,
                                                int copy_to_input_tee, int input_tee,
                                                int copy_to_storage, size_t *input_size,
                                                Sirikata::MuxReader::ResizableByteBuffer *storage,
                                                std::vector<uint8_t> *byte_return,
                                                bool is_socket) {
    bool close_input = false;
    bool failed = false;
    MD5_CTX context;
    MD5_Init(&context);
    if (start_byte < header.size()) {
        MD5_Update(&context, &header[start_byte], header.size() - start_byte);
        if (byte_return) {
            byte_return->insert(byte_return->end(), &header[start_byte], &header[start_byte] + (header.size() - start_byte));
        }
    }
    if (send_header) {
        size_t offset = 0;
        do {
            auto delta = write(input_tee, &header[offset], header.size() - offset);
            if (delta <= 0) {
                if (delta < 0 && errno == EINTR) {
                    continue;
                } else {
                    if (byte_return) {
                        failed = true;
                        (void) failed;
                        break; // we can't simply exit if subprocess quits
                    }
                    custom_exit(ExitCode::OS_ERROR);
                }
            }
            offset += delta;
        } while (offset < header.size());
    }
    *input_size = header.size();
    uint8_t buffer[65536] = { 0 };
#ifdef _WIN32
    std::thread worker(std::bind(&md5_and_copy_to_tee,
        copy_to_input_tee, input_tee, &context, input_size, start_byte, end_byte, close_input, is_socket, byte_return));
#if 1
    while(!failed) {
        auto old_size = storage->size();
        if (storage->how_much_reserved() < old_size + 65536) {
            storage->reserve(storage->how_much_reserved() * 2);
        }
        storage->resize(storage->how_much_reserved());

        auto delta = read(copy_to_storage, storage->data() + old_size, storage->size() - old_size);
        if (delta < 0) {
            if (errno == EINTR) {
                storage->resize(old_size);
                continue;
            }
            if (byte_return) {
                failed = true;
                break;
            }
            custom_exit(ExitCode::SHORT_READ);
        }
        storage->resize(old_size + delta);
        if (delta == 0) {
            break;
        }
    }
#else
    FILE * fp = _fdopen(copy_to_storage, "rb");
    while (true) {
        auto old_size = storage->size();
        if (storage->how_much_reserved() < old_size + 65536) {
            storage->reserve(storage->how_much_reserved() * 2);
        }
        storage->resize(storage->how_much_reserved());
        auto ret = fread(storage->data() + old_size, 1, storage->size() - old_size, fp);
        storage->resize(old_size + ret);
        if (ret == 0) {
            break;
        }        
    }
    fclose(fp);
#endif
    worker.join();
#else
#ifndef EMSCRIPTEN
    fd_set readfds, writefds, errorfds;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);
#endif
    int max_fd = -1;
    int copy_to_input_tee_flags = 0;
    int input_tee_flags = 0;
    int copy_to_storage_flags = 0;
    while((input_tee_flags = fcntl(input_tee, F_GETFL, 0)) == -1
          && errno == EINTR){}
    while (fcntl(input_tee, F_SETFL, input_tee_flags | O_NONBLOCK) == -1
           && errno == EINTR){}
    while((copy_to_input_tee_flags = fcntl(copy_to_input_tee, F_GETFL, 0)) == -1
          && errno == EINTR){}
    while(fcntl(copy_to_input_tee, F_SETFL, copy_to_input_tee_flags | O_NONBLOCK) == -1
          && errno == EINTR) {}
    while ((copy_to_storage_flags = fcntl(copy_to_storage, F_GETFL, 0)) == -1
           && errno == EINTR){}
    while (fcntl(copy_to_storage, F_SETFL, copy_to_storage_flags | O_NONBLOCK) == -1
           && errno == EINTR) {}
    static_assert(sizeof(buffer) >= header.size(), "Buffer must be able to hold header");
    uint32_t cursor = 0;
    bool finished = false;
    bool input_fully_read = false;
    while (input_fully_read == false || !finished) {
#ifndef EMSCRIPTEN
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&errorfds);
#endif
        //fprintf(stderr, "Overarching loop\n");
        max_fd = 0;
        if (copy_to_storage != -1) {
#ifndef EMSCRIPTEN
            FD_SET(copy_to_storage, &readfds);
            FD_SET(copy_to_storage, &errorfds);
            if (copy_to_storage + 1 > max_fd) {
                max_fd = copy_to_storage + 1;
            }
#endif
        }
        if (copy_to_input_tee != -1) {
            if (cursor < sizeof(buffer)) {
#ifndef EMSCRIPTEN
                FD_SET(copy_to_input_tee, &readfds);
                FD_SET(copy_to_input_tee, &errorfds);
#endif
                if (copy_to_input_tee + 1 > max_fd) {
                    max_fd = copy_to_input_tee + 1;
                }
            }
        } else {
            // copy to input_tee is closed
            if (input_tee != -1 && cursor == 0) { // we copied everything here
                //fprintf(stderr, "CLosing %d\n", input_tee);
                while (close(input_tee) < 0 && errno == EINTR) {}
                input_tee = -1;
            }
        }
        if (input_tee != -1 && cursor != 0) {
#ifndef EMSCRIPTEN
            FD_SET(input_tee, &writefds);
            FD_SET(input_tee, &errorfds);
#endif
            if (input_tee + 1 > max_fd) {
                max_fd = input_tee + 1;
            }
        }
        //fprintf(stderr, "START POLL %d\n", max_fd);
        int ret = 1;
#ifndef EMSCRIPTEN
        ret = select(max_fd, &readfds, &writefds, &errorfds, NULL);
#endif
        //fprintf(stderr, "FIN POLL %d\n", ret);
        if (ret == 0 || (ret < 0 && errno == EINTR)) {
            continue;
        }
        //fprintf(stderr, "Checking ev\n");
        if (copy_to_input_tee != -1 && cursor < sizeof(buffer)
#ifndef EMSCRIPTEN
            && (FD_ISSET(copy_to_input_tee, &readfds) || FD_ISSET(copy_to_input_tee, &errorfds))
#endif
            ) {

            always_assert(cursor < sizeof(buffer)); // precondition to being in the poll
            size_t max_to_read = sizeof(buffer) - cursor;
            if (end_byte != 0 && max_to_read > end_byte - *input_size) {
                max_to_read = end_byte - *input_size;
            }
            ssize_t del = read(copy_to_input_tee, &buffer[cursor], max_to_read);
            if (del == 0) {
              input_fully_read = true;
              while (fcntl(copy_to_input_tee, F_SETFL, copy_to_input_tee_flags) == -1
                     && errno == EINTR){}
                if (close_input) {
                    //fprintf(stderr, "CLosing %d\n", copy_to_input_tee);
                    while (close(copy_to_input_tee) < 0 && errno == EINTR) {}
                }
                //fprintf(stderr,"input:Should close(%d) size:%ld\n", copy_to_input_tee, *input_size);
                copy_to_input_tee = -1;
            } else if (del > 0) {
                if (*input_size + del > start_byte) {
                    if (*input_size >= start_byte) {
                        MD5_Update(&context, &buffer[cursor], del);
                        if (byte_return) {
                            if (byte_return->size() + del > MAX_PERMISSIVE_LEPTON_SIZE) {
                                byte_return->clear();
                                byte_return = NULL;
                            } else {
                                byte_return->insert(byte_return->end(),
                                                    &buffer[cursor],
                                                    &buffer[cursor] + del);
                            }
                        }
                    } else {
                        size_t offset = (start_byte - *input_size);
                        MD5_Update(&context, &buffer[cursor + offset], del - offset);
                        if (byte_return) {
                            if (byte_return->size() + del > MAX_PERMISSIVE_LEPTON_SIZE) {
                                byte_return->clear();
                                byte_return = NULL;
                            } else {
                                byte_return->insert(byte_return->end(),
                                                    &buffer[cursor + offset],
                                                    &buffer[cursor + offset] + del - offset);
                            }
                        }
                    }
                }
                *input_size += del;
                cursor += del;
                if (end_byte != 0 && *input_size == end_byte) {
                  while (fcntl(copy_to_input_tee, F_SETFL, copy_to_input_tee_flags) == -1
                         && errno == EINTR){}
                    if (close_input) {
                        //fprintf(stderr, "CLosing %d\n", copy_to_input_tee);
                        while (close(copy_to_input_tee) < 0 && errno == EINTR) {}
                    }
                    //fprintf(stderr,"input:Should close(%d) size:%ld\n", copy_to_input_tee, *input_size);
                    input_fully_read = true;
                    copy_to_input_tee = -1;
                }
            } else if (!(errno == EINTR || errno == EWOULDBLOCK  || errno == EAGAIN)) {
                //fprintf(stderr,"%d) retry Err\n", copy_to_input_tee);
            } else {
                //fprintf(stderr, "Error %d\n", errno);
                break;
            }
            //fprintf(stderr,"%d) Reading %ld bytes for total of %ld\n", copy_to_input_tee, del, *input_size);
        }
        if (copy_to_storage != -1
#ifndef EMSCRIPTEN
            && (FD_ISSET(copy_to_storage, &readfds) || FD_ISSET(copy_to_storage, &errorfds))
#endif
            ) {
            if (storage->how_much_reserved() < storage->size() + sizeof(buffer)) {
                storage->reserve(storage->how_much_reserved() * 2);
            }
            size_t old_size = storage->size();
            storage->resize(storage->how_much_reserved());
            ssize_t del = read(copy_to_storage,
                               &(*storage)[old_size],
                               storage->size() - old_size);
            //fprintf(stderr, "Want %ld bytes, but read %ld\n", storage->size() - old_size,  del);
            if (del < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                //fprintf(stderr, "EAGAIN %d\n", errno);
                storage->resize(old_size);
            } else if (del < 0) {
                //fprintf(stderr, "Error %d\n", errno);
	      if (input_fully_read) {
                break; //can't break from the whole loop if subprocess unable to deliver
              }
            }else{
                if (del == 0) {
                    storage->resize(old_size);
                    finished = true;
                    //fprintf(stderr,"back_copy:Should close(%d):size %ld\n",copy_to_storage,storage->size());
                    //fprintf(stderr, "CLosing %d\n", copy_to_storage);
                    while (close(copy_to_storage) < 0 && errno == EINTR) {}
                    copy_to_storage = -1;
                }

                if (del > 0) {
                    storage->resize(old_size + del);
                    //fprintf(stderr, "len Storage is %ld\n", storage->size());
                }
            }
        }
        if (input_tee != -1 && cursor != 0
#ifndef EMSCRIPTEN
            && (FD_ISSET(input_tee, &writefds) || FD_ISSET(input_tee, &errorfds))
#endif
            ) {
            always_assert (cursor != 0);//precondition to being in the pollfd set
            ssize_t del = write(input_tee, buffer, cursor);
            //fprintf(stderr, "fd: %d: Writing %ld data to %d\n", input_tee, del, cursor);
            //fprintf(stderr, "A");
            if (del == 0) {
                //fprintf(stderr, "B\n");
                while (close(input_tee) < 0 && errno == EINTR) {}
                //fprintf(stderr,"output_to_compressor:Should close (%d) \n", input_tee );
                input_tee = -1;
            }
            //fprintf(stderr, "C\n");
            if (del > 0) {
                //fprintf(stderr, "D\n");
                if (del < cursor) {
                    //fprintf(stderr, "E %ld %ld\n", del, cursor - del);
                    memmove(buffer, buffer + del, cursor - del);
                }
                cursor -= del;
            }
            if (del < 0) {
                while (close(input_tee) < 0 && errno == EINTR) {}
                //fprintf(stderr,"output_to_compressor:Should close (%d) \n", input_tee );
                input_tee = -1;
            }
        }
	if (input_tee == -1) {
          cursor = 0;
	}
        if (cursor == 0 && copy_to_input_tee == -1 && input_tee != -1) {
            //fprintf(stderr,"E\n");
            //fprintf(stderr, "CLosing %d\n", input_tee);
            while (close(input_tee) < 0 && errno == EINTR) {}
            //fprintf(stderr,"output_to_compressor:Should close (%d) \n", input_tee );
            input_tee = -1;
        }
        //fprintf(stderr,"F\n");
    }
    // reset the nonblocking nature of the fd
    if (input_tee != -1) {
      while (fcntl(input_tee, F_SETFL, input_tee_flags) == -1 &&
             errno == EINTR){}
    }
    if (copy_to_storage != -1) {
      while (fcntl(copy_to_storage, F_SETFL, copy_to_storage_flags) == -1 &&
             errno == EINTR){}
    }
#endif

    *input_size -= start_byte;
    Sirikata::Array1d<uint8_t, 16> retval;
    MD5_Final(&retval[0], &context);
    return retval;
}
void discard_stderr(int fd) {
    char buffer[4097];
    buffer[sizeof(buffer) - 1] = '\0';
    while (true) {
        auto del = read(fd, buffer, sizeof(buffer) - 1);
        if (del <= 0) {
            if (del < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
        buffer[del] = '\0';
        fprintf(stderr, "%s", buffer);
    }
}

#if 1//def __APPLE__
std::mutex subprocess_lock;
#endif

SubprocessConnection start_subprocess(int argc, const char **argv, bool pipe_stderr, bool stderr_to_nul) {
#if 1//def __APPLE__
    std::lock_guard<std::mutex> lok(subprocess_lock);
#endif
    SubprocessConnection retval;
    memset(&retval, 0, sizeof(retval));
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    HANDLE hChildStd_IN_Rd;
    HANDLE hChildStd_IN_Wr;

    HANDLE hChildStd_OUT_Rd;
    HANDLE hChildStd_OUT_Wr;

    HANDLE hChildStd_ERR_Rd;
    HANDLE hChildStd_ERR_Wr;
    bool simpler = true;
    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        custom_exit(ExitCode::OS_ERROR);
    }
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        custom_exit(ExitCode::OS_ERROR);
    }
    if (pipe_stderr || !simpler) {
        if (!CreatePipe(&hChildStd_ERR_Rd, &hChildStd_ERR_Wr, &saAttr, 0)) {
            custom_exit(ExitCode::OS_ERROR);
        }
        if (!SetHandleInformation(hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0)) {
            custom_exit(ExitCode::OS_ERROR);
        }
    }
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) {
        custom_exit(ExitCode::OS_ERROR);
    }
    if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
        custom_exit(ExitCode::OS_ERROR);
    }
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    memset(&siStartInfo, 0, sizeof(siStartInfo));
    memset(&piProcInfo, 0, sizeof(piProcInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);

    if (pipe_stderr || !simpler) {
        siStartInfo.hStdError = hChildStd_ERR_Wr;
    } else {
        if (!stderr_to_nul) {
            siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        }
    }
    siStartInfo.hStdOutput = hChildStd_OUT_Wr;
    siStartInfo.hStdInput = hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    std::vector<char> command_line;
    for (int i = 0; i < argc; ++i) {
        if (i == 0) {
            command_line.push_back('\"');
        }
        else {
            command_line.push_back(' ');
        }
        command_line.insert(command_line.end(), argv[i], argv[i] + strlen(argv[i]));
        if (i == 0) {
            command_line.push_back('\"');
        }
    }
    command_line.push_back('\0');
    if (!CreateProcess(argv[0],
        &command_line[0],
        NULL, // process security attributes
        NULL, // primary thread security attributes,
        TRUE, // handles inherited,
        0, // flags,
        NULL, // use parent environment,
        NULL, // use current dir,
        &siStartInfo,
        &piProcInfo)) {
        fprintf(stderr, "Failed To start subprocess with command line ", command_line);
        custom_exit(ExitCode::OS_ERROR);
    }
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    if (pipe_stderr || !simpler) {
        CloseHandle(hChildStd_ERR_Wr);
        while ((retval.pipe_stderr = _open_osfhandle((intptr_t)hChildStd_ERR_Rd,
            O_APPEND | O_RDONLY)) == -1 && errno == EINTR) {
        }
    } else {
        retval.pipe_stderr = -1;
    }
    if (simpler == false && !pipe_stderr) {
        std::thread discard_stderr_t(std::bind(&discard_stderr, retval.pipe_stderr));
        discard_stderr_t.detach();
        retval.pipe_stderr = -1;
    }
    CloseHandle(hChildStd_OUT_Wr);
    while ((retval.pipe_stdout = _open_osfhandle((intptr_t)hChildStd_OUT_Rd,
        O_APPEND | O_RDONLY)) == -1 && errno == EINTR) {
    }
    CloseHandle(hChildStd_IN_Rd);
    while ((retval.pipe_stdin = _open_osfhandle((intptr_t)hChildStd_IN_Wr,
        O_APPEND | O_WRONLY)) == -1 && errno == EINTR) {
    }
#else
    int stdin_pipes[2] = { -1, -1 };
    int stdout_pipes[2] = { -1, -1 };
    int stderr_pipes[2] = { -1, -1 };
    {
#ifdef __APPLE__
        while (pipe(stdin_pipes) < 0 && errno == EINTR) {}
        while (fcntl(stdin_pipes[0], F_SETFD, FD_CLOEXEC) < 0 && errno == EINTR) {}
        while (fcntl(stdin_pipes[1], F_SETFD, FD_CLOEXEC) < 0 && errno == EINTR) {}
        while (pipe(stdout_pipes) < 0 && errno == EINTR) {}
        while (fcntl(stdout_pipes[0], F_SETFD, FD_CLOEXEC) < 0 && errno == EINTR) {}
        while (fcntl(stdout_pipes[1], F_SETFD, FD_CLOEXEC) < 0 && errno == EINTR) {}
        if (pipe_stderr) {
            while (pipe(stderr_pipes) < 0 && errno == EINTR) {}
            while (fcntl(stderr_pipes[0], F_SETFD, FD_CLOEXEC) < 0 && errno == EINTR) {}
            while (fcntl(stderr_pipes[1], F_SETFD, FD_CLOEXEC) < 0 && errno == EINTR) {}
        }
#else
        while (pipe2(stdin_pipes, O_CLOEXEC) < 0 && errno == EINTR) {}
        while (pipe2(stdout_pipes, O_CLOEXEC) < 0 && errno == EINTR) {}
        if (pipe_stderr) {
            while (pipe2(stderr_pipes, O_CLOEXEC) < 0 && errno == EINTR) {}
        }
#endif
    }
    if ((retval.sub_pid = fork()) == 0) {
        while (close(stdin_pipes[1]) == -1 && errno == EINTR) {}
        while (close(stdout_pipes[0]) == -1 && errno == EINTR) {}
        if (pipe_stderr) {
            while (close(stderr_pipes[0]) == -1 && errno == EINTR) {}
        }
        while (close(0) == -1 && errno == EINTR) {}

        while (dup2(stdin_pipes[0], 0) == -1 && errno == EINTR) {}

        while (close(1) == -1 && errno == EINTR) {}
        while (dup2(stdout_pipes[1], 1) == -1 && errno == EINTR) {}
        if (pipe_stderr) {
            while (close(2) == -1 && errno == EINTR) {}
            while (dup2(stderr_pipes[1], 2) == -1 && errno == EINTR) {}
        }
        if (stderr_to_nul) {
            while (close(2) == -1 && errno == EINTR) {}
            int devnull;
            while ((devnull = open("/dev/null", O_RDWR, S_IWUSR | S_IRUSR)) == -1 && errno == EINTR) {
            }
            if (devnull != -1) {
                while (dup2(devnull, 2) == -1 && errno == EINTR) {}
            }
        }
        std::vector<char*> args(argc + 1);
        for (int i = 0; i < argc; ++i) {
            args[i] = (char*)argv[i];
        }
        args[argc] = NULL;
        execvp(args[0], &args[0]);
    }
    while (close(stdin_pipes[0]) == -1 && errno == EINTR) {}
    while (close(stdout_pipes[1]) == -1 && errno == EINTR) {}
    if (pipe_stderr) {
        while (close(stderr_pipes[1]) == -1 && errno == EINTR) {}
    }
    retval.pipe_stdin = stdin_pipes[1];
    retval.pipe_stdout = stdout_pipes[0];
    retval.pipe_stderr = stderr_pipes[0];
#endif
    return retval;
}

}
