/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <thread>
#include <vector>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include "smalljpeg.hh"
ssize_t read_until(int fd, void *buf, size_t size) {
    size_t progress = 0;
    while (progress < size) {
        ssize_t status = read(fd, (char*)buf + progress, size - progress);
        if (status == 0) { // EOF
            return progress;
        }
        if (status == -1) {
            if (errno != EINTR) {
                if (progress == 0) {
                        return -1;
                }else {
                    return progress;
                }
            }
        } else {
            progress += status;
        }
    }
    return progress;
}

ssize_t write_until(int fd, const void *buf, size_t size) {
    size_t progress = 0;
    while (progress < size) {
        ssize_t status = write(fd, (char*)buf + progress, size - progress);
        if (status == -1) {
            if (status == 0) { // EOF
                return 0;
            }
            if (errno != EINTR) {
                return -1;
            }
        } else {
            progress += status;
        }
    }
    return progress;
}
void pipe_between(int output, int input, size_t *out_size) {
    char buffer[BUFSIZ];
    *out_size = 0;
    while(true) {
        ssize_t input_size = read_until(output, buffer, BUFSIZ);
        if (input_size > 0) {
            size_t osize = write_until(input, buffer, input_size);
            assert(osize > 0);
            *out_size += osize;
        }
        if (input_size < BUFSIZ) { // we're done reading
            break;
        }
    }
    int status = -1;
    do {
        status = close(input);
    } while (status == -1 && errno == EINTR);
}
void check_out(int output, const unsigned char *data, size_t data_size, bool *ok) {
    std::vector<unsigned char> roundtrip(BUFSIZ);
    size_t roundtrip_size = 0;
    *ok = false;
    while(true) {
        size_t cur_read = read_until(output, roundtrip.data() + roundtrip_size, roundtrip.size() - roundtrip_size);
        if (cur_read > 0) {
            roundtrip_size += cur_read;
            roundtrip.resize(roundtrip.size() + BUFSIZ);
        }
        if (cur_read == 0) {
            break;
        }
    }
    if (data_size == roundtrip_size) {
        if (memcmp(roundtrip.data(), data, data_size) == 0) {
            *ok = true;
        }
    }
}
int main() {
    int encode_stdin[2];
    int encode_stdout[2];
    int status = pipe(encode_stdin);
    if (status != 0) {
        return status;
    }
    status = pipe(encode_stdout);
    if (status != 0) {
        return status;
    }
    pid_t encoder_pid = fork();
    if (encoder_pid == 0) {
        fclose(stdin);
        do {
            status = dup(encode_stdin[0]);
        } while (status == -1 && errno == EINTR);
        assert(status >= 0);
        do {
            status = close(encode_stdin[1]);
        } while (status == -1 && errno == EINTR);
        fclose(stdout);
        do {
            status = dup(encode_stdout[1]);
        } while (status == -1 && errno == EINTR);
        assert(status >= 0);
        do {
            status = close(encode_stdout[0]);
        } while (status == -1 && errno == EINTR);
        execlp("./lepton", "-s", "-", NULL);
    }
    do {
        status = close(encode_stdin[0]);
    } while (status == -1 && errno == EINTR);
    do {
        status = close(encode_stdout[1]);
    } while (status == -1 && errno == EINTR);

    int decode_stdin[2];
    int decode_stdout[2];
    status = pipe(decode_stdin);
    if (status != 0) {
        return status;
    }
    status = pipe(decode_stdout);
    if (status != 0) {
        return status;
    }

    pid_t decoder_pid = fork();
    if (decoder_pid == 0) {
        fclose(stdin);
        int status = -1;
        do {
            status = close(encode_stdin[1]);
        } while (status == -1 && errno == EINTR);

        do {
            status = close(encode_stdout[0]);
        } while (status == -1 && errno == EINTR);
        do {
            status = dup(decode_stdin[0]);
        } while (status == -1 && errno == EINTR);
        assert(status >= 0 && "WTF");
        close(decode_stdin[1]);
        fclose(stdout);
        do {
            status = dup(decode_stdout[1]);
        } while (status == -1 && errno == EINTR);
        assert(status >= 0 && "WTF");
        close(decode_stdout[0]);
        execlp("./lepton", "-s", "-", NULL);
    }
    do {
        status = close(decode_stdin[0]);
    } while (status == -1 && errno == EINTR);
    do {
        status = close(decode_stdout[1]);
    } while (status == -1 && errno == EINTR);
    bool fileSame = false;
    size_t size = 0;
    std::thread first_pipe(std::bind(&pipe_between, encode_stdout[0], decode_stdin[1], &size));
    std::vector<unsigned char> testImage(abstractJpeg, abstractJpeg+sizeof(abstractJpeg));
    std::thread round_trip(std::bind(&check_out, decode_stdout[0], testImage.data(), testImage.size(), &fileSame));
    ssize_t ret = write_until(encode_stdin[1], testImage.data(), testImage.size());
    if (ret != sizeof(abstractJpeg)) {
        return 2;
    }
    do {
        status = close(encode_stdin[1]);
    } while (status == -1 && errno == EINTR);

    first_pipe.join();
    round_trip.join();
    fprintf(stderr, "Uncompressed size %ld, Compressed size %ld\n", testImage.size(), size);
    if (!fileSame) {
        return 3;
    }
    int encoder_exit = 1;
    int decoder_exit = 1;
    waitpid(encoder_pid, &encoder_exit, 0);
    waitpid(decoder_pid, &decoder_exit, 0);
    if (encoder_exit) {
        return encoder_exit;
    }
    return decoder_exit;
}
