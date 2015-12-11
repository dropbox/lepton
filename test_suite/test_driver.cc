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
#include "smalljpg.hh"
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
enum {
    MAX_ARGS=32
};
void print_args(const char*const*const args) {
    for (int i = 0; i < MAX_ARGS; ++i) {
        if (args[i]) {
            fprintf(stderr, "%s ",args[i]);
        }else {
            fprintf(stderr, "(NULL)\n");
            break;
        }
    }
}
int get_last_arg(const char*const*const args) {
    for (int i = 0; i < MAX_ARGS; ++i) {
        if (args[i] == NULL) {
            return i;
        }
    }
    assert(false && "Args must be null termintaed");
    return MAX_ARGS - 1;
}
int run_test(const std::vector<unsigned char> &testImage,
             bool use_lepton, bool jailed, int inject_failure_level,
             bool expect_failure, bool expect_decoder_failure, const char* memory, const char* thread_memory) {
/*
    pid_t subfork;
    subfork = fork();
    if (subfork) {
        int status;
        waitpid(subfork, &status, 0);
        return WEXITSTATUS(status);
        }*/
    if (expect_failure || expect_decoder_failure) {
        signal(SIGPIPE, SIG_IGN);
    }
    const char * encode_args[MAX_ARGS] = {"./lepton", "-timing=test_timing", NULL};
    const char * decode_args[MAX_ARGS] = {"./lepton", NULL};
    if (!use_lepton) {
        encode_args[get_last_arg(encode_args)] = "-ujg";
    }
    if (inject_failure_level) {
        const char ** which_args = encode_args;
        if ((inject_failure_level == 3 || inject_failure_level == 4)) {
            which_args = decode_args;
        }
        fprintf(stderr, "Inject failure level is %d so is it encode args? %d or decode_args? %d\n",
                inject_failure_level, (which_args == encode_args), (which_args == decode_args));
        which_args[get_last_arg(which_args)]
            = (inject_failure_level & 1) ? "-injectsyscall=1" : "-injectsyscall=2";
    }
    if (!jailed) {
        encode_args[get_last_arg(encode_args)] = "-unjailed";
        decode_args[get_last_arg(decode_args)] = "-unjailed";
    }
    if (memory) {
        encode_args[get_last_arg(encode_args)] = memory;
        decode_args[get_last_arg(decode_args)] = memory;
    }
    if (thread_memory) {
        encode_args[get_last_arg(encode_args)] = thread_memory;
        decode_args[get_last_arg(decode_args)] = thread_memory;
    }
    encode_args[get_last_arg(encode_args)] = "-";
    decode_args[get_last_arg(decode_args)] = "-";
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
    FILE * fp = fopen("lepton", "r");
    if (fp) {
        fclose(fp);
    } else {
        exit(1); // file does not exist
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
        print_args(encode_args);
        execvp("./lepton", (char *const*)encode_args);
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
        print_args(decode_args);
        execvp("./lepton", (char *const*)decode_args);
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
    fprintf(stderr, "%d vs %d\n", expect_failure, expect_decoder_failure);
    if (expect_failure) {
        if (encoder_exit) {
            fprintf(stderr, "But this is expected since we do not support this image type yet.\n"
                    "Failed on encode with code %d\n", WEXITSTATUS(encoder_exit));
        }
        return encoder_exit != 0 ? 0 : 1;
    }
    if (expect_decoder_failure == false && (size_t)ret != testImage.size()) {
        return(2);
    }

    fprintf(stderr, "Uncompressed size %ld, Compressed size %ld\n", testImage.size(), size);
    if (expect_decoder_failure == false && !fileSame) {
        return(3);
    }
    int decoder_exit = 1;
    waitpid(decoder_pid, &decoder_exit, 0);
    do {
        status = close(decode_stdout[0]);
    } while (status == -1 && errno == EINTR);
    fprintf(stderr, "EXIT STATUS %d %d\n", WEXITSTATUS(encoder_exit), WEXITSTATUS(encoder_exit));
    if (expect_decoder_failure) {
        if (decoder_exit) {
            fprintf(stderr, "But this is expected since we were making sure the decoder did fail.\n"
                    "Failed on decode with code %d\n", WEXITSTATUS(encoder_exit));
        }
        return decoder_exit != 0 ? 0 : 1;
    }
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
int test_file(int argc, char **argv, bool use_lepton, bool jailed, int inject_syscall_level,
              const std::vector<const char *> &filenames, bool expect_encode_failure, bool expect_decode_failure,
              const char* memory, const char* thread_memory) {
    assert(argc > 0);
    for (int i = int(strlen(argv[0])) - 1; i > 0; --i) {
        if (argv[0][i] == '/' || argv[0][i] == '\\') {
            argv[0][i] = '\0';
            int status = 0;
            do {
                status = chdir(argv[0]);
            } while (status == -1 && errno == EINTR);
            do {
                status = chdir("..");
            } while (status == -1 && errno == EINTR);
            break;
        }
    }
    std::vector<unsigned char> testImage(abstractJpeg, abstractJpeg+sizeof(abstractJpeg));
    if (filenames.empty()) {
        return run_test(testImage, use_lepton, jailed, inject_syscall_level, expect_encode_failure, expect_decode_failure, memory, thread_memory);
    }
    for (std::vector<const char *>::const_iterator filename = filenames.begin(); filename != filenames.end(); ++filename) {
        testImage = load(*filename);
        fprintf(stderr, "Loading iPhone %ld\n", testImage.size());
        int retval = run_test(testImage, use_lepton, jailed, inject_syscall_level,
                              expect_encode_failure, expect_decode_failure, memory, thread_memory);
        if (retval) {
            return retval;
        }
    }
    return 0;
}
