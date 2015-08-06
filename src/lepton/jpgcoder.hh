/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <atomic>
#include "../io/Reader.hh"
//extern int cmpc;
extern std::atomic<int> errorlevel;
extern std::string errormessage;
void process_file(Sirikata::DecoderReader* reader, Sirikata::DecoderWriter *writer);
