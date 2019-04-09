/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef JPGCODER_HH_
#define JPGCODER_HH_
#include <atomic>
#include <functional>
#include "../vp8/util/nd_array.hh"
#include "../vp8/util/options.hh"
#include "../io/Reader.hh"
//extern int cmpc;
extern uint8_t get_current_file_lepton_version();
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
                  int file_input_length,
                  bool force_zlib0);
void check_decompression_memory_bound_ok();
namespace TimingHarness {
#define FOREACH_TIMING_STAGE(CB) \
    CB(TS_MAIN) \
    CB(TS_MODEL_INIT_BEGIN) \
    CB(TS_MODEL_INIT) \
    CB(TS_ACCEPT) \
    CB(TS_THREAD_STARTED) \
    CB(TS_READ_STARTED) \
    CB(TS_READ_FINISHED) \
    CB(TS_JPEG_DECODE_STARTED) \
    CB(TS_JPEG_DECODE_FINISHED) \
    CB(TS_STREAM_MULTIPLEX_STARTED) \
    CB(TS_STREAM_MULTIPLEX_FINISHED) \
    CB(TS_THREAD_WAIT_STARTED) \
    CB(TS_THREAD_WAIT_FINISHED) \
    CB(TS_ARITH_STARTED) \
    CB(TS_ARITH_FINISHED) \
    CB(TS_JPEG_RECODE_STARTED) \
    CB(TS_JPEG_RECODE_FINISHED) \
    CB(TS_STREAM_FLUSH_STARTED) \
    CB(TS_STREAM_FLUSH_FINISHED) \
    CB(TS_DONE)
#define MAKE_TIMING_STAGE_ENUM(VALUE) VALUE,
#define GENERATE_TIMING_STRING(VALUE) #VALUE,
enum TimingStages_ {
    FOREACH_TIMING_STAGE(MAKE_TIMING_STAGE_ENUM)
    NUM_STAGES,
};
extern Sirikata::Array1d<Sirikata::Array1d<uint64_t, NUM_STAGES>, MAX_NUM_THREADS> timing;
extern uint64_t get_time_us(bool force=false);
void print_results();
}
#endif
