/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <atomic>
#include <functional>
#include "../io/Reader.hh"
//extern int cmpc;
extern std::atomic<int> errorlevel;
extern std::string errormessage;
extern uint64_t g_time_bound_ms;
namespace IOUtil {
class FileReader;
class FileWriter;
}
void gen_nop();
void process_file(IOUtil::FileReader* reader,
                  IOUtil::FileWriter *writer,
                  const std::function<void()> &signal_data_recv = &gen_nop,
                  int file_input_length = 0);
