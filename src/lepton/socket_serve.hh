#include <functional>
#include "../io/ioutil.hh"

struct ServiceInfo {
    bool listen_tcp;
    int port;
    int zlib_port;
    bool listen_uds;
    int listen_backlog;
    int max_children;
    const char * uds;
    ServiceInfo() {
        listen_tcp = false;
        port = 2402;
        zlib_port = 2403;
        uds = NULL;
        listen_uds = true;
        listen_backlog = 16;

        max_children = 0;
    }
};

typedef std::function<void(IOUtil::FileReader*,// data to work upon
                           IOUtil::FileWriter*,// returned data
                           uint32_t,//max_file_length
                           bool // force_zlib
                          )> SocketServeWorkFunction;
#ifndef _WIN32
void socket_serve(const SocketServeWorkFunction& work_fn,
                  uint32_t max_file_length,
                  const ServiceInfo &service_info);
#endif
