#include "../../vp8/util/memory.hh"
#ifndef _WIN32

#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#if defined(__APPLE__) || defined(BSD)
#include <sys/wait.h>
#else
#include <wait.h>
#endif
#include <errno.h>
#include "jpgcoder.hh"
#include "../io/ioutil.hh"
static char hex_nibble(uint8_t val) {
    if (val < 10) return val + '0';
    return val - 10 + 'a';
}


static const char last_prefix[] = "/tmp/";
static const char last_postfix[2][7]={".iport", ".oport"};
static char last_pipes[sizeof(last_postfix) / sizeof(last_postfix[0])][128] = {};

static void name_cur_pipes(FILE * dev_random) {
    char random_data[16] = {0};
    auto retval = fread(random_data, 1, sizeof(random_data), dev_random);
    (void)retval;// dev random should yield reasonable results
    for (size_t pipe_id = 0; pipe_id < sizeof(last_postfix) / sizeof(last_postfix[0]); ++pipe_id) {
        memcpy(last_pipes[pipe_id], last_prefix, strlen(last_prefix));
        size_t offset = strlen(last_prefix);
        for (size_t i = 0; i < sizeof(random_data); ++i) {
            always_assert(offset + 3 < sizeof(last_pipes[i]));
            uint8_t hex = random_data[i];
            last_pipes[pipe_id][offset] = hex_nibble(hex>> 4);
            last_pipes[pipe_id][offset + 1] = hex_nibble(hex & 0xf);
            offset += 2;
            if (i == 4 || i == 6 || i == 8 || i == 14) {
                last_pipes[pipe_id][offset] = '-';
                ++offset;
            }
        }
        memcpy(last_pipes[pipe_id]+offset, last_postfix[pipe_id], sizeof(last_postfix[pipe_id]));
    }
}

static void exit_on_stdin(pid_t child) {
    if (!child) {
        int ret = fclose(stdin);
        always_assert(ret == 0);
        return;
    }
    int ret = fclose(stdout);
    always_assert(ret == 0);
    (void)getc(stdin);
    (void)kill(child, SIGQUIT);
    sleep(1); // 1 second to clean up its temp pipes
    (void)kill(child, SIGKILL);
    ret = fclose(stderr);
    always_assert(ret == 0);
    custom_exit(ExitCode::SUCCESS);
}

static void cleanup_pipes(int) {
    for (size_t i = 0;i < sizeof(last_pipes)/sizeof(last_pipes[0]); ++i) {
        if (last_pipes[i][0]) { // if we've started serving pipes
            unlink(last_pipes[i]);
        }
    }
    custom_exit(ExitCode::EARLY_EXIT);
}
void fork_serve() {
    exit_on_stdin(fork());
    (void)signal(SIGINT, &cleanup_pipes);
    (void)signal(SIGQUIT, &cleanup_pipes);
    (void)signal(SIGTERM, &cleanup_pipes);
    FILE* dev_random = fopen("/dev/urandom", "rb");
    while (true) {
        name_cur_pipes(dev_random);
        char cur_pipes[sizeof(last_pipes) / sizeof(last_pipes[0])][sizeof(last_pipes[0])];
        memcpy(cur_pipes, last_pipes, sizeof(cur_pipes));
        if(mkfifo(last_pipes[0], S_IWUSR | S_IRUSR) == -1) {
            perror("pipe");
        }
        if(mkfifo(last_pipes[1], S_IWUSR | S_IRUSR) == -1) {
            perror("pipe");
        }
        fprintf(stdout, "%s\n%s\n", last_pipes[0], last_pipes[1]);
        if (fflush(stdout) != 0) {
            perror("sync");
        }
        int reader_pipe = -1;
        do {
            reader_pipe = open(cur_pipes[0], O_RDONLY);
        } while(reader_pipe < 0 && errno == EINTR);
        int writer_pipe = -1;
        do {
            writer_pipe = open(cur_pipes[1], O_WRONLY);
        } while(writer_pipe < 0 && errno == EINTR);
        (void)unlink(cur_pipes[0]);
        (void)unlink(cur_pipes[1]);
        pid_t serve_file = fork();
        if (serve_file == 0) {
            while (close(1) < 0 && errno == EINTR){ // close stdout
            }
            // leave stderr open for complaints
            IOUtil::FileReader reader(reader_pipe, 0, false);
            IOUtil::FileWriter writer(writer_pipe, false, false);
            process_file(&reader, &writer, 0, false);
            custom_exit(ExitCode::SUCCESS);
        } else {
            int err = -1;
            do {
                err = close(reader_pipe);
            } while (err < 0 && errno == EINTR);
            do {
                err = close(writer_pipe);
            } while (err < 0 && errno == EINTR);
        }
        {
            int status;
            while (waitpid(-1, &status, WNOHANG) > 0) {
            }
        }
    }
}
#endif
