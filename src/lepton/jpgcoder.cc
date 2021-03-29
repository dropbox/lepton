/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/**
Copyright (c) 2006...2016, Matthias Stirner and HTW Aalen University
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

volatile int volatile1024 = 1024;
#include "../vp8/util/memory.hh"
#include "../vp8/util/debug.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <ctime>
#include <memory>
#include <atomic>
#include <signal.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/types.h>
    #include <unistd.h>
#else
    #include <io.h>
#include <chrono>
#include <ctime>
#endif
#ifdef __linux__
#include <sys/sysinfo.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#endif

#ifdef __aarch64__
#define USE_SCALAR 1
#endif

#ifndef USE_SCALAR
#include <emmintrin.h>
#include <immintrin.h>
#endif

#include "jpgcoder.hh"
#include "recoder.hh"
#include "bitops.hh"
#include "htables.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "vp8_decoder.hh"
#include "vp8_encoder.hh"
#include "simple_decoder.hh"
#include "simple_encoder.hh"
#include "fork_serve.hh"
#include "socket_serve.hh"
#include "validation.hh"
#include "../io/ZlibCompression.hh"
#include "../io/BrotliCompression.hh"
#include "../io/MemReadWriter.hh"
#include "../io/BufferedIO.hh"
#include "../io/Zlib0.hh"
#include "../io/Seccomp.hh"
#include "../vp8/encoder/vpx_bool_writer.hh"
#include "generic_compress.hh"
#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif
unsigned char g_zlib_0_writer[sizeof(Sirikata::Zlib0Writer)];
void * uninit_g_zlib_0_writer = &g_zlib_0_writer[0];
unsigned char EOI[ 2 ] = { 0xFF, 0xD9 }; // EOI segment
extern int r_bitcount;
int g_argc = 0;
const char** g_argv = NULL;
#ifndef GIT_REVISION
#include "version.hh"
#ifndef GIT_REVISION
#define GIT_REVISION "unknown"
#endif
#endif
bool g_permissive = false;
bool fast_exit = true;
#ifdef SKIP_VALIDATION
bool g_skip_validation = true;
#else
bool g_skip_validation = false;
#endif
#define QUANT(cmp,bpos) ( cmpnfo[cmp].qtable[ bpos ] )
#define MAX_V(cmp,bpos) ( ( freqmax[bpos] + QUANT(cmp,bpos) - 1 ) /  QUANT(cmp,bpos) )

#define ENVLI(s,v)        ( ( v > 0 ) ? v : ( v - 1 ) + ( 1 << s ) )
#define DEVLI(s,n)        ((s) == 0 ? (n) : ( ( (n) >= ( 1 << ((s) - 1) ) ) ? (n) : (n) + 1 - ( 1 << (s) ) ))
#define E_ENVLI(s,v)    ( v - ( 1 << s ) )
#define E_DEVLI(s,n)    ( n + ( 1 << s ) )

#define COS_DCT(l,s,n)  ( cos( ( ( 2 * l + 1 ) * s * M_PI ) / ( 2 * n ) ) )
#define C_DCT(n)        ( ( n == 0 ) ? ( 1 ) : ( sqrt( 2 ) ) )
#define DCT_SCALE        sqrt( 8 )

#define ABS(v1)            ( (v1 < 0) ? -v1 : v1 )
#define ABSDIFF(v1,v2)    ( (v1 > v2) ? (v1 - v2) : (v2 - v1) )
#define IPOS(w,v,h)        ( ( v * w ) + h )
#define NPOS(n1,n2,p)    ( ( ( p / n1 ) * n2 ) + ( p % n1 ) )
#define ROUND_F(v1)        ( (v1 < 0) ? (int) (v1 - 0.5) : (int) (v1 + 0.5) )
inline uint16_t B_SHORT(uint8_t v1, uint8_t v2) {
    return ( ( ((int) v1) << 8 ) + ((int) v2) );
}
#define CLAMPED(l,h,v)    ( ( v < l ) ? l : ( v > h ) ? h : v )

#define MEM_ERRMSG    "out of memory error"
#define FRD_ERRMSG    "could not read file / file not found: %s"
#define FWR_ERRMSG    "could not write file / file write-protected: %s"
size_t local_atoi(const char *data);
namespace TimingHarness {

Sirikata::Array1d<Sirikata::Array1d<uint64_t, NUM_STAGES>, MAX_NUM_THREADS> timing = {{{{0}}}};

uint64_t get_time_us(bool force) {
#ifdef _WIN32
    return std::chrono::duration_cast<std::chrono::microseconds>
        (std::chrono::high_resolution_clock::now().time_since_epoch()).count();
#else
    if (force || !g_use_seccomp) {
        struct timeval val = {0,0};
        gettimeofday(&val,NULL);
        uint64_t retval = val.tv_sec;
        retval *= 1000000;
        retval += val.tv_usec;
        return retval;
    }
#endif
    return 0;
}
const char * stage_names[] = {FOREACH_TIMING_STAGE(GENERATE_TIMING_STRING) "EOF"};
void print_results() {
    if (!g_use_seccomp) {
        uint64_t earliest_time = get_time_us();
        for (int i = 0; i < NUM_STAGES; ++i) {
            for (unsigned int j = 0; j < MAX_NUM_THREADS && j < NUM_THREADS; ++j) {
                if (timing[j][i] && timing[j][i] < earliest_time) {
                    earliest_time = timing[j][i];
                }
            }
        }
        for (int i = 0; i < NUM_STAGES; ++i) {
            for (unsigned int j = 0; j < MAX_NUM_THREADS && j < NUM_THREADS; ++j) {
                if (timing[j][i]) {
                    fprintf(stderr,
                            "%s\t(%d)\t%f\n",
                            stage_names[i], j,
                            (timing[j][i] - earliest_time) * 0.000001);
                }
            }
        }
    }
}
}
/* -----------------------------------------------
    struct & enum declarations
    ----------------------------------------------- */
enum {
    JPG_READ_BUFFER_SIZE = 1024 * 256,
    ABIT_WRITER_PRELOAD = 4096 * 1024 + 1024
};

enum ACTION {
    comp  =  1,
    forkserve = 2,
    socketserve = 3,
    info = 4,
    lepton_concatenate = 5
};


namespace {
uint32_t LEtoUint32(const uint8_t*buffer) {
    uint32_t retval = buffer[3];
    retval <<=8;
    retval |= buffer[2];
    retval <<= 8;
    retval |= buffer[1];
    retval <<= 8;
    retval |= buffer[0];
    return retval;
}
}

void uint32toLE(uint32_t value, uint8_t *retval) {
    retval[0] = uint8_t(value & 0xff);
    retval[1] = uint8_t((value >> 8) & 0xff);
    retval[2] = uint8_t((value >> 16) & 0xff);
    retval[3] = uint8_t((value >> 24) & 0xff);
}
/* -----------------------------------------------
    function declarations: main interface
    ----------------------------------------------- */

// returns the max size of the input file
int initialize_options( int argc, const char*const* argv );
void execute(const std::function<bool()> &);
void show_help( void );


/* -----------------------------------------------
    function declarations: main functions
    ----------------------------------------------- */

bool check_file(int fd_in, int fd_out, uint32_t max_file_size, bool force_zlib0,
                bool is_embedded_jpeg, Sirikata::Array1d<uint8_t, 2> two_byte_header,
                bool is_socket);

template <class stream_reader>
bool read_jpeg(std::vector<std::pair<uint32_t,
                                     uint32_t>> *huff_input_offset,
               stream_reader *jpg_str_in,
               Sirikata::Array1d<uint8_t, 2> header,
               bool is_embedded_jpeg);
bool read_jpeg_wrapper(std::vector<std::pair<uint32_t,
                                     uint32_t>> *huff_input_offset,
                       ibytestream *jpg_str_in,
                       Sirikata::Array1d<uint8_t, 2> header,
                       bool is_embedded_jpeg) {
    return read_jpeg(huff_input_offset, jpg_str_in, header, is_embedded_jpeg);
}

bool read_jpeg_and_copy_to_side_channel(std::vector<std::pair<uint32_t,
                                                    uint32_t>> *huff_input_offset,
                                        ibytestreamcopier *jpg_str_in,
                                        Sirikata::Array1d<uint8_t, 2> header,
                                        bool is_embedded_jpeg) {
    return read_jpeg(huff_input_offset, jpg_str_in, header, is_embedded_jpeg);
}

struct MergeJpegProgress;
bool decode_jpeg(const std::vector<std::pair<uint32_t,
                                   uint32_t> > &huff_input_offset,
                 std::vector<ThreadHandoff>*row_thread_handoffs);
bool recode_jpeg( void );

bool adapt_icos( void );
bool check_value_range( void );
bool write_ujpg(std::vector<ThreadHandoff> row_thread_handoffs,
                std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >*jpeg_file_raw_bytes);
bool read_ujpg( void );
unsigned char read_fixed_ujpg_header( void );
bool reset_buffers( void );


/* -----------------------------------------------
    function declarations: jpeg-specific
    ----------------------------------------------- */
bool is_jpeg_header(Sirikata::Array1d<uint8_t, 2> header) {
    return header[0] == 0xFF && header[1] == 0xD8;
}

// baseline single threaded decoding need only two rows of the image in memory
bool setup_imginfo_jpg(bool only_allocate_two_image_rows);
bool parse_jfif_jpg( unsigned char type, unsigned int len, uint32_t alloc_len, unsigned char* segment );
bool rebuild_header_jpg( void );

int decode_block_seq( abitreader* huffr, huffTree* dctree, huffTree* actree, short* block );
int encode_block_seq( abitwriter* huffw, huffCodes* dctbl, huffCodes* actbl, short* block );

int decode_dc_prg_fs( abitreader* huffr, huffTree* dctree, short* block );
int encode_dc_prg_fs( abitwriter* huffw, huffCodes* dctbl, short* block );
int decode_ac_prg_fs( abitreader* huffr, huffTree* actree, short* block,
                        unsigned int* eobrun, int from, int to );
int encode_ac_prg_fs( abitwriter* huffw, huffCodes* actbl, short* block,
                        unsigned int* eobrun, int from, int to );

int decode_dc_prg_sa( abitreader* huffr, short* block );
int encode_dc_prg_sa( abitwriter* huffw, short* block );
int decode_ac_prg_sa( abitreader* huffr, huffTree* actree, short* block,
                        unsigned int* eobrun, int from, int to );
int encode_ac_prg_sa( abitwriter* huffw, abytewriter* storw, huffCodes* actbl,
                        short* block, unsigned int* eobrun, int from, int to );

int decode_eobrun_sa( abitreader* huffr, short* block, unsigned int* eobrun, int from, int to );
int encode_eobrun( abitwriter* huffw, huffCodes* actbl, unsigned int* eobrun );
int encode_crbits( abitwriter* huffw, abytewriter* storw );

int next_huffcode( abitreader *huffw, huffTree *ctree , Billing min_bill, Billing max_bill);
int next_mcupos( int* mcu, int* cmp, int* csc, int* sub, int* dpos, int* rstw, int cs_cmpc);
int next_mcuposn( int* cmp, int* dpos, int* rstw );
int skip_eobrun( int* cmp, int* dpos, int* rstw, unsigned int* eobrun );

bool build_huffcodes( unsigned char *clen, uint32_t clenlen,  unsigned char *cval, uint32_t cvallen,
                huffCodes *hc, huffTree *ht );





/* -----------------------------------------------
    function declarations: developers functions
    ----------------------------------------------- */

// these are developers functions, they are not needed
// in any way to compress jpg or decompress ujg
bool write_hdr( void );
bool write_huf( void );
bool write_info( void );
clock_t pre_byte = 0;
clock_t post_byte = 0;
clock_t read_done = 0;
clock_t overall_start = 0;

/* -----------------------------------------------
    global variables: data storage
    ----------------------------------------------- */

size_t g_decompression_memory_bound = 0;
Sirikata::Array1d<Sirikata::Array1d<unsigned short, 64>, 4> qtables; // quantization tables
Sirikata::Array1d<Sirikata::Array1d<huffCodes, 4>, 2> hcodes; // huffman codes
Sirikata::Array1d<Sirikata::Array1d<huffTree, 4>, 2> htrees; // huffman decoding trees
Sirikata::Array1d<Sirikata::Array1d<unsigned char, 4>, 2> htset;// 1 if huffman table is set
bool embedded_jpeg = false;
unsigned char* grbgdata            =     NULL;    // garbage data
unsigned char* hdrdata          =   NULL;   // header data
unsigned char* huffdata         =   NULL;   // huffman coded data
int            hufs             =    0  ;   // size of huffman data
uint32_t       hdrs             =    0  ;   // size of header
uint32_t       zlib_hdrs        =    0  ;   // size of compressed header
size_t         total_framebuffer_allocated = 0; // framebuffer allocated
int            grbs             =    0  ;   // size of garbage
int            prefix_grbs = 0; // size of prefix;
unsigned char *prefix_grbgdata = NULL; // if prefix_grb is specified, header is not prepended

std::vector<unsigned int>  rstp;   // restart markers positions in huffdata
std::vector<unsigned int>  scnp;   // scan start positions in huffdata
int            rstc             =    0  ;   // count of restart markers
int            scnc             =    0  ;   // count of scans
int            rsti             =    0  ;   // restart interval
int8_t         padbit           =    -1 ;   // padbit (for huffman coding)
std::vector<unsigned char> rst_err;   // number of wrong-set RST markers per scan
std::vector<unsigned int> rst_cnt;
bool rst_cnt_set = false;
int            max_file_size    =    0  ;   // support for truncated jpegs 0 means full jpeg
size_t            start_byte       =    0;     // support for producing a slice of jpeg
size_t         jpeg_embedding_offset = 0;
unsigned int min_encode_threads = 1;
size_t max_encode_threads = 
#ifdef DEFAULT_SINGLE_THREAD
                                         1
#else
                                         MAX_NUM_THREADS
#endif
                                         ;
UncompressedComponents colldata; // baseline sorted DCT coefficients



/* -----------------------------------------------
    global variables: info about image
    ----------------------------------------------- */

// seperate info for each color component
Sirikata::Array1d<componentInfo, 4> cmpnfo;

int cmpc        = 0; // component count
int imgwidth    = 0; // width of image
int imgheight   = 0; // height of image

int sfhm        = 0; // max horizontal sample factor
int sfvm        = 0; // max verical sample factor
int mcuv        = 0; // mcus per line
unsigned int mcuh        = 0; // mcus per collumn
int mcuc        = 0; // count of mcus
bool early_eof_encountered = false;

int max_cmp = 0; // the maximum component in a truncated image
int max_bpos = 0; // the maximum band in a truncated image
int max_dpos[4] = {}; // the maximum dpos in a truncated image
int max_sah = 0; // the maximum bit in a truncated image


void standard_eof(abytewriter* hdrw, abytewriter* huffw) {
    // get pointer for header data & size
    hdrdata  = hdrw->getptr_aligned();
    hdrs     = hdrw->getpos();
    // get pointer for huffman data & size
    huffdata = huffw->getptr_aligned();
    hufs     = huffw->getpos();
}

void early_eof(abytewriter* hdrw, abytewriter* huffw) {
    early_eof_encountered = true;
    standard_eof(hdrw, huffw);
}


/* -----------------------------------------------
    global variables: info about current scan
    ----------------------------------------------- */

int cs_cmpc      =   0  ; // component count in current scan
Sirikata::Array1d<int, 4> cs_cmp = {{ 0 }}; // component numbers  in current scan
int cs_from      =   0  ; // begin - band of current scan ( inclusive )
int cs_to        =   0  ; // end - band of current scan ( inclusive )
int cs_sah       =   0  ; // successive approximation bit pos high
int cs_sal       =   0  ; // successive approximation bit pos low
void kill_workers(void * workers, uint64_t num_workers);
BaseDecoder* g_decoder = NULL;
GenericWorker * get_worker_threads(unsigned int num_workers) {
    // in this case decoding is asymmetric to encoding, just forget the assert
    if (NUM_THREADS < 2) {
        return NULL;
    }
    GenericWorker* retval = GenericWorker::get_n_worker_threads(num_workers);
    TimingHarness::timing[0][TimingHarness::TS_THREAD_STARTED] = TimingHarness::get_time_us();

    return retval;
}

template <class BoolDecoder>VP8ComponentDecoder<BoolDecoder> *makeBoth(bool threaded, bool start_workers) {
    VP8ComponentDecoder<BoolDecoder> *retval = new VP8ComponentDecoder<BoolDecoder>(threaded);
    TimingHarness::timing[0][TimingHarness::TS_MODEL_INIT] = TimingHarness::get_time_us();
    if (start_workers) {
        retval->registerWorkers(get_worker_threads(
                                    NUM_THREADS
                                    ),
                                NUM_THREADS
            );
    }
    return retval;
}

template <class BoolDecoder>BaseEncoder *makeEncoder(bool threaded, bool start_workers) {
    TimingHarness::timing[0][TimingHarness::TS_MODEL_INIT_BEGIN] = TimingHarness::get_time_us();
    VP8ComponentEncoder<BoolDecoder> * retval = new VP8ComponentEncoder<BoolDecoder>(threaded, IsDecoderAns<BoolDecoder>::IS_ANS);
    TimingHarness::timing[0][TimingHarness::TS_MODEL_INIT] = TimingHarness::get_time_us();
    if (start_workers) {
        retval->registerWorkers(get_worker_threads(NUM_THREADS - 1), NUM_THREADS - 1);
    }
    return retval;
}
BaseDecoder *makeDecoder(bool threaded, bool start_workers, bool ans) {
    if (ans) {
#ifdef ENABLE_ANS_EXPERIMENTAL
        return makeBoth<ANSBoolReader>(threaded, start_workers);
#else
        always_assert(false && "ANS compile flag not selected");
#endif
    }
    return makeBoth<VPXBoolReader>(threaded, start_workers);
}
/* -----------------------------------------------
    global variables: info about files
    ----------------------------------------------- */
int    jpgfilesize;            // size of JPEG file
int    ujgfilesize;            // size of UJG file
int    jpegtype = 0;        // type of JPEG coding: 0->unknown, 1->sequential, 2->progressive
F_TYPE filetype;            // type of current file
F_TYPE ofiletype = LEPTON;            // desired type of output file
bool g_do_preload = false;
std::unique_ptr<BaseEncoder> g_encoder;

std::unique_ptr<BaseDecoder> g_reference_to_free;
ServiceInfo g_socketserve_info;
bool g_threaded = true;
// this overrides the progressive bit in the header so that legacy progressive files may be decoded
bool g_force_progressive = false;
bool g_allow_progressive = 
#ifdef DEFAULT_ALLOW_PROGRESSIVE
    true
#else
    false
#endif
    ;
bool g_unkillable = false;
uint64_t g_time_bound_ms = 0;
int g_inject_syscall_test = 0;
bool g_force_zlib0_out = false;

Sirikata::DecoderReader* str_in  = NULL;    // input stream
bounded_iostream* str_out = NULL;    // output stream
// output stream
IOUtil::FileWriter * ujg_out = NULL;
IOUtil::FileReader * ujg_base_in = NULL;

const char** filelist = NULL;        // list of files to process
int    file_cnt = 0;        // count of files in list (1 for input only)
int    file_no  = 0;        // number of current file
/* -----------------------------------------------
    global variables: messages
    ----------------------------------------------- */

std::string errormessage;
std::atomic<int> errorlevel(0);
// meaning of errorlevel:
// -1 -> wrong input
// 0 -> no error
// 1 -> warning
// 2 -> fatal error


/* -----------------------------------------------
    global variables: settings
    ----------------------------------------------- */

int  verbosity  = 0;        // level of verbosity
bool overwrite  = false;    // overwrite files yes / no
int  err_tresh  = 1;        // error threshold ( proceed on warnings yes (2) / no (1) )
bool disc_meta  = false;    // discard meta-info yes / no

bool developer  = false;    // allow developers functions yes/no
ACTION action   = comp;        // what to do with JPEG/UJG files

FILE*  msgout   = stderr;    // stream for output of messages
bool   pipe_on  = false;    // use stdin/stdout instead of filelist



void sig_nop(int){}
/* -----------------------------------------------
    global variables: info about program
    ----------------------------------------------- */

unsigned char ujgversion   = 1;
bool g_even_thread_split = false;
uint8_t get_current_file_lepton_version() {
    return ujgversion;
}
static const char*  appname      = "lepton";
static const unsigned char   ujg_header[] = { 'U', 'J' };
static const unsigned char   lepton_header[] = { 0xcf, 0x84 }; // the tau symbol for a tau lepton in utf-8
static const unsigned char   zlepton_header[] = { 0xce, 0xb6 }; // the zeta symbol for a zlib compressed lepton


FILE * timing_log = NULL;
char current_operation = '\0';
#ifdef _WIN32
clock_t current_operation_begin = 0;
clock_t current_operation_first_byte = 0;
clock_t current_operation_end = 0;
#else
struct timeval current_operation_begin = {0, 0};
struct timeval current_operation_first_byte = {0, 0};
struct timeval current_operation_end = {0, 0};
#endif

void timing_operation_start( char operation ) {
#ifndef _WIN32
    if (g_use_seccomp) {
        return;
    }
    current_operation = operation;
#ifdef _WIN32
    current_operation_begin = clock();
    current_operation_first_byte = 0;
    current_operation_end = 0;
#else
    gettimeofday(&current_operation_begin, NULL);
    memset(&current_operation_first_byte, 0, sizeof(current_operation_first_byte));
    memset(&current_operation_end, 0, sizeof(current_operation_end));
#endif
    fprintf(stderr,"START ACHIEVED %ld %ld\n",
            (long)current_operation_begin.tv_sec, (long)current_operation_begin.tv_usec );
#endif
}

void timing_operation_first_byte( char operation ) {
#ifndef _WIN32
    if (g_use_seccomp) {
        return;
    }
    dev_assert(current_operation == operation);
#ifdef _WIN32
    if (current_operation_first_byte == 0) {
        current_operation_first_byte = clock();
    }
#else
    if (current_operation_first_byte.tv_sec == 0 &&
        current_operation_first_byte.tv_usec == 0) {
        gettimeofday(&current_operation_first_byte, NULL);
    }
#endif
#endif
}

void timing_operation_complete( char operation ) {
#ifndef _WIN32
    if (g_use_seccomp) {
        return;
    }
    dev_assert(current_operation == operation);
#ifdef _WIN32
    current_operation_end = clock();
    if (timing_log) {
        double begin_to_end = (current_operation_end - current_operation_begin) / (double)CLOCKS_PER_SEC;
        double begin_to_first_byte = begin_to_end;
        if (current_operation_first_byte != 0) { // if we were successful
            begin_to_first_byte = (current_operation_first_byte - current_operation_begin) / (double)CLOCKS_PER_SEC;
        }
        fprintf(timing_log, "%c %f %f\n", current_operation, begin_to_first_byte, begin_to_end);
        fflush(timing_log);
    }
    current_operation_end = 0;
    current_operation_begin = 0;
    current_operation_first_byte = 0;
#else
    gettimeofday(&current_operation_end, NULL);
    if (timing_log) {
        double begin = current_operation_begin.tv_sec + (double)current_operation_begin.tv_usec / 1000000.;
        double end = current_operation_end.tv_sec + (double)current_operation_end.tv_usec / 1000000.;
        double first_byte = current_operation_first_byte.tv_sec + (double)current_operation_first_byte.tv_usec / 1000000.;
        double begin_to_end = end - begin;
        double begin_to_first_byte = begin_to_end;
        if (current_operation_first_byte.tv_sec != 0) { // if we were successful
            begin_to_first_byte = first_byte - begin;
        }
        fprintf(timing_log, "%c %f %f\n", current_operation, begin_to_first_byte, begin_to_end);
        fflush(timing_log);
    }
    memset(&current_operation_end, 0, sizeof(current_operation_end));
    memset(&current_operation_begin, 0, sizeof(current_operation_begin));
    memset(&current_operation_first_byte, 0, sizeof(current_operation_first_byte));
#endif
#endif
}

size_t local_atoi(const char *data) {
    const char * odata = data;
    size_t retval = 0;
    int counter = 0;
    while (*data) {
        if (*data >= '0' && *data <='9') {
            retval *= 10;
            retval += *data - '0';
            ++data;
            ++counter;
            if (counter > 16) {
                fprintf(stderr, "Could not allocate so much memory %s\n", odata);
                exit(1);
            }
        } else if ('M' == *data) {
            retval *= 1024 * 1024;
            break;
        } else if ('K' == *data) {
            retval *= 1024;
            break;
        } else {
            fprintf(stderr, "Could not allocate alphanumeric memory %s\n", odata);
            exit(1);
        }
    }
    return retval;
}
bool starts_with(const char * a, const char * b) {
    while (*b) {
        if (*a != *b) {
            return false;
        }
        ++a;
        ++b;
    }
    return true;
}
void compute_thread_mem(const char * arg,
                        size_t * mem_init,
                        size_t * thread_mem_init,
                        bool *needs_huge_pages,
                        bool *avx2upgrade) {
    if (strcmp(arg, "-hugepages") == 0) {
        *needs_huge_pages = true;
    }
    if ( strcmp(arg, "-avx2upgrade") == 0) {
        *avx2upgrade = true;
    }
    if (strstr(arg, "-help")) {
        show_help();
        exit(0);
    }
    if (strcmp(arg, "-h") == 0) {
        show_help();
        exit(0);
    }
    const char mem_arg_name[]="-memory=";
    const char thread_mem_arg_name[]="-threadmemory=";
    if (starts_with(arg, mem_arg_name)) {
        arg += strlen(mem_arg_name);
        *mem_init = local_atoi(arg);
    }
    if (starts_with(arg, thread_mem_arg_name)) {
        arg += strlen(thread_mem_arg_name);
        *thread_mem_init = local_atoi(arg);
    }
}
/* -----------------------------------------------
    main-function
    ----------------------------------------------- */

#ifdef EMSCRIPTEN
const char *fake_argv[] =  {
    "lepton-scalar",
    "-skipverify",
    "-singlethread",
    "-",
};

const int fake_argc = sizeof(fake_argv) / sizeof(char *);
int EMSCRIPTEN_KEEPALIVE main(void) {
    const int argc = fake_argc;
    const char **argv = fake_argv;
    g_argc = argc;
    g_argv = argv;
    TimingHarness::timing[0][TimingHarness::TS_MAIN]
        = TimingHarness::get_time_us(true);
    size_t thread_mem_limit = 128 * 1024 * 1024;
    size_t mem_limit = 1280 * 1024 * 1024 - thread_mem_limit * (MAX_NUM_THREADS - 1);
    bool needs_huge_pages = false;
    for (int i = 1; i < argc; ++i) {
        bool avx2upgrade = false;
        compute_thread_mem(argv[i],
                           &mem_limit,
                           &thread_mem_limit,
                           &needs_huge_pages,
                           &avx2upgrade);
    }

    // the system needs 33 megs of ram ontop of the uncompressed image buffer.
    // This adds a few extra megs just to keep things real
    UncompressedComponents::max_number_of_blocks = ( mem_limit / 4 ) * 3;
    if (mem_limit > 48 * 1024 * 1024) {
        UncompressedComponents::max_number_of_blocks = mem_limit - 36 * 1024 * 1024;
    }
    UncompressedComponents::max_number_of_blocks /= (sizeof(uint16_t) * 64);
    int n_threads = MAX_NUM_THREADS - 1;
    clock_t begin = 0, end = 1;

    int error_cnt = 0;
    int warn_cnt  = 0;

    int acc_jpgsize = 0;
    int acc_ujgsize = 0;

    int speed, bpms;
    float cr;

    errorlevel.store(0);

    // read options from command line
    int max_file_size = initialize_options( argc, argv );
    if (action != forkserve && action != socketserve) {
        // write program info to screen
        fprintf( msgout,  "%s v%i.0-%s\n",
                 appname, ujgversion, GIT_REVISION );
    }
    // check if user input is wrong, show help screen if it is
    if ((file_cnt == 0 && action != forkserve && action != socketserve)
        || ((!developer) && ((action != comp && action != forkserve && action != socketserve)))) {
        show_help();
        return -1;
    }


    // (re)set program has to be done first
    reset_buffers();

    // process file(s) - this is the main function routine
    begin = clock();
    if (file_cnt > 2) {
        show_help();
        custom_exit(ExitCode::FILE_NOT_FOUND);
    }
    process_file(nullptr, nullptr, max_file_size, g_force_zlib0_out);
    if (errorlevel.load() >= err_tresh) error_cnt++;
    if (errorlevel.load() == 1 ) warn_cnt++;
    if ( errorlevel.load() < err_tresh ) {
        acc_jpgsize += jpgfilesize;
        acc_ujgsize += ujgfilesize;
    }
    if (!g_use_seccomp) {
        end = clock();
    }
    if (action != socketserve && action != forkserve) {
        // show statistics
        fprintf(msgout,  "\n\n-> %i file(s) processed, %i error(s), %i warning(s)\n",
                file_cnt, error_cnt, warn_cnt);
    }
    if ( ( file_cnt > error_cnt ) && ( verbosity > 0 ) )
    if ( action == comp ) {
        speed = (int) ( (double) (( end - begin ) * 1000) / CLOCKS_PER_SEC );
        bpms  = ( speed > 0 ) ? ( acc_jpgsize / speed ) : acc_jpgsize;
        cr    = ( acc_jpgsize > 0 ) ? ( 100.0 * acc_ujgsize / acc_jpgsize ) : 0;

        fprintf( msgout,  " --------------------------------- \n" );
        fprintf( msgout,  " time taken        : %8i msec\n", speed );
        fprintf( msgout,  " avrg. byte per ms : %8i byte\n", bpms );
        fprintf( msgout,  " avrg. comp. ratio : %8.2f %%\n", cr );
        fprintf( msgout,  " --------------------------------- \n" );
    }
    return error_cnt == 0 ? 0 : 1;
}
#else
int app_main( int argc, char** argv )
{
    g_argc = argc;
    g_argv = (const char **)argv;
    TimingHarness::timing[0][TimingHarness::TS_MAIN]
        = TimingHarness::get_time_us(true);
    size_t thread_mem_limit = 
#ifdef HIGH_MEMORY
        64 * 1024 * 1024
#else
        3 * 1024 * 1024
#endif
        ;//8192;
    size_t mem_limit = 
#ifdef HIGH_MEMORY
        1024 * 1024 * 1024 - thread_mem_limit * (MAX_NUM_THREADS - 1)
#else
        176 * 1024 * 1024 - thread_mem_limit * (MAX_NUM_THREADS - 1)
#endif
        ;
    bool needs_huge_pages = false;
    for (int i = 1; i < argc; ++i) {
        bool avx2upgrade = false;
        compute_thread_mem(argv[i],
                           &mem_limit,
                           &thread_mem_limit,
                           &needs_huge_pages,
                           &avx2upgrade);
#ifndef __AVX2__
#ifndef __clang__
#ifndef __aarch64__
#ifndef _ARCH_PPC        
#ifndef _WIN32
        if (avx2upgrade &&
            __builtin_cpu_supports("avx2")
) {
            for (int j = i + 1; j < argc; ++j) {
                argv[j - 1] = argv[j];
            }
            --argc;
            argv[argc] = NULL; // since we have eliminated the upgrade arg...
            size_t command_len = strlen(argv[0]);
            size_t postfix_len = strlen("-avx") + 1;
            char * command = (char*)malloc(postfix_len + command_len);
            memcpy(command, argv[0], command_len);
            memcpy(command + command_len, "-avx", postfix_len);
            char * old_command = argv[0];
            argv[0] = command;
            execvp(command, argv);
            argv[0] = old_command; // exec failed
        }
#endif
#endif
#endif
#endif
#endif
    }

    // the system needs 33 megs of ram ontop of the uncompressed image buffer.
    // This adds a few extra megs just to keep things real
    UncompressedComponents::max_number_of_blocks = ( mem_limit / 4 ) * 3;
    if (mem_limit > 48 * 1024 * 1024) {
        UncompressedComponents::max_number_of_blocks = mem_limit - 36 * 1024 * 1024;
    }
    UncompressedComponents::max_number_of_blocks /= (sizeof(uint16_t) * 64);
    int n_threads = MAX_NUM_THREADS;
#ifndef __linux__
    n_threads += 4;
#endif
#if !defined(_WIN32) && !defined(EMSCRIPTEN)
    Sirikata::memmgr_init(mem_limit,
                          thread_mem_limit,
                          n_threads,
                          256,
                          needs_huge_pages);
#endif
    clock_t begin = 0, end = 1;

    int error_cnt = 0;
    int warn_cnt  = 0;

    int acc_jpgsize = 0;
    int acc_ujgsize = 0;

    int speed, bpms;
    float cr;

    errorlevel.store(0);

    // read options from command line
    int max_file_size = initialize_options( argc, argv );
    if (action != forkserve && action != socketserve) {
        // write program info to screen
        fprintf( msgout,  "%s v%i.0-%s\n",
                 appname, ujgversion, GIT_REVISION );
    }
    // check if user input is wrong, show help screen if it is
    if ((file_cnt == 0 && action != forkserve && action != socketserve)
        || ((!developer) && ((action != lepton_concatenate && action != comp && action != forkserve && action != socketserve)))) {
        show_help();
        return -1;
    }


    // (re)set program has to be done first
    reset_buffers();

    // process file(s) - this is the main function routine
    begin = clock();
    if (file_cnt > 2 && action != lepton_concatenate) {
        show_help();
        custom_exit(ExitCode::FILE_NOT_FOUND);
    }
    if (action == forkserve) {
#ifdef _WIN32
        abort(); // not implemented
#else
        fork_serve();
#endif
    } else if (action == socketserve) {
#ifdef _WIN32
        abort(); // not implemented
#else
        socket_serve(&process_file, max_file_size, g_socketserve_info);
#endif
    } else {
        process_file(nullptr, nullptr, max_file_size, g_force_zlib0_out);
    }
    if (errorlevel.load() >= err_tresh) error_cnt++;
    if (errorlevel.load() == 1 ) warn_cnt++;
    if ( errorlevel.load() < err_tresh ) {
        acc_jpgsize += jpgfilesize;
        acc_ujgsize += ujgfilesize;
    }
    if (!g_use_seccomp) {
        end = clock();
    }
    if (action != socketserve && action != forkserve) {
        // show statistics
        fprintf(msgout,  "\n\n-> %i file(s) processed, %i error(s), %i warning(s)\n",
                file_cnt, error_cnt, warn_cnt);
    }
    if ( ( file_cnt > error_cnt ) && ( verbosity > 0 ) )
    if ( action == comp ) {
        speed = (int) ( (double) (( end - begin ) * 1000) / CLOCKS_PER_SEC );
        bpms  = ( speed > 0 ) ? ( acc_jpgsize / speed ) : acc_jpgsize;
        cr    = ( acc_jpgsize > 0 ) ? ( 100.0 * acc_ujgsize / acc_jpgsize ) : 0;

        fprintf( msgout,  " --------------------------------- \n" );
        fprintf( msgout,  " time taken        : %8i msec\n", speed );
        fprintf( msgout,  " avrg. byte per ms : %8i byte\n", bpms );
        fprintf( msgout,  " avrg. comp. ratio : %8.2f %%\n", cr );
        fprintf( msgout,  " --------------------------------- \n" );
    }


    return error_cnt == 0 ? 0 : 1;
}
#endif

/* ----------------------- Begin of main interface functions -------------------------- */

/* -----------------------------------------------
    reads in commandline arguments
    ----------------------------------------------- */
char g_dash[] = "-";
// returns the maximum file size
int initialize_options( int argc, const char*const * argv )
{
    const char** tmp_flp;
    int tmp_val;
    int max_file_size = 0;
    // get memory for filelist & preset with NULL
    filelist = (const char**)custom_calloc(argc * sizeof(char*));

    // preset temporary filelist pointer
    tmp_flp = filelist;
    // read in arguments
    while ( --argc > 0 ) {
        argv++;
        // switches begin with '-'
        if ( sscanf( (*argv), "-v%i", &tmp_val ) == 1 ){
            verbosity = tmp_val;
            verbosity = ( verbosity < 0 ) ? 0 : verbosity;
            verbosity = ( verbosity > 2 ) ? 2 : verbosity;
        }
        else if ( strcmp((*argv), "-o" ) == 0 ) {
            overwrite = true;
        }
        else if (strcmp((*argv), "-revision" ) == 0 || strcmp((*argv), "--revision") == 0) {
            printf("%s\n", GIT_REVISION);
            exit(0);
        }
        else if (strcmp((*argv), "-version" ) == 0 || strcmp((*argv), "--version") == 0) {
            printf("%02x\n", ujgversion);
            exit(0);
        } else if ( strcmp((*argv), "-preload" ) == 0 ) {
            g_do_preload = true;
        } else if ( strcmp((*argv), "-decode" ) == 0 ) { // deprecated commands to preload it all
            g_do_preload = true;
        } else if ( strcmp((*argv), "-recode" ) == 0 ) {
            g_do_preload = true;
        } else if ( strcmp((*argv), "-p" ) == 0 ) {
            err_tresh = 2;
        }
        else if ( strncmp((*argv), "-timebound=", strlen("-timebound=")) == 0) {
            char * endptr = NULL;
            g_time_bound_ms = strtoll((*argv) + strlen("-timebound="), &endptr, 10);
            if (endptr) {
                if (strcmp(endptr, "s") == 0) {
                    g_time_bound_ms *= 1000;
                } else if (strcmp(endptr, "us") == 0) {
                    g_time_bound_ms /= 1000;
                } else if (strcmp(endptr, "ms") != 0) {
                    fprintf(stderr, "Time must have units (ms or s)\n");
                    exit(1);
                }
            }
        }
        else if ( strcmp((*argv), "-zlib0" ) == 0)  {
            g_force_zlib0_out = true;
        }
        else if ( strcmp((*argv), "-unkillable" ) == 0)  {
            g_unkillable = true;
        }
        else if ( strcmp((*argv), "-singlethread" ) == 0)  {
            g_threaded = false;
        }
        else if ( strcmp((*argv), "-allowprogressive" ) == 0)  {
            g_allow_progressive = true;
        }
        else if ( strcmp((*argv), "-forceprogressive" ) == 0)  {
            g_allow_progressive = true;
            g_force_progressive = true;
        }
        else if ( strcmp((*argv), "-rejectprogressive" ) == 0)  {
            g_allow_progressive = false;
        }
        else if ( strcmp((*argv), "-unjailed" ) == 0)  {
            g_use_seccomp = false;
        } else if ( strcmp((*argv), "-multithread" ) == 0 || strcmp((*argv), "-m") == 0)  {
            g_threaded = true;
        } else if ( strcmp((*argv), "-evensplit" ) == 0)  {
            g_even_thread_split = true;
        } else if ( strstr((*argv), "-recodememory=") == *argv ) {
            g_decompression_memory_bound
                = local_atoi(*argv + strlen("-recodememory="));
        } else if ( strstr((*argv), "-memory=") == *argv ) {

        } else if ( strstr((*argv), "-hugepages") == *argv ) {

        } else if ( strstr((*argv), "-defermd5") == *argv ) {

        } else if ( strstr((*argv), "-avx2upgrade") == *argv ) {

        } else if ( strstr((*argv), "-threadmemory=") == *argv ) {

        } else if ( strncmp((*argv), "-timing=", strlen("-timing=") ) == 0 ) {
            timing_log = fopen((*argv) + strlen("-timing="), "a");
        } else if (strncmp((*argv), "-maxencodethreads=", strlen("-maxencodethreads=") ) == 0 ) {
            max_encode_threads = local_atoi((*argv) + strlen("-maxencodethreads="));
            if (max_encode_threads > MAX_NUM_THREADS) {
                custom_exit(ExitCode::VERSION_UNSUPPORTED);
            }
        } else if (strcmp((*argv), "-lepcat") == 0) {
            action = lepton_concatenate;
        } else if (strncmp((*argv), "-minencodethreads=", strlen("-minencodethreads=") ) == 0 ) {
            min_encode_threads = local_atoi((*argv) + strlen("-minencodethreads="));
        } else if ( strncmp((*argv), "-injectsyscall=", strlen("-injectsyscall=") ) == 0 ) {
            g_inject_syscall_test = strtol((*argv) + strlen("-injectsyscall="), NULL, 10);
        } else if ( strcmp((*argv), "-skipvalidation") == 0 ) {
            g_skip_validation = true;
        } else if ( strcmp((*argv), "-skipvalidate") == 0 ) {
            g_skip_validation = true;
        } else if ( strcmp((*argv), "-skipverify") == 0 ) {
            g_skip_validation = true;
        } else if ( strcmp((*argv), "-skipverification") == 0 ) {
            g_skip_validation = true;
        } else if ( strcmp((*argv), "-skiproundtrip") == 0 ) {
            g_skip_validation = true;
        } else if ( strcmp((*argv), "-validate") == 0 ) {
            g_skip_validation = false;
        } else if ( strcmp((*argv), "-validation") == 0 ) {
            g_skip_validation = false;
        } else if ( strcmp((*argv), "-verify") == 0 ) {
            g_skip_validation = false;
        } else if ( strcmp((*argv), "-verification") == 0 ) {
            g_skip_validation = false;
        } else if ( strcmp((*argv), "-roundtrip") == 0 ) {
            g_skip_validation = false;
        } else if ( strcmp((*argv), "-permissive") == 0 ) {
            g_permissive = true;
#ifndef _WIN32
            signal(SIGPIPE, SIG_IGN);
#endif
        } else if ( strcmp((*argv), "-brotliheader") == 0 ) {
            if (ujgversion < 2) {
                ujgversion = 2; // use brotli to compress the header and trailer rather than zlib
            }
        } else if ( strcmp((*argv), "-ans") == 0 ) {
#ifdef ENABLE_ANS_EXPERIMENTAL
            ujgversion = 3; // use brotli to compress the header and trailer rather than zlib and ANS encoder/decoder
#else
            always_assert(false && "ANS selected via command line but not enabled in build flags");
#endif          
        } else if ( strncmp((*argv), "-maxchildren=", strlen("-maxchildren=") ) == 0 ) {
            g_socketserve_info.max_children = strtol((*argv) + strlen("-maxchildren="), NULL, 10);
        }
        else if ( strncmp((*argv), "-listenbacklog=", strlen("-listenbacklog=") ) == 0 ) {
            g_socketserve_info.listen_backlog = strtol((*argv) + strlen("-listenbacklog="), NULL, 10);
        }
        else if ( strncmp((*argv), "-startbyte=", strlen("-startbyte=") ) == 0 ) {
            start_byte = local_atoi((*argv) + strlen("-startbyte="));
        }        
        else if ( strncmp((*argv), "-embedding=", strlen("-embedding=") ) == 0 ) {
            jpeg_embedding_offset = local_atoi((*argv) + strlen("-embedding="));
            embedded_jpeg = true;
        }
        else if ( strncmp((*argv), "-trunc=", strlen("-trunc=") ) == 0 ) {
            max_file_size = local_atoi((*argv) + strlen("-trunc="));
        }
        else if ( strncmp((*argv), "-trunctiming=", strlen("-trunctiming=") ) == 0 ) {
            timing_log = fopen((*argv) + strlen("-trunctiming="), "w");
        }
        else if ( strcmp((*argv), "-d" ) == 0 ) {
            disc_meta = true;
        }
        else if ( strcmp((*argv), "-dev") == 0 ) {
            developer = true;
        } else if ( ( strcmp((*argv), "-ujg") == 0 ) ||
                    ( strcmp((*argv), "-ujpg") == 0 )) {
            fprintf(stderr, "FOUND UJG ARG: using that as output\n");
            action = comp;
            ofiletype = UJG;
#ifndef _WIN32
        } else if ( strcmp((*argv), "-fork") == 0 ) {    
            action = forkserve;
            // sets it up in serving mode
            msgout = stderr;
            // use "-" as placeholder for the socket
            *(tmp_flp++) = g_dash;
        } else if ( strncmp((*argv), "-socket", strlen("-socket")) == 0 ) {
            if (action != socketserve) {
                action = socketserve;
                // sets it up in serving mode
                msgout = stderr;
                // use "-" as placeholder for the socket
                *(tmp_flp++) = g_dash;
            }
            if ((*argv)[strlen("-socket")] == '=') {
                g_socketserve_info.uds = (*argv) + strlen("-socket=");
            }
        } else if ( strncmp((*argv), "-listen", strlen("-listen")) == 0 ) {
            g_socketserve_info.listen_tcp = true;
            if (action != socketserve) {
                action = socketserve;
                // sets it up in serving mode
                msgout = stderr;
                // use "-" as placeholder for the socket
                *(tmp_flp++) = g_dash;
            }
            if ((*argv)[strlen("-listen")] == '=') {
                g_socketserve_info.port = atoi((*argv) + strlen("-listen="));
            }
        } else if ( strncmp((*argv), "-zliblisten", strlen("-zliblisten")) == 0 ) {
            g_socketserve_info.zlib_port = atoi((*argv) + strlen("-zliblisten="));
#endif
        } else if ( strcmp((*argv), "-") == 0 ) {    
            msgout = stderr;
            // set binary mode for stdin & stdout
            #ifdef _WIN32
                setmode( fileno( stdin ), O_BINARY );
                setmode( fileno( stdout ), O_BINARY );
            #endif
            // use "-" as placeholder for stdin
            *(tmp_flp++) = g_dash;
        }
        else {
            // if argument is not switch, it's a filename
            *(tmp_flp++) = *argv;
        }
    }
    for ( file_cnt = 0; filelist[ file_cnt ] != NULL; file_cnt++ ) {
    }
    if (start_byte != 0) {
        // Encode of partial progressive images not allowed
        g_allow_progressive = false;
    }
    if (g_time_bound_ms && action == forkserve) {
        fprintf(stderr, "Time bound action only supported with UNIX domain sockets\n");
        exit(1);
    }
    if (g_do_preload && g_skip_validation) {
        VP8ComponentDecoder<VPXBoolReader> *d = makeBoth<VPXBoolReader>(g_threaded, g_threaded && action != forkserve && action != socketserve);
        g_encoder.reset(d);
        g_decoder = d;
    }
    return max_file_size;
}
size_t decompression_memory_bound() {
    if (ofiletype == UJG || filetype == UJG) {
        return 0;
    }
    size_t cumulative_buffer_size = 0;
    size_t streaming_buffer_size = 0;
    size_t current_run_size = 0;
    for (int i = 0; i < colldata.get_num_components(); ++i) {
        size_t streaming_size = 
            colldata.block_width(i)
            * 2 * NUM_THREADS * 64 * sizeof(uint16_t);
        size_t frame_buffer_size = colldata.component_size_allocated(i);
        if (cs_cmpc != colldata.get_num_components() || jpegtype != 1) {
            streaming_size = frame_buffer_size;
        } else if (filetype != JPEG) {
            if (!g_threaded) {
                frame_buffer_size = colldata.block_width(i) * 2 * 64 * sizeof(uint16_t);

            } else {
                frame_buffer_size = streaming_size;
            }
        }
        cumulative_buffer_size += frame_buffer_size;
        streaming_buffer_size += streaming_size;
    }
    current_run_size = cumulative_buffer_size;

    size_t bit_writer_augmentation = 0;
    if (g_allow_progressive) {
        for (size_t cur_size = jpgfilesize - 1; cur_size; cur_size >>=1) {
            bit_writer_augmentation |= cur_size;
        }
        bit_writer_augmentation += 1; // this is used to compute the buffer size of the abit_writer for writing
    }
    size_t garbage_augmentation = 0;
    for (size_t cur_size = hdrs - 1; cur_size; cur_size >>=1) {
        garbage_augmentation |= cur_size;
    }
    garbage_augmentation += 1; // this is used to compute the buffer size of the abit_writer for writing
    int non_preloaded_mux = 4096 * 1024 + 131072; // only 1 thread hence only one extra 131072
    size_t decode_header_needed_size = hdrs + zlib_hdrs * 3;
    if (zlib_hdrs && zlib_hdrs * 2 < hdrs) {
        size_t doubled = zlib_hdrs * 2;
        do {
            decode_header_needed_size += doubled;
            doubled *= 2;
        } while (doubled < (size_t)hdrs);
    }
    size_t single_threaded_model_bonus = 0;
    size_t single_threaded_buffer_bonus = 0; //the threads have to save their output to 3/4 of the jpeg before writing it
    if (g_decoder) {
        single_threaded_model_bonus += g_decoder->get_model_worker_memory_usage();
    } else if (g_encoder) {
        single_threaded_model_bonus += g_encoder->get_decode_model_worker_memory_usage();
    }
    if (filetype != JPEG && !g_threaded) {
        single_threaded_buffer_bonus += jpgfilesize;
    }
    size_t abit_writer = 0;
    if (g_allow_progressive) {

        if (zlib_hdrs * 3 < ABIT_WRITER_PRELOAD * 2 + 64) {
            if (zlib_hdrs * 3 < ABIT_WRITER_PRELOAD + 64) {
                abit_writer += ABIT_WRITER_PRELOAD * 2 + 64;// these can't be reused memory
            } else {
                abit_writer += ABIT_WRITER_PRELOAD + 64;// these can't be reused
            }
        }
    } else {
        abit_writer += 65536 + 64;
    }
    if (g_allow_progressive &&
        jpgfilesize > ABIT_WRITER_PRELOAD) {
        // we currently buffer the whole jpeg in memory while streaming out
        abit_writer += 3 * jpgfilesize;
    }
    size_t total = Sirikata::memmgr_size_allocated();
    ptrdiff_t decom_memory_bound = total;
    decom_memory_bound -= current_run_size;
    decom_memory_bound += streaming_buffer_size;
    decom_memory_bound -= single_threaded_model_bonus;
    decom_memory_bound += single_threaded_buffer_bonus;
    if (decom_memory_bound < 1){
        decom_memory_bound = 1;
    }
    if (filetype == JPEG) {
        decom_memory_bound = streaming_buffer_size
            + abit_writer + jpgfilesize + sizeof(ProbabilityTablesBase)
            + garbage_augmentation + decode_header_needed_size + non_preloaded_mux;
    }
    return decom_memory_bound;
}

void check_decompression_memory_bound_ok() {
    if (g_decompression_memory_bound) {
        size_t adjustment = 0;
        if (!uninit_g_zlib_0_writer) {
            adjustment = 8192; // add an extra 8kb if we're decoding zlib
        }
        if (decompression_memory_bound() > g_decompression_memory_bound + adjustment) {
            custom_exit(ExitCode::TOO_MUCH_MEMORY_NEEDED);
        }
    }
}
void test_syscall_injection(std::atomic<int>*value) {
#ifndef _WIN32
    char buf[128 + 1];
    buf[sizeof(buf) - 1] = 0;
    value->store(-1);
    char * ret = getcwd(buf, sizeof(buf) - 1);
    value->store(ret ? 1 : 2);
#endif
}
bool recode_baseline_jpeg_wrapper() {
    bool retval = recode_baseline_jpeg(str_out, max_file_size);
    if (!retval) {
        errorlevel.store(2);
        return retval;
    }
    // get filesize
    jpgfilesize = str_out->getsize();
    if (ujg_base_in) {
        ujgfilesize = ujg_base_in->getsize();
    } else {
        ujgfilesize = 4096 * 1024;
    }
#ifndef _WIN32
    if (!g_use_seccomp) {
        clock_t final = clock();
        struct timeval fin = {0,0};
        gettimeofday(&fin,NULL);
        double begin = current_operation_begin.tv_sec + (double)current_operation_begin.tv_usec / 1000000.;
        double end = fin.tv_sec + (double)fin.tv_usec / 1000000.;
        double first_byte = current_operation_first_byte.tv_sec + (double)current_operation_first_byte.tv_usec / 1000000.;
        double begin_to_end = end - begin;
        double begin_to_first_byte = begin_to_end;
        if (current_operation_first_byte.tv_sec != 0) { // if we were successful
            begin_to_first_byte = first_byte - begin;
        }

        fprintf(stderr, "TIMING (new method): %f to first byte %f total\n",
                begin_to_first_byte,
                begin_to_end);
        (void)final;
        fprintf(stderr, "Read took: %f\n",
                (read_done - overall_start)/(double)CLOCKS_PER_SEC);
    }
#endif
    // store last scan & restart positions
    if ( !rstp.empty() )
        rstp.at(rstc) = hufs;


    return retval;
}





int open_fdin(const char *ifilename,
              IOUtil::FileReader *reader,
              Sirikata::Array1d<uint8_t, 2> &header,
              ssize_t *bytes_read,
              bool *is_socket) {
    int fdin = -1;    
    if (reader != NULL) {
        *is_socket = reader->is_socket();
        fdin = reader->get_fd();
    }
    else if (strcmp(ifilename, "-") == 0) {
        fdin = 0;
        *is_socket = false;
    }
    else {
        *is_socket = false;
         do {
            fdin = open(ifilename, O_RDONLY
#ifdef _WIN32
                |O_BINARY
#endif
            );
        } while (fdin == -1 && errno == EINTR);
        if (fdin == -1) {
            const char * errormessage = "Input file unable to be opened for writing:";
            while(write(2, errormessage, strlen(errormessage)) == -1 && errno == EINTR) {}
            while(write(2, ifilename, strlen(ifilename)) == -1 && errno == EINTR) {}
            while(write(2, "\n", 1) == -1 && errno == EINTR) {}
        }
    }
    *bytes_read = 0;
    ssize_t data_read = 0;
    do {
        data_read = read(fdin, &header[0], 2);
    } while (data_read == -1 && errno == EINTR);
    if (data_read >= 0) {
        *bytes_read = data_read;
    }
    if (__builtin_expect(data_read < 2, false)) {
        do {
            data_read = read(fdin, &header[1], 1);
        } while (data_read == -1 && errno == EINTR);
        if (data_read >= 0) {
            *bytes_read += data_read;
        }
    }
    if (data_read < 0) {
        perror("read");
        const char * fail = "Failed to read 2 byte header\n";
        while(write(2, fail, strlen(fail)) == -1 && errno == EINTR) {}        
    }
    return fdin;
}

std::string uniq_filename(std::string filename) {
    FILE * fp = fopen(filename.c_str(), "rb");
    while (fp != NULL) {
        fclose(fp);
        filename += "_";
        fp = fopen(filename.c_str(), "rb");
    }
    return filename;
}

std::string postfix_uniq(const std::string &filename, const char * ext) {
    std::string::size_type where =filename.find_last_of("./\\");
    if (where == std::string::npos || filename[where] != '.') {
        return uniq_filename(filename + ext);
    }
    return uniq_filename(filename.substr(0, where) + ext);
}


int open_fdout(const char *ifilename,
                    IOUtil::FileWriter *writer,
               bool is_embedded_jpeg,
                    Sirikata::Array1d<uint8_t, 2> fileid,
                    bool force_compressed_output,
                    bool *is_socket) {
    if (writer != NULL) {
        *is_socket = writer->is_socket();
        return writer->get_fd();
    }
    *is_socket = false;
    if (strcmp(ifilename, "-") == 0) {
        return 1;
    }
    int retval = -1;
    std::string ofilename;
    // check file id, determine filetype
    if (file_no + 1 < file_cnt && ofilename != ifilename) {
        ofilename = filelist[file_no + 1];
    } else if (is_jpeg_header(fileid) || is_embedded_jpeg || g_permissive) {
        ofilename = postfix_uniq(ifilename, (ofiletype == UJG ? ".ujg" : ".lep"));
    } else if ( ( ( fileid[0] == ujg_header[0] ) && ( fileid[1] == ujg_header[1] ) )
                || ( ( fileid[0] == lepton_header[0] ) && ( fileid[1] == lepton_header[1] ) )
                || ( ( fileid[0] == zlepton_header[0] ) && ( fileid[1] == zlepton_header[1] ) ) ){
        if ((fileid[0] == zlepton_header[0] && fileid[1] == zlepton_header[1])
            || force_compressed_output) {
            ofilename = postfix_uniq(ifilename, ".jpg.z");
        } else {
            ofilename = postfix_uniq(ifilename, ".jpg");
        }
    }
    do {
        retval = open(ofilename.c_str(), O_WRONLY|O_CREAT|O_TRUNC
#ifdef _WIN32
            | O_BINARY
#endif
            , 0
#ifdef _WIN32
            | S_IREAD| S_IWRITE
#else
            | S_IWUSR | S_IRUSR
#endif
        );
    }while (retval == -1 && errno == EINTR);
    if (retval == -1) {
        const char * errormessage = "Output file unable to be opened for writing:";
        while(write(2, errormessage, strlen(errormessage)) == -1 && errno == EINTR) {}
        while(write(2, ofilename.c_str(), ofilename.length()) == -1 && errno == EINTR) {}
        while(write(2, "\n", 1) == -1 && errno == EINTR) {}
        custom_exit(ExitCode::FILE_NOT_FOUND);
    }
    return retval;
}


void prep_for_new_file() {
    r_bitcount = 0;
    if (prefix_grbgdata) {
        aligned_dealloc(prefix_grbgdata);
        prefix_grbgdata = NULL;
    }
    if (grbgdata && grbgdata != &EOI[0]) {
        aligned_dealloc(grbgdata);
        grbgdata = NULL;
    }

    prefix_grbs = 0;
    reset_buffers();
    auto cur_num_threads = read_fixed_ujpg_header();
    always_assert(cur_num_threads <= NUM_THREADS); // this is an invariant we need to maintain
    str_out->prep_for_new_file();
}

void concatenate_files(int fdint, int fdout);

void process_file(IOUtil::FileReader* reader,
                  IOUtil::FileWriter *writer,
                  int max_file_size,
                  bool force_zlib0)
{
    clock_t begin = 0, end = 1;
    const char* actionmsg  = NULL;
    const char* errtypemsg = NULL;
    int speed, bpms;
    float cr;


    if (g_inject_syscall_test == 2) {
        unsigned int num_workers = std::max(
            NUM_THREADS - 1,
            1U);
        GenericWorker* generic_workers = get_worker_threads(num_workers);
        if (g_inject_syscall_test == 2) {
            for (size_t i = 0; i < num_workers; ++i) {
                std::atomic<int> value;
                value.store(0);
                generic_workers[i].work = std::bind(&test_syscall_injection, &value);
                generic_workers[i].activate_work();
                generic_workers[i].instruct_to_exit();
                generic_workers[i].join_via_syscall();
                if (value.load() < 1) {
                    abort(); // this should exit_group
                }
            }
            g_threaded = false;
        }
    }
    // main function routine
    errorlevel.store(0);
    jpgfilesize = 0;
    ujgfilesize = 0;

    Sirikata::Array1d<uint8_t, 2> header = {{0, 0}};
    const char * ifilename = filelist[file_no];
    bool is_socket = false;
    ssize_t bytes_read =0 ;
    int fdin = open_fdin(ifilename, reader, header, &bytes_read, &is_socket);
    /*
    if (g_permissive && bytes_read < 2) {
        std::vector<uint8_t> input(bytes_read);
        if (bytes_read > 0) {
            memcpy(&input[0], header.data, bytes_read);
        }
        Sirikata::MuxReader::ResizableByteBuffer lepton_data;
        ExitCode exit_code = ExitCode::UNSUPPORTED_JPEG;
        ValidationContinuation validation_exit_code = generic_compress(&input, &lepton_data, &exit_code);
        if (exit_code != ExitCode::SUCCESS) {
            custom_exit(exit_code);
        }
        if (validation_exit_code != ValidationContinuation::ROUNDTRIP_OK) {
            custom_exit(ExitCode::UNSUPPORTED_JPEG);
        }
        int fdout = open_fdout(ifilename, writer, true, header, g_force_zlib0_out || force_zlib0, &is_socket);
        for (size_t data_sent = 0; data_sent < lepton_data.size();) {
            ssize_t sent = write(fdout,
                                 lepton_data.data() + data_sent,
                                 lepton_data.size() - data_sent);
            if (sent < 0 && errno == EINTR){
                continue;
            }
            if (sent <= 0) {
                custom_exit(ExitCode::SHORT_READ);
            }
            data_sent += sent;
        }
        //fprintf(stderr, "OK...\n");
        custom_exit(ExitCode::SUCCESS);
        
        }*/
    int fdout = -1;
    if ((embedded_jpeg || is_jpeg_header(header) || g_permissive) && (g_permissive ||  !g_skip_validation)) {
        //fprintf(stderr, "ENTERED VALIDATION...\n");
        ExitCode validation_exit_code = ExitCode::SUCCESS;
        Sirikata::MuxReader::ResizableByteBuffer lepton_data;
        std::vector<uint8_t> permissive_jpeg_return_backing;
        switch (validateAndCompress(&fdin, &fdout, header,
                                    bytes_read,
                                    start_byte, max_file_size,
                                    &validation_exit_code,
                                    &lepton_data,
                                    g_argc,
                                    g_argv,
                                    g_permissive,
                                    is_socket,
                                    g_permissive? &permissive_jpeg_return_backing:NULL)) {
          case ValidationContinuation::CONTINUE_AS_JPEG:
            //fprintf(stderr, "CONTINUE AS JPEG...\n");
            is_socket = false;
            break;
          case ValidationContinuation::CONTINUE_AS_LEPTON:
            embedded_jpeg = false;
            is_socket = false;
            g_force_zlib0_out = false;
            force_zlib0 = false;
            if (ofiletype ==  UJG) {
                filetype = UJG;
                header[0] = ujg_header[0];
                header[1] = ujg_header[1];
            } else {
                filetype = LEPTON;
                header[0] = lepton_header[0];
                header[1] = lepton_header[1];
            }
            //fprintf(stderr, "CONTINUE AS LEPTON...\n");
            break;
        case ValidationContinuation::EVALUATE_AS_PERMISSIVE:
            if (permissive_jpeg_return_backing.size() == 0) {
                custom_exit(ExitCode::UNSUPPORTED_JPEG);
            }
            fdout = open_fdout(ifilename, writer, embedded_jpeg, header, g_force_zlib0_out || force_zlib0, &is_socket);
            {ExitCode validation_exit_code = ExitCode::UNSUPPORTED_JPEG;
            generic_compress(&permissive_jpeg_return_backing, &lepton_data, &validation_exit_code);
            if (validation_exit_code != ExitCode::SUCCESS) {
                custom_exit(validation_exit_code);
            }}
            for (size_t data_sent = 0; data_sent < lepton_data.size();) {
                ssize_t sent = write(fdout,
                                     lepton_data.data() + data_sent,
                                     lepton_data.size() - data_sent);
                if (sent < 0 && errno == EINTR){
                    continue;
                }
                if (sent <= 0) {
                    custom_exit(ExitCode::SHORT_READ);
                }
                data_sent += sent;
            }
            //fprintf(stderr, "OK...\n");
            custom_exit(ExitCode::SUCCESS);
            break;
        case ValidationContinuation::ROUNDTRIP_OK:
            fdout = open_fdout(ifilename, writer, embedded_jpeg, header, g_force_zlib0_out || force_zlib0, &is_socket);
            for (size_t data_sent = 0; data_sent < lepton_data.size();) {
                ssize_t sent = write(fdout,
                                     lepton_data.data() + data_sent,
                                     lepton_data.size() - data_sent);
                if (sent < 0 && errno == EINTR){
                    continue;
                }
                if (sent <= 0) {
                    custom_exit(ExitCode::SHORT_READ);
                }
                data_sent += sent;
            }
            //fprintf(stderr, "OK...\n");
            custom_exit(ExitCode::SUCCESS);
          case ValidationContinuation::BAD:
          default:
            always_assert(validation_exit_code != ExitCode::SUCCESS);
            custom_exit(validation_exit_code);
        }        
    } else {
        if (action != lepton_concatenate) {
            fdout = open_fdout(ifilename, writer, embedded_jpeg, header, g_force_zlib0_out || force_zlib0, &is_socket);
        }
    }
    if (action == lepton_concatenate) {
        concatenate_files(fdin, fdout);
        return;
    }
    // check input file and determine filetype
    check_file(fdin, fdout, max_file_size, force_zlib0, embedded_jpeg, header, is_socket);
    
    begin = clock();
    if ( filetype == JPEG )
    {


        if (ofiletype == LEPTON) {
            if (!g_encoder) {
                if (ujgversion == 3) {
#ifdef ENABLE_ANS_EXPERIMENTAL
                    g_encoder.reset(makeEncoder<ANSBoolReader>(g_threaded, g_threaded));
#else
                    always_assert(false&&"ANS-encoded file encountered but ANS not selected in build flags");
#endif
                } else {
                    g_encoder.reset(makeEncoder<VPXBoolReader>(g_threaded, g_threaded));
                }
                TimingHarness::timing[0][TimingHarness::TS_MODEL_INIT] = TimingHarness::get_time_us();
                g_decoder = NULL;
            } else if (g_threaded && (action == socketserve || action == forkserve)) {
                g_encoder->registerWorkers(get_worker_threads(NUM_THREADS - 1), NUM_THREADS  - 1);
            }
        }else if (ofiletype == UJG) {
            g_encoder.reset(new SimpleComponentEncoder);
            g_decoder = NULL;
        }
    } else if (filetype == LEPTON) {
        NUM_THREADS = read_fixed_ujpg_header();
        if (NUM_THREADS == 1) {
            g_threaded = false; // with singlethreaded, doesn't make sense to split out reader/writer
        }
        if (!g_decoder) {
            g_decoder = makeDecoder(g_threaded, g_threaded, ujgversion == 3);
            TimingHarness::timing[0][TimingHarness::TS_MODEL_INIT] = TimingHarness::get_time_us();
            g_reference_to_free.reset(g_decoder);
        } else if (NUM_THREADS > 1 && g_threaded && (action == socketserve || action == forkserve)) {
            g_decoder->registerWorkers(get_worker_threads(NUM_THREADS), NUM_THREADS);
        }
    }else if (filetype == UJG) {
        (void)read_fixed_ujpg_header();
        g_decoder = new SimpleComponentDecoder;
        g_reference_to_free.reset(g_decoder);
    }
#ifndef _WIN32
    //FIXME
    if (g_time_bound_ms) {
        struct itimerval bound;
        bound.it_value.tv_sec = g_time_bound_ms / 1000;
        bound.it_value.tv_usec = (g_time_bound_ms % 1000) * 1000;
        bound.it_interval.tv_sec = 0;
        bound.it_interval.tv_usec = 0;
        int ret = setitimer(ITIMER_REAL, &bound, NULL);

        dev_assert(ret == 0 && "Timer must be able to be set");
        if (ret != 0) {
            exit((int)ExitCode::OS_ERROR);
        }
    }
#endif
    if (g_unkillable) { // only set this after the time bound has been set
        if (!g_time_bound_ms) {
            fprintf(stderr, "Only allowed to set unkillable for items with a time bound\n");
            exit(1);
        }
        signal(SIGTERM, &sig_nop);
#ifndef _WIN32
        signal(SIGQUIT, &sig_nop);
#endif
    }

    if (g_use_seccomp) {
        Sirikata::installStrictSyscallFilter(true);
    }
#ifndef _WIN32
    if (g_inject_syscall_test == 1) {
        char buf[128 + 1];
        buf[sizeof(buf) - 1] = 0;
        char * ret = getcwd(buf, sizeof(buf) - 1);
        (void)ret;
    }
#endif
    // get specific action message
    if ( filetype == UNK ) {
        actionmsg = "unknown filetype";
    } else if (action == info) {
        actionmsg = "Parsing";
    } else if ( filetype == JPEG ) {
        actionmsg = "Writing to LEPTON\n";
    } else {
        actionmsg = "Decompressing to JPEG\n";
    }

    if ( verbosity > 0 ) {
        while (write(2, actionmsg , strlen(actionmsg)) < 0 && errno == EINTR) {}
    }


    std::vector<std::pair<uint32_t, uint32_t> > huff_input_offset;
    if ( filetype == JPEG )
    {
        switch ( action )
        {
            case lepton_concatenate:
              fprintf(stderr, "Unable to concatenate raw JPEG files together\n");
              custom_exit(ExitCode::VERSION_UNSUPPORTED);
              break;
            case comp:
            case forkserve:
            case socketserve:
                timing_operation_start( 'c' );
                TimingHarness::timing[0][TimingHarness::TS_READ_STARTED] = TimingHarness::get_time_us();
                {
                    std::vector<uint8_t,
                                Sirikata::JpegAllocator<uint8_t> > jpeg_file_raw_bytes;
                    unsigned int jpg_ident_offset = 2;
                    if (start_byte == 0) {
                        ibytestream str_jpg_in(str_in,
                                               jpg_ident_offset,
                                               Sirikata::JpegAllocator<uint8_t>());

                        execute(std::bind(&read_jpeg_wrapper, &huff_input_offset, &str_jpg_in, header, embedded_jpeg));
                    } else {
                        ibytestreamcopier str_jpg_in(str_in,
                                                     jpg_ident_offset,
                                                     max_file_size,
                                                     Sirikata::JpegAllocator<uint8_t>());
                        str_jpg_in.mutate_read_data().push_back(0xff);
                        str_jpg_in.mutate_read_data().push_back(0xd8);
                        execute(std::bind(&read_jpeg_and_copy_to_side_channel,
                                          &huff_input_offset, &str_jpg_in, header,
                                          embedded_jpeg));
                        jpeg_file_raw_bytes.swap(str_jpg_in.mutate_read_data());
                    }
                    TimingHarness::timing[0][TimingHarness::TS_JPEG_DECODE_STARTED] =
                        TimingHarness::timing[0][TimingHarness::TS_READ_FINISHED] = TimingHarness::get_time_us();
                    std::vector<ThreadHandoff> luma_row_offsets;
                    execute(std::bind(&decode_jpeg, huff_input_offset, &luma_row_offsets));
                    TimingHarness::timing[0][TimingHarness::TS_JPEG_DECODE_FINISHED]
                        = TimingHarness::get_time_us();
                    //execute( check_value_range );
                    execute(std::bind(&write_ujpg,
                                      std::move(luma_row_offsets),
                                      jpeg_file_raw_bytes.empty() ? NULL : &jpeg_file_raw_bytes));
                }
                timing_operation_complete( 'c' );
                break;

            case info:
                {
                    unsigned int jpg_ident_offset = 2;
                    ibytestream str_jpg_in(str_in, jpg_ident_offset, Sirikata::JpegAllocator<uint8_t>());
                    execute(std::bind(read_jpeg_wrapper, &huff_input_offset, &str_jpg_in, header,
                                      embedded_jpeg));
                }
                execute( write_info );
                break;
        }
    }
    else if ( filetype == UJG || filetype == LEPTON)
    {
        switch ( action )
        {
            case lepton_concatenate:
              always_assert(false && "should have been handled above");
            case comp:
            case forkserve:
            case socketserve:
                if (!g_use_seccomp) {
                    overall_start = clock();
                }
                timing_operation_start( 'd' );
                TimingHarness::timing[0][TimingHarness::TS_READ_STARTED] = TimingHarness::get_time_us();
                while (true) {
                    execute( read_ujpg ); // replace with decompression function!
                    TimingHarness::timing[0][TimingHarness::TS_READ_FINISHED] = TimingHarness::get_time_us();
                    if (!g_use_seccomp) {
                        read_done = clock();
                    }
                    TimingHarness::timing[0][TimingHarness::TS_JPEG_RECODE_STARTED] = TimingHarness::get_time_us();
                    if (filetype != UJG && !g_allow_progressive) {
                        execute(recode_baseline_jpeg_wrapper);
                    } else {
                        execute(recode_jpeg);
                    }
                    timing_operation_complete( 'd' );
                    TimingHarness::timing[0][TimingHarness::TS_JPEG_RECODE_FINISHED] = TimingHarness::get_time_us();
                    Sirikata::Array1d<uint8_t, 6> trailer_new_header;
                    std::pair<uint32_t, Sirikata::JpegError> continuity;
                    size_t off = 0;
                    while (off < trailer_new_header.size()) {
                        continuity = str_in->Read(&trailer_new_header[off], trailer_new_header.size() - off);
                        off += continuity.first;
                        if (continuity.second != Sirikata::JpegError::nil()) {
                            break;
                        }
                    }
                    if (continuity.second != Sirikata::JpegError::nil()) {
                        break;
                    } else if (trailer_new_header[4] != header[0] ||  trailer_new_header[5] != header[1]) {
                        break;
                    } else {
                        prep_for_new_file();
                    }
                }
                str_out->close();
                break;
            case info:
                execute( read_ujpg );
                execute( write_info );
                break;
        }
    }
    if (!fast_exit) {
        // close iostreams
        if ( str_in  != NULL ) delete( str_in  ); str_in  = NULL;
        if ( str_out != NULL ) delete( str_out ); str_out = NULL;
        if ( ujg_out != NULL ) delete( ujg_out ); ujg_out = NULL;
        // delete if broken or if output not needed
        if ((!pipe_on) && ((errorlevel.load() >= err_tresh)
                           || (action != comp && action != forkserve && action != socketserve))) {
            // FIXME: can't delete broken output--it's gone already
        }
    }
    TimingHarness::timing[0][TimingHarness::TS_DONE] = TimingHarness::get_time_us();
    TimingHarness::print_results();
    if (!g_use_seccomp) {
        end = clock();
    }
    {
        size_t bound = decompression_memory_bound();
        char bound_out[] = "XXXXXXXXXX bytes needed to decompress this file\n";
        bound_out[0] = '0' + (bound / 1000000000)%10;
        bound_out[1] = '0' + (bound / 100000000)%10;
        bound_out[2] = '0' + (bound / 10000000)%10;
        bound_out[3] = '0' + (bound / 1000000)%10;
        bound_out[4] = '0' + (bound / 100000)%10;
        bound_out[5] = '0' + (bound / 10000)%10;
        bound_out[6] = '0' + (bound / 1000)%10;
        bound_out[7] = '0' + (bound / 100)%10;
        bound_out[8] = '0' + (bound / 10)%10;
        bound_out[9] = '0' + (bound / 1)%10;
        const char * to_write = bound_out;
        while(to_write[0] == '0') {
            ++to_write;
        }
        while(write(2, to_write, strlen(to_write)) < 0 && errno == EINTR) {
        }
    }
    print_bill(2);
    // speed and compression ratio calculation
    speed = (int) ( (double) (( end - begin ) * 1000) / CLOCKS_PER_SEC );
    bpms  = ( speed > 0 ) ? ( jpgfilesize / speed ) : jpgfilesize;
    cr    = ( jpgfilesize > 0 ) ? ( 100.0 * ujgfilesize / jpgfilesize ) : 0;

    switch ( verbosity )
    {
        case 0:
          if ( errorlevel.load() < err_tresh ) {
                if (action == comp ) {
                    fprintf(stderr, "%d %d\n",(int)ujgfilesize, (int)jpgfilesize);
                    char percentage_report[]=" XX.XX%\n";
                    double pct = cr + .005;
                    percentage_report[0] = '0' + (int)(pct / 100) % 10;
                    percentage_report[1] = '0' + (int)(pct / 10) % 10;
                    percentage_report[2] = '0' + (int)(pct) % 10;
                    percentage_report[4] = '0' + (int)(pct * 10) % 10;
                    percentage_report[5] = '0' + (int)(pct * 100) % 10;
                    char * output = percentage_report;
                    if (cr < 100) {
                        ++output;
                    }
                    while (write(2, output, strlen(output)) < 0 && errno == EINTR) {
                    }
                }
                else {
                    fprintf( msgout,  "DONE\n" );
                }
            }
            break;

        case 1:
          if ( errorlevel.load() < err_tresh ) fprintf( msgout,  "DONE\n" );
            else fprintf( msgout,  "ERROR\n" );
            break;

        case 2:
            fprintf( msgout,  "\n----------------------------------------\n" );
            if ( errorlevel.load() < err_tresh ) fprintf( msgout,  "-> %s OK\n", actionmsg );
            break;
    }

    switch ( errorlevel.load() )
    {
        case 0:
            errtypemsg = "none";
            break;

        case 1:
            if ( errorlevel.load() < err_tresh )
                errtypemsg = "warning (ignored)";
            else
                errtypemsg = "warning (skipped file)";
            break;

        case 2:
            errtypemsg = "fatal error";
            break;
    }

    if ( errorlevel.load() > 0 )
    {
        if (false && action != socketserve && action != forkserve) {
            fprintf( stderr, " %s:\n", errtypemsg  );
            fprintf( stderr, " %s\n", errormessage.c_str() );
            if ( verbosity > 1 )
                fprintf( stderr, " (in file \"%s\")\n", filelist[ file_no ] );
        }
    }
    if ( (verbosity > 0) && (errorlevel.load() < err_tresh) )
    if ( action == comp )
    {
        fprintf( msgout,  " time taken  : %7i msec\n", speed );
        fprintf( msgout,  " byte per ms : %7i byte\n", bpms );
        fprintf( msgout,  " comp. ratio : %7.2f %%\n", cr );
    }

    if ( ( verbosity > 1 ) && ( action == comp ) )
        fprintf( msgout,  "\n" );
    LeptonDebug::dumpDebugData();
    if (errorlevel.load()) {
        custom_exit(ExitCode::UNSUPPORTED_JPEG); // custom exit will delete generic_workers
    } else {
        custom_exit(ExitCode::SUCCESS);
    }
    reset_buffers();
}


/* -----------------------------------------------
    main-function execution routine
    ----------------------------------------------- */

void execute(const std::function<bool()> &function)
{
    clock_t begin = 0, end = 0;
    bool success;



    if ( errorlevel.load() < err_tresh )
    {
        // get statusmessage
        //function();
        // write statusmessage
        // set starttime
        if (!g_use_seccomp) {
            begin = clock();
        }
        // call function
        success = function();
        // set endtime
        if (!g_use_seccomp) {
            end = clock();
        }

        // write statusmessage
        if ( success ) {
            if (verbosity == 2 && !g_use_seccomp) {
                fprintf( msgout,  "%6ims",
                         (int) ( (double) (( end - begin ) * 1000) / CLOCKS_PER_SEC ) );
            }
        }
        else {
            if ( verbosity == 2 ) {
                while(write(2, "ERROR\n", strlen("ERROR\n")) < 0 && errno == EINTR) {

                }
            }
        }
    }
}


/* -----------------------------------------------
    shows help in case of wrong input
    ----------------------------------------------- */

void show_help( void )
{
    fprintf(msgout, "Usage: %s [switches] input_file [output_file]", appname );
    fprintf(msgout, "\n" );
    fprintf(msgout, "\n" );
    fprintf(msgout, " [-version]       File format version of lepton codec\n" );
    fprintf(msgout, " [-revision]      GIT Hash of lepton source that built this binary\n");
    fprintf(msgout, " [-zlib0]         Instead of a jpg, return a zlib-compressed jpeg\n");
    fprintf(msgout, " [-startbyte=<n>] Encoded file will only contain data at and after <n>\n");
    fprintf(msgout, " [-trunc=<n>]     Encoded file will be truncated at size <n> - startbyte\n");
//    fprintf(msgout, " [-avx2upgrade]   Try to exec <binaryname>-avx if avx is available\n");
//    fprintf(msgout, " [-injectsyscall={1..4}]  Inject a \"chdir\" syscall & check SECCOMP crashes\n");
    fprintf(msgout, " [-unjailed]      Do not jail this process (use only with trusted data)\n" );
    fprintf(msgout, " [-singlethread]  Do not clone threads to operate on the input file\n" );
    fprintf(msgout, " [-maxencodethreads=<n>] Can use <n> threads to decode: higher=bigger file\n");
    fprintf(msgout, " [-allowprogressive] Allow progressive jpegs through the compressor\n");
    fprintf(msgout, " [-rejectprogressive] Reject encoding of progressive jpegs\n");
    fprintf(msgout, " [-timebound=<>ms]For -socket, enforce a timeout since first byte received\n");
    fprintf(msgout, " [-lepcat] Concatenate lepton files together into a file that contains multiple substrings\n");
    fprintf(msgout, " [-memory=<>M]    Upper bound on the amount of memory allocated by main\n");
    fprintf(msgout, " [-threadmemory=<>M] Bound on the amount of memory allocated by threads\n");
    fprintf(msgout, " [-recodememory=<>M] Check that a singlethreaded recode only uses <>M mem\n");
#ifndef _WIN32
    fprintf(msgout, " [-hugepages]     Allocate from the hugepages on the system\n");
    fprintf(msgout, " [-socket=<name>] Serve requests on a Unix Domain Socket at path <name>\n" );
    fprintf(msgout, " [-listen=<port>] Serve requests on a TCP socket on <port> (default 2402)\n" );
    fprintf(msgout, " [-listenbacklog=<n>] n clients queued for encoding if maxchildren reached\n" );
    fprintf(msgout, " [-zliblisten=<port>] Serve requests on a TCP socket on <port> (def 2403)\n" );
    fprintf(msgout, " [-maxchildren]   Max codes to ever spawn at the same time in socket mode\n");
#endif
    fprintf(msgout, " [-benchmark]     Run a benchmark on optional [<input_file>] (or included file)\n");
    fprintf(msgout, " [-verbose]       Run the benchmark in verbose mode (more output to stderr)\n");
    fprintf(msgout, " [-benchreps=<n>] Number of trials to run the benchmark for each category\n");
    fprintf(msgout, " [-benchthreads=<n>] Max number of parallel codings to benchmark\n");
#ifdef SKIP_VALIDATION
    fprintf(msgout, " [-validate]      Round-trip this file when encoding [default:off]\n");
#else
    fprintf(msgout, " [-validate]      Round-trip this file when encoding [default:on]\n");
    fprintf(msgout, " [-skipvalidate]  Avoid round-trip check when encoding (Warning: unsafe)\n");
#endif
}

/* ----------------------- End of main interface functions -------------------------- */

/* ----------------------- Begin of main functions -------------------------- */


void nop (Sirikata::DecoderWriter*w, size_t) {
}

//void static_cast_to_zlib_and_call (Sirikata::DecoderWriter*w, size_t size) {
//    (static_cast<Sirikata::Zlib0Writer*>(w))->setFullFileSize(size);
//}


/* -----------------------------------------------
    check file and determine filetype
    ----------------------------------------------- */
unsigned char read_fixed_ujpg_header() {
    Sirikata::Array1d<unsigned char, 22> header;
    header.memset(0);

    if (IOUtil::ReadFull(str_in, header.begin(), 22) != 22) {
        custom_exit(ExitCode::SHORT_READ);
    }
    // check version number
    if (header[0] != 1 && header[0] != 2 && header[0] != 3 && header[0] != 4 && header[0] != ujgversion) {
        // let us roll out a new version gently
        fprintf( stderr, "incompatible file, use %s v%i.%i",
            appname, header[ 0 ] / 10, header[ 0 ] % 10 );
        custom_exit(ExitCode::VERSION_UNSUPPORTED);
    }
    ujgversion = header[0];
    if (header[1] == 'X') {
    } else if (header[1] != 'Z' && header[1] != 'Y') {
        char err[] = "?: Unknown Item in header instead of Z";
        err[0] = header[1];
        while(write(2, err, sizeof(err) - 1) < 0 && errno == EINTR) {
        }
    }
    if (header[1] == 'Z' || (header[1] & 1) == ('Y' & 1)) {
        if (!g_force_progressive) {
            g_allow_progressive = false;
        }
    }
    unsigned char num_threads_hint = header[2];
    always_assert(num_threads_hint != 0);
    if (num_threads_hint < NUM_THREADS && num_threads_hint != 0) {
        NUM_THREADS = num_threads_hint;
    }
// full size of the original file
    Sirikata::Array1d<unsigned char, 4>::Slice file_size = header.slice<18,22>();
    max_file_size = LEtoUint32(file_size.begin());
    return NUM_THREADS;
}

bool check_file(int fd_in, int fd_out, uint32_t max_file_size, bool force_zlib0,
                bool is_embedded_jpeg, Sirikata::Array1d<uint8_t, 2> fileid, bool is_socket)
{
    IOUtil::FileReader * reader = IOUtil::BindFdToReader(fd_in, max_file_size, is_socket);
    if (!reader) {
        custom_exit(ExitCode::FILE_NOT_FOUND);
    }
    reader->mark_some_bytes_already_read((uint32_t)fileid.size());
    if (is_socket) {
        dev_assert(fd_in == fd_out);
    }
    IOUtil::FileWriter * writer = IOUtil::BindFdToWriter(fd_out == -1 ? 1 /* stdout */ : fd_out, is_socket);
    ujg_base_in = reader;
    // check file id, determine filetype
    if (is_embedded_jpeg || is_jpeg_header(fileid)) {
        str_in = new Sirikata::BufferedReader<JPG_READ_BUFFER_SIZE>(reader);
        // file is JPEG
        filetype = JPEG;
        NUM_THREADS = std::min(NUM_THREADS, (unsigned int)max_encode_threads);
        // open output stream, check for errors
        ujg_out = writer;
    }
    else if ( ( ( fileid[0] == ujg_header[0] ) && ( fileid[1] == ujg_header[1] ) )
              || ( ( fileid[0] == lepton_header[0] ) && ( fileid[1] == lepton_header[1] ) )
              || ( ( fileid[0] == zlepton_header[0] ) && ( fileid[1] == zlepton_header[1] ) ) ){
        str_in = reader;
        bool compressed_output = (fileid[0] == zlepton_header[0]) && (fileid[1] == zlepton_header[1]);
        compressed_output = compressed_output || g_force_zlib0_out || force_zlib0;
        // file is UJG
        filetype = (( fileid[0] == ujg_header[0] ) && ( fileid[1] == ujg_header[1] ) ) ? UJG : LEPTON;
        std::function<void(Sirikata::DecoderWriter*, size_t file_size)> known_size_callback = &nop;
        Sirikata::DecoderWriter * write_target = writer;
        if (compressed_output) {
            Sirikata::Zlib0Writer * zwriter;
            if (uninit_g_zlib_0_writer) {
                zwriter = new(uninit_g_zlib_0_writer)Sirikata::Zlib0Writer(writer, 0);
                uninit_g_zlib_0_writer = NULL;
            }else {
                zwriter = new Sirikata::Zlib0Writer(writer, 0);
            }
            known_size_callback = &nop;
            write_target = zwriter;
        }
        str_out = new bounded_iostream( write_target,
                                        known_size_callback,
                                        Sirikata::JpegAllocator<uint8_t>());
        if ( str_out->chkerr() ) {
            fprintf( stderr, FWR_ERRMSG, filelist[file_no]);
            errorlevel.store(2);
            return false;
        }
    }
    else {
        // file is neither
        filetype = UNK;
        fprintf( stderr, "filetype of file \"%s\" is unknown", filelist[ file_no ] );
        errorlevel.store(2);
        return false;
    }


    return true;
}

bool is_needed_for_second_block(const std::vector<unsigned char>&segment) {
    if (segment.size() <= 2) {
        return true; // don't understand this type of header
    }
    if (segment[0] != 0xff) {
        return true; // don't understand this type of header
    }
    switch (segment[1]) {
      case 0xC4: // DHT segment
      case 0xDB: // DQT segment
      case 0xDD: // DRI segment
      case 0xDA: // Start of scan
      case 0xC0:
      case 0xC1:
      case 0xC2:
        return true;
      case 0xD8:
      case 0xD9:
        dev_assert(false && "This should be filtered out by the previous loop");
        return true;
      default:
        return false;
    }
}
/* -----------------------------------------------
    Read in header & image data
    ----------------------------------------------- */
template<class input_byte_stream>
bool read_jpeg(std::vector<std::pair<uint32_t,
                                     uint32_t>> *huff_input_offsets,
               input_byte_stream *jpg_in,
               Sirikata::Array1d<uint8_t, 2> header,
               bool is_embedded_jpeg){
    if (jpeg_embedding_offset) {
        prefix_grbs = jpeg_embedding_offset + 2;
        prefix_grbgdata = aligned_alloc(prefix_grbs);
        prefix_grbgdata[0] = header[0];
        prefix_grbgdata[1] = header[1];
        prefix_grbs = jpg_in->read(prefix_grbgdata + 2, jpeg_embedding_offset);
        always_assert((size_t)prefix_grbs == jpeg_embedding_offset); // the ffd8 gets baked in...again
    }
    std::vector<unsigned char> segment(1024); // storage for current segment
    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   crst = 0; // current rst marker counter
    unsigned int   cpos = 0; // rst marker counter
    unsigned char  tmp;

    abytewriter* huffw;
    abytewriter* hdrw;
    abytewriter* grbgw;

    // preset count of scans
    scnc = 0;
    // start headerwriter
    hdrw = new abytewriter( 4096 );
    hdrs = 0; // size of header data, start with 0

    // start huffman writer
    huffw = new abytewriter( 0 );
    hufs  = 0; // size of image data, start with 0

    // JPEG reader loop
    while ( true ) {
        if ( type == 0xDA ) { // if last marker was sos
            // switch to huffman data reading mode
            cpos = 0;
            crst = 0;
            while ( true ) {
                huff_input_offsets->push_back(std::pair<uint32_t, uint32_t>(huffw->getpos(),
                                                                            jpg_in->getsize()));
                // read byte from imagedata
                if ( jpg_in->read_byte( &tmp) == false ) {
                    early_eof(hdrw, huffw);
                    fprintf(stderr, "Early EOF\n");
                    break;
                }
                // non-0xFF loop
                if ( tmp != 0xFF ) {
                    crst = 0;
                    while ( tmp != 0xFF ) {
                        huffw->write( tmp );
                        if ( jpg_in->read_byte( &tmp ) == false ) {
                            early_eof(hdrw, huffw);
                            break;
                        }
                    }
                }

                // treatment of 0xFF
                if ( tmp == 0xFF ) {
                    if ( jpg_in->read_byte( &tmp ) == false ) {
                        early_eof(hdrw, huffw);
                        break; // read next byte & check
                    }
                    if ( tmp == 0x00 ) {
                        crst = 0;
                        // no zeroes needed -> ignore 0x00. write 0xFF
                        huffw->write( 0xFF );
                        write_byte_bill(Billing::DELIMITERS, false, 1);
                    }
                    else if ( tmp == 0xD0 + ( cpos & 7 ) ) { // restart marker
                        // increment rst counters
                        write_byte_bill(Billing::DELIMITERS, false, 2);
                        cpos++;
                        crst++;
                        while (rst_cnt.size() <= (size_t)scnc) {
                            rst_cnt.push_back(0);
                        }
                        ++rst_cnt.at(scnc);
                    }
                    else { // in all other cases leave it to the header parser routines
                        // store number of falsely set rst markers
                        if((int)rst_err.size() < scnc) {
                            rst_err.insert(rst_err.end(), scnc - rst_err.size(), 0);
                        }
                        rst_err.push_back(crst);
                        // end of current scan
                        scnc++;
                        always_assert(rst_err.size() == (size_t)scnc && "All reset errors must be accounted for");
                        // on with the header parser routines
                        segment[ 0 ] = 0xFF;
                        segment[ 1 ] = tmp;
                        break;
                    }
                }
                else {
                    // otherwise this means end-of-file, so break out
                    break;
                }
            }
        }
        else {
            // read in next marker
            if ( jpg_in->read( segment.data(), 2 ) != 2 ) break;
            if ( segment[ 0 ] != 0xFF ) {
                // ugly fix for incorrect marker segment sizes
                fprintf( stderr, "size mismatch in marker segment FF %2X", type );
                errorlevel.store(2);
                if ( type == 0xFE ) { //  if last marker was COM try again
                    if ( jpg_in->read( segment.data(), 1) != 1 ) break;
                    if ( segment[ 0 ] == 0xFF ) errorlevel.store(1);
                }
                if ( errorlevel.load() == 2 ) {
                    delete ( hdrw );
                    delete ( huffw );
                    return false;
                }
            }
        }

        // read segment type
        type = segment[ 1 ];

        // if EOI is encountered make a quick exit
        if ( type == EOI[1] ) {
            standard_eof(hdrw, huffw);
            // everything is done here now
            break;
        }

        // read in next segments' length and check it
        if ( jpg_in->read( segment.data() + 2, 2 ) != 2 ) break;
        len = 2 + B_SHORT( segment[ 2 ], segment[ 3 ] );
        if ( len < 4 ) break;

        // realloc segment data if needed
        segment.resize(len);

        // read rest of segment, store back in header writer
        if ( jpg_in->read( ( segment.data() + 4 ), ( len - 4 ) ) !=
            ( unsigned short ) ( len - 4 ) ) break;
        if (start_byte == 0 || is_needed_for_second_block(segment)) {
            hdrw->write_n( segment.data(), len );
        }
    }
    // JPEG reader loop end

    // free writers
    delete ( hdrw );
    delete ( huffw );

    // check if everything went OK
    if ( hdrs == 0 ) {
        fprintf( stderr, "unexpected end of data encountered in header" );
        errorlevel.store(2);
        return false;
    }
    if ( hufs == 0 ) {
        fprintf( stderr, "unexpected end of data encountered in huffman" );
        errorlevel.store(2);
        return false;
    }

    // store garbage at EOI
    grbgw = new abytewriter( 1024 );
    unsigned char grb0 = jpg_in->get_penultimate_read();
    unsigned char grb1 = jpg_in->get_last_read();
    grbgw->write( grb0 ); // should be 0xff (except if truncated)
    grbgw->write( grb1 ); // should be d9 (except if truncated)
    while( true ) {
        len = jpg_in->read( segment.data(), segment.size() );
        if ( len == 0 ) break;
        grbgw->write_n( segment.data(), len );
    }
    grbgdata = grbgw->getptr_aligned();
    grbs     = grbgw->getpos();
    delete ( grbgw );
    if (grbs == sizeof(EOI) && 0 == memcmp(grbgdata, EOI, sizeof(EOI))) {
        grbs = 0;
        aligned_dealloc(grbgdata);
        grbgdata = NULL;
    }

    // get filesize
    jpgfilesize = jpg_in->getsize();

    // parse header for image info
    if ( !setup_imginfo_jpg(false) ) {
        return false;
    }


    return true;
}


enum MergeJpegStreamingStatus{
    STREAMING_ERROR = 0,
    STREAMING_SUCCESS = 1,
    STREAMING_NEED_DATA = 2,
    STREAMING_DISABLED = 3
};

bool aligned_memchr16ff(const unsigned char *local_huff_data) {
#if USE_SCALAR
    return memchr(local_huff_data, 0xff, 16) != NULL;
#else
    __m128i buf = _mm_load_si128((__m128i const*)local_huff_data);
    __m128i ff = _mm_set1_epi8(-1);
    __m128i res = _mm_cmpeq_epi8(buf, ff);
    uint32_t movmask = _mm_movemask_epi8(res);
    bool retval = movmask != 0x0;
    dev_assert (retval == (memchr(local_huff_data, 0xff, 16) != NULL));
    return retval;
#endif
}

unsigned char hex_to_nibble(char val) {
    if (val >= 'A' && val <= 'F') {
        return val - 'A' + 10;
    }
    if (val >= 'a' && val <= 'f') {
        return val - 'a' + 10;
    }
    return val - '0';
}
unsigned char hex_pair_to_byte(char big, char little) {
    return hex_to_nibble(big) * 16 + hex_to_nibble(little);
}
bool hex_to_bin(unsigned char *output, const char *input, size_t output_size) {
    size_t i = 0;
    for (; i < output_size && input[i * 2] && input[i * 2 + 1]; ++i) {
        output[i] = hex_pair_to_byte(input[i * 2], input[i * 2 + 1]);
    }
    return i == output_size;
}
bool rst_cnt_ok(int scan, unsigned int num_rst_markers_this_scan) {
    if (rstp.empty()) {
        return false;
    }
    if (!rst_cnt_set) {
        return true;
    }
    return rst_cnt.size() > (size_t)scan - 1 && num_rst_markers_this_scan < rst_cnt.at(scan - 1);
}


ThreadHandoff crystallize_thread_handoff(abitreader *reader,
                                         const std::vector<std::pair<uint32_t, uint32_t> >&huff_input_offsets,
                                         int mcu_y,
                                         int lastdc[4],
                                         int luma_mul) {
    auto iter = std::lower_bound(huff_input_offsets.begin(), huff_input_offsets.end(),
                                 std::pair<uint32_t, uint32_t>(reader->getpos(), reader->getpos()));
    uint32_t mapped_item = 0;
    if (iter != huff_input_offsets.begin()) {
        --iter;
    }
    if (iter != huff_input_offsets.end()) {
        mapped_item = iter->second;
        mapped_item += reader->getpos() - iter->first;
    }
    //fprintf(stderr, "ROWx (%08lx): %x -> %x\n", reader->debug_peek(), reader->getpos(), mapped_item);
    ThreadHandoff retval = ThreadHandoff::zero();
    retval.segment_size = mapped_item; // the caller will need to take the difference of the chosen items
    // to compute the actual segment size
    for (unsigned int i = 0; i < 4 && i < sizeof(retval.last_dc)/ sizeof(retval.last_dc[0]); ++i) {
        retval.last_dc[i] = lastdc[i];
        retval.luma_y_start = luma_mul * mcu_y;
        retval.luma_y_end = luma_mul * (mcu_y + 1);
    }

    std::tie( retval.num_overhang_bits, retval.overhang_byte ) = reader->overhang();

/*
    fprintf(stderr, "%d: %d -> %d  lastdc %d %d %d size %d overhang %d (cnt: %d)\n",
            mcu_y,
            retval.luma_y_start,
            retval.luma_y_end,
            retval.last_dc[0],
            retval.last_dc[1],
            retval.last_dc[2],
            retval.segment_size,
            retval.overhang_byte,
            retval.num_overhang_bits);
*/
    return retval;
}

MergeJpegStreamingStatus merge_jpeg_streaming(MergeJpegProgress *stored_progress, const unsigned char * local_huff_data, unsigned int max_byte_coded,
                                              bool flush) {
    MergeJpegProgress progress(stored_progress);
    unsigned char SOI[ 2 ] = { 0xFF, 0xD8 }; // SOI segment
    //unsigned char EOI[ 2 ] = { 0xFF, 0xD9 }; // EOI segment

    unsigned char  type = 0x00; // type of current marker segment

    if (progress.ipos == 0 && progress.hpos == 0 && progress.scan == 1 && progress.within_scan == false) {
        always_assert(max_file_size > grbs && "Lepton only supports files that have some scan data");
        str_out->set_bound(max_file_size - grbs);

        // write SOI
        str_out->write( SOI, 2 );
    }

    // JPEG writing loop
    while ( true )
    {
        if (!progress.within_scan) {
            progress.within_scan = true;
            // store current header position
            unsigned int   tmp; // temporary storage variable
            tmp = progress.hpos;

            // seek till start-of-scan
            for ( type = 0x00; type != 0xDA; ) {
                if ( 3 + (uint64_t) progress.hpos >= hdrs ) break;
                type = hdrdata[ progress.hpos + 1 ];
                int len = 2 + B_SHORT( hdrdata[ progress.hpos + 2 ], hdrdata[progress.hpos + 3 ] );
                progress.hpos += len;
            }
            unsigned int actual_progress_hpos = std::min(progress.hpos, hdrs);
            // write header data to file
            str_out->write( hdrdata + tmp, ( actual_progress_hpos - tmp ) );
            for (unsigned int i = actual_progress_hpos; i < progress.hpos; ++i) {
                str_out->write("", 1); // write out null bytes beyond buffer
            }
            if ((!g_use_seccomp) && post_byte == 0) {
                post_byte = clock();
            }

            // get out if last marker segment type was not SOS
            if ( type != 0xDA ) break;

            // (re)set corrected rst pos
            progress.cpos = 0;
            progress.ipos = scnp.at(progress.scan - 1);
        }
        if ((int)progress.scan > scnc + 1) { // don't want to go beyond our known number of scans (FIXME: danielrh@ is this > or >= )
            break;
        }
        if (progress.ipos < max_byte_coded) {
            timing_operation_first_byte( 'd' );
        }
        // write & expand huffman coded image data
        unsigned int progress_ipos = progress.ipos;
        unsigned int progress_scan = scnp.at(progress.scan);
        unsigned int rstp_progress_rpos = rstp.empty() ? INT_MAX : rstp[ progress.rpos ];
        const unsigned char mrk = 0xFF; // marker start
        const unsigned char stv = 0x00; // 0xFF stuff value
        for ( ; progress_ipos & 0xf; progress_ipos++ ) {
            if (__builtin_expect(!(progress_ipos < max_byte_coded && (progress_scan == 0 || progress_ipos < progress_scan)), 0)) {
                break;
            }
            uint8_t byte_to_write = local_huff_data[progress_ipos];
            str_out->write_byte(byte_to_write);
            // check current byte, stuff if needed
            if (__builtin_expect(byte_to_write == 0xFF, 0))
                str_out->write_byte(stv);
            // insert restart markers if needed
            if (__builtin_expect(progress_ipos == rstp_progress_rpos, 0)) {
                if (rst_cnt_ok(progress.scan, progress.num_rst_markers_this_scan)) {
                    const unsigned char rst = 0xD0 + ( progress.cpos & 7);
                    str_out->write_byte(mrk);
                    str_out->write_byte(rst);
                    progress.rpos++; progress.cpos++;
                    rstp_progress_rpos = rstp.at(progress.rpos);
                    ++progress.num_rst_markers_this_scan;
                }
            }
        }

        while(true) {
            if (__builtin_expect(!(progress_ipos + 15 < max_byte_coded && (progress_scan == 0 || progress_ipos + 15 < progress_scan)), 0)) {
                break;
            }
            if ( __builtin_expect(aligned_memchr16ff(local_huff_data + progress_ipos)
                                  || (progress_ipos <= rstp_progress_rpos
                                      && progress_ipos + 15 >= rstp_progress_rpos), 0)){
                // insert restart markers if needed
                for (int veci = 0 ; veci < 16; ++veci, ++progress_ipos ) {
                    if (__builtin_expect(progress_ipos == rstp_progress_rpos, 0)) {
                        uint8_t byte_to_write = local_huff_data[progress_ipos];
                        str_out->write_byte(byte_to_write);
                        // check current byte, stuff if needed
                        if (__builtin_expect(byte_to_write == 0xFF, 0)) {
                            str_out->write_byte(stv);
                        }
                        if (rst_cnt_ok(progress.scan, progress.num_rst_markers_this_scan)) {
                                const unsigned char rst = 0xD0 + ( progress.cpos & 7);
                                str_out->write_byte(mrk);
                                str_out->write_byte(rst);
                                progress.rpos++; progress.cpos++;
                                rstp_progress_rpos = rstp.at(progress.rpos);
                                ++progress.num_rst_markers_this_scan;
                        }
                    } else {
                        uint8_t byte_to_write = local_huff_data[progress_ipos];
                        str_out->write_byte(byte_to_write);
                        // check current byte, stuff if needed
                        if (__builtin_expect(byte_to_write == 0xFF, 0)) {
                            str_out->write_byte(stv);
                        }
                    }
                }
            } else {
                str_out->write(local_huff_data + progress_ipos, 16);
                progress_ipos+=16;
            }
        }
        for ( ; ; progress_ipos++ ) {
            if (__builtin_expect(!(progress_ipos < max_byte_coded && (progress_scan == 0 || progress_ipos < progress_scan)), 0)) {
                break;
            }
            uint8_t byte_to_write = local_huff_data[progress_ipos];
            str_out->write_byte(byte_to_write);
            // check current byte, stuff if needed
            if (__builtin_expect(byte_to_write == 0xFF, 0))
                str_out->write_byte(stv);
            // insert restart markers if needed
            if (__builtin_expect(progress_ipos == rstp_progress_rpos, 0)) {
                if (rst_cnt_ok(progress.scan, progress.num_rst_markers_this_scan )) {
                    const unsigned char rst = 0xD0 + ( progress.cpos & 7);
                    str_out->write_byte(mrk);
                    str_out->write_byte(rst);
                    progress.rpos++; progress.cpos++;
                    rstp_progress_rpos = rstp.at(progress.rpos);
                    ++progress.num_rst_markers_this_scan;
                }
            }
        }
        progress.ipos = progress_ipos;
        if (scnp.at(progress.scan) == 0 && !flush) {
            return STREAMING_NEED_DATA;
        }
        if (progress.ipos >= max_byte_coded && progress.ipos != scnp.at(progress.scan) && !flush) {
            return STREAMING_NEED_DATA;
        }
        // insert false rst markers at end if needed
        if (progress.scan - 1 < rst_err.size()) {
            while ( rst_err.at(progress.scan - 1) > 0 ) {
                const unsigned char rst = 0xD0 + ( progress.cpos & 7 );
                str_out->write_byte(mrk);
                str_out->write_byte(rst);
                progress.cpos++;    rst_err.at(progress.scan - 1)--;
            }
        }
        progress.num_rst_markers_this_scan = 0;
        progress.within_scan = false;
        // proceed with next scan
        progress.scan++;
        if(str_out->has_reached_bound()) {
            check_decompression_memory_bound_ok();
            break;
        }
    }

    // write EOI (now EOI is stored in garbage of at least 2 bytes)
    // this guarantees that we can stop the write in time.
    // if it used too much memory
    // str_out->write( EOI, 1, 2 );
    str_out->set_bound(max_file_size);
    check_decompression_memory_bound_ok();
    // write garbage if needed
    if ( grbs > 0 )
        str_out->write( grbgdata, grbs );
    check_decompression_memory_bound_ok();
    str_out->flush();

    // errormessage if write error
    if ( str_out->chkerr() ) {
        fprintf( stderr, "write error, possibly drive is full" );
        errorlevel.store(2);
        return STREAMING_ERROR;
    }
    // get filesize

    jpgfilesize = str_out->getsize();
    // get filesize
    if (ujg_base_in) {
        ujgfilesize = ujg_base_in->getsize();
    } else {
        ujgfilesize = 4096 * 1024;
    }
#ifndef _WIN32
    //FIXME
    if (!g_use_seccomp) {
        clock_t final = clock();
        struct timeval fin = {0,0};
        gettimeofday(&fin,NULL);
        double begin = current_operation_begin.tv_sec + (double)current_operation_begin.tv_usec / 1000000.;
        double end = fin.tv_sec + (double)fin.tv_usec / 1000000.;
        double first_byte = current_operation_first_byte.tv_sec + (double)current_operation_first_byte.tv_usec / 1000000.;
        double begin_to_end = end - begin;
        double begin_to_first_byte = begin_to_end;
        if (current_operation_first_byte.tv_sec != 0) { // if we were successful
            begin_to_first_byte = first_byte - begin;
        }

        fprintf(stderr, "TIMING (new method): %f to first byte %f total\n",
                begin_to_first_byte,
                begin_to_end);
        (void)final;
/*
        fprintf(stderr, "TIMING (recode): %f to first byte %f total\n",
                (double)(post_byte - pre_byte)/(double)CLOCKS_PER_SEC,
                (final - pre_byte)/(double)CLOCKS_PER_SEC);
        fprintf(stderr, "TIMING(overall): %f to first byte %f total\n",
                (post_byte - overall_start)/(double)CLOCKS_PER_SEC,
                (final - overall_start)/(double)CLOCKS_PER_SEC);
*/
        fprintf(stderr, "Read took: %f\n",
                (read_done - overall_start)/(double)CLOCKS_PER_SEC);
    }
#endif
    return STREAMING_SUCCESS;

}




/* -----------------------------------------------
    JPEG decoding routine
    ----------------------------------------------- */

bool decode_jpeg(const std::vector<std::pair<uint32_t, uint32_t> > & huff_input_offsets,
                 std::vector<ThreadHandoff>*luma_row_offset_return)
{
    abitreader* huffr; // bitwise reader for image data

    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   hpos = 0; // current position in header

    int lastdc[ 4 ] = {0, 0, 0, 0}; // last dc for each component
    Sirikata::Aligned256Array1d<int16_t,64> block; // store block for coeffs
    int peobrun; // previous eobrun
    unsigned int eobrun; // run of eobs
    int rstw; // restart wait counter

    int cmp, bpos, dpos;
    int mcu = 0, sub, csc;
    int eob, sta;
    bool is_baseline = true;
    max_cmp = 0; // the maximum component in a truncated image
    max_bpos = 0; // the maximum band in a truncated image
    memset(max_dpos, 0, sizeof(max_dpos)); // the maximum dpos in a truncated image
    max_sah = 0; // the maximum bit in a truncated image

    // open huffman coded image data for input in abitreader
    huffr = new abitreader( huffdata, hufs );
    // preset count of scans
    scnc = 0;

    // JPEG decompression loop
    while ( true )
    {
        // seek till start-of-scan, parse only DHT, DRI and SOS
        for ( type = 0x00; type != 0xDA; ) {
            if ( 3 + ( uint64_t ) hpos >= hdrs ) break;
            type = hdrdata[ hpos + 1 ];
            len = 2 + B_SHORT( hdrdata[ hpos + 2 ], hdrdata[ hpos + 3 ] );
            if ( ( type == 0xC4 ) || ( type == 0xDA ) || ( type == 0xDD ) ) {
                std::vector<unsigned char> over_data;
                unsigned char * hdr_seg_data = NULL;
                if ((uint64_t)hpos + (uint64_t)len > (uint64_t)hdrs) {
                    over_data.insert(over_data.end(), &hdrdata[hpos], &hdrdata[hpos] + (hdrs - hpos));
                    over_data.resize(len);
                    hdr_seg_data = &over_data[0];
                } else {
                    hdr_seg_data = &( hdrdata[ hpos ] );
                }
                if ( !parse_jfif_jpg( type, len, len, hdr_seg_data ) ) {
                    delete huffr;
                    return false;
                }
            }
            hpos += len;
        }

        // get out if last marker segment type was not SOS
        if ( type != 0xDA ) break;

        // check if huffman tables are available
        for ( csc = 0; csc < cs_cmpc; csc++ ) {
            cmp = cs_cmp[ csc ];
            if ( (( jpegtype == 1 || (( cs_cmpc > 1 || cs_to == 0 ) && cs_sah == 0 )) && htset[ 0 ][ cmpnfo[cmp].huffdc ] == 0 ) || 
                 ( jpegtype == 1 && htset[ 1 ][ cmpnfo[cmp].huffdc ] == 0 ) ||
                 ( cs_cmpc == 1 && cs_to > 0 && cs_sah == 0 && htset[ 1 ][ cmpnfo[cmp].huffac ] == 0 ) ) {
                fprintf( stderr, "huffman table missing in scan%i", scnc );
                delete huffr;
                errorlevel.store(2);
                return false;
            }
        }


        // intial variables set for decoding
        cmp  = cs_cmp[ 0 ];
        csc  = 0;
        mcu  = 0;
        sub  = 0;
        dpos = 0;
        if (!huffr->eof) {
            max_bpos = std::max(max_bpos, cs_to);
            // FIXME: not sure why only first bit of cs_sah is examined but 4 bits of it are stored
            max_sah = std::max(max_sah, std::max(cs_sal,cs_sah));
            for (int i = 0; i < cs_cmpc; ++i) {
                max_cmp = std::max(max_cmp, cs_cmp[i]);
            }
        }
/*
        // startup
        luma_row_offset_return->push_back(crystallize_thread_handoff(huffr,
                                                                     huff_input_offsets,
                                                                     mcu / mcuh,
                                                                     lastdc,
                                                                     cmpnfo[0].bcv / mcuv));
*/
        bool do_handoff_print = true;
        // JPEG imagedata decoding routines
        while ( true )
        {
            // (re)set last DCs for diff coding
            lastdc[ 0 ] = 0;
            lastdc[ 1 ] = 0;
            lastdc[ 2 ] = 0;
            lastdc[ 3 ] = 0;

            // (re)set status
            sta = 0;

            // (re)set eobrun
            eobrun  = 0;
            peobrun = 0;

            // (re)set rst wait counter
            rstw = rsti;
            if (cs_cmpc != colldata.get_num_components()) {
                if (!g_allow_progressive) {
                    custom_exit(ExitCode::PROGRESSIVE_UNSUPPORTED);
                } else {
                    is_baseline = false;
                }
            }

            if (jpegtype != 1) {
                if (!g_allow_progressive) {
                    custom_exit(ExitCode::PROGRESSIVE_UNSUPPORTED);
                } else {
                    is_baseline = false;
                }
            }
            // decoding for interleaved data
            if ( cs_cmpc > 1 )
            {
                if ( jpegtype == 1 ) {
                    // ---> sequential interleaved decoding <---
                    while ( sta == 0 ) {
                        if (do_handoff_print) {
                            luma_row_offset_return->push_back(crystallize_thread_handoff(huffr,
                                                                                         huff_input_offsets,
                                                                                         mcu / mcuh,
                                                                                         lastdc,
                                                                                         cmpnfo[0].bcv / mcuv));
                            do_handoff_print = false;
                        }

                        if(!huffr->eof) {
                            max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block read
                        }
                        // decode block
                        eob = decode_block_seq( huffr,
                            &(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
                            &(htrees[ 1 ][ cmpnfo[cmp].huffac ]),
                            block.begin() );
                        if ( eob > 1 && !block[ eob - 1 ] ) {
                            fprintf( stderr, "cannot encode image with eob after last 0" );
                            errorlevel.store(1);
                        }

                        // fix dc
                        block[ 0 ] += lastdc[ cmp ];
                        lastdc[ cmp ] = block[ 0 ];

                        AlignedBlock&aligned_block = colldata.mutable_block((BlockType)cmp, dpos);

                        // copy to colldata
                        for ( bpos = 0; bpos < eob; bpos++ ) {
                            aligned_block.mutable_coefficients_zigzag(bpos) = block[ bpos ];
                        }
                        // check for errors, proceed if no error encountered
                        int old_mcu = mcu;
                        if ( eob < 0 ) sta = -1;
                        else sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw, cs_cmpc);
                        if (mcu % mcuh == 0 && old_mcu !=  mcu) {
                            do_handoff_print = true;
                            //fprintf(stderr, "ROW %d\n", (int)row_handoff.size());
                            
                        }
                        if(huffr->eof) {
                            sta = 2;
                            break;
                        }

                    }
                }
                else if ( cs_sah == 0 ) {
                    // ---> progressive interleaved DC decoding <---
                    // ---> succesive approximation first stage <---
                    while ( sta == 0 ) {
                        if (do_handoff_print) {
                            luma_row_offset_return->push_back(crystallize_thread_handoff(huffr,
                                                                                         huff_input_offsets,
                                                                                         mcu / mcuh,
                                                                                         lastdc,
                                                                                         cmpnfo[0].bcv / mcuv));
                            do_handoff_print = false;
                        }
                        if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                        sta = decode_dc_prg_fs( huffr,
                            &(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
                            block.begin() );

                        // fix dc for diff coding
                        colldata.set((BlockType)cmp,0,dpos) = block[0] + lastdc[ cmp ];
                        
                        uint16_t u_last_dc = lastdc[ cmp ] = colldata.set((BlockType)cmp,0,dpos);
                        u_last_dc <<= cs_sal; // lastdc might be negative--this avoids UB
                        // bitshift for succesive approximation
                        colldata.set((BlockType)cmp,0,dpos) = u_last_dc;

                        // next mcupos if no error happened
                        int old_mcu = mcu;
                        if ( sta != -1 ) {
                            sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw, cs_cmpc);
                        }
                        if (mcu % mcuh == 0 && old_mcu !=  mcu) {
                            do_handoff_print = true;
                            //fprintf(stderr, "ROW %d\n", (int)row_handoff.size());
                            
                        }
                        if(huffr->eof) {
                            sta = 2;
                            break;
                        }

                    }
                }
                else {
                    // ---> progressive interleaved DC decoding <---
                    // ---> succesive approximation later stage <---
                    while ( sta == 0 ) {
                        if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                        // decode next bit
                        sta = decode_dc_prg_sa( huffr,
                            block.begin() );

                        // shift in next bit
                        colldata.set((BlockType)cmp,0,dpos) += block[0] << cs_sal;

                        // next mcupos if no error happened
                        if ( sta != -1 )
                            sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw, cs_cmpc);
                        if(huffr->eof) {
                            sta = 2;
                            break;
                        }

                    }
                }
            }
            else // decoding for non interleaved data
            {
                if ( jpegtype == 1 ) {
                    int vmul = cmpnfo[0].bcv / mcuv;
                    int hmul = cmpnfo[0].bch / mcuh;
                    // ---> sequential non interleaved decoding <---
                    while ( sta == 0 ) {
                        if (do_handoff_print) {
                            luma_row_offset_return->push_back(crystallize_thread_handoff(huffr,
                                                                                         huff_input_offsets,
                                                                                         (dpos/(hmul * vmul)) / mcuh,
                                                                                         lastdc,
                                                                                         cmpnfo[0].bcv / mcuv));
                            do_handoff_print = false;
                        }
                        if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                        // decode block
                        eob = decode_block_seq( huffr,
                            &(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
                            &(htrees[ 1 ][ cmpnfo[cmp].huffac ]),
                            block.begin() );
                        if ( eob > 1 && !block[ eob - 1 ] ) {
                            fprintf( stderr, "cannot encode image with eob after last 0" );
                            errorlevel.store(1);
                        }
                        // fix dc
                        block[ 0 ] += lastdc[ cmp ];
                        lastdc[ cmp ] = block[ 0 ];

                        // copy to colldata
                        AlignedBlock& aligned_block = colldata.mutable_block((BlockType)cmp, dpos);
                        for ( bpos = 0; bpos < eob; bpos++ ) {
                            aligned_block.mutable_coefficients_zigzag(bpos) = block[ bpos ];
                        }
                        
                        // check for errors, proceed if no error encountered
                        if ( eob < 0 ) sta = -1;
                        else sta = next_mcuposn( &cmp, &dpos, &rstw);
                        mcu = dpos / (hmul * vmul);
                        if (cmp == 0 && (mcu % mcuh == 0) && (dpos %(hmul *vmul) == 0)) {
                            do_handoff_print = true;

                        }
                        if(huffr->eof) {
                            sta = 2;
                            break;
                        }

                    }
                }
                else if ( cs_to == 0 ) {
                    if ( cs_sah == 0 ) {
                        // ---> progressive non interleaved DC decoding <---
                        // ---> succesive approximation first stage <---
                        while ( sta == 0 ) {
                            if (do_handoff_print) {
                                luma_row_offset_return->push_back(crystallize_thread_handoff(huffr,
                                                                                             huff_input_offsets,
                                                                                             dpos / cmpnfo[cmp].bch,
                                                                                             lastdc,
                                                                                             cmpnfo[0].bcv / mcuv));
                                do_handoff_print = false;
                            }

                            if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                            sta = decode_dc_prg_fs( huffr,
                                &(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
                                block.begin() );

                            // fix dc for diff coding
                            colldata.set((BlockType)cmp,0,dpos) = block[0] + lastdc[ cmp ];
                            lastdc[ cmp ] = colldata.set((BlockType)cmp,0,dpos);

                            // bitshift for succesive approximation
                            colldata.set((BlockType)cmp,0,dpos) <<= cs_sal;

                            // check for errors, increment dpos otherwise
                            if ( sta != -1 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if (cmp == 0 && dpos % cmpnfo[cmp].bch == 0) {
                                do_handoff_print = true;
                            }
                            if(huffr->eof) {
                                sta = 2;
                                break;
                            }

                        }
                    }
                    else {
                        // ---> progressive non interleaved DC decoding <---
                        // ---> succesive approximation later stage <---
                        while( sta == 0 ) {
                            if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                            // decode next bit
                            sta = decode_dc_prg_sa( huffr,
                                block.begin() );

                            // shift in next bit
                            colldata.set((BlockType)cmp,0,dpos) += block[0] << cs_sal;

                            // check for errors, increment dpos otherwise
                            if ( sta != -1 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if(huffr->eof) {
                                sta = 2;
                                break;
                            }

                        }
                    }
                }
                else {
                    if ( cs_sah == 0 ) {
                        // ---> progressive non interleaved AC decoding <---
                        // ---> succesive approximation first stage <---
                        while ( sta == 0 ) {
                            if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                            // decode block
                            eob = decode_ac_prg_fs( huffr,
                                                    &(htrees[ 1 ][ cmpnfo[cmp].huffac ]),
                                                    block.begin(), &eobrun, cs_from, cs_to );

                            // check for non optimal coding
                            if ( ( eob == cs_from ) && ( eobrun > 0 ) &&
                                ( peobrun > 0 ) && ( peobrun <
                                hcodes[ 1 ][ cmpnfo[cmp].huffac ].max_eobrun - 1 ) ) {
                                fprintf( stderr,
                                    "reconstruction of non optimal coding not supported" );
                                errorlevel.store(1);
                            }
                            AlignedBlock &aligned_block = colldata.mutable_block((BlockType)cmp, dpos);
                            // copy to colldata
                            for ( bpos = cs_from; bpos < eob; bpos++ ) {
                                uint16_t block_bpos = block[ bpos ];
                                block_bpos <<= cs_sal; // prevents UB since block_bpos could be negative
                                aligned_block.mutable_coefficients_zigzag(bpos) = block_bpos;
                            }
                            // check for errors
                            if ( eob < 0 ) sta = -1;
                            else sta = skip_eobrun( &cmp, &dpos, &rstw, &eobrun );

                            // proceed only if no error encountered
                            if ( sta == 0 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if(huffr->eof) {
                                sta = 2;
                                break;
                            }

                        }
                    }
                    else {
                        // ---> progressive non interleaved AC decoding <---
                        // ---> succesive approximation later stage <---
                        while ( sta == 0 ) {
                            // copy from colldata
                            AlignedBlock &aligned_block = colldata.mutable_block((BlockType)cmp, dpos);
                            for ( bpos = cs_from; bpos <= cs_to; bpos++ ) {
                                block[ bpos ] = aligned_block.coefficients_zigzag(bpos);
                            }
                            if ( eobrun == 0 ) {
                                if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                                // decode block (long routine)
                                eob = decode_ac_prg_sa( huffr,
                                                        &(htrees[ 1 ][ cmpnfo[cmp].huffac ]),
                                                        block.begin(), &eobrun, cs_from, cs_to );

                                // check for non optimal coding
                                if ( ( eob == cs_from ) && ( eobrun > 0 ) &&
                                    ( peobrun > 0 ) && ( peobrun <
                                    hcodes[ 1 ][ cmpnfo[cmp].huffac ].max_eobrun - 1 ) ) {
                                    fprintf( stderr,
                                        "reconstruction of non optimal coding not supported" );
                                    errorlevel.store(1);
                                }

                            }
                            else {
                                if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                                // decode block (short routine)
                                eob = decode_eobrun_sa( huffr,
                                                        block.begin(), &eobrun, cs_from, cs_to );
                                if ( eob > 1 && !block[ eob - 1 ] ) {
                                    fprintf( stderr, "cannot encode image with eob after last 0" );
                                    errorlevel.store(1);
                                }
                            }
                            // store eobrun
                            peobrun = eobrun;
                            // copy back to colldata
                            for ( bpos = cs_from; bpos <= cs_to; bpos++ ) {
                                uint16_t block_bpos = block[ bpos ];
                                block_bpos <<= cs_sal;
                                aligned_block.mutable_coefficients_zigzag(bpos) += block_bpos;
                            }
                            // proceed only if no error encountered
                            if ( eob < 0 ) sta = -1;
                            else sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if(huffr->eof) {
                                sta = 2;
                                break;
                            }
                        }
                    }
                }
            }
            // unpad huffman reader / check padbit
            if ( padbit != -1 ) {
                if ( padbit != huffr->unpad( padbit ) ) {
                    fprintf( stderr, "inconsistent use of padbits" );
                    padbit = 1;
                    errorlevel.store(1);
                }
            }
            else {
                padbit = huffr->unpad( padbit );
            }
            // evaluate status
            if ( sta == -1 ) { // status -1 means error
                fprintf( stderr, "decode error in scan%i / mcu%i",
                    scnc, ( cs_cmpc > 1 ) ? mcu : dpos );
                delete huffr;
                errorlevel.store(2);
                return false;
            }
            else if ( sta == 2 ) { // status 2/3 means done
                scnc++; // increment scan counter
                break; // leave decoding loop, everything is done here
            }
            // else if ( sta == 1 ); // status 1 means restart - so stay in the loop
        }
    }
    if (early_eof_encountered) {
        colldata.set_truncation_bounds(max_cmp, max_bpos, max_dpos, max_sah);
    }
    luma_row_offset_return->push_back(crystallize_thread_handoff(huffr, huff_input_offsets, (uint16_t)(mcu / mcuh), lastdc, cmpnfo[0].bcv / mcuv));
    for (size_t i = 1; i < luma_row_offset_return->size(); ++i) {
        if ((*luma_row_offset_return)[i].luma_y_start < 
            (*luma_row_offset_return)[i-1].luma_y_end) {
            (*luma_row_offset_return)[i].luma_y_start = (*luma_row_offset_return)[i-1].luma_y_end;
        }
    }
    // check for unneeded data
    if ( !huffr->eof ) {
        fprintf( stderr, "unneeded data found after coded image data" );
        errorlevel.store(1);
    }

    // clean up
    delete( huffr );

    if (is_baseline) {
        g_allow_progressive = false;
    }
    return true;
}


/* -----------------------------------------------
    JPEG encoding routine
    ----------------------------------------------- */

bool recode_jpeg( void )
{
    if (!g_use_seccomp) {
        pre_byte = clock();
    }
    abitwriter*  huffw; // bitwise writer for image data
    abytewriter* storw; // bytewise writer for storage of correction bits

    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   hpos = 0; // current position in header

    int lastdc[ 4 ]; // last dc for each component
    Sirikata::Aligned256Array1d<int16_t, 64> block; // store block for coeffs
    unsigned int eobrun; // run of eobs
    int rstw; // restart wait counter

    int cmp, bpos, dpos;
    int mcu, sub, csc;
    int eob, sta;
    int tmp;

    // open huffman coded image data in abitwriter
    huffw = new abitwriter( ABIT_WRITER_PRELOAD, max_file_size);
    huffw->fillbit = padbit;

    // init storage writer
    storw = new abytewriter( ABIT_WRITER_PRELOAD);

    // preset count of scans and restarts
    scnc = 0;
    rstc = 0;
    MergeJpegProgress streaming_progress;

    // JPEG decompression loop
    while ( true )
    {
        // seek till start-of-scan, parse only DHT, DRI and SOS
        for ( type = 0x00; type != 0xDA; ) {
            if ( hpos >= hdrs ) break;
            type = hpos + 1 < hdrs ? hdrdata[ hpos + 1 ] : 0;
            len = 2 + B_SHORT( (size_t)hpos + 2 < (size_t)hdrs ? hdrdata[ (size_t)hpos + 2 ]:0,
                               (size_t)hpos + 3 < (size_t)hdrs ? hdrdata[ (size_t)hpos + 3 ] :0);
            if ( ( type == 0xC4 ) || ( type == 0xDA ) || ( type == 0xDD ) ) {
                if ( !parse_jfif_jpg( type, len, len > hdrs - hpos ? hdrs - hpos : len, &( hdrdata[ hpos ] ) ) ) {
                    delete huffw;
                    delete storw;
                    return false;
                }
                int max_scan = 0;
                for (int i = 0; i < cmpc; ++i) {
                    max_scan = std::max(max_scan, cmpnfo[i].bcv);
                }
                rstp.reserve(max_scan);
                scnp.reserve(max_scan);
                hpos += len;
            }
            else {
                hpos += len;
                continue;
            }
        }

        // get out if last marker segment type was not SOS
        if ( type != 0xDA ) break;


        // (re)alloc scan positons array
        while ((int)scnp.size() < scnc + 2) {
            scnp.push_back(0);
        }

        // (re)alloc restart marker positons array if needed
        if ( rsti > 0 ) {
            tmp = rstc + ( ( cs_cmpc > 1 ) ?
                ( mcuc / rsti ) : ( cmpnfo[ cs_cmp[ 0 ] ].bc / rsti ) );
            while ((int)rstp.size() <= tmp ) {
                rstp.push_back((unsigned int) -1 );
            }
        }

        // intial variables set for encoding
        cmp  = cs_cmp[ 0 ];
        csc  = 0;
        mcu  = 0;
        sub  = 0;
        dpos = 0;

        // store scan position
        scnp.at(scnc) = huffw->getpos();
        scnp.at(scnc + 1) = 0; // danielrh@ avoid uninitialized memory when doing progressive writeout
        bool first_pass = true;
        // JPEG imagedata encoding routines
        while ( true )
        {
            // (re)set last DCs for diff coding
            lastdc[ 0 ] = 0;
            lastdc[ 1 ] = 0;
            lastdc[ 2 ] = 0;
            lastdc[ 3 ] = 0;

            // (re)set status
            sta = 0;

            // (re)set eobrun
            eobrun = 0;

            // (re)set rst wait counter
            rstw = rsti;
            if (cs_cmpc != colldata.get_num_components() && !g_allow_progressive) {
                custom_exit(ExitCode::PROGRESSIVE_UNSUPPORTED);
            }
            if (jpegtype != 1 && !g_allow_progressive) {
                custom_exit(ExitCode::PROGRESSIVE_UNSUPPORTED);
            }
            if ((jpegtype != 1 || cs_cmpc != colldata.get_num_components())
                && colldata.is_memory_optimized(0)
                && first_pass) {
                colldata.init(cmpnfo, cmpc, mcuh, mcuv, false);
            }
            first_pass = false;
            // encoding for interleaved data
            if ( cs_cmpc > 1 )
            {
                if ( jpegtype == 1 ) {
                    // ---> sequential interleaved encoding <---
                    while ( sta == 0 ) {
                        // copy from colldata
                        const AlignedBlock &aligned_block = colldata.block((BlockType)cmp, dpos);
                        //fprintf(stderr, "Reading from cmp(%d) dpos %d\n", cmp, dpos);
                        for ( bpos = 0; bpos < 64; bpos++ ) {
                            block[bpos] = aligned_block.coefficients_zigzag(bpos);
                        }
                        int16_t dc = block[0];
                        // diff coding for dc
                        block[ 0 ] -= lastdc[ cmp ];
                        lastdc[ cmp ] = dc;

                        // encode block
                        eob = encode_block_seq( huffw,
                                                &(hcodes[ 0 ][ cmpnfo[cmp].huffdc ]),
                                                &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                                                block.begin() );

                        // check for errors, proceed if no error encountered
                        if ( eob < 0 ) sta = -1;
                        else sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw, cs_cmpc);
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                        }
                        if (str_out->has_exceeded_bound()) {
                            sta = 2;
                        }
                    }
                }
                else if ( cs_sah == 0 ) {
                    // ---> progressive interleaved DC encoding <---
                    // ---> succesive approximation first stage <---
                    while ( sta == 0 ) {
                        // diff coding & bitshifting for dc
                        tmp = colldata.at((BlockType)cmp , 0 , dpos ) >> cs_sal;
                        block[ 0 ] = tmp - lastdc[ cmp ];
                        lastdc[ cmp ] = tmp;

                        // encode dc
                        sta = encode_dc_prg_fs( huffw,
                                                &(hcodes[ 0 ][ cmpnfo[cmp].huffdc ]),
                                                block.begin() );

                        // next mcupos if no error happened
                        if ( sta != -1 )
                            sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw, cs_cmpc);
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                        }
                        if (str_out->has_exceeded_bound()) {
                            sta = 2;
                        }
                    }
                }
                else {
                    // ---> progressive interleaved DC encoding <---
                    // ---> succesive approximation later stage <---
                    while ( sta == 0 ) {
                        // fetch bit from current bitplane
                        block[ 0 ] = BITN( colldata.at((BlockType)cmp , 0 , dpos ), cs_sal );

                        // encode dc correction bit
                        sta = encode_dc_prg_sa( huffw, block.begin() );

                        // next mcupos if no error happened
                        if ( sta != -1 )
                            sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw, cs_cmpc);
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                        }
                        if (str_out->has_exceeded_bound()) {
                            sta = 2;
                        }

                    }
                }
            }
            else // encoding for non interleaved data
            {
                if ( jpegtype == 1 ) {
                    // ---> sequential non interleaved encoding <---
                    while ( sta == 0 ) {
                        const AlignedBlock& aligned_block = colldata.block((BlockType)cmp, dpos);
                        // copy from colldata
                        int16_t dc = block[ 0 ] = aligned_block.dc();
                        for ( bpos = 1; bpos < 64; bpos++ )
                            block[ bpos ] = aligned_block.coefficients_zigzag(bpos);

                        // diff coding for dc
                        block[ 0 ] -= lastdc[ cmp ];
                        lastdc[ cmp ] = dc;

                        // encode block
                        eob = encode_block_seq( huffw,
                            &(hcodes[ 0 ][ cmpnfo[cmp].huffdc ]),
                            &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                                                block.begin() );

                        // check for errors, proceed if no error encountered
                        if ( eob < 0 ) sta = -1;
                        else sta = next_mcuposn( &cmp, &dpos, &rstw);
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                        }
                        if (str_out->has_exceeded_bound()) {
                            sta = 2;
                        }

                    }
                }
                else if ( cs_to == 0 ) {
                    if ( cs_sah == 0 ) {
                        // ---> progressive non interleaved DC encoding <---
                        // ---> succesive approximation first stage <---
                        while ( sta == 0 ) {
                            // diff coding & bitshifting for dc
                            tmp = colldata.at((BlockType)cmp , 0 , dpos ) >> cs_sal;
                            block[ 0 ] = tmp - lastdc[ cmp ];
                            lastdc[ cmp ] = tmp;

                            // encode dc
                            sta = encode_dc_prg_fs( huffw,
                                &(hcodes[ 0 ][ cmpnfo[cmp].huffdc ]),
                                                    block.begin() );

                            // check for errors, increment dpos otherwise
                            if ( sta != -1 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if (sta == 0 && huffw->no_remainder()) {
                                merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                            }
                            if (str_out->has_exceeded_bound()) {
                                sta = 2;
                            }

                        }
                    }
                    else {
                        // ---> progressive non interleaved DC encoding <---
                        // ---> succesive approximation later stage <---
                        while ( sta == 0 ) {
                            // fetch bit from current bitplane
                            block[ 0 ] = BITN( colldata.at((BlockType)cmp , 0 , dpos ), cs_sal );

                            // encode dc correction bit
                            sta = encode_dc_prg_sa( huffw, block.begin() );

                            // next mcupos if no error happened
                            if ( sta != -1 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                        }
                        if (str_out->has_exceeded_bound()) {
                            sta = 2;
                        }
                    }
                }
                else {
                    if ( cs_sah == 0 ) {
                        // ---> progressive non interleaved AC encoding <---
                        // ---> succesive approximation first stage <---
                        while ( sta == 0 ) {
                            const AlignedBlock& aligned_block = colldata.block((BlockType)cmp, dpos);
                            // copy from colldata
                            for ( bpos = cs_from; bpos <= cs_to; bpos++ ) {
                                block[ bpos ] =
                                    FDIV2( aligned_block.coefficients_zigzag(bpos), cs_sal );
                            }
                            // encode block
                            eob = encode_ac_prg_fs( huffw,
                                &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                                                    block.begin(), &eobrun, cs_from, cs_to );

                            // check for errors, proceed if no error encountered
                            if ( eob < 0 ) sta = -1;
                            else sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if (sta == 0 && huffw->no_remainder()) {
                                merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                            }
                            if (str_out->has_exceeded_bound()) {
                                sta = 2;
                            }

                        }

                        // encode remaining eobrun
                        encode_eobrun( huffw,
                            &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                            &eobrun );

                    }
                    else {
                        // ---> progressive non interleaved AC encoding <---
                        // ---> succesive approximation later stage <---
                        while ( sta == 0 ) {
                            const AlignedBlock& aligned_block= colldata.block((BlockType)cmp, dpos);
                            // copy from colldata
                            for ( bpos = cs_from; bpos <= cs_to; bpos++ ) {
                                block[ bpos ] =
                                    FDIV2( aligned_block.coefficients_zigzag(bpos), cs_sal );
                            }
                            // encode block
                            eob = encode_ac_prg_sa( huffw, storw,
                                &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                                block.begin(), &eobrun, cs_from, cs_to );

                            // check for errors, proceed if no error encountered
                            if ( eob < 0 ) sta = -1;
                            else sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if (sta == 0 && huffw->no_remainder()) {
                                merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                            }
                            if (str_out->has_exceeded_bound()) {
                                sta = 2;
                            }

                        }

                        // encode remaining eobrun
                        encode_eobrun( huffw,
                            &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                            &eobrun );

                        // encode remaining correction bits
                        encode_crbits( huffw, storw );
                    }
                }
            }

            // pad huffman writer
            huffw->pad( padbit );

            // evaluate status
            if ( sta == -1 ) { // status -1 means error
                fprintf( stderr, "encode error in scan%i / mcu%i",
                    scnc, ( cs_cmpc > 1 ) ? mcu : dpos );
                delete huffw;
                errorlevel.store(2);
                return false;
            }
            else if ( sta == 2 ) { // status 2 means done
                scnc++; // increment scan counter
                break; // leave decoding loop, everything is done here
            }
            else if ( sta == 1 ) { // status 1 means restart
                if ( rsti > 0 ) // store rstp & stay in the loop
                    rstp.at(rstc++) = huffw->getpos() - 1;
            }
            huffw->flush_no_pad();
            dev_assert(huffw->no_remainder() && "this should have been padded");
            if (huffw->no_remainder()) {
                merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
            }
        }
    }

    // safety check for error in huffwriter
    if ( huffw->error ) {
        delete huffw;
        fprintf( stderr, MEM_ERRMSG );
        errorlevel.store(2);
        return false;
    }

    // get data into huffdata
    huffdata = huffw->getptr();
    hufs = huffw->getpos();
    always_assert(huffw->no_remainder() && "this should have been padded");
    merge_jpeg_streaming(&streaming_progress, huffdata, hufs, true);
    if (!fast_exit) {
        delete huffw;

        // remove storage writer
        delete storw;
    }
    // store last scan & restart positions
    if ((size_t)scnc >= scnp.size()) {
        delete huffw;
        fprintf( stderr, MEM_ERRMSG );
        errorlevel.store(2);
        return false;
    }
    scnp.at(scnc) = hufs;
    if ( !rstp.empty() )
        rstp.at(rstc) = hufs;


    return true;
}




/* -----------------------------------------------
    checks range of values, error if out of bounds
    ----------------------------------------------- */

bool check_value_range( void )
{
    int bad_cmp = 0, bad_bpos = 0, bad_dpos = 0;
    bool bad_colldata = false;
    // out of range should never happen with unmodified JPEGs
    for (int cmp = 0; cmp < cmpc && cmp < 4; cmp++ ) {
        int absmax[64];
        for (int bpos = 0; bpos < 64; bpos++ ) {
            absmax[zigzag_to_aligned.at(bpos)] = MAX_V( cmp, bpos );
        }
        for (int dpos = 0; dpos < cmpnfo[cmp].bc && dpos <= max_dpos[cmp] ; dpos++ ) {
            const int16_t * raw_data = colldata.block_nosync((BlockType)cmp, dpos).raw_data();
            for (int aligned_pos = 0; aligned_pos < 64; ++aligned_pos, ++raw_data) {
                if ((*raw_data) > absmax[aligned_pos] ||
                    (*raw_data) < -absmax[aligned_pos]) {
                    int bpos = aligned_to_zigzag.at(aligned_pos);
                    if (!early_eof_encountered) {
                        fprintf( stderr, "value out of range error: cmp%i, frq%i, val %i, max %i",
                             cmp, bpos, colldata.at_nosync((BlockType)cmp,bpos,dpos), absmax[aligned_pos] );
                        errorlevel.store(2);
                        return false;
                    }
                    bad_cmp = cmp;
                    bad_bpos = bpos;
                    bad_dpos = dpos;
                    colldata.set((BlockType)bad_cmp, bad_bpos, bad_dpos) = 0; // zero this puppy out
                    bad_colldata = true;
                }
            }
        }
    }
    if (bad_colldata) {
        colldata.set((BlockType)bad_cmp, bad_bpos, bad_dpos) = 0; // zero this puppy out
    }
    return true;
}


class ThreadHandoffSegmentCompare {
public: bool operator() (const ThreadHandoff &a,
                         const ThreadHandoff &b) const {
    return a.segment_size < b.segment_size;
}
};

/* -----------------------------------------------
    write uncompressed JPEG file
    ----------------------------------------------- */
bool write_ujpg(std::vector<ThreadHandoff> row_thread_handoffs,
                std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >*jpeg_file_raw_bytes)
{
    unsigned char ujpg_mrk[ 64 ];
    bool has_lepton_entropy_coding = (ofiletype == LEPTON || filetype == LEPTON );
    Sirikata::JpegError err = Sirikata::JpegError::nil();

    if (!has_lepton_entropy_coding) {
        // UJG-Header
        err = ujg_out->Write( ujg_header, 2 ).second;
    } else {
        // lepton-Header
        err = ujg_out->Write( lepton_header, 2 ).second;
    }
    // store version number
    ujpg_mrk[ 0 ] = ujgversion;
    ujg_out->Write( ujpg_mrk, 1 );

    // discard meta information from header if needed
    if ( disc_meta )
        if ( !rebuild_header_jpg() )
            return false;
    if (start_byte) {
        std::vector<ThreadHandoff> local_row_thread_handoffs;
        for (std::vector<ThreadHandoff>::iterator i = row_thread_handoffs.begin(),
                 ie = row_thread_handoffs.end(); i != ie; ++i) {
            auto j = i;
            ++j;
            if ((j == ie || i->segment_size >= start_byte)
                && (max_file_size == 0 || i->segment_size <= max_file_size + start_byte)) {
                local_row_thread_handoffs.push_back(*i);
                //fprintf(stderr, "OK: %d (%d %d)\n", i->segment_size, i->luma_y_start, i->luma_y_end);
            } else {
                //fprintf(stderr, "XX: %d (%d %d)\n", i->segment_size, i->luma_y_start, i->luma_y_end);
            }
        }
        row_thread_handoffs.swap(local_row_thread_handoffs);
    }
    if (start_byte) {
        always_assert(jpeg_file_raw_bytes);
    }
    if (start_byte && jpeg_file_raw_bytes && !row_thread_handoffs.empty()) { // FIXME: respect jpeg_embedding?
        if (row_thread_handoffs[0].segment_size >= start_byte) {
            prefix_grbs = row_thread_handoffs[0].segment_size - start_byte;
            if (row_thread_handoffs.size() > 1) {
                if (prefix_grbs) {
                    --prefix_grbs; //FIXME why is this ?!
                }
            }
        } else {
            prefix_grbs = 0;
            custom_exit(ExitCode::ONLY_GARBAGE_NO_JPEG);
        }
        if (prefix_grbs > 0) {
            prefix_grbgdata = aligned_alloc(prefix_grbs);
            always_assert(jpeg_file_raw_bytes->size() >= (size_t)prefix_grbs + start_byte);
            memcpy(prefix_grbgdata,
                   &(*jpeg_file_raw_bytes)[start_byte],
                   std::min((size_t)prefix_grbs,
                            jpeg_file_raw_bytes->size() - start_byte));
        } else {
            prefix_grbgdata = aligned_alloc(1); // so it's nonnull
            prefix_grbgdata[0] = 0;
        }
    }
    Sirikata::MemReadWriter mrw((Sirikata::JpegAllocator<uint8_t>()));
#if 0
    for (uint32_t i = 0; i < row_thread_handoffs.size() ; ++ i) {
        fprintf(stderr,
                "Row [%d - %d], %d size %d overhang byte %d num overhang bits %d  dc %d %d %d\n",
                (int)row_thread_handoffs[i].luma_y_start,
                (int)row_thread_handoffs[i].luma_y_end,
                (int)i,
                (int)row_thread_handoffs[i].segment_size,
                (int)row_thread_handoffs[i].overhang_byte,
                (int)row_thread_handoffs[i].num_overhang_bits,
                (int)row_thread_handoffs[i].last_dc[0],
                (int)row_thread_handoffs[i].last_dc[1],
                (int)row_thread_handoffs[i].last_dc[2]);
    }
#endif
    uint32_t framebuffer_byte_size = row_thread_handoffs.back().segment_size - row_thread_handoffs.front().segment_size;
    uint32_t num_rows = row_thread_handoffs.size();
    NUM_THREADS = std::min(NUM_THREADS, (unsigned int)max_encode_threads);
    if (num_rows / 2 < NUM_THREADS) {
        unsigned int desired_count = std::max((unsigned int)num_rows / 2,
                                              (unsigned int)min_encode_threads);
        NUM_THREADS = std::min(std::max(desired_count, 1U), (unsigned int)NUM_THREADS);
    }
    if (framebuffer_byte_size < 125000) {
        NUM_THREADS = std::min(std::max(min_encode_threads, 1U), (unsigned int)NUM_THREADS);
    } else if (framebuffer_byte_size < 250000) {
        NUM_THREADS = std::min(std::max(min_encode_threads, 2U), (unsigned int)NUM_THREADS);
    } else if (framebuffer_byte_size < 500000) {
        NUM_THREADS = std::min(std::max(min_encode_threads, 4U), (unsigned int)NUM_THREADS);
    }
    //fprintf(stderr, "Byte size %d num_rows %d Using num threads %u\n", framebuffer_byte_size, num_rows, NUM_THREADS);
    std::vector<ThreadHandoff> selected_splits(NUM_THREADS);
    std::vector<int> split_indices(NUM_THREADS);
    for (uint32_t i = 0; g_even_thread_split == false && i < NUM_THREADS - 1 ; ++ i) {
        ThreadHandoff desired_handoff = row_thread_handoffs.back();
        if(max_file_size && max_file_size + start_byte < desired_handoff.segment_size) {
            desired_handoff.segment_size += row_thread_handoffs.front().segment_size;
        }
        desired_handoff.segment_size -= row_thread_handoffs.front().segment_size;
        
        desired_handoff.segment_size *= (i + 1);
        desired_handoff.segment_size /= NUM_THREADS;
        desired_handoff.segment_size += row_thread_handoffs.front().segment_size;
        auto split = std::lower_bound(row_thread_handoffs.begin() + 1, row_thread_handoffs.end(),
                                      desired_handoff,
                                      ThreadHandoffSegmentCompare());
        if (split == row_thread_handoffs.begin() && split != row_thread_handoffs.end()) {
            //++split;
        } else if (split != row_thread_handoffs.begin() + 1) {
            --split;
        }
        split_indices[i] = split - row_thread_handoffs.begin();
    }
    for (uint32_t i = 0; g_even_thread_split && i < NUM_THREADS - 1 ; ++ i) {
        split_indices[i] = row_thread_handoffs.size() * (i + 1) / NUM_THREADS;
    }
    for (uint32_t index = 0; index < NUM_THREADS - 1 ; ++ index) {
        if (split_indices[index] == split_indices[index + 1]) {
            for (uint32_t i = 0; i < NUM_THREADS - 1 ; ++ i) {
                split_indices[i] = (i + 1) * row_thread_handoffs.size() / NUM_THREADS;
            }
            break;
        }
    }
    split_indices[NUM_THREADS - 1] = row_thread_handoffs.size() - 1;
    size_t last_split_index = 0;
    for (size_t i = 0; i < selected_splits.size(); ++i) {
        size_t beginning_of_range = last_split_index;
        size_t end_of_range = split_indices[i];
        //fprintf(stderr, "Beginning %ld end %ld\n", beginning_of_range, end_of_range);
        last_split_index = end_of_range;
        always_assert( end_of_range < row_thread_handoffs.size() );
        selected_splits[i] = row_thread_handoffs[ end_of_range ] - row_thread_handoffs[ beginning_of_range ];
        if (i + 1 == selected_splits.size() && row_thread_handoffs[ end_of_range ].num_overhang_bits) {
            ++selected_splits[i].segment_size; // need room for that last byte to hold the overhang byte
        }
#if 0
        fprintf(stderr, "%d->%d) %d - %d {%ld}\n", selected_splits[i].luma_y_start,
                selected_splits[i].luma_y_end, 
                row_thread_handoffs[ beginning_of_range ].segment_size,
                row_thread_handoffs[ end_of_range ].segment_size, row_thread_handoffs.size());
#endif
/*
        if (i + 1 == selected_splits.size()) {
            int tmp = selected_splits[i].segment_size;
            selected_splits[i].segment_size = jpgfilesize - row_thread_handoffs[ beginning_of_range ].segment_size;
            fprintf(stderr, "Split size was %x and is %x - %x = %x\n", tmp, jpgfilesize, row_thread_handoffs[ beginning_of_range ].segment_size, selected_splits[i].segment_size);
        }
*/
    }
#if 0
    for (uint32_t i = 0; i < selected_splits.size() ; ++ i) {
        fprintf(stderr,
                "Row [%d - %d] %d size %d overhang byte %d num overhang bits %d  dc %d %d %d\n",
                (int)selected_splits[i].luma_y_start,
                (int)selected_splits[i].luma_y_end,

                (int)i,
                (int)selected_splits[i].segment_size,
                (int)selected_splits[i].overhang_byte,
                (int)selected_splits[i].num_overhang_bits,
                (int)selected_splits[i].last_dc[0],
                (int)selected_splits[i].last_dc[1],
                (int)selected_splits[i].last_dc[2]);
    }
#endif

    always_assert(start_byte||!selected_splits[0].luma_y_start);
    // write header to file
    // marker: "HDR" + [size of header]
    unsigned char hdr_mrk[] = {'H', 'D', 'R'};
    err = mrw.Write( hdr_mrk, sizeof(hdr_mrk) ).second;
    uint32toLE(hdrs, ujpg_mrk);
    err = mrw.Write( ujpg_mrk, 4).second;
    // data: data from header
    mrw.Write( hdrdata, hdrs );
    // beginning here: recovery information (needed for exact JPEG recovery)

    // marker: P0D"
    unsigned char pad_mrk[] = {'P', '0', 'D'};
    err = mrw.Write( pad_mrk, sizeof(pad_mrk) ).second;
    // data: padbit
    err = mrw.Write( (unsigned char*) &padbit, 1 ).second;

    // write luma splits
    unsigned char luma_mrk[1] = {'H'};
    err = mrw.Write( luma_mrk, sizeof(luma_mrk) ).second;
    // data: serialized luma splits
    auto serialized_splits = ThreadHandoff::serialize(&selected_splits[0], selected_splits.size());
    err = mrw.Write(&serialized_splits[0], serialized_splits.size()).second;

    if (!rst_cnt.empty()) {
        unsigned char frs_mrk[] = {'C', 'R', 'S'};
        err = mrw.Write( frs_mrk, 3 ).second;
        uint32toLE((uint32_t)rst_cnt.size(), ujpg_mrk);
        err = mrw.Write( ujpg_mrk, 4).second;
        for (size_t i = 0; i < rst_cnt.size(); ++i) {
            uint32toLE((uint32_t)rst_cnt.at(i), ujpg_mrk);
            err = mrw.Write( ujpg_mrk, 4).second;
        }
    }
    // write number of false set RST markers per scan (if available) to file
    if (!rst_err.empty()) {
        // marker: "FRS" + [number of scans]
        unsigned char frs_mrk[] = {'F', 'R', 'S'};
        err = mrw.Write( frs_mrk, 3 ).second;
        uint32toLE((uint32_t)rst_err.size(), ujpg_mrk);
        err = mrw.Write( ujpg_mrk, 4).second;
        // data: numbers of false set markers
        err = mrw.Write( rst_err.data(), rst_err.size() ).second;
    }
    if (early_eof_encountered) {
        unsigned char early_eof[] = {'E', 'E', 'E'};
        err = mrw.Write( early_eof, sizeof(early_eof) ).second;
        uint32toLE(max_cmp, ujpg_mrk);
        uint32toLE(max_bpos, ujpg_mrk + 4);
        uint32toLE(max_sah, ujpg_mrk + 8);
        uint32toLE(max_dpos[0], ujpg_mrk + 12);
        uint32toLE(max_dpos[1], ujpg_mrk + 16);
        uint32toLE(max_dpos[2], ujpg_mrk + 20);
        uint32toLE(max_dpos[3], ujpg_mrk + 24);
        err = mrw.Write(ujpg_mrk, 28).second;
    }
    // write garbage (data including and after EOI) (if any) to file
    if ( prefix_grbs > 0 || prefix_grbgdata != NULL) {
        // marker: "GRB" + [size of garbage]
        unsigned char grb_mrk[] = {'P', 'G', embedded_jpeg ? (unsigned char)'E': (unsigned char)'R'};
        err = mrw.Write( grb_mrk, sizeof(grb_mrk) ).second;
        uint32toLE(prefix_grbs, ujpg_mrk);
        err = mrw.Write( ujpg_mrk, 4 ).second;
        // data: garbage data
        err = mrw.Write( prefix_grbgdata, prefix_grbs ).second;
    }
    // write garbage (data including and after EOI) (if any) to file
    if ( grbs > 0 ) {
        // marker: "GRB" + [size of garbage]
        unsigned char grb_mrk[] = {'G', 'R', 'B'};
        err = mrw.Write( grb_mrk, sizeof(grb_mrk) ).second;
        uint32toLE(grbs, ujpg_mrk);
        err = mrw.Write( ujpg_mrk, 4 ).second;
        // data: garbage data
        err = mrw.Write( grbgdata, grbs ).second;
    }
    if (mrw.buffer().size() > 1024 * 1024) {
        //custom_exit(ExitCode::HEADER_TOO_LARGE);
    }
    std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> > compressed_header;
    if (ujgversion == 1) {
        compressed_header =
            Sirikata::ZlibDecoderCompressionWriter::Compress(mrw.buffer().data(),
                                                             mrw.buffer().size(),
                                                             Sirikata::JpegAllocator<uint8_t>());
    } else {
        compressed_header = Sirikata::BrotliCodec::Compress(mrw.buffer().data(),
                                                            mrw.buffer().size(),
                                                            Sirikata::JpegAllocator<uint8_t>());
    }
    write_byte_bill(Billing::HEADER, false, 2 + hdrs + prefix_grbs + grbs);
    static_assert(MAX_NUM_THREADS <= 255, "We only have a single byte for num threads");
    always_assert(NUM_THREADS <= 255);
    unsigned char zed[] = {'\0'};
    if (start_byte != 0) {
        zed[0] = (unsigned char)'Y';
    } else if (g_allow_progressive) {
        zed[0] = (unsigned char)'X';
    } else {
        zed[0] = (unsigned char)'Z';
    }
    err =  ujg_out->Write(zed, sizeof(zed)).second;
    unsigned char num_threads[] = {(unsigned char)NUM_THREADS};
    err =  ujg_out->Write(num_threads, sizeof(num_threads)).second;
    unsigned char zero3[3] = {};
    err =  ujg_out->Write(zero3, sizeof(zero3)).second;
    unsigned char git_revision[12] = {0}; // we only have 12 chars in the header for this
    hex_to_bin(git_revision, GIT_REVISION, sizeof(git_revision));
    err = ujg_out->Write(git_revision, sizeof(git_revision) ).second;
    uint32toLE(jpgfilesize - start_byte, ujpg_mrk);
    err = ujg_out->Write( ujpg_mrk, 4).second;
    write_byte_bill(Billing::HEADER, true, 24);
    uint32toLE((uint32_t)compressed_header.size(), ujpg_mrk);
    err = ujg_out->Write( ujpg_mrk, 4).second;
    write_byte_bill(Billing::HEADER, true, 4);
    auto err2 = ujg_out->Write(compressed_header.data(),
                               compressed_header.size());
    write_byte_bill(Billing::HEADER, true, compressed_header.size());
    zlib_hdrs = compressed_header.size();
    if (err != Sirikata::JpegError::nil() || err2.second != Sirikata::JpegError::nil()) {
        fprintf( stderr, "write error, possibly drive is full" );
        errorlevel.store(2);
        return false;
    }
    unsigned char cmp_mrk[] = {'C', 'M', 'P'};
    err = ujg_out->Write( cmp_mrk, sizeof(cmp_mrk) ).second;
    write_byte_bill(Billing::HEADER, true, 3);
    while (g_encoder->encode_chunk(&colldata, ujg_out,
                                   &selected_splits[0], selected_splits.size()) == CODING_PARTIAL) {
    }
    
    // errormessage if write error
    if ( err != Sirikata::JpegError::nil() ) {
        fprintf( stderr, "write error, possibly drive is full" );
        errorlevel.store(2);
        return false;
    }

    // get filesize, if avail
    if (ujg_out) {
        ujgfilesize = ujg_out->getsize();
    }


    return true;
}


/* -----------------------------------------------
    read uncompressed JPEG file
    ----------------------------------------------- */
#if !defined(USE_STANDARD_MEMORY_ALLOCATORS) && !defined(_WIN32) && !defined(EMSCRIPTEN)
void mem_nop (void *opaque, void *ptr){

}
void * mem_init_nop(size_t prealloc_size, uint8_t align){
    return NULL;
}
void* mem_realloc_nop(void * ptr, size_t size, size_t *actualSize, unsigned int movable, void *opaque){
    return NULL;
}
#endif

Sirikata::MemReadWriter *header_reader = NULL;

bool read_ujpg( void )
{
    using namespace IOUtil;
    using namespace Sirikata;
//    colldata.start_decoder_worker_thread(std::bind(&simple_decoder, &colldata, str_in));
    unsigned char ujpg_mrk[ 64 ];
    // this is where we will enable seccomp, before reading user data
    write_byte_bill(Billing::HEADER, true, 24); // for the fixed header

    str_out->call_size_callback(max_file_size);
    uint32_t compressed_header_size = 0;
    if (ReadFull(str_in, ujpg_mrk, 4) != 4) {
        custom_exit(ExitCode::SHORT_READ);
    }
    write_byte_bill(Billing::HEADER, true, 4);

    compressed_header_size = LEtoUint32(ujpg_mrk);
    if (compressed_header_size > 128 * 1024 * 1024 || max_file_size > 128 * 1024 * 1024) {
        always_assert(false && "Only support images < 128 megs");
        return false; // bool too big
    }
    bool pending_header_reads = false;
    if (header_reader == NULL) {
        std::vector<uint8_t, JpegAllocator<uint8_t> > compressed_header_buffer(compressed_header_size);
        IOUtil::ReadFull(str_in, compressed_header_buffer.data(), compressed_header_buffer.size());
        header_reader = new MemReadWriter((JpegAllocator<uint8_t>()));
        {
            if (ujgversion == 1) {
                JpegAllocator<uint8_t> no_free_allocator;
#if !defined(USE_STANDARD_MEMORY_ALLOCATORS) && !defined(_WIN32) && !defined(EMSCRIPTEN)
                no_free_allocator.setup_memory_subsystem(32 * 1024 * 1024,
                                                         16,
                                                         &mem_init_nop,
                                                         &MemMgrAllocatorMalloc,
                                                         &mem_nop,
                                                         &mem_realloc_nop,
                                                         &MemMgrAllocatorMsize);
#endif
                std::pair<std::vector<uint8_t,
                                      Sirikata::JpegAllocator<uint8_t> >,
                          JpegError> uncompressed_header_buffer(
                              ZlibDecoderDecompressionReader::Decompress(compressed_header_buffer.data(),
                                                                         compressed_header_buffer.size(),
                                                                         no_free_allocator,
                                                                         max_file_size + 2048));
                if (uncompressed_header_buffer.second) {
                    always_assert(false && "Data not properly zlib coded");
                    return false;
                }
                zlib_hdrs = compressed_header_buffer.size();
                header_reader->SwapIn(uncompressed_header_buffer.first, 0);
            } else {
                std::pair<std::vector<uint8_t,
                                      Sirikata::JpegAllocator<uint8_t> >,
                          JpegError> uncompressed_header_buffer(
                              Sirikata::BrotliCodec::Decompress(compressed_header_buffer.data(),
                                                                compressed_header_buffer.size(),
                                                                JpegAllocator<uint8_t>(),
                                                                ((size_t)max_file_size) * 2 + 128 * 1024 * 1024));
                if (uncompressed_header_buffer.second) {
                    always_assert(false && "Data not properly zlib coded");
                    return false;
                }
                zlib_hdrs = compressed_header_buffer.size();
                header_reader->SwapIn(uncompressed_header_buffer.first, 0);            
            }
        }
        write_byte_bill(Billing::HEADER,
                        true,
                        compressed_header_buffer.size());
    } else {
        always_assert(compressed_header_size == 0 && "Special concatenation requires 0 size header");
    }
    grbs = sizeof(EOI);
    grbgdata = EOI; // if we don't have any garbage, assume FFD9 EOI
    // read header from file
    ReadFull(header_reader, ujpg_mrk, 3 ) ;
    // check marker
    if ( memcmp( ujpg_mrk, "HDR", 3 ) == 0 ) {
        // read size of header, alloc memory
        ReadFull(header_reader, ujpg_mrk, 4 );
        hdrs = LEtoUint32(ujpg_mrk);
        hdrdata = (unsigned char*) aligned_alloc(hdrs);
        memset(hdrdata, 0, hdrs);
        if ( hdrdata == NULL ) {
            fprintf( stderr, MEM_ERRMSG );
            errorlevel.store(2);
            return false;
        }
        // read hdrdata
        ReadFull(header_reader, hdrdata, hdrs );
    }
    else {
        fprintf( stderr, "HDR marker not found" );
        errorlevel.store(2);
        return false;
    }
    bool memory_optimized_image = (filetype != UJG) && !g_allow_progressive;
    // parse header for image-info
    if ( !setup_imginfo_jpg(memory_optimized_image) )
        return false;

    // beginning here: recovery information (needed for exact JPEG recovery)

    // read padbit information from file
    ReadFull(header_reader, ujpg_mrk, 3 );
    // check marker
    if ( memcmp( ujpg_mrk, "P0D", 3 ) == 0 ) {
        // This is a more nuanced pad byte that can have different values per bit
        header_reader->Read( reinterpret_cast<unsigned char*>(&padbit), 1 );
    }
    else if ( memcmp( ujpg_mrk, "PAD", 3 ) == 0 ) {
        // this is a single pad bit that is implied to have all the same values
        header_reader->Read( reinterpret_cast<unsigned char*>(&padbit), 1 );
        if (!(padbit == 0 || padbit == 1 ||padbit == -1)) {
            while (write(2,
                        "Legacy Padbit must be 0, 1 or -1\n",
                         strlen("Legacy Padbit must be 0, 1 or -1\n")) < 0
                   && errno == EINTR) {
            }
            custom_exit(ExitCode::STREAM_INCONSISTENT);
        }
        if (padbit == 1) {
            padbit = 0x7f; // all 6 bits set
        }
    }
    else {
        fprintf( stderr, "PAD marker not found" );
        errorlevel.store(2);
        return false;
    }
    std::vector<ThreadHandoff> thread_handoff;
    // read further recovery information if any
    while ( ReadFull(header_reader, ujpg_mrk, 3 ) == 3 ) {
        // check marker
        if ( memcmp( ujpg_mrk, "CRS", 3 ) == 0 ) {
            rst_cnt_set = true;
            ReadFull(header_reader, ujpg_mrk, 4);
            rst_cnt.resize(LEtoUint32(ujpg_mrk));
            for (size_t i = 0; i < rst_cnt.size(); ++i) {
                ReadFull(header_reader, ujpg_mrk, 4);
                rst_cnt.at(i) = LEtoUint32(ujpg_mrk);
            }
        } else if ( memcmp( ujpg_mrk, "HHX", 2 ) == 0 ) { // only look at first two bytes
            size_t to_alloc = ThreadHandoff::get_remaining_data_size_from_two_bytes(ujpg_mrk + 1) + 2;
            if(to_alloc) {
                std::vector<unsigned char> data(to_alloc);
                data[0] = ujpg_mrk[1];
                data[1] = ujpg_mrk[2];
                ReadFull(header_reader, &data[2], to_alloc - 2);
                thread_handoff = ThreadHandoff::deserialize(&data[0], to_alloc);
            }
        } else if ( memcmp( ujpg_mrk, "FRS", 3 ) == 0 ) {
            // read number of false set RST markers per scan from file
            ReadFull(header_reader, ujpg_mrk, 4);
            scnc = LEtoUint32(ujpg_mrk);
            
            rst_err.insert(rst_err.end(), scnc - rst_err.size(), 0);
            // read data
            ReadFull(header_reader, rst_err.data(), scnc );
        }
        else if ( memcmp( ujpg_mrk, "GRB", 3 ) == 0 ) {
            // read garbage (data after end of JPG) from file
            ReadFull(header_reader, ujpg_mrk, 4);
            grbs = LEtoUint32(ujpg_mrk);
            grbgdata = aligned_alloc(grbs);
            if ( grbgdata == NULL ) {
                fprintf( stderr, MEM_ERRMSG );
                errorlevel.store(2);
                return false;
            }
            memset(grbgdata, 0, grbs);
            // read garbage data
            ReadFull(header_reader, grbgdata, grbs );
        }
        else if ( memcmp( ujpg_mrk, "PGR", 3 ) == 0 || memcmp( ujpg_mrk, "PGE", 3 ) == 0 ) {
            // read prefix garbage (data before beginning of JPG) from file
            if (ujpg_mrk[2] == 'E') {
                // embedded jpeg: full header required
                embedded_jpeg = true;
            }
            ReadFull(header_reader, ujpg_mrk, 4);
            prefix_grbs = LEtoUint32(ujpg_mrk);
            prefix_grbgdata = aligned_alloc(prefix_grbs);
            memset(prefix_grbgdata, 0, sizeof(prefix_grbs));
            if ( prefix_grbgdata == NULL ) {
                fprintf( stderr, MEM_ERRMSG );
                errorlevel.store(2);
                return false;
            }
            // read garbage data
            ReadFull(header_reader, prefix_grbgdata, prefix_grbs );
        }
        else if ( memcmp( ujpg_mrk, "SIZ", 3 ) == 0 ) {
            // full size of the original file
            ReadFull(header_reader, ujpg_mrk, 4);
            max_file_size = LEtoUint32(ujpg_mrk);
        }
        else if ( memcmp( ujpg_mrk, "EEE", 3) == 0) {
            ReadFull(header_reader, ujpg_mrk, 28);
            max_cmp = LEtoUint32(ujpg_mrk);
            max_bpos = LEtoUint32(ujpg_mrk + 4);
            max_sah = LEtoUint32(ujpg_mrk + 8);
            max_dpos[0] = LEtoUint32(ujpg_mrk + 12);
            max_dpos[1] = LEtoUint32(ujpg_mrk + 16);
            max_dpos[2] = LEtoUint32(ujpg_mrk + 20);
            max_dpos[3] = LEtoUint32(ujpg_mrk + 24);
            early_eof_encountered = true;
            colldata.set_truncation_bounds(max_cmp, max_bpos, max_dpos, max_sah);
        }
        else {
            if (memcmp(ujpg_mrk, "CNT", 3) == 0 ) {
                pending_header_reads = true;
                break;
            } else if (memcmp(ujpg_mrk, "CMP", 3) == 0 ) {
                break;
            } else {
                fprintf( stderr, "unknown data found" );
                errorlevel.store(2);
            }
            return false;
        }
    }
    if (!pending_header_reads) {
        delete header_reader;
        header_reader = NULL;
    }
    write_byte_bill(Billing::HEADER,
                    false,
                    2 + hdrs + prefix_grbs + grbs);

    ReadFull(str_in, ujpg_mrk, 3 ) ;
    write_byte_bill(Billing::HEADER, true, 3);

    write_byte_bill(Billing::DELIMITERS, true, 4 * NUM_THREADS); // trailing vpx_encode bits
    write_byte_bill(Billing::HEADER, true, 4); //trailing size

    if (memcmp(ujpg_mrk, "CMP", 3) != 0) {
        always_assert(false && "CMP must be present (uncompressed) in the file or CNT continue marker");
        return false; // not a JPG
    }
    colldata.signal_worker_should_begin();
    g_decoder->initialize(str_in, thread_handoff);
    colldata.start_decoder(g_decoder);
    return true;
}


/* -----------------------------------------------
    set each variable to its initial value
    ----------------------------------------------- */

bool reset_buffers( void )
{
    int cmp, bpos;
    int i;


    // -- free buffers --

    // free buffers & set pointers NULL
    if ( hdrdata  != NULL ) aligned_dealloc ( hdrdata );
    if ( huffdata != NULL ) aligned_dealloc ( huffdata );
    if ( grbgdata != NULL && grbgdata != EOI ) aligned_dealloc ( grbgdata );
    rst_err.clear();
    rstp.resize(0);
    scnp.resize(0);
    hdrdata   = NULL;
    huffdata  = NULL;
    grbgdata  = NULL;

    // free image arrays
    colldata.reset();


    // -- set variables --

    // preset componentinfo
    for ( cmp = 0; cmp < 4; cmp++ ) {
        cmpnfo[ cmp ].sfv = -1;
        cmpnfo[ cmp ].sfh = -1;
        cmpnfo[ cmp ].mbs = -1;
        cmpnfo[ cmp ].bcv = -1;
        cmpnfo[ cmp ].bch = -1;
        cmpnfo[ cmp ].bc  = -1;
        cmpnfo[ cmp ].ncv = -1;
        cmpnfo[ cmp ].nch = -1;
        cmpnfo[ cmp ].nc  = -1;
        cmpnfo[ cmp ].sid = -1;
        cmpnfo[ cmp ].jid = -1;
        cmpnfo[ cmp ].qtable = NULL;
        cmpnfo[ cmp ].huffdc = -1;
        cmpnfo[ cmp ].huffac = -1;
    }

    // preset imgwidth / imgheight / component count
    imgwidth  = 0;
    imgheight = 0;
    cmpc      = 0;

    // preset mcu info variables / restart interval
    sfhm      = 0;
    sfvm      = 0;
    mcuc      = 0;
    mcuh      = 0;
    mcuv      = 0;
    rsti      = 0;
    max_file_size = 0; // this file isn't truncated
    // reset quantization / huffman tables
    for ( i = 0; i < 4; i++ ) {
        htset[ 0 ][ i ] = 0;
        htset[ 1 ][ i ] = 0;
        for ( bpos = 0; bpos < 64; bpos++ )
            qtables[ i ][ bpos ] = 0;
    }

    // preset jpegtype
    jpegtype  = 0;

    // reset padbit
    padbit = -1;

    return true;
}

/* ----------------------- End of main functions -------------------------- */

/* ----------------------- Begin of JPEG specific functions -------------------------- */


/* -----------------------------------------------
    Parses header for imageinfo
    ----------------------------------------------- */
bool setup_imginfo_jpg(bool only_allocate_two_image_rows)
{
    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   hpos = 0; // position in header

    int cmp;

    // header parser loop
    while ( hpos < hdrs ) {
        type = hpos + 1 < hdrs ? hdrdata[ hpos + 1 ] : 0;
        len = 2 + B_SHORT( hpos + 2 < hdrs ? hdrdata[ hpos + 2 ] : 0, hpos + 3 < hdrs ? hdrdata[ hpos + 3 ] : 0);
        // do not parse DHT & DRI
        if ( ( type != 0xDA ) && ( type != 0xC4 ) && ( type != 0xDD ) ) {
            if ( !parse_jfif_jpg( type, len, hdrs-hpos, &( hdrdata[ hpos ] ) ) )
                return false;
        }
        hpos += len;
    }

    // check if information is complete
    if ( cmpc == 0 ) {
        fprintf( stderr, "header contains incomplete information" );
        errorlevel.store(2);
        return false;
    }
    for ( cmp = 0; cmp < cmpc; cmp++ ) {
        if ( ( cmpnfo[cmp].sfv == 0 ) ||
             ( cmpnfo[cmp].sfh == 0 ) ||
             ( cmpnfo[cmp].qtable == NULL ) ||
             ( cmpnfo[cmp].qtable[0] == 0 ) ||
             ( jpegtype == 0 ) ) {
            fprintf( stderr, "header information is incomplete" );
            errorlevel.store(2);
            return false;
        }
    }
        
    // do all remaining component info calculations
    for ( cmp = 0; cmp < cmpc; cmp++ ) {
        if ( cmpnfo[ cmp ].sfh > sfhm ) sfhm = cmpnfo[ cmp ].sfh;
        if ( cmpnfo[ cmp ].sfv > sfvm ) sfvm = cmpnfo[ cmp ].sfv;
    }
    mcuv = ( int ) ceil( (float) imgheight / (float) ( 8 * sfhm ) );
    mcuh = ( int ) ceil( (float) imgwidth  / (float) ( 8 * sfvm ) );
    mcuc  = mcuv * mcuh;
    int maxChromaWidth = 0;
    int maxChromaHeight = 0;
    int maxLumaWidth = 0;
    int maxLumaHeight = 0;
    for ( cmp = 0; cmp < cmpc; cmp++ ) {
        cmpnfo[ cmp ].mbs = cmpnfo[ cmp ].sfv * cmpnfo[ cmp ].sfh;
        cmpnfo[ cmp ].bcv = mcuv * cmpnfo[ cmp ].sfh;
        cmpnfo[ cmp ].bch = mcuh * cmpnfo[ cmp ].sfv;
        cmpnfo[ cmp ].bc  = cmpnfo[ cmp ].bcv * cmpnfo[ cmp ].bch;
        cmpnfo[ cmp ].ncv = ( int ) ceil( (float) imgheight *
                            ( (float) cmpnfo[ cmp ].sfh / ( 8.0 * sfhm ) ) );
        cmpnfo[ cmp ].nch = ( int ) ceil( (float) imgwidth *
                            ( (float) cmpnfo[ cmp ].sfv / ( 8.0 * sfvm ) ) );
        cmpnfo[ cmp ].nc  = cmpnfo[ cmp ].ncv * cmpnfo[ cmp ].nch;
        cmpnfo[cmp].check_valid_value_range();
        if (cmp == 0) {
            maxLumaWidth = cmpnfo[ cmp ].bch * 8;
            maxLumaHeight = cmpnfo[ cmp ].bcv * 8;
        } else {
            if (maxChromaWidth < cmpnfo[ cmp ].bch * 8) {
                maxChromaWidth = cmpnfo[ cmp ].bch * 8;
            }
            if (maxChromaHeight < cmpnfo[ cmp ].bcv * 8) {
                maxChromaHeight = cmpnfo[ cmp ].bcv * 8;
            }
        }
    }
    LeptonDebug::setupDebugData(maxLumaWidth, maxLumaHeight,
                                maxChromaWidth, maxChromaHeight);

    // decide components' statistical ids
    if ( cmpc <= 3 ) {
        for ( cmp = 0; cmp < cmpc; cmp++ ) cmpnfo[ cmp ].sid = cmp;
    }
    else {
        for ( cmp = 0; cmp < cmpc; cmp++ ) cmpnfo[ cmp ].sid = 0;
    }
    size_t start_allocated = Sirikata::memmgr_size_allocated();
    // alloc memory for further operations
    colldata.init(cmpnfo, cmpc, mcuh, mcuv, jpegtype == 1 && only_allocate_two_image_rows);
    size_t end_allocated = Sirikata::memmgr_size_allocated();
    total_framebuffer_allocated = end_allocated - start_allocated;
    return true;
}


/* -----------------------------------------------
    Parse routines for JFIF segments
    ----------------------------------------------- */
bool parse_jfif_jpg( unsigned char type, unsigned int len, unsigned int alloc_len, unsigned char* segment )
{
    unsigned int hpos = 4; // current position in segment, start after segment header
    int lval, rval; // temporary variables
    int skip;
    int cmp;
    int i;


    switch ( type )
    {
        case 0xC4: // DHT segment
            // build huffman trees & codes
            while ( hpos < len ) {
                lval = LBITS( hpos < alloc_len ? segment[ hpos ] : 0, 4 );
                rval = RBITS( hpos < alloc_len ? segment[ hpos ]: 0, 4 );
                if ( ((lval < 0) || (lval >= 2)) || ((rval < 0) || (rval >= 4)) )
                    break;

                hpos++;
                // build huffman codes & trees
                if (!build_huffcodes( &(segment[ hpos + 0 ]), alloc_len > hpos ? alloc_len - hpos : 0,
                                      &(segment[ hpos + 16 ]),  alloc_len > hpos + 16 ? alloc_len - hpos - 16 : 0,
                                      &(hcodes[ lval ][ rval ]), &(htrees[ lval ][ rval ]) )) {
                    errorlevel.store(2);
                    return false;
                }
                htset[ lval ][ rval ] = 1;

                skip = 16;
                for ( i = 0; i < 16; i++ )
                    skip += ( int ) (hpos + i < alloc_len ? segment[ hpos + i ] : 0);
                hpos += skip;
            }

            if ( hpos != len ) {
                // if we get here, something went wrong
                fprintf( stderr, "size mismatch in dht marker" );
                errorlevel.store(2);
                return false;
            }
            return true;

        case 0xDB: // DQT segment
            // copy quantization tables to internal memory
            while ( hpos < len ) {
                lval = LBITS( hpos < alloc_len ? segment[ hpos ] :  0, 4 );
                rval = RBITS( hpos < alloc_len ? segment[ hpos ] : 0, 4 );
                if ( (lval < 0) || (lval >= 2) ) break;
                if ( (rval < 0) || (rval >= 4) ) break;
                hpos++;
                if ( lval == 0 ) { // 8 bit precision
                    for ( i = 0; i < 64; i++ ) {
                        qtables[ rval ][ i ] = ( unsigned short ) (hpos + i < alloc_len ? segment[ hpos + i ] : 0);
                        if ( qtables[ rval ][ i ] == 0 ) break;
                    }
                    hpos += 64;
                }
                else { // 16 bit precision
                    for ( i = 0; i < 64; i++ ) {
                        qtables[ rval ][ i ] =
                            B_SHORT( (hpos + (2*i)< alloc_len ? segment[ hpos + (2*i) ] : 0), (hpos + 2*i+1 < alloc_len?segment[ hpos + (2*i) + 1 ]:0) );
                        if ( qtables[ rval ][ i ] == 0 ) break;
                    }
                    hpos += 128;
                }
            }

            if ( hpos != len ) {
                // if we get here, something went wrong
                fprintf( stderr, "size mismatch in dqt marker" );
                errorlevel.store(2);
                return false;
            }
            return true;

        case 0xDD: // DRI segment
            // define restart interval
          rsti = B_SHORT( hpos < alloc_len ? segment[ hpos ]:0, hpos +1 < alloc_len ?segment[ hpos + 1 ]:0 );
            return true;

        case 0xDA: // SOS segment
            // prepare next scan
            cs_cmpc = hpos < alloc_len ? segment[ hpos ] : 0;
            if ( cs_cmpc > cmpc ) {
                fprintf( stderr, "%i components in scan, only %i are allowed",
                            cs_cmpc, cmpc );
                errorlevel.store(2);
                return false;
            }
            hpos++;
            for ( i = 0; i < cs_cmpc; i++ ) {
                for ( cmp = 0; ( (hpos < alloc_len ? segment[ hpos ]:0) != cmpnfo[ cmp ].jid ) && ( cmp < cmpc ); cmp++ );
                if ( cmp == cmpc ) {
                    fprintf( stderr, "component id mismatch in start-of-scan" );
                    errorlevel.store(2);
                    return false;
                }
                cs_cmp[ i ] = cmp;
                cmpnfo[ cmp ].huffdc = LBITS( hpos + 1< alloc_len ? segment[ hpos + 1 ]:0, 4 );
                cmpnfo[ cmp ].huffac = RBITS( hpos + 1 < alloc_len ? segment[ hpos + 1 ]:0, 4 );
                if ( ( cmpnfo[ cmp ].huffdc < 0 ) || ( cmpnfo[ cmp ].huffdc >= 4 ) ||
                     ( cmpnfo[ cmp ].huffac < 0 ) || ( cmpnfo[ cmp ].huffac >= 4 ) ) {
                    fprintf( stderr, "huffman table number mismatch" );
                    errorlevel.store(2);
                    return false;
                }
                hpos += 2;
            }
            cs_from = hpos < alloc_len ? segment[ hpos + 0 ]:0;
            cs_to   = hpos + 1 < alloc_len ? segment[ hpos + 1 ]:0;
            cs_sah  = LBITS( hpos + 2 < alloc_len ? segment[ hpos + 2 ]:0, 4 );
            cs_sal  = RBITS( hpos +2  <  alloc_len ? segment[ hpos + 2 ]:0, 4 );
            // check for errors
            if ( ( cs_from > cs_to ) || ( cs_from > 63 ) || ( cs_to > 63 ) ) {
                fprintf( stderr, "spectral selection parameter out of range" );
                errorlevel.store(2);
                return false;
            }
            if ( ( cs_sah >= 12 ) || ( cs_sal >= 12 ) ) {
                fprintf( stderr, "successive approximation parameter out of range" );
                errorlevel.store(2);
                return false;
            }
            return true;

        case 0xC0: // SOF0 segment
            // coding process: baseline DCT

        case 0xC1: // SOF1 segment
            // coding process: extended sequential DCT

        case 0xC2: // SOF2 segment
            // coding process: progressive DCT

            // set JPEG coding type
            if ( type == 0xC2 )
                jpegtype = 2;
            else
                jpegtype = 1;

            // check data precision, only 8 bit is allowed
            lval = hpos < alloc_len ? segment[ hpos ]:0;
            if ( lval != 8 ) {
                fprintf( stderr, "%i bit data precision is not supported", lval );
                errorlevel.store(2);
                return false;
            }

            // image size, height & component count
            imgheight = B_SHORT( hpos +  1  < alloc_len ? segment[ hpos + 1 ]:0, hpos +  2 < alloc_len ?segment[ hpos + 2 ] :0);
            imgwidth  = B_SHORT( hpos + 3 < alloc_len ?segment[ hpos + 3 ]:0, hpos + 4 < alloc_len ?segment[ hpos + 4 ]:0 );
            cmpc      = hpos + 5 < alloc_len ?  segment[ hpos + 5 ]:0;
            if ( cmpc > 4 ) {
                cmpc = 4;
                fprintf( stderr, "image has %i components, max 4 are supported", cmpc );
                errorlevel.store(2);
                return false;
            }
            hpos += 6;
            // components contained in image
            for ( cmp = 0; cmp < cmpc; cmp++ ) {
                cmpnfo[ cmp ].jid = hpos < alloc_len ? segment[ hpos ]:0;
                cmpnfo[ cmp ].sfv = LBITS( hpos + 1 < alloc_len ? segment[ hpos + 1 ]:0, 4 );
                cmpnfo[ cmp ].sfh = RBITS( hpos + 1 < alloc_len ? segment[ hpos + 1 ]:0, 4 );
                if (cmpnfo[ cmp ].sfv > 4
                    || cmpnfo[ cmp ].sfh > 4) {
                    custom_exit(ExitCode::SAMPLING_BEYOND_FOUR_UNSUPPORTED);
                }
#ifndef ALLOW_3_OR_4_SCALING_FACTOR
                if (cmpnfo[ cmp ].sfv > 2
                    || cmpnfo[ cmp ].sfh > 2) {
                    custom_exit(ExitCode::SAMPLING_BEYOND_TWO_UNSUPPORTED);
                }
#endif
                uint32_t quantization_table_value = hpos + 2 < alloc_len ? segment[ hpos + 2 ]:0;
                if (quantization_table_value >= qtables.size()) {
                    errorlevel.store(2);
                    return false;
                }
                cmpnfo[ cmp ].qtable = qtables[quantization_table_value].begin();
                hpos += 3;
            }
    
            return true;

        case 0xC3: // SOF3 segment
            // coding process: lossless sequential
            fprintf( stderr, "sof3 marker found, image is coded lossless" );
            errorlevel.store(2);
            return false;

        case 0xC5: // SOF5 segment
            // coding process: differential sequential DCT
            fprintf( stderr, "sof5 marker found, image is coded diff. sequential" );
            errorlevel.store(2);
            return false;

        case 0xC6: // SOF6 segment
            // coding process: differential progressive DCT
            fprintf( stderr, "sof6 marker found, image is coded diff. progressive" );
            errorlevel.store(2);
            return false;

        case 0xC7: // SOF7 segment
            // coding process: differential lossless
            fprintf( stderr, "sof7 marker found, image is coded diff. lossless" );
            errorlevel.store(2);
            return false;
    
        case 0xC9: // SOF9 segment
            // coding process: arithmetic extended sequential DCT
            fprintf( stderr, "sof9 marker found, image is coded arithm. sequential" );
            errorlevel.store(2);
            return false;
    
        case 0xCA: // SOF10 segment
            // coding process: arithmetic extended sequential DCT
            fprintf( stderr, "sof10 marker found, image is coded arithm. progressive" );
            errorlevel.store(2);
            return false;
    
        case 0xCB: // SOF11 segment
            // coding process: arithmetic extended sequential DCT
            fprintf( stderr, "sof11 marker found, image is coded arithm. lossless" );
            errorlevel.store(2);
            return false;
    
        case 0xCD: // SOF13 segment
            // coding process: arithmetic differntial sequential DCT
            fprintf( stderr, "sof13 marker found, image is coded arithm. diff. sequential" );
            errorlevel.store(2);
            return false;
    
        case 0xCE: // SOF14 segment
            // coding process: arithmetic differential progressive DCT
            fprintf( stderr, "sof14 marker found, image is coded arithm. diff. progressive" );
            errorlevel.store(2);
            return false;

        case 0xCF: // SOF15 segment
            // coding process: arithmetic differntial lossless
            fprintf( stderr, "sof15 marker found, image is coded arithm. diff. lossless" );
            errorlevel.store(2);
            return false;
    
        case 0xE0: // APP0 segment
        case 0xE1: // APP1 segment
        case 0xE2: // APP2 segment
        case 0xE3: // APP3 segment
        case 0xE4: // APP4 segment
        case 0xE5: // APP5 segment
        case 0xE6: // APP6 segment
        case 0xE7: // APP7 segment
        case 0xE8: // APP8 segment
        case 0xE9: // APP9 segment
        case 0xEA: // APP10 segment
        case 0xEB: // APP11 segment
        case 0xEC: // APP12segment
        case 0xED: // APP13 segment
        case 0xEE: // APP14 segment
        case 0xEF: // APP15 segment
        case 0xFE: // COM segment
            // do nothing - return true
            return true;
    
        case 0xD0: // RST0 segment
        case 0xD1: // RST1segment
        case 0xD2: // RST2 segment
        case 0xD3: // RST3 segment
        case 0xD4: // RST4 segment
        case 0xD5: // RST5 segment
        case 0xD6: // RST6 segment
        case 0xD7: // RST7 segment
            // return errormessage - RST is out of place here
            fprintf( stderr, "rst marker found out of place" );
            errorlevel.store(2);
            return false;

        case 0xD8: // SOI segment
            // return errormessage - start-of-image is out of place here
            fprintf( stderr, "soi marker found out of place" );
            errorlevel.store(2);
            return false;

        case 0xD9: // EOI segment
            // return errormessage - end-of-image is out of place here
            fprintf( stderr, "eoi marker found out of place" );
            errorlevel.store(2);
            return false;
    
        default: // unknown marker segment
            // return warning
            fprintf( stderr, "unknown marker found: FF %2X", type );
            errorlevel.store(1);
            return true;
    }
}


/* -----------------------------------------------
    JFIF header rebuilding routine
    ----------------------------------------------- */
bool rebuild_header_jpg( void )
{
    abytewriter* hdrw; // new header writer

    unsigned char  type = 0x00; // type of current marker segment
    uint32_t   len  = 0; // length of current marker segment
    uint32_t   hpos = 0; // position in header


    // start headerwriter
    hdrw = new abytewriter( 4096 );

    // header parser loop
    while ( hpos < hdrs && (uint64_t)hpos + 3 < (uint64_t)hdrs ) {
        type = hpos + 1 < hdrs ?  hdrdata[ hpos + 1 ] : 0;
        len = 2 + B_SHORT( hpos + 2 < hdrs ? hdrdata[ hpos + 2 ]:0, hpos + 3 < hdrs ? hdrdata[ hpos + 3 ] :0);
        // discard any unneeded meta info
        if ( ( type == 0xDA ) || ( type == 0xC4 ) || ( type == 0xDB ) ||
             ( type == 0xC0 ) || ( type == 0xC1 ) || ( type == 0xC2 ) ||
             ( type == 0xDD ) ) {
            uint32_t to_copy = hpos + len < hdrs ? len : hdrs - hpos;
            hdrw->write_n( &(hdrdata[ hpos ]), to_copy);
            if (to_copy <  len) {
                for (uint32_t i = 0;i <to_copy -len;++i) {
                    uint8_t zero = 0;
                    hdrw->write_n(&zero, 1);
                }
            }
        }
        hpos += len;
    }

    // replace current header with the new one
    custom_free( hdrdata );
    hdrdata = hdrw->getptr_aligned();
    hdrs    = hdrw->getpos();
    delete( hdrw );


    return true;
}

/* -----------------------------------------------
    sequential block decoding routine
    ----------------------------------------------- */
int decode_block_seq( abitreader* huffr, huffTree* dctree, huffTree* actree, short* block )
{
    unsigned short n;
    unsigned char  s;
    unsigned char  z;
    int eob = 64;
    int bpos;
    int hc;


    // decode dc
    hc = next_huffcode( huffr, dctree, Billing::EXP0_DC, Billing::EXPN_DC);
    if ( hc < 0 ) return -1; // return error
    else s = ( unsigned char ) hc;
    n = huffr->read( s );
    if (s) {
        write_bit_bill(Billing::RES_DC, false, s - 1);
        write_bit_bill(Billing::SIGN_DC, false, 1);
    }
    block[ 0 ] = DEVLI( s, n );
    bool eof_fixup = false;
    // decode ac
    for ( bpos = 1; bpos < 64; )
    {
        // decode next
        hc = next_huffcode( huffr, actree,
                            is_edge(bpos) ? Billing::BITMAP_EDGE : Billing::BITMAP_7x7,
                            is_edge(bpos) ? Billing::EXPN_EDGE : Billing::EXPN_7x7);
        // analyse code
        if ( hc > 0 ) {
            z = LBITS( hc, 4 );
            s = RBITS( hc, 4 );
            n = huffr->read( s );
            if (s) {
                write_bit_bill(is_edge(bpos) ? Billing::RES_EDGE : Billing::RES_7x7, false, s - 1);
                write_bit_bill(is_edge(bpos) ? Billing::SIGN_EDGE : Billing::SIGN_7x7, false, 1);
            }
            if ( ( z + bpos ) >= 64 ) {
                eof_fixup = true;
                break;
            }
            while ( z > 0 ) { // write zeroes
                block[ bpos++ ] = 0;
                z--;
            }
            block[ bpos++ ] = ( short ) DEVLI( s, n ); // decode cvli
        }
        else if ( hc == 0 ) { // EOB
            eob = bpos;
            // while( bpos < 64 ) // fill remaining block with zeroes
            //    block[ bpos++ ] = 0;
            break;
        }
        else {
            return -1; // return error
        }
    }
    if (eof_fixup) {
        always_assert(huffr->eof && "If 0run is longer than the block must be truncated");
        for(;bpos < eob; ++bpos) {
            block[bpos] = 0;
        }
        if (eob) {
            block[eob - 1] = 1; // set the value to something matching the EOB
        }
    }
    // return position of eob
    return eob;
}



/* -----------------------------------------------
    progressive DC decoding routine
    ----------------------------------------------- */
int decode_dc_prg_fs( abitreader* huffr, huffTree* dctree, short* block )
{
    unsigned short n;
    unsigned char  s;
    int hc;


    // decode dc
    hc = next_huffcode( huffr, dctree, Billing::EXP0_DC, Billing::EXPN_DC);
    if ( hc < 0 ) return -1; // return error
    else s = ( unsigned char ) hc;
    n = huffr->read( s );
    block[ 0 ] = DEVLI( s, n );


    // return 0 if everything is ok
    return 0;
}


/* -----------------------------------------------
    progressive DC encoding routine
    ----------------------------------------------- */
int encode_dc_prg_fs( abitwriter* huffw, huffCodes* dctbl, short* block )
{
    unsigned short n;
    unsigned char  s;
    int tmp;


    // encode DC
    tmp = block[ 0 ];
    s = uint16bit_length(ABS(tmp));
    n = ENVLI( s, tmp );
    huffw->write( dctbl->cval[ s ], dctbl->clen[ s ] );
    huffw->write( n, s );


    // return 0 if everything is ok
    return 0;
}


/* -----------------------------------------------
    progressive AC decoding routine
    ----------------------------------------------- */
int decode_ac_prg_fs( abitreader* huffr, huffTree* actree, short* block, unsigned int* eobrun, int from, int to )
{
    unsigned short n;
    unsigned char  s;
    unsigned char  z;
    int eob = to + 1;
    int bpos;
    int hc;
    int l;
    int r;


    // check eobrun
    if ( (*eobrun) > 0 ) {
        for ( bpos = from; bpos <= to; ++bpos)
            block[ bpos ] = 0;
        (*eobrun)--;
        return from;
    }

    // decode ac
    for ( bpos = from; bpos <= to; )
    {
        // decode next
        hc = next_huffcode( huffr, actree,
                            is_edge(bpos) ? Billing::BITMAP_EDGE : Billing::BITMAP_7x7,
                            is_edge(bpos) ? Billing::EXPN_EDGE : Billing::EXPN_7x7);
        if ( hc < 0 ) return -1;
        l = LBITS( hc, 4 );
        r = RBITS( hc, 4 );
        // analyse code
        if ( ( l == 15 ) || ( r > 0 ) ) { // decode run/level combination
            z = l;
            s = r;
            n = huffr->read( s );
            if ( ( z + bpos ) > to )
                return -1; // run is to long
            while ( z > 0 ) { // write zeroes
                block[ bpos++ ] = 0;
                z--;
            }
            block[ bpos++ ] = ( short ) DEVLI( s, n ); // decode cvli
        }
        else { // decode eobrun
            eob = bpos;
            s = l;
            n = huffr->read( s );
            (*eobrun) = E_DEVLI( s, n );
            // while( bpos <= to ) // fill remaining block with zeroes
            //    block[ bpos++ ] = 0;
            (*eobrun)--; // decrement eobrun ( for this one )
            break;
        }
    }


    // return position of eob
    return eob;
}


/* -----------------------------------------------
    progressive AC encoding routine
    ----------------------------------------------- */
int encode_ac_prg_fs( abitwriter* huffw, huffCodes* actbl, short* block, unsigned int* eobrun, int from, int to )
{
    unsigned short n;
    unsigned char  s;
    unsigned char  z;
    int bpos;
    int hc;
    int tmp;

    // encode AC
    z = 0;
    for ( bpos = from; bpos <= to; bpos++ )
    {
        // if nonzero is encountered
        tmp = block[ bpos ];
        if ( tmp != 0 ) {
            // encode eobrun
            encode_eobrun( huffw, actbl, eobrun );
            // write remaining zeroes
            while ( z >= 16 ) {
                huffw->write( actbl->cval[ 0xF0 ], actbl->clen[ 0xF0 ] );
                z -= 16;
            }
            // vli encode
            s = nonzero_bit_length(ABS(tmp));
            n = ENVLI( s, tmp);
            hc = ( ( z << 4 ) + s );
            // write to huffman writer
            huffw->write( actbl->cval[ hc ], actbl->clen[ hc ] );
            huffw->write( n, s );
            // reset zeroes
            z = 0;
        }
        else { // increment zero counter
            z++;
        }
    }

    // check eob, increment eobrun if needed
    if ( z > 0 ) {
        (*eobrun)++;
        // check eobrun, encode if needed
        if ( (*eobrun) == actbl->max_eobrun )
            encode_eobrun( huffw, actbl, eobrun );
        return 1 + to - z;
    }
    else {
        return 1 + to;
    }
}


/* -----------------------------------------------
    progressive DC SA decoding routine
    ----------------------------------------------- */
int decode_dc_prg_sa( abitreader* huffr, short* block )
{
    // decode next bit of dc coefficient
    block[ 0 ] = huffr->read( 1 );

    // return 0 if everything is ok
    return 0;
}


/* -----------------------------------------------
    progressive DC SA encoding routine
    ----------------------------------------------- */
int encode_dc_prg_sa( abitwriter* huffw, short* block )
{
    // enocode next bit of dc coefficient
    huffw->write( block[ 0 ], 1 );

    // return 0 if everything is ok
    return 0;
}


/* -----------------------------------------------
    progressive AC SA decoding routine
    ----------------------------------------------- */
int decode_ac_prg_sa( abitreader* huffr, huffTree* actree, short* block, unsigned int* eobrun, int from, int to )
{
    unsigned short n;
    unsigned char  s;
    signed char    z;
    signed char    v;
    int bpos = from;
    int eob = to;
    int hc;
    int l;
    int r;


    // decode AC succesive approximation bits
    if ( (*eobrun) == 0 )
    while ( bpos <= to )
    {
        // decode next
        hc = next_huffcode( huffr, actree,
                            is_edge(bpos) ? Billing::BITMAP_EDGE : Billing::BITMAP_7x7,
                            is_edge(bpos) ? Billing::EXPN_EDGE : Billing::EXPN_7x7);

        if ( hc < 0 ) return -1;
        l = LBITS( hc, 4 );
        r = RBITS( hc, 4 );
        // analyse code
        if ( ( l == 15 ) || ( r > 0 ) ) { // decode run/level combination
            z = l;
            s = r;
            if ( s == 0 ) v = 0;
            else if ( s == 1 ) {
                n = huffr->read( 1 );
                v = ( n == 0 ) ? -1 : 1; // fast decode vli
            }
            else return -1; // decoding error
            // write zeroes / write correction bits
            while ( true ) {
                if ( block[ bpos ] == 0 ) { // skip zeroes / write value
                    if ( z > 0 ) z--;
                    else {
                        block[ bpos++ ] = v;
                        break;
                    }
                }
                else { // read correction bit
                    n = huffr->read( 1 );
                    block[ bpos ] = ( block[ bpos ] > 0 ) ? n : -n;
                }
                if ( bpos++ >= to ) return -1; // error check            
            }
        }
        else { // decode eobrun
            eob = bpos;
            s = l;
            n = huffr->read( s );
            (*eobrun) = E_DEVLI( s, n );
            break;
        }
    }

    // read after eob correction bits
    if ( (*eobrun) > 0 ) {
        for ( ; bpos <= to; bpos++ ) {
            if ( block[ bpos ] != 0 ) {
                n = huffr->read( 1 );
                block[ bpos ] = ( block[ bpos ] > 0 ) ? n : -n;
            }
        }
        // decrement eobrun
        (*eobrun)--;
    }

    // return eob
    return eob;
}


/* -----------------------------------------------
    progressive AC SA encoding routine
    ----------------------------------------------- */
int encode_ac_prg_sa( abitwriter* huffw, abytewriter* storw, huffCodes* actbl, short* block, unsigned int* eobrun, int from, int to )
{
    unsigned short n;
    unsigned char  s;
    unsigned char  z;
    int eob = from;
    int bpos;
    int hc;
    int tmp;

    // check if block contains any newly nonzero coefficients and find out position of eob
    for ( bpos = to; bpos >= from; bpos-- )    {
        if ( ( block[ bpos ] == 1 ) || ( block[ bpos ] == -1 ) ) {
            eob = bpos + 1;
            break;
        }
    }

    // encode eobrun if needed
    if ( ( eob > from ) && ( (*eobrun) > 0 ) ) {
        encode_eobrun( huffw, actbl, eobrun );
        encode_crbits( huffw, storw );
    }

    // encode AC
    z = 0;
    for ( bpos = from; bpos < eob; bpos++ )
    {
        tmp = block[ bpos ];
        // if zero is encountered
        if ( tmp == 0 ) {
            z++; // increment zero counter
            if ( z == 16 ) { // write zeroes if needed
                huffw->write( actbl->cval[ 0xF0 ], actbl->clen[ 0xF0 ] );
                encode_crbits( huffw, storw );
                z = 0;
            }
        }
        // if nonzero is encountered
        else if ( ( tmp == 1 ) || ( tmp == -1 ) ) {
            // vli encode
            s = nonzero_bit_length(ABS(tmp));
            n = ENVLI( s, tmp );
            hc = ( ( z << 4 ) + s );
            // write to huffman writer
            huffw->write( actbl->cval[ hc ], actbl->clen[ hc ] );
            huffw->write( n, s );
            // write correction bits
            encode_crbits( huffw, storw );
            // reset zeroes
            z = 0;
        }
        else { // store correction bits
            n = block[ bpos ] & 0x1;
            storw->write( n );
        }
    }

    // fast processing after eob
    for ( ;bpos <= to; bpos++ )
    {
        if ( block[ bpos ] != 0 ) { // store correction bits
            n = block[ bpos ] & 0x1;
            storw->write( n );
        }
    }

    // check eob, increment eobrun if needed
    if ( eob <= to ) {
        (*eobrun)++;
        // check eobrun, encode if needed
        if ( (*eobrun) == actbl->max_eobrun ) {
            encode_eobrun( huffw, actbl, eobrun );
            encode_crbits( huffw, storw );
        }
    }

    // return eob
    return eob;
}


/* -----------------------------------------------
    run of EOB SA decoding routine
    ----------------------------------------------- */
int decode_eobrun_sa( abitreader* huffr, short* block, unsigned int* eobrun, int from, int to )
{
    unsigned short n;
    int bpos;


    // fast eobrun decoding routine for succesive approximation
    for ( bpos = from; bpos <= to; bpos++ ) {
        if ( block[ bpos ] != 0 ) {
            n = huffr->read( 1 );
            block[ bpos ] = ( block[ bpos ] > 0 ) ? n : -n;
        }
    }

    // decrement eobrun
    (*eobrun)--;


    return 0;
}


/* -----------------------------------------------
    run of EOB encoding routine
    ----------------------------------------------- */
int encode_eobrun( abitwriter* huffw, huffCodes* actbl, unsigned int* eobrun )
{
    unsigned short n;
    unsigned int  s;
    int hc;


    if ( (*eobrun) > 0 ) {
        while ( (*eobrun) > actbl->max_eobrun ) {
            huffw->write( actbl->cval[ 0xE0 ], actbl->clen[ 0xE0 ] );
            huffw->write( E_ENVLI( 14, 32767 ), 14 );
            (*eobrun) -= actbl->max_eobrun;
        }
        s = uint16bit_length((*eobrun));
        dev_assert(s && "actbl->max_eobrun needs to be > 0");
        if (s) s--;
        n = E_ENVLI( s, (*eobrun) );
        hc = ( s << 4 );
        huffw->write( actbl->cval[ hc ], actbl->clen[ hc ] );
        huffw->write( n, s );
        (*eobrun) = 0;
    }


    return 0;
}


/* -----------------------------------------------
    correction bits encoding routine
    ----------------------------------------------- */
int encode_crbits( abitwriter* huffw, abytewriter* storw )
{
    unsigned char* data;
    int len;
    int i;


    // peek into data from abytewriter
    len = storw->getpos();
    if ( len == 0 ) return 0;
    data = storw->peekptr_aligned();

    // write bits to huffwriter
    for ( i = 0; i < len; i++ )
        huffw->write( data[ i ], 1 );

    // reset abytewriter, discard data
    storw->reset();


    return 0;
}


/* -----------------------------------------------
    returns next code (from huffman-tree & -data)
    ----------------------------------------------- */
int next_huffcode( abitreader *huffw, huffTree *ctree, Billing min_bill, Billing max_bill)
{
    int node = 0;


    while ( node < 256 ) {
#if defined(ENABLE_BILLING) || !defined(NDEBUG)
        write_bit_bill(min_bill, false, 1);
        if (min_bill != max_bill) {
            min_bill = (Billing)((int)min_bill + 1);
        }
#endif
        node = ( huffw->read( 1 ) == 1 ) ?
                ctree->r[ node ] : ctree->l[ node ];
        if ( node == 0 ) break;
    }

    return ( node - 256 );
}



/* -----------------------------------------------
    calculates next position (non interleaved)
    ----------------------------------------------- */
int next_mcuposn( int* cmp, int* dpos, int* rstw )
{
    // increment position
    (*dpos)++;

    // fix for non interleaved mcu - horizontal
    if ( cmpnfo[(*cmp)].bch != cmpnfo[(*cmp)].nch ) {
        if ( (*dpos) % cmpnfo[(*cmp)].bch == cmpnfo[(*cmp)].nch )
            (*dpos) += ( cmpnfo[(*cmp)].bch - cmpnfo[(*cmp)].nch );
    }

    // fix for non interleaved mcu - vertical
    if ( cmpnfo[(*cmp)].bcv != cmpnfo[(*cmp)].ncv ) {
        if ( (*dpos) / cmpnfo[(*cmp)].bch == cmpnfo[(*cmp)].ncv )
            (*dpos) = cmpnfo[(*cmp)].bc;
    }

    // check position
    if ( (*dpos) >= cmpnfo[(*cmp)].bc ) return 2;
    else if ( rsti > 0 )
        if ( --(*rstw) == 0 ) return 1;


    return 0;
}


/* -----------------------------------------------
    skips the eobrun, calculates next position
    ----------------------------------------------- */
int skip_eobrun( int* cmp, int* dpos, int* rstw, unsigned int* eobrun )
{
    if ( (*eobrun) > 0 ) // error check for eobrun
    {
        // compare rst wait counter if needed
        if ( rsti > 0 ) {
            if ( (int)(*eobrun) > (*rstw) )
                return -1;
            else
                (*rstw) -= (*eobrun);
        }

        // fix for non interleaved mcu - horizontal
        if ( cmpnfo[(*cmp)].bch != cmpnfo[(*cmp)].nch ) {
            (*dpos) += ( ( ( (*dpos) % cmpnfo[(*cmp)].bch ) + (*eobrun) ) /
                        cmpnfo[(*cmp)].nch ) * ( cmpnfo[(*cmp)].bch - cmpnfo[(*cmp)].nch );
        }

        // fix for non interleaved mcu - vertical
        if ( cmpnfo[(*cmp)].bcv != cmpnfo[(*cmp)].ncv ) {
            if ( (*dpos) / cmpnfo[(*cmp)].bch >= cmpnfo[(*cmp)].ncv )
                (*dpos) += ( cmpnfo[(*cmp)].bcv - cmpnfo[(*cmp)].ncv ) *
                        cmpnfo[(*cmp)].bch;
        }

        // skip blocks
        (*dpos) += (*eobrun);

        // reset eobrun
        (*eobrun) = 0;

        // check position
        if ( (*dpos) == cmpnfo[(*cmp)].bc ) return 2;
        else if ( (*dpos) > cmpnfo[(*cmp)].bc ) return -1;
        else if ( rsti > 0 )
            if ( (*rstw) == 0 ) return 1;
    }

    return 0;
}


/* -----------------------------------------------
    creates huffman-codes & -trees from dht-data
    ----------------------------------------------- */
bool build_huffcodes( unsigned char *clen, uint32_t clenlen, unsigned char *cval, uint32_t cvallen, huffCodes *hc, huffTree *ht )
{
    int nextfree;
    int code;
    int node;
    int i, j, k;


    // fill with zeroes
    memset( hc->clen, 0, 256 * sizeof( short ) );
    memset( hc->cval, 0, 256 * sizeof( short ) );
    memset( ht->l, 0, 256 * sizeof( short ) );
    memset( ht->r, 0, 256 * sizeof( short ) );

    // 1st part -> build huffman codes

    // creating huffman-codes
    k = 0;
    code = 0;

    // symbol-value of code is its position in the table
    for( i = 0; i < 16; i++ ) {
        uint32_t clen_index = i & 0xff;
        for( j = 0; j < (int) (clen_index < clenlen ? clen[clen_index] : 0); j++ ) {
            uint32_t cval_index = k&0xff;
            uint8_t cval_val= cval_index < cvallen ? cval[cval_index] : 0;
            hc->clen[ (int) cval_val&0xff] = 1 + i;
            hc->cval[ (int) cval_val&0xff] = code;

            k++;
            code++;
        }
        code = code << 1;
    }

    // find out eobrun max value
    hc->max_eobrun = 0;
    for ( i = 14; i >= 0; i-- ) {
        if ( hc->clen[(i << 4) & 255] > 0 ) {
            hc->max_eobrun = ( 2 << i ) - 1;
            break;
        }
    }

    // 2nd -> part use codes to build the coding tree

    // initial value for next free place
    nextfree = 1;
    const char * huffman_no_space = "Huffman table out of space\n";
    // work through every code creating links between the nodes (represented through ints)
    for ( i = 0; i < 256; i++ )    {
        // (re)set current node
        node = 0;
        // go through each code & store path
        for ( j = hc->clen[i] - 1; j > 0; j-- ) {
            if (node <= 0xff) {
                if ( BITN( hc->cval[i], j ) == 1 ) {
                    if ( ht->r[node] == 0 ) {
                         ht->r[node] = nextfree++;
                    }
                    node = ht->r[node];
                }
                else {
                    if ( ht->l[node] == 0 ) {
                        ht->l[node] = nextfree++;
                    }
                    node = ht->l[node];
                }
            } else {
                while(write(2, huffman_no_space, strlen(huffman_no_space)) == -1 && errno == EINTR) {}
                if (filetype == JPEG) {
                    return false;
                }
            }
        }
        if (node <= 0xff) {
            // last link is number of targetvalue + 256
            if ( hc->clen[i] > 0 ) {
                if ( BITN( hc->cval[i], 0 ) == 1 ) {
                    ht->r[node] = i + 256;
                } else {
                    ht->l[node] = i + 256;
                }
            }
        } else {
            while(write(2, huffman_no_space, strlen(huffman_no_space)) == -1 && errno == EINTR) {}
            if (filetype == JPEG) {
                return false; // we accept any .lep file that was encoded this way
            }
        }
    }
    return true;
}

/* ----------------------- End of JPEG specific functions -------------------------- */

/* ----------------------- Begin of developers functions -------------------------- */





/* -----------------------------------------------
    Writes info to textfile
    ----------------------------------------------- */
bool write_info( void )
{
    FILE* fp;
    const char* fn = "stdout";

    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   hpos = 0; // position in header

    int cmp, bpos;
    int i;


    // open file for output
    fp = stdout;
    if ( fp == NULL ){
        fprintf( stderr, FWR_ERRMSG, fn);
        errorlevel.store(2);
        return false;
    }

    // info about image
    fprintf( fp, "<Infofile for JPEG image:>\n\n\n");
    fprintf( fp, "coding process: %s\n", ( jpegtype == 1 ) ? "sequential" : "progressive" );
    // fprintf( fp, "no of scans: %i\n", scnc );
    fprintf( fp, "imageheight: %i / imagewidth: %i\n", imgheight, imgwidth );
    fprintf( fp, "component count: %i\n", cmpc );
    fprintf( fp, "mcu count: %i/%i/%i (all/v/h)\n\n", mcuc, mcuv, mcuh );

    // info about header
    fprintf( fp, "\nfile header structure:\n" );
    fprintf( fp, " type  length   hpos\n" );
    // header parser loop
    for ( hpos = 0; hpos < hdrs; hpos += len ) {
        type = hpos + 1 < hdrs ? hdrdata[ hpos + 1 ] : 0 ;
        len = 2 + B_SHORT( hpos  + 2 < hdrs ? hdrdata[ hpos + 2 ] : 0, hpos + 3 < hdrs ? hdrdata[ hpos + 3 ] : 0);
        fprintf( fp, " FF%2X  %6i %6i\n", type, len, hpos );
    }
    fprintf( fp, " _END       0 %6i\n", hpos );
    fprintf( fp, "\n" );

    // info about components
    for ( cmp = 0; cmp < cmpc; cmp++ ) {
        fprintf( fp, "\n" );
        fprintf( fp, "component number %i ->\n", cmp );
        fprintf( fp, "sample factors: %i/%i (v/h)\n", cmpnfo[cmp].sfv, cmpnfo[cmp].sfh );
        fprintf( fp, "blocks per mcu: %i\n", cmpnfo[cmp].mbs );
        fprintf( fp, "block count (mcu): %i/%i/%i (all/v/h)\n",
            cmpnfo[cmp].bc, cmpnfo[cmp].bcv, cmpnfo[cmp].bch );
        fprintf( fp, "block count (sng): %i/%i/%i (all/v/h)\n",
            cmpnfo[cmp].nc, cmpnfo[cmp].ncv, cmpnfo[cmp].nch );
        fprintf( fp, "quantiser table ->" );
        for ( i = 0; i < 64; i++ ) {
            bpos = zigzag[ i ];
            if ( ( i % 8 ) == 0 ) fprintf( fp, "\n" );
            fprintf( fp, "%4i, ", QUANT( cmp, bpos ) );
        }
        fprintf( fp, "\n" );
        fprintf( fp, "maximum values ->" );
        for ( i = 0; i < 64; i++ ) {
            bpos = zigzag[ i ];
            if ( ( i % 8 ) == 0 ) fprintf( fp, "\n" );
            fprintf( fp, "%4i, ", MAX_V( cmp, bpos ) );
        }
        fprintf( fp, "\n\n" );
    }


    fclose( fp );


    return true;
}

/* ----------------------- End of developers functions -------------------------- */

/* ----------------------- End of file -------------------------- */
