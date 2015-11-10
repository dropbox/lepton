/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <atomic>
#include "../io/Reader.hh"
//extern int cmpc;
extern std::atomic<int> errorlevel;
extern std::string errormessage;
namespace IOUtil {
class FileReader;
class FileWriter;
}
void process_file(IOUtil::FileReader* reader, IOUtil::FileWriter *writer, int file_input_length = 0);
