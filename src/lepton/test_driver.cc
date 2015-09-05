/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <thread>
#include <string>
#include <vector>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
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
                return progress;
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
    int status;
    do {
        status = close(input);
    } while (status == -1 && errno == EINTR);
    do {
        status = close(output);
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
        } else {
            fprintf(stderr, "Files differ in their contents\n");
        }
    } else {
        fprintf(stderr, "Files differ in size %ld != %ld\n", data_size, roundtrip_size);
    }
    int status;
    do {
        status = close(output);
    } while (status == -1 && errno == EINTR);

}

int run_test(const std::vector<unsigned char> &testImage, bool use_lepton, bool expect_failure) {
/*
    pid_t subfork;
    subfork = fork();
    if (subfork) {
        int status;
        waitpid(subfork, &status, 0);
        return WEXITSTATUS(status);
        }*/

    int encode_stdin[2];
    int encode_stdout[2];
    int status = pipe(encode_stdin);
    if (status != 0) {
        return(status);
    }
    status = pipe(encode_stdout);
    if (status != 0) {
        return(status);
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
        if (use_lepton) {
            execlp("./lepton", "./lepton", "-s", "-timing=test_timing", "-", NULL);
        } else {
            execlp("./lepton", "./lepton", "-ujg", "-s", "-", NULL);
        }
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
        return(status);
    }
    status = pipe(decode_stdout);
    if (status != 0) {
        return(status);
    }

    pid_t decoder_pid = fork();
    if (decoder_pid == 0) {

        fclose(stdin);
        do {
            status = dup(decode_stdin[0]);
        } while (status == -1 && errno == EINTR);
        assert(status >= 0);
        close(decode_stdin[1]);
        fclose(stdout);
        do {
            status = dup(decode_stdout[1]);
        } while (status == -1 && errno == EINTR);
        assert(status >= 0);
        close(decode_stdout[0]);

        int status = -1;
        do {
            status = close(encode_stdin[1]);
        } while (status == -1 && errno == EINTR);

        do {
            status = close(encode_stdout[0]);
        } while (status == -1 && errno == EINTR);

        execlp("./lepton", "./lepton", "-s", "-", NULL);
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

    std::thread round_trip(std::bind(&check_out, decode_stdout[0], testImage.data(), testImage.size(), &fileSame));
    ssize_t ret = write_until(encode_stdin[1], testImage.data(), testImage.size());
    do {
        status = close(encode_stdin[1]); // close the stream we're encoding
        // need to put the threads into a joinable state
    } while (status == -1 && errno == EINTR);
    first_pipe.join();
    round_trip.join();
    int encoder_exit = 1;
    waitpid(encoder_pid, &encoder_exit, 0);
    if (expect_failure) {
        if (WEXITSTATUS(encoder_exit)) {
            fprintf(stderr, "But this is expected since we do not support this image type yet.\n"
                    "Failed on encode with code %d\n", WEXITSTATUS(encoder_exit));
        }
        return WEXITSTATUS(encoder_exit) != 0 ? 0 : 1;
    }
    if ((size_t)ret != testImage.size()) {
        return(2);
    }

    fprintf(stderr, "Uncompressed size %ld, Compressed size %ld\n", testImage.size(), size);
    if (!fileSame) {
        return(3);
    }
    int decoder_exit = 1;
    waitpid(decoder_pid, &decoder_exit, 0);
    do {
        status = close(decode_stdout[0]);
    } while (status == -1 && errno == EINTR);
    fprintf(stderr, "EXIT STATUS %d %d\n", WEXITSTATUS(encoder_exit), WEXITSTATUS(encoder_exit));
    if (WEXITSTATUS(encoder_exit)) {
        return(WEXITSTATUS(encoder_exit));
    }
    return(WEXITSTATUS(decoder_exit));

}
std::vector<unsigned char> load(const char *filename) {
    /* check if srcdir is defined, in which case prepend */
    std::string full_filename;
    const char * const srcdir = getenv( "srcdir" );
    if ( srcdir ) {
        full_filename += srcdir;
        full_filename += "/";
    }
    full_filename += filename;

    FILE * fp = fopen(full_filename.c_str(), "rb");
    if ( not fp ) {
        perror( "fopen" );
        return {};
    }
    fseek(fp, 0, SEEK_END);
    size_t where = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<unsigned char> retval(where);
    size_t readd = fread(retval.data(), where, 1, fp);
    (void)readd;
    assert(readd == 1);
    fclose(fp);
    return retval;
}
int test_file(int argc, char **argv, bool use_lepton, const std::vector<const char *> &filenames,
              bool expect_failure) {
    assert(argc > 0);
    for (int i = int(strlen(argv[0])) - 1; i > 0; --i) {
        if (argv[0][i] == '/' || argv[0][i] == '\\') {
            argv[0][i] = '\0';
            int status = 0;
            do {
                status = chdir(argv[0]);
            } while (status == -1 && errno == EINTR);
            break;
        }
    }
    std::vector<unsigned char> testImage(abstractJpeg, abstractJpeg+sizeof(abstractJpeg));
    if (filenames.empty()) {
        return run_test(testImage, use_lepton, expect_failure);
    }
    for (std::vector<const char *>::const_iterator filename = filenames.begin(); filename != filenames.end(); ++filename) {
        testImage = load(*filename);
        fprintf(stderr, "Loading iPhone %ld\n", testImage.size());
        int retval = run_test(testImage, use_lepton, expect_failure);
        if (retval) {
            return retval;
        }
    }
    return 0;
}
