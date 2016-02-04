#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
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
#include <set>
static char hex_nibble(uint8_t val) {
    if (val < 10) return val + '0';
    return val - 10 + 'a';
}

static void always_assert(bool expr) {
    if (!expr) custom_exit(ExitCode::ASSERTION_FAILURE);
}

static const char last_prefix[] = "/tmp/";
static const char last_postfix[]=".uport";
static char socket_name[sizeof((struct sockaddr_un*)0)->sun_path] = {};
static const char lock_ext[]=".lock";
static char socket_lock[sizeof((struct sockaddr_un*)0)->sun_path + sizeof(lock_ext)];

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
        if (socket_lock[0]) {
            unlink(socket_lock);
        }
        exit(0);
        return;
    }
    custom_exit(ExitCode::SUCCESS);
}

static void nop(int){}
pid_t accept_new_connection(int active_connection,
                            const SocketServeWorkFunction& work,
                            uint32_t global_max_length,
                            int lock_fd) {
    pid_t serve_file = fork();
    if (serve_file == 0) {
        is_parent_process = false;
        while (close(1) < 0 && errno == EINTR){ // close stdout
        }
        if (lock_fd >= 0) {
            while (close(lock_fd) < 0 && errno == EINTR){
                // close socket lock so future servers may reacquire the lock
            }
        }
        IOUtil::FileReader reader(active_connection, global_max_length);
        IOUtil::FileWriter writer(active_connection, false);
        work(&reader,
             &writer,
             global_max_length);
        custom_exit(ExitCode::SUCCESS);
    } else {
        while (close(active_connection) < 0 && errno == EINTR){
            // close the Unix Domain Socket
        }
    }
    return serve_file;
}
int should_wait_bitmask(size_t children_size,
                        uint32_t max_children) {
    if (max_children && children_size >= max_children) {
        return 0;
    }
    return WNOHANG;
}
void serving_loop(int unix_domain_socket_server,
                  const SocketServeWorkFunction& work,
                  uint32_t global_max_length,
                  uint32_t max_children,
                  bool do_cleanup_socket,
                  int lock_fd) {
    std::set<pid_t> children;
    int status;
    while(true) {
        for (pid_t term_pid = 0;
             (term_pid = waitpid(-1,
                                 &status,
                                 should_wait_bitmask(children.size(), max_children))) > 0;) {
            std::set<pid_t>::iterator where = children.find(term_pid);
            if (where != children.end()) {
                children.erase(where);
            } else {
                fprintf(stderr, "Pid %d not found as child of this\n", term_pid);
                assert(false && "pid msut be in child\n");
            }
            fprintf(stderr, "Child %d exited with code %d\n", term_pid, status);
            fflush(stderr);
        }
        struct sockaddr_un client;
        socklen_t len = sizeof(client);
        int active_connection = accept(unix_domain_socket_server,
                                       (sockaddr*)&client, &len);
        if (active_connection >= 0) {
            children.insert(accept_new_connection(active_connection,
                                                  work,
                                                  global_max_length,
                                                  lock_fd));
        } else {
            if (errno != EINTR) {
                cleanup_socket(0);
            }
        }
    }
}

void socket_serve(const SocketServeWorkFunction &work_fn,
                  uint32_t global_max_length,
                  const char * optional_socket_file_name,
                  uint32_t max_children) {
    bool do_cleanup_socket = true;
    int lock_fd = -1;
    if (optional_socket_file_name != NULL) {
        do_cleanup_socket = false;
        size_t len = strlen(optional_socket_file_name);
        if (len + 1 < sizeof(socket_name)) {
            memcpy(socket_name, optional_socket_file_name, len);
            socket_name[len] = '\0';
        } else {
            fprintf(stderr, "Path too long for %s\n", optional_socket_file_name);
            always_assert(false && "input file name too long\n");
        }
        memcpy(socket_lock, socket_name, sizeof(socket_name));
        memcpy(socket_lock + strlen(socket_lock), lock_ext, sizeof(lock_ext));
        int lock_file = -1;
        do {
            lock_file = open(socket_lock, O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        } while(lock_file < 0 && errno == EINTR);
        if (lock_file >= 0) {
            lock_fd = lock_file;
            int err = 0;
            do {
                err = ::flock(lock_file, LOCK_EX|LOCK_NB);
            } while(err < 0 && errno == EINTR);
            if (err == 0) {
                do {
                    err = remove(socket_name);
                }while (err < 0 && errno == EINTR);
                signal(SIGINT, &cleanup_socket);
                // if we have the lock we can clean it up
                signal(SIGQUIT, &cleanup_socket);
                signal(SIGTERM, &cleanup_socket);
                do_cleanup_socket = true;
            }
        }
    } else {
        FILE* dev_random = fopen("/dev/urandom", "rb");
        name_socket(dev_random);
        fclose(dev_random);
        signal(SIGINT, &cleanup_socket);
        signal(SIGQUIT, &cleanup_socket);
        signal(SIGTERM, &cleanup_socket);
    }
    signal(SIGCHLD, &nop);
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
    serving_loop(socket_fd, work_fn, global_max_length, max_children, do_cleanup_socket, lock_fd);
}
