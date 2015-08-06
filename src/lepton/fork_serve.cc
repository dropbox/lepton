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
void get_temp_name(char data[sizeof(sockaddr_un::sun_path)], const char * postfix) {
    FILE* fp = fopen("/dev/urandom", "rb");
    std::string local = "/tmp/";
    for (int i = 0;i < 16; ++i) {
        int hex = fgetc(fp);
        local += hex_nibble(hex>> 4);
        local += hex_nibble(hex&0xf);
        if (i == 4 || i == 6 || i == 8 || i == 14) local += '-';
    }
    local += postfix;
    memcpy(data, local.data(), std::min(sizeof(sockaddr_un::sun_path), local.size()));
    data[sizeof(sockaddr_un::sun_path) - 1] = '\0';
    if(local.size() < sizeof(sockaddr_un::sun_path) - 1) {
        data[local.size()] = '\0';
    }
}

void single_serve(int local_socket, bool do_listen_zlib) {
    while (true){
        struct sockaddr_un remote;
        unsigned int t = sizeof(remote);
        int remote_socket = accept(local_socket, (struct sockaddr*)&remote, &t);
        if (remote_socket == -1) {
            continue;
        }
        if (fork() == 0) {
            close(local_socket);
            printf("HURRAH");
            close(remote_socket);
            exit(0);
        }
    }
}

int setup_socket(const char * extension, struct sockaddr_un *local_address) {
    int local_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (local_socket == -1) {
        perror("socket");
        exit(1);
    }
    local_address->sun_family = AF_UNIX;
    get_temp_name(local_address->sun_path, extension);
    unlink(local_address->sun_path);
    int zlen = strlen(local_address->sun_path) + sizeof(local_address->sun_family);
    if (bind(local_socket, (struct sockaddr *)local_address, zlen) == -1) {
        perror("bind");
        exit(1);
    }
    if (listen(local_socket, 32) == -1) {
        perror("listen");
        exit(1);
    }
}

void fork_serve() {
    
    struct sockaddr_un local_zlib, local_raw;
    int local_raw_socket = setup_socket(".jport", &local_raw);
    int local_zlib_socket = setup_socket(".zport", &local_zlib);
    fprintf(stdout, "%s\n%s\n", local_raw.sun_path, local_zlib.sun_path);
    fflush(stdout);
    pid_t raw_serve = fork();
    if (raw_serve == 0) {
        single_serve(local_raw_socket, false);
        exit(0);
    }
    pid_t zlib_serve = fork();
    if (zlib_serve == 0) {
        single_serve(local_zlib_socket, true);
        exit(0);
    }
    getc(stdin);
    kill(raw_serve, SIGKILL);
    kill(zlib_serve, SIGKILL);
    unlink(local_raw.sun_path);
    unlink(local_zlib.sun_path);
}
