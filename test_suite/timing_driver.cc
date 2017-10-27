/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <thread>
#include <string>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/time.h>
#include "../src/lepton/smalljpg.hh"

#define always_assert(val) do { if (!(val)) {fprintf(stderr, "%s:%d: %s", __FILE__, __LINE__, #val); abort();} } while(false)

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
        if (status == 0) { // EOF
            return progress;
        }
        if (status == -1) {
            if (errno != EINTR) {
                return -1;
            }
        } else {
            progress += status;
        }
    }
    return progress;
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
        fprintf(stderr,
                "Files differ in size %lu != %lu\n",
                (unsigned long)data_size,
                (unsigned long)roundtrip_size);
    }
    int status;
    do {
        status = close(output);
    } while (status == -1 && errno == EINTR);

}
enum {
    MAX_ARGS=33
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
    always_assert(false && "Args must be null termintaed");
    return MAX_ARGS - 1;
}

ssize_t get_uncompressed_image_size(const unsigned char * indata,
                                   const size_t size) {
    if (size < 24) {
        return -1;
    }
    ssize_t retval = indata[23];
    retval *= 256;
    retval += indata[22];
    retval *= 256;
    retval += indata[21];
    retval *= 256;
    retval += indata[20];
    return retval;
}

ssize_t write_close_read_until(int input_fd,
                               const unsigned char *indata,
                               size_t indata_size,
                               int output_fd,
                               unsigned char *outdata,
                               ssize_t outdata_max_size) {
    int flags = fcntl(input_fd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }
    flags |= O_NONBLOCK;
    if (fcntl(input_fd, F_SETFL, flags) < 0) {
        return -1;
    }
    ssize_t out_size = 0;
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    while(outdata_max_size) {
        if (!indata_size) { // all input consumed, simply read()
            ssize_t data_read = read_until(output_fd, outdata, outdata_max_size);
            if (data_read >= 0) {
                return out_size + data_read;
            }
            return out_size;
        }
        FD_ZERO(&rfds);
        FD_ZERO(&efds);
        FD_ZERO(&wfds);
        FD_SET(output_fd, &rfds);
        FD_SET(output_fd, &efds);
        FD_SET(input_fd, &wfds);
        FD_SET(input_fd, &efds);

        int err = select(std::max(input_fd, output_fd) + 1, &rfds, &wfds, &efds, NULL);
        if (err < 0) {
            perror("Select");
            return out_size;
        }
        if (indata_size && FD_ISSET(input_fd, &wfds)) {
            ssize_t data_written = write(input_fd, indata, indata_size);
            if (data_written > 0) {
                indata_size -= data_written;
                indata += data_written;
            }
            if (indata_size == 0) {
                while(close(input_fd) < 0 && errno == EINTR) {
                }
            }
        }
        if (FD_ISSET(output_fd, &rfds)) {
            ssize_t data_read = read(output_fd, outdata, outdata_max_size);
            if (data_read > 0) {
                outdata_max_size -= data_read;
                outdata += data_read;
                out_size += data_read;
                if (outdata_max_size == 0) {
                    return out_size;
                }
            }
            if (data_read == 0) {
                return out_size;
            }
        }
        if (FD_ISSET(output_fd, &efds)) {
            if (out_size > 0) {
                return out_size;
            }
            return -1;
        }
    }
    return out_size;
}
double get_cur_time_double() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double retval = tv.tv_usec;
    retval *= 0.000001;
    retval += tv.tv_sec;
    return retval;
}
void sleep_a_bit() {
    usleep(250000); // sleep 1/4 second
}
int run_test(const std::vector<unsigned char> &testImage,
             bool use_lepton, bool jailed, int inject_failure_level, int allow_progressive_files,
             bool multithread,
             bool expect_failure, bool expect_decoder_failure,
             const char* encode_memory, const char *decode_memory, const char * singlethread_recode_memory, const char* thread_memory,
             bool use_brotli,
             bool force_no_ans) {
    std::vector<unsigned char> leptonBuffer(use_lepton ? testImage.size()
                                           : testImage.size() * 40 + 4096 * 1024);
    std::vector<unsigned char> roundtripBuffer(testImage.size());

    if (expect_failure || expect_decoder_failure) {
        signal(SIGPIPE, SIG_IGN);
    }
    const char * encode_args[MAX_ARGS] = {"./lepton", "-hugepages", "-decode", NULL};
    const char * decode_args[MAX_ARGS] = {"./lepton", "-hugepages", "-recode", NULL};
    if (!use_lepton) {
        encode_args[get_last_arg(encode_args)] = "-ujg";
    }
    unsigned char expected_ujg_version = 1;
    if (use_brotli) {
        encode_args[get_last_arg(encode_args)] = "-brotliheader";
        expected_ujg_version = 2;
    }
#ifdef ENABLE_ANS_EXPERIMENTAL
    if (!force_no_ans) {
        encode_args[get_last_arg(encode_args)] = "-ans";
        expected_ujg_version = 3;
    }
#endif
    if (inject_failure_level) {
        const char ** which_args = encode_args;
        if ((inject_failure_level == 3 || inject_failure_level == 4)) {
            which_args = decode_args;
        }
        fprintf(stderr, "Inject failure level is %d so is it encode args? %d or decode_args? %d\n",
                inject_failure_level, (which_args == encode_args), (which_args == decode_args));
        which_args[get_last_arg(which_args)]
            = (inject_failure_level & 1) ? "-injectsyscall=1" : "-injectsyscall=2";
        encode_args[get_last_arg(encode_args)] = "-singlethread";
        decode_args[get_last_arg(decode_args)] = "-singlethread";
    } else if (!multithread) {
        encode_args[get_last_arg(encode_args)] = "-singlethread";
        decode_args[get_last_arg(decode_args)] = "-singlethread";
    } else {
        encode_args[get_last_arg(encode_args)] = "-minencodethreads=8";
    }
    if (!jailed) {
        encode_args[get_last_arg(encode_args)] = "-unjailed";
        decode_args[get_last_arg(decode_args)] = "-unjailed";
    }
    if (rand() < RAND_MAX / 2) {
        encode_args[get_last_arg(encode_args)] = "-defermd5";
    }
    if (allow_progressive_files == 1) {
        encode_args[get_last_arg(encode_args)] = "-allowprogressive";
        decode_args[get_last_arg(decode_args)] = "-allowprogressive";
    }
    if (allow_progressive_files == -1) {
        encode_args[get_last_arg(encode_args)] = "-rejectprogressive";
        decode_args[get_last_arg(decode_args)] = "-rejectprogressive";
    }
    if (encode_memory) {
        encode_args[get_last_arg(encode_args)] = encode_memory;
    }
    if (decode_memory) {
        decode_args[get_last_arg(decode_args)] = decode_memory;
    }
    if (thread_memory) {
        encode_args[get_last_arg(encode_args)] = thread_memory;
        decode_args[get_last_arg(decode_args)] = thread_memory;
    }
    if (singlethread_recode_memory) {
        encode_args[get_last_arg(encode_args)] = singlethread_recode_memory;
        decode_args[get_last_arg(decode_args)] = singlethread_recode_memory;
    } else {
        encode_args[get_last_arg(encode_args)] = "-recodememory=24M";
        decode_args[get_last_arg(decode_args)] = "-recodememory=24M";
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
        always_assert(status >= 0);
        do {
            status = close(encode_stdin[1]);
        } while (status == -1 && errno == EINTR);
        fclose(stdout);
        do {
            status = dup(encode_stdout[1]);
        } while (status == -1 && errno == EINTR);
        always_assert(status >= 0);
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
        always_assert(status >= 0);
        close(decode_stdin[1]);
        fclose(stdout);
        do {
            status = dup(decode_stdout[1]);
        } while (status == -1 && errno == EINTR);
        always_assert(status >= 0);
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
    bool presend_header = false;
    if (!presend_header) {
        sleep_a_bit();
    }
    double startEncode = get_cur_time_double();
    ssize_t ret = write_until(encode_stdin[1], testImage.data(), 2);
    if (!expect_failure) {
        always_assert(ret == 2 && "Input program must at least accept the 2 byte header before block");
    }
    if (presend_header) {
        sleep_a_bit();
        startEncode = get_cur_time_double();
    }
    always_assert(testImage.size() > 2);
    ret = write_close_read_until(encode_stdin[1],
                                         testImage.data() + 2, testImage.size() - 2,
                                         encode_stdout[0],
                                         leptonBuffer.data(), leptonBuffer.size());
    double stopEncode = get_cur_time_double();
    int encoder_exit = 1;
    waitpid(encoder_pid, &encoder_exit, 0);
    double encodeShutdown = get_cur_time_double();
    fprintf(stderr, "Timing encode: %f encode process exit: %f\n",
            stopEncode - startEncode, encodeShutdown - startEncode);
    if (expect_failure) {
        if (encoder_exit) {
            fprintf(stderr, "But this is expected since we were making sure the encoder did fail.\n"
                    "Failed on encode with code %d\n", WEXITSTATUS(encoder_exit));
        }
        return encoder_exit != 0 ? 0 : 1;
    }
    always_assert(ret > 0);
    leptonBuffer.resize(ret);
    if (leptonBuffer.size() > 2) {
        always_assert(leptonBuffer.data()[2] == expected_ujg_version);
    }
    if (use_lepton) {
        size_t result = get_uncompressed_image_size(leptonBuffer.data(),
                                                    leptonBuffer.size());
        if (result != testImage.size()) {
            fprintf(stderr, "Output Size %ld != %ld\n", result, testImage.size());
        }
        always_assert(result == (size_t)testImage.size() &&
                      "Lepton representation must have encoded proper size");
    }
    always_assert(roundtripBuffer.size() == testImage.size());
    always_assert(leptonBuffer.size() > 2);
    {
        FILE * lfp = fopen("/tmp/X.lep","w");
        fwrite(leptonBuffer.data(), leptonBuffer.size(), 1, lfp);
        fclose(lfp);
    }
    double startDecode = get_cur_time_double();
    ret = write_until(decode_stdin[1], leptonBuffer.data(), 2);
    if (!expect_decoder_failure) {
        always_assert(ret == 2 && "Input program must at least accept the 2 byte header before block");
    }
    if (presend_header) {
        sleep_a_bit();
        startDecode = get_cur_time_double();
    }
    ret = write_close_read_until(decode_stdin[1],
                                         leptonBuffer.data() + 2, leptonBuffer.size() - 2,
                                         decode_stdout[0],
                                         roundtripBuffer.data(), roundtripBuffer.size());
    double stopDecode = get_cur_time_double();
    int decoder_exit = 1;
    waitpid(decoder_pid, &decoder_exit, 0);
    double decodeShutdown = get_cur_time_double();
    fprintf(stderr, "Timing decode: %f decode process exit: %f\n",
            stopDecode - startDecode, decodeShutdown - startDecode);
    fprintf(stderr, "EXIT STATUS %d (%d) %d (%d)\n",
            WEXITSTATUS(encoder_exit), encoder_exit,
            WEXITSTATUS(decoder_exit), decoder_exit);
    if (expect_decoder_failure) {
        if (decoder_exit) {
            fprintf(stderr, "But this is expected since we were making sure the decoder did fail.\n"
                    "Failed on decode with code %d: %d\n", WEXITSTATUS(decoder_exit), decoder_exit);
        }
        return decoder_exit != 0 ? 0 : 1;
    }
    if (WEXITSTATUS(encoder_exit)) {
        return(WEXITSTATUS(encoder_exit));
    }
    if(WEXITSTATUS(decoder_exit)) {
        return WEXITSTATUS(decoder_exit);
    }
    if (encoder_exit) {
        return encoder_exit;
    }
    if(decoder_exit) {
        return decoder_exit;
    }
    if (roundtripBuffer.size() != testImage.size()) {
        fprintf(stderr, "Size mismatch");
        return 1;
    }
    if (memcmp(roundtripBuffer.data(), testImage.data(), testImage.size()) != 0) {
        fprintf(stderr, "Size mismatch");
        return 1;
    }
    return 0;
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
    always_assert(readd == 1);
    fclose(fp);
    return retval;
}
int test_file(int argc, char **argv, bool use_lepton, bool jailed, int inject_syscall_level, int allow_progressive_files, bool multithread,
              const std::vector<const char *> &filenames, bool expect_encode_failure, bool expect_decode_failure,
              const char* encode_memory, const char * decode_memory, const char * singlethread_recode_memory, const char* thread_memory,
              bool use_brotli, bool force_no_ans) {
    always_assert(argc > 0);
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
        return run_test(testImage, use_lepton, jailed, inject_syscall_level, allow_progressive_files, multithread,
                        expect_encode_failure, expect_decode_failure, encode_memory, decode_memory, singlethread_recode_memory, thread_memory,
                        use_brotli, force_no_ans);
    }
    for (std::vector<const char *>::const_iterator filename = filenames.begin(); filename != filenames.end(); ++filename) {
        testImage = load(*filename);
        fprintf(stderr, "Loading %lu\n", (unsigned long)testImage.size());
        int retval = run_test(testImage,
                              use_lepton, jailed, inject_syscall_level, allow_progressive_files, multithread,
                              expect_encode_failure, expect_decode_failure, encode_memory, decode_memory, singlethread_recode_memory, thread_memory,
                              use_brotli, force_no_ans);
        if (retval) {
            return retval;
        }
    }
    return 0;
}
