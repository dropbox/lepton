#include "jpgcoder.hh"
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
char hex_nibble(uint8_t val) {
    if (val < 10) return val + '0';
    return val - 10 + 'a';
}

void always_assert(bool expr) {
    if (!expr) exit(1);
}

const char last_prefix[] = "/dev/shm"
const char last_postfix[3][7]={".iport", ".jport", ".zport"};
char last_pipes[sizeof(last_postfix) / sizeof(last_postfix[0])][128] = {};

void name_cur_pipes(FILE * dev_random) {
    char random_data[16] = {0};
    fread(random_data, 1, sizeof(random_data), dev_random);
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
    fclose(stderr);
    getc(stdin);
    kill(child, SIGQUIT);
    sleep(1); // 1 second to clean up its temp pipes
    kill(child, SIGKILL);
}
std::vector<pid_t> termList;

void remove_child_from_term_list(pid_t pid) {
    std::vector<pid_t>::iterator where = std::find(termList.begin(), termList.end(), pid);
    if (where != termList.end()) {
        *where = 0;
    }
}

std::vector<pid_t>::iterator kill_linked_children() {
    std::vector<pid_t>::iterator candidate = termList.end();
    always_assert((termList.size() & 1) == 0); // must be even
    for (std::vector<pid_t>::iterator i = termList.begin(), ie = candidate;
         i != ie && i + 1 != ie; i += 2) {
        if (*i == 0 || *(i + 1) == 0) {
            if (*i) {
                kill(*i, SIGKILL); // the children are linked together
                *i = 0;
            }
            if (*(i + 1)) {
                kill(*(i + 1), SIGKILL); // the children are linked together
                *(i + 1) = 0;
            }
            if (ie == candidate) {
                candidate = i;
            }
        }
    }
    return candidate;
}

void wait_for_all_children() {
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) {
            break;
        }
        removeChildFromTermList(pid);
    }
}
void sigchild_handler(int) {
    wait_for_all_children();
    kill_linked_children();
}

void add_linked_children(pid_t a, pid_t b) {
    wait_for_all_children();
    candidate = kill_linked_children();
    if (candidate != termList.end()) {
        *candidate = a;
        ++candidate;
        *candidate = b;
        while (termList.size() > 2 && termList[termList.size() - 2] == 0 && termList.back() == 0) {
            termList.pop_back();
            termList.pop_back();
        }
    } else {
        termList.push_back(a);
        termList.push_back(b);
    }
}
void terminate_all_children() {
    for (pid_t pid : termList) {
        kill(pid, SIGKILL);
    }
}
int cleanup_pipes(int) {
    for (size_t i = 0;i < sizeof(last_pipes)/sizeof(last_pipes[0]); ++i) {
        if (last_pipes[i][0]) { // if we've started serving pipes
            unlink(last_pipes[i]);
        }
    }
    terminate_all_children();
    return 0;
}
void fork_serve() {
    exit_on_stdin(fork);
    signal(SIGQUIT, cleanup_pipes);
    signal(SIGTERM, cleanup_pipes);
    FILE* dev_random = fopen("/dev/urandom", "rb");
    while (true) {
        name_cur_pipes(fp);
        char cur_pipes[sizeof(last_pipes) / sizeof(last_pipes[0])][sizeof(last_pipes[0])];
        memcpy(cur_pipes, last_pipes);
        if(mkfifo(last_pipes[0], S_IWUSR | S_IRUSR) == -1) {
            perror("pipe");
        }
        if(mkfifo(last_pipes[1], S_IWUSR | S_IRUSR) == -1) {
            perror("pipe");
        }
        if(mkfifo(last_pipes[2], S_IWUSR | S_IRUSR) == -1) {
            perror("pipe");
        }
        fprintf(stdout, "%s\n", last_pipes[0]);
        if (fflush(stdout) != 0) {
            perror("sync");
        }
        IOUtil::FileReader * reader = OpenFileOrPipe(last_pipes[0], 0, 0, 0);
        if (reader) {
            pid_t raw_serve = fork();
            if (raw_serve == 0) {
                single_serve(cur_pipes[0], cur_pipes[1], cur_pipes[2], false);
                exit(0);
            }
            pid_t zlib_serve = fork();
            if (zlib_serve == 0) {
                single_serve(cur_pipes[0], cur_pipes[2], cur_pipes[1], true);
                exit(0);
            }
            add_linked_children(raw_serve, zlib_serve);
        }
    }
}
