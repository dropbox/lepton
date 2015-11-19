#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <sys/time.h>
#ifndef __APPLE__
#include <wait.h>
#else
#include <sys/wait.h>
#endif
#include <poll.h>
#include <errno.h>
#include "../io/Reader.hh"
#include "socket_serve.hh"
#include "../../vp8/util/memory.hh"

static char hex_nibble(uint8_t val) {
    if (val < 10) return val + '0';
    return val - 10 + 'a';
}

static void always_assert(bool expr) {
    if (!expr) custom_exit(1);
}

static const char last_prefix[] = "/tmp/";
static const char last_postfix[]=".uport";
static char socket_name[sizeof((struct sockaddr_un*)0)->sun_path] = {};

bool is_parent_process = true;

static void name_socket(FILE * dev_random) {
    char random_data[16] = {0};
    auto retval = fread(random_data, 1, sizeof(random_data), dev_random);
    (void)retval;// dev random should yield reasonable results
    memcpy(socket_name, last_prefix, strlen(last_prefix));
    size_t offset = strlen(last_prefix);
    for (size_t i = 0; i < sizeof(random_data); ++i) {
        always_assert(offset + 3 + sizeof(last_postfix) < sizeof(socket_name));
        uint8_t hex = random_data[i];
        socket_name[offset] = hex_nibble(hex>> 4);
        socket_name[offset + 1] = hex_nibble(hex & 0xf);
        offset += 2;
        if (i == 4 || i == 6 || i == 8 || i == 14) {
            socket_name[offset] = '-';
            ++offset;
        }
    }
    always_assert(offset + sizeof(last_postfix) < sizeof(socket_name));
    memcpy(socket_name+offset, last_postfix, sizeof(last_postfix));
}

static void cleanup_socket(int) {
    if (is_parent_process) {
        unlink(socket_name);
        return;
    }
    custom_exit(0);
}
/**
 * This closes the timer_pipe which will signal the main thread to start the clock for this pid
 */
static void subprocess_start_timer(int timer_pipe) {
    while(close(timer_pipe) < 0 && errno == EINTR) {

    }
}

struct ProcessInfo {
    uint64_t start_ms;
    pid_t pid;
    int pipe_fd;
};

pollfd make_pollfd(int fd) {
    pollfd retval;
    memset(&retval, 0, sizeof(retval));
    retval.fd = fd;
    retval.events = POLLIN;
    return retval;
}

ProcessInfo accept_new_connection(int active_connection,
                                  const SocketServeWorkFunction& work,
                                  uint32_t global_max_length) {
    int timer_pipe[2] = {-1, -1};
    while(pipe(timer_pipe) < 0 && errno == EINTR) {
    }
    pid_t serve_file = fork();
    if (serve_file == 0) {
        is_parent_process = false;
        while (close(1) < 0 && errno == EINTR){ // close stdout
        }
        while (close(timer_pipe[0]) < 0 && errno == EINTR){
            // close timer pipe read end (will close write end on data recv)
        }
        IOUtil::FileReader reader(active_connection, global_max_length);
        IOUtil::FileWriter writer(active_connection, false);
        work(&reader,
             &writer,
             std::bind(&subprocess_start_timer,
                       timer_pipe[1]
                 ),
             global_max_length);
        custom_exit(0);
    } else {
        while (close(active_connection) < 0 && errno == EINTR){
            // close the Unix Domain Socket
        }
        while (close(timer_pipe[1]) < 0 && errno == EINTR){
          // close the end of the timer start pipe, so the child may close it
          // to signal that it has received data and the clock is running
        }
    }
    ProcessInfo process_info;
    process_info.start_ms = 0;
    process_info.pid = serve_file;
    process_info.pipe_fd = timer_pipe[0];
    return process_info;
}

void serving_loop(int unix_domain_socket_server,
                  const SocketServeWorkFunction& work,
                  uint64_t time_bound_ms,
                  uint32_t global_max_length) {
    std::vector<ProcessInfo> process_map;
    std::vector<pid_t> terminated_processes;
    std::vector<pollfd> fd_of_interest;
    std::vector<int> active_fds;
    fd_of_interest.push_back(make_pollfd(0));
    fd_of_interest.push_back(make_pollfd(unix_domain_socket_server));
    int64_t sleep_for = time_bound_ms;
    while(true) {
        active_fds.clear();
        int num_fd = poll(&fd_of_interest[0],
                      fd_of_interest.size(),
                      sleep_for);
        if (fd_of_interest[0].revents) {
            cleanup_socket(0);
            return; // exit
        } else if (fd_of_interest[1].revents) { // new_socket
            struct sockaddr_un client;
            socklen_t len = sizeof(client);
            int active_connection = accept(unix_domain_socket_server,
                                           (sockaddr*)&client, &len);
            process_map.push_back(accept_new_connection(active_connection,
                                                        work,
                                                        global_max_length));
            assert(num_fd > 0);
            --num_fd;
        }
        for (std::vector<pollfd>::const_iterator i = fd_of_interest.begin(),
                 ie = fd_of_interest.end(); num_fd && i != ie; ++i) {
            if (i->revents) {
                active_fds.push_back(i->fd);
                --num_fd;
            }
        }
        terminated_processes.clear();
        int status;
        pid_t term_pid = 0;
        while ((term_pid = waitpid(-1, &status, WNOHANG)) > 0) {
            terminated_processes.push_back(term_pid);
        }
        struct timeval now = {0,0};
        gettimeofday(&now, NULL);
        uint64_t now_ms = now.tv_sec;
        now_ms *= 1000;
        now_ms += now.tv_usec / 1000;
        uint64_t min_start = now_ms;
        fd_of_interest.resize(2); // get stdin and new_process_pipe only
        {
            std::vector<ProcessInfo>::iterator cur_process = process_map.begin(),
                end_process = process_map.end();
            while(cur_process != end_process) {
                if (std::find(active_fds.begin(),
                              active_fds.end(),
                              cur_process->pipe_fd) != active_fds.end()) {
                    cur_process->start_ms = now_ms;
                    while(close(cur_process->pipe_fd) < 0 && errno == EINTR) {
                    }
                    cur_process->pipe_fd = -1;
                }
                if (std::find(terminated_processes.begin(),
                              terminated_processes.end(),
                              cur_process->pid) != terminated_processes.end()) {
                    if (cur_process->pipe_fd != -1) {
                        while(close(cur_process->pipe_fd) < 0 && errno == EINTR) {
                        }
                        cur_process->pipe_fd = -1;
                    }
                    --end_process;
                    *cur_process = *end_process;
                } else {
                    if (cur_process->pipe_fd != -1) {
                        fd_of_interest.push_back(make_pollfd(cur_process->pipe_fd));
                    }
                    uint64_t delta_ms = now_ms - cur_process->start_ms;
                    if (cur_process->start_ms && time_bound_ms && delta_ms > time_bound_ms) {
                        fprintf(stderr, "Time Bound Reached: Killing %d\n", cur_process->pid);
                        kill(cur_process->pid, delta_ms > time_bound_ms * 2 ? SIGKILL : SIGTERM);
                    } else {
                        if (cur_process->start_ms && cur_process->start_ms < min_start) {
                            min_start = cur_process->start_ms;
                        }

                    }
                    ++cur_process;
                }
            }
            size_t processes_to_pop = process_map.end() - end_process;
            assert(processes_to_pop <= process_map.size());
            process_map.resize(process_map.size() - processes_to_pop);
        }
        sleep_for = time_bound_ms ? time_bound_ms - (now_ms - min_start) : 100000;
        if (sleep_for < 0) {
            sleep_for = 0;
        }
    }
}

void socket_serve(const SocketServeWorkFunction &work_fn,
                  uint64_t time_bound_ms,
                  uint32_t global_max_length) {
    FILE* dev_random = fopen("/dev/urandom", "rb");
    name_socket(dev_random);
    fclose(dev_random);
    int new_process_pipe[2];
    while(pipe(new_process_pipe) < 0 && errno == EINTR) {
    }
    signal(SIGINT, &cleanup_socket);
    signal(SIGQUIT, &cleanup_socket);
    signal(SIGTERM, &cleanup_socket);
    // listen
    struct sockaddr_un address;
    memset(&address, 0, sizeof(struct sockaddr_un));
    int socket_fd;
    int err;
    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    always_assert(socket_fd > 0);
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, socket_name, std::min(strlen(socket_name), sizeof(address.sun_path)));
    err = bind(socket_fd, (struct sockaddr*)&address, sizeof(address));
    always_assert(err == 0);
    err = listen(socket_fd, 16);
    always_assert(err == 0);
    fprintf(stdout, "%s\n", socket_name);
    fflush(stdout);
    serving_loop(socket_fd, work_fn, time_bound_ms, global_max_length);
}
