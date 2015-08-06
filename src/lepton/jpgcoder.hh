/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <atomic>
#include "../io/Reader.hh"
extern int cmpc;
extern std::atomic<int> errorlevel;
extern char errormessage[128];
void process_file(Sirikata::DecoderReader *reader = nullptr);
