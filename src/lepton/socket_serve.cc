#ifndef _WIN32
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
#include <netinet/in.h>
#include <sys/time.h>
#if defined(__APPLE__) || defined(BSD)
#include <sys/wait.h>
#else
#include <sys/signalfd.h>
#include <wait.h>
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

static const char last_prefix[] = "/tmp/";
static const char last_postfix[]=".uport";
static const char zlast_postfix[]=".z0";

static char socket_name[sizeof((struct sockaddr_un*)0)->sun_path] = {};
static char zsocket_name[sizeof((struct sockaddr_un*)0)->sun_path] = {};
static const char lock_ext[]=".lock";
bool random_name = false;
static char socket_lock[sizeof((struct sockaddr_un*)0)->sun_path + sizeof(lock_ext)];
int lock_file = -1;

bool is_parent_process = true;

static void name_socket(FILE * dev_random) {
    random_name = true;
    char random_data[16] = {0};
    auto retval = fread(random_data, 1, sizeof(random_data), dev_random);
    (void)retval;// dev random should yield reasonable results
    memcpy(socket_name, last_prefix, strlen(last_prefix));
    memcpy(zsocket_name, last_prefix, strlen(last_prefix));
    size_t offset = strlen(last_prefix);
    for (size_t i = 0; i < sizeof(random_data); ++i) {
        always_assert(offset + 3 + sizeof(last_postfix) < sizeof(socket_name));
        always_assert(offset + 3 + sizeof(zlast_postfix) < sizeof(zsocket_name));
        uint8_t hex = random_data[i];
        socket_name[offset] = hex_nibble(hex>> 4);
        socket_name[offset + 1] = hex_nibble(hex & 0xf);
        zsocket_name[offset] = hex_nibble(hex>> 4);
        zsocket_name[offset + 1] = hex_nibble(hex & 0xf);
        offset += 2;
        if (i == 4 || i == 6 || i == 8 || i == 14) {
            socket_name[offset] = '-';
            zsocket_name[offset] = '-';
            ++offset;
        }
    }
    always_assert(offset + sizeof(last_postfix) < sizeof(socket_name));
    always_assert(offset + sizeof(zlast_postfix) < sizeof(zsocket_name));
    always_assert(offset + sizeof(lock_ext) < sizeof(socket_lock));
    memcpy(socket_name+offset, last_postfix, sizeof(last_postfix));
    memcpy(zsocket_name+offset, zlast_postfix, sizeof(zlast_postfix));

    memcpy(socket_lock, socket_name, offset);
    memcpy(socket_lock+offset, lock_ext, sizeof(lock_ext));
}

static void cleanup_socket(int) {
    if (is_parent_process) {
        unlink(socket_name);
        unlink(zsocket_name);
        if (socket_lock[0] && random_name) {
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
                            int lock_fd,
                            bool force_zlib) {
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
        IOUtil::FileReader reader(active_connection, global_max_length, true);
        IOUtil::FileWriter writer(active_connection, false, true);
        work(&reader,
             &writer,
             global_max_length,
             force_zlib);
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

int make_sigchld_fd() {
    int fd = -1;
#if !(defined(__APPLE__) || defined(BSD))
    sigset_t sigset;
    int err = sigemptyset(&sigset);
    always_assert(err == 0);
    err = sigaddset(&sigset, SIGCHLD);
    always_assert(err == 0);

    // the signalfd will only receive SIG_BLOCK'd signals
    err = sigprocmask(SIG_BLOCK, &sigset, NULL);
    always_assert(err == 0);

    fd = signalfd(-1, &sigset, 0);
    always_assert(fd != -1);
#endif
    return fd;
}
void write_num_children(size_t num_children) {
    if (num_children > 0xff) {
        num_children = 0xff;
    }
    // lets just keep a byte of state about the number of children
    if (lock_file != -1) {
        int err;
        while((err = lseek(lock_file, 0, SEEK_SET)) < 0 && errno == EINTR){
        }
        uint8_t num_children_byte = (uint8_t)num_children;
        while((err = write(lock_file, &num_children_byte, sizeof(num_children_byte))) < 0 && errno == EINTR) {
        }
    }
}
void serving_loop(int unix_domain_socket_server,
                  int unix_domain_socket_server_zlib,
                  int tcp_socket_server,
                  int tcp_socket_server_zlib,
                  const SocketServeWorkFunction& work,
                  uint32_t global_max_length,
                  uint32_t max_children,
                  bool do_cleanup_socket,
                  int lock_fd) {
    int sigchild_fd = make_sigchld_fd();

    int num_fds = 0;
    struct pollfd fds[5];
    if (sigchild_fd != -1) {
        fds[0].fd = sigchild_fd;
        fds[0].events = POLLIN | POLLERR | POLLHUP;
        num_fds+= 1;
    }
    if (unix_domain_socket_server_zlib != -1) {
        fds[num_fds].fd = unix_domain_socket_server_zlib;
        ++num_fds;
    }
    if (tcp_socket_server_zlib != -1) {
        fds[num_fds].fd = tcp_socket_server_zlib;
        ++num_fds;
    }
    if (unix_domain_socket_server != -1) {
        fds[num_fds].fd = unix_domain_socket_server;
        ++num_fds;
    }
    if (tcp_socket_server != -1) {
        fds[num_fds].fd = tcp_socket_server;
        ++num_fds;
    }
    for (int i = 0; i < num_fds; ++i) {
      int err;
      while ((err = fcntl(fds[i].fd, F_SETFL, O_NONBLOCK)) == -1
             && errno == EINTR) {}
      always_assert(err == 0);
      fds[i].events = POLLIN;
    }
    std::set<pid_t> children;
    int status;
    while(true) {
        write_num_children(children.size());
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
            if (WIFEXITED(status)) {
                fprintf(stderr, "Child %d exited with code %d\n", term_pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr, "Child %d exited with signal %d\n", term_pid, WTERMSIG(status));
            } else {
                fprintf(stderr, "Child %d exited with another cause: %d\n", term_pid, status);
            }
            fflush(stderr);
            write_num_children(children.size());
        }
        int ret = poll(fds, num_fds, sigchild_fd == -1 ? 60 : -1);
        // need a timeout (30 ms) in case a SIGCHLD was missed between the waitpid and the poll
        if (ret == 0) { // no events ready, just timed out, check for missed SIGCHLD
            continue;
        }
        if (ret < 0 && errno == EINTR) {
            continue;
        }
        for (int i = 0; i < num_fds; ++i) {
            if (fds[i].revents & POLLIN) {
                fds[i].revents = 0;
                if (fds[i].fd == sigchild_fd) {
#if !(defined(__APPLE__) || defined(BSD))
                    struct signalfd_siginfo info;
                    ssize_t ignore = read(fds[i].fd, &info, sizeof(info));
                    (void)ignore;
#endif
                    continue; // we can't receive on this
                }
                struct sockaddr_un client;
                socklen_t len = sizeof(client);
                int active_connection = accept(fds[i].fd,
                                               (sockaddr*)&client, &len);
                if (active_connection >= 0) {
                      int flags;
                    while ((flags = fcntl(active_connection, F_GETFL, 0)) == -1
                           && errno == EINTR){}
                    always_assert(flags != -1);
                      if (flags & O_NONBLOCK) {
                        flags &= ~O_NONBLOCK;
                        // inheritance of nonblocking flag not specified across systems
                        while (fcntl(active_connection, F_SETFL, flags) == -1
                               && errno == EINTR){}
                    }
                    children.insert(accept_new_connection(active_connection,
                                                          work,
                                                          global_max_length,
                                                          lock_fd,
                                                          fds[i].fd == unix_domain_socket_server_zlib
                                                          || fds[i].fd == tcp_socket_server_zlib));
                } else {
                    if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN) {
                        fprintf(stderr, "Error accepting connection: %s", strerror(errno));
                        cleanup_socket(0);
                    }
                }
            }
        }
    }
}
int setup_tcp_socket(int port, int listen_backlog) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    always_assert(socket_fd > 0);    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    int optval = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(socket_fd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0) {
        custom_exit(ExitCode::COULD_NOT_BIND_PORT);
    }
    int err = listen(socket_fd, listen_backlog);
    always_assert(err == 0);
    return socket_fd;
}
int setup_socket(const char *file_name, int listen_backlog) {
    int err;
    int socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    always_assert(socket_fd > 0);
    struct sockaddr_un address;
    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, file_name, std::min(strlen(file_name), sizeof(address.sun_path)));
    err = bind(socket_fd, (struct sockaddr*)&address, sizeof(address));
    always_assert(err == 0);
    err = listen(socket_fd, listen_backlog);
    int ret = chmod(file_name, 0666);
    (void)ret;
    always_assert(err == 0);
    return socket_fd;
}
void socket_serve(const SocketServeWorkFunction &work_fn,
                  uint32_t global_max_length,
                  const ServiceInfo &service_info) {
    bool do_cleanup_socket = true;
    int lock_fd = -1;
    if (service_info.uds != NULL) {
        do_cleanup_socket = false;
        size_t len = strlen(service_info.uds);
        if (len + 1 < sizeof(socket_name)) {
            memcpy(socket_name, service_info.uds, len);
            socket_name[len] = '\0';
        } else {
            fprintf(stderr, "Path too long for %s\n", service_info.uds);
            always_assert(false && "input file name too long\n");
        }
        memcpy(socket_lock, socket_name, sizeof(socket_name));
        memcpy(zsocket_name, socket_name, sizeof(socket_name));
        memcpy(socket_lock + strlen(socket_lock), lock_ext, sizeof(lock_ext));
        memcpy(zsocket_name + strlen(zsocket_name), zlast_postfix, sizeof(zlast_postfix));
        do {
            lock_file = open(socket_lock,
                             O_RDWR|O_CREAT|O_TRUNC,
                             S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
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
                do {
                    err = remove(zsocket_name);
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
        int fret = fclose(dev_random);
        always_assert(fret == 0);
        do {
            lock_file = open(socket_lock,
                             O_RDWR|O_CREAT|O_TRUNC,
                             S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
        } while(lock_file < 0 && errno == EINTR);
        signal(SIGINT, &cleanup_socket);
        signal(SIGQUIT, &cleanup_socket);
        signal(SIGTERM, &cleanup_socket);
    }
    signal(SIGCHLD, &nop);
    // listen
    int socket_fd = -1;
    int zsocket_fd = -1;
    int socket_tcp = -1;
    int zsocket_tcp = -1;
    if (service_info.listen_uds) {
        socket_fd = setup_socket(socket_name, service_info.listen_backlog);
        zsocket_fd = setup_socket(zsocket_name, service_info.listen_backlog);
    }
    if (service_info.listen_tcp) {
        socket_tcp = setup_tcp_socket(service_info.port, service_info.listen_backlog);
        zsocket_tcp = setup_tcp_socket(service_info.zlib_port, service_info.listen_backlog);
    }
    
    fprintf(stdout, "%s\n", socket_name);
    fflush(stdout);
    serving_loop(socket_fd, zsocket_fd, socket_tcp, zsocket_tcp,
                 work_fn, global_max_length, service_info.max_children, do_cleanup_socket, lock_fd);
}
#endif
