#include <functional>
#include "../io/ioutil.hh"
typedef std::function<void(IOUtil::FileReader*,// data to work upon
                           IOUtil::FileWriter*,// returned data
                           const std::function<void()>&,// callback when work begins
                           uint32_t//max_file_length
                          )> SocketServeWorkFunction;

void socket_serve(const SocketServeWorkFunction& work_fn,
                  uint64_t time_bound_ms,
                  uint32_t max_file_length,
                  const char * optional_socket_file_name = NULL);
