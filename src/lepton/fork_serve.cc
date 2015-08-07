#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#ifndef __APPLE__
#include <wait.h>
#else
#include <sys/wait.h>
#endif
#include <errno.h>
#include "jpgcoder.hh"
#include "../io/ioutil.hh"
char hex_nibble(uint8_t val) {
    if (val < 10) return val + '0';
    return val - 10 + 'a';
}

void always_assert(bool expr) {
    if (!expr) exit(1);
}

const char last_prefix[] = "/tmp/";
const char last_postfix[2][7]={".iport", ".oport"};
char last_pipes[sizeof(last_postfix) / sizeof(last_postfix[0])][128] = {};

void name_cur_pipes(FILE * dev_random) {
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

void exit_on_stdin(pid_t child) {
    if (!child) {
        fclose(stdin);
        return;
    }
    fclose(stdout);
    getc(stdin);
    kill(child, SIGQUIT);
    sleep(1); // 1 second to clean up its temp pipes
    kill(child, SIGKILL);
    fclose(stderr);
    exit(0);
}

void cleanup_pipes(int) {
    for (size_t i = 0;i < sizeof(last_pipes)/sizeof(last_pipes[0]); ++i) {
        if (last_pipes[i][0]) { // if we've started serving pipes
            unlink(last_pipes[i]);
        }
    }
}
void fork_serve() {
    exit_on_stdin(fork());
    signal(SIGQUIT, &cleanup_pipes);
    signal(SIGTERM, &cleanup_pipes);
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
        fprintf(stdout, "%s\n", last_pipes[0]);
        if (fflush(stdout) != 0) {
            perror("sync");
        }
        FILE * reader_pipe = fopen(cur_pipes[0], "rb");
        FILE * writer_pipe = fopen(cur_pipes[1], "wb");
        unlink(cur_pipes[0]);
        unlink(cur_pipes[1]);
        pid_t serve_file = fork();
        if (serve_file == 0) {
            while (close(1) < 0 && errno == EINTR){ // close stdout
            }
            // leave stderr open for complaints
            IOUtil::FileReader reader(reader_pipe);
            IOUtil::FileWriter writer(writer_pipe);
            process_file(&reader, &writer);
            exit(0);
        } else {
            fclose(reader_pipe);
            fclose(writer_pipe);
        }
        {
            int status;
            while (waitpid(-1, &status, WNOHANG) > 0) {
            }
        }
    }
}
