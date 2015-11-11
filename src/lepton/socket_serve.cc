#include "../../vp8/util/memory.hh"
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
    unlink(socket_name);
}

void socket_serve(uint32_t global_max_length) {
    FILE* dev_random = fopen("/dev/urandom", "rb");
    name_socket(dev_random);
    fclose(dev_random);
    signal(SIGQUIT, &cleanup_socket);
    signal(SIGTERM, &cleanup_socket);
    // listen
    
    struct sockaddr_un address, client;
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
    while (true) {
        socklen_t len = sizeof(client);
        int active_connection = accept(socket_fd, (sockaddr*)&client, &len);
        pid_t serve_file = fork();
        if (serve_file == 0) {
            while (close(1) < 0 && errno == EINTR){ // close stdout
            }
            IOUtil::FileReader reader(active_connection, global_max_length);
            IOUtil::FileWriter writer(active_connection, false);
            process_file(&reader, &writer, global_max_length);
            custom_exit(0);
        } else {
            int err = -1;
            do {
                err = close(active_connection);
            } while (err < 0 && errno == EINTR);
        }
        {
            int status;
            while (waitpid(-1, &status, WNOHANG) > 0) {
            }
        }
    }
}
