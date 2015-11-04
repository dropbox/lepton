/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#ifndef _WIN32
#include <sys/time.h>
#include <sys/types.h>
    #include <unistd.h>
#else
    #include <io.h>
#endif
#ifdef __linux
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#endif
#include "emmintrin.h"
#include "bitops.hh"
#include "htables.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "vp8_decoder.hh"
#include "vp8_encoder.hh"
#include "simple_decoder.hh"
#include "simple_encoder.hh"
#include "fork_serve.hh"
#include "../io/ZlibCompression.hh"
#include "../io/MemReadWriter.hh"
#include "../io/BufferedIO.hh"
#include "../io/Zlib0.hh"
bool fast_exit = true;
#define QUANT(cmp,bpos) ( cmpnfo[cmp].qtable[ bpos ] )
#define MAX_V(cmp,bpos) ( ( freqmax[bpos] + QUANT(cmp,bpos) - 1 ) /  QUANT(cmp,bpos) )

#define ENVLI(s,v)        ( ( v > 0 ) ? v : ( v - 1 ) + ( 1 << s ) )
#define DEVLI(s,n)        ( ( n >= ( 1 << (s - 1) ) ) ? n : n + 1 - ( 1 << s ) )
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
#define B_SHORT(v1,v2)    ( ( ((int) v1) << 8 ) + ((int) v2) )
#define CLAMPED(l,h,v)    ( ( v < l ) ? l : ( v > h ) ? h : v )

#define MEM_ERRMSG    "out of memory error"
#define FRD_ERRMSG    "could not read file / file not found: %s"
#define FWR_ERRMSG    "could not write file / file write-protected: %s"

/* -----------------------------------------------
    struct & enum declarations
    ----------------------------------------------- */
enum {JPG_READ_BUFFER_SIZE = 1024 * 256};
enum ACTION {     comp  =  1, forkserve = 2, info = 3 };

enum F_TYPE {   JPEG = 0, UJG = 1, LEPTON=2, UNK = 3        };

struct huffCodes {
    unsigned short cval[ 256 ];
    unsigned short clen[ 256 ];
    unsigned short max_eobrun;
};

struct huffTree {
    unsigned short l[ 256 ];
    unsigned short r[ 256 ];
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

void uint32toLE(uint32_t value, uint8_t *retval) {
    retval[0] = uint8_t(value & 0xff);
    retval[1] = uint8_t((value >> 8) & 0xff);
    retval[2] = uint8_t((value >> 16) & 0xff);
    retval[3] = uint8_t((value >> 24) & 0xff);
}
}
/* -----------------------------------------------
    function declarations: main interface
    ----------------------------------------------- */

void initialize_options( int argc, char** argv );
void process_file(Sirikata::DecoderReader* reader, Sirikata::DecoderWriter *writer);
void execute( bool (*function)() );
void show_help( void );


/* -----------------------------------------------
    function declarations: main functions
    ----------------------------------------------- */

bool check_file( Sirikata::DecoderReader*, Sirikata::DecoderWriter* );
bool read_jpeg( void );
struct MergeJpegProgress;
bool merge_jpeg_streaming( MergeJpegProgress * prog, int num_scans);
bool merge_jpeg( void );
bool decode_jpeg( void );
bool recode_jpeg( void );
bool adapt_icos( void );
bool check_value_range( void );
bool write_ujpg( void );
bool read_ujpg( void );
bool reset_buffers( void );


/* -----------------------------------------------
    function declarations: jpeg-specific
    ----------------------------------------------- */

bool setup_imginfo_jpg( void );
bool parse_jfif_jpg( unsigned char type, unsigned int len, unsigned char* segment );
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

int next_huffcode( abitreader *huffw, huffTree *ctree );
int next_mcupos( int* mcu, int* cmp, int* csc, int* sub, int* dpos, int* rstw );
int next_mcuposn( int* cmp, int* dpos, int* rstw );
int skip_eobrun( int* cmp, int* dpos, int* rstw, unsigned int* eobrun );

void build_huffcodes( unsigned char *clen, unsigned char *cval,
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
struct MergeJpegProgress {
    //unsigned int   len ; // length of current marker segment
    unsigned int   hpos; // current position in header
    unsigned int   ipos; // current position in imagedata
    unsigned int   rpos; // current restart marker position
    unsigned int   cpos; // in scan corrected rst marker position
    unsigned int   scan; // number of current scan
    unsigned char  type; // type of current marker segment
    bool within_scan;
    MergeJpegProgress *parent;
    MergeJpegProgress() {
        //len  = 0; // length of current marker segment
        hpos = 0; // current position in header
        ipos = 0; // current position in imagedata
        rpos = 0; // current restart marker position
        cpos = 0; // in scan corrected rst marker position
        scan = 1; // number of current scan
        type = 0x00; // type of current marker segment
        within_scan = false;
        parent = NULL;
    }
    MergeJpegProgress(MergeJpegProgress*par) {
        memcpy(this, par, sizeof(MergeJpegProgress));
        parent = par;
    }
    ~MergeJpegProgress() {
        if (parent != NULL) {
            MergeJpegProgress *origParent = parent->parent;
            memcpy(parent, this, sizeof(MergeJpegProgress));
            parent->parent = origParent;
        }
    }
private:
        MergeJpegProgress(const MergeJpegProgress&other); // disallow copy construction
        MergeJpegProgress& operator=(const MergeJpegProgress&other); // disallow gets
};

/* -----------------------------------------------
    global variables: data storage
    ----------------------------------------------- */

bool do_streaming = true;
bool g_vectorized_encode_block = false;

unsigned short qtables[4][64];                // quantization tables
huffCodes      hcodes[2][4];                // huffman codes
huffTree       htrees[2][4];                // huffman decoding trees
unsigned char  htset[2][4];                    // 1 if huffman table is set
unsigned char* grbgdata            =     NULL;    // garbage data
unsigned char* hdrdata          =   NULL;   // header data
unsigned char* huffdata         =   NULL;   // huffman coded data
int            hufs             =    0  ;   // size of huffman data
int            hdrs             =    0  ;   // size of header
int            grbs             =    0  ;   // size of garbage

std::vector<unsigned int>  rstp;   // restart markers positions in huffdata
std::vector<unsigned int>  scnp;   // scan start positions in huffdata
int            rstc             =    0  ;   // count of restart markers
int            scnc             =    0  ;   // count of scans
int            rsti             =    0  ;   // restart interval
char           padbit           =    -1 ;   // padbit (for huffman coding)
std::vector<unsigned char> rst_err;   // number of wrong-set RST markers per scan

int            max_file_size    =    0  ;   // support for truncated jpegs 0 means full jpeg

UncompressedComponents colldata; // baseline sorted DCT coefficients



/* -----------------------------------------------
    global variables: info about image
    ----------------------------------------------- */

// seperate info for each color component
componentInfo cmpnfo[ 4 ];

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
int cs_cmp[ 4 ]  = { 0 }; // component numbers  in current scan
int cs_from      =   0  ; // begin - band of current scan ( inclusive )
int cs_to        =   0  ; // end - band of current scan ( inclusive )
int cs_sah       =   0  ; // successive approximation bit pos high
int cs_sal       =   0  ; // successive approximation bit pos low



/* -----------------------------------------------
    global variables: info about files
    ----------------------------------------------- */
int    jpgfilesize;            // size of JPEG file
int    ujgfilesize;            // size of UJG file
int    jpegtype = 0;        // type of JPEG coding: 0->unknown, 1->sequential, 2->progressive
F_TYPE filetype;            // type of current file
F_TYPE ofiletype = LEPTON;            // desired type of output file

std::unique_ptr<BaseEncoder> g_encoder;
std::unique_ptr<BaseDecoder> g_decoder;
bool g_threaded = true;

Sirikata::DecoderReader* str_in  = NULL;    // input stream
bounded_iostream* str_out = NULL;    // output stream
// output stream
Sirikata::DecoderWriter* ujg_out = NULL;
IOUtil::FileWriter * ujg_base = NULL;
IOUtil::FileReader * ujg_base_in = NULL;

char** filelist = NULL;        // list of files to process
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


/* -----------------------------------------------
    global variables: info about program
    ----------------------------------------------- */

const unsigned char ujgversion   = 129;
static const char*  subversion   = "a";
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
}

void timing_operation_first_byte( char operation ) {
    if (g_use_seccomp) {
        return;
    }
    assert(current_operation == operation);
#ifdef _WIN32
    if (current_operation_first_byte == 0) {
        current_operation_first_byte = clock();
    }
#else
    if (current_operation_first_byte.tv_sec == 0 &&
        current_operation_first_byte.tv_usec == 0) {
        gettimeofday(&current_operation_first_byte, NULL);
        fprintf(stderr,"FIRST BYTE ACHIEVED %ld %ld\n",
                (long)current_operation_first_byte.tv_sec,
                (long)current_operation_first_byte.tv_usec );
    }

#endif
}

void timing_operation_complete( char operation ) {
    if (g_use_seccomp) {
        return;
    }
    assert(current_operation == operation);
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
}

unsigned char g_executable_md5[16];

/* -----------------------------------------------
    main-function
    ----------------------------------------------- */

int main( int argc, char** argv )
{
    int n_threads = NUM_THREADS - 1;
#ifndef __linux
    n_threads += 4;
#endif
    Sirikata::memmgr_init(768 * 1024 * 1024,
                64 * 1024 * 1024,
                n_threads,
                256);
    compute_md5(argv[0], g_executable_md5);
    clock_t begin = 0, end = 1;

    int error_cnt = 0;
    int warn_cnt  = 0;

    int acc_jpgsize = 0;
    int acc_ujgsize = 0;

    int speed, bpms;
    float cr;

    errorlevel.store(0);
#ifdef __APPLE__
    // the profiler doesn't enjoy passing along command line arguments
    char default_argv0[] = "/Users/danielrh/src/lepton/src/lepton";
    char default_argv1[] = "/Users/danielrh/src/lepton/images/iphone.lep";
    char *default_argv[]= {default_argv0, default_argv1, NULL};
    if (argc == 1) {
        argc = 2;
        argv = default_argv;
    }
#endif

    // read options from command line
    initialize_options( argc, argv );
    if (action != forkserve) {
        // write program info to screen
        fprintf( msgout,  "%s v%i.%i%s\n",
                 appname, ujgversion / 10, ujgversion % 10, subversion );
    }
    // check if user input is wrong, show help screen if it is
    if ( ( file_cnt == 0 && action != forkserve) ||
        ( ( !developer ) && ( (action != comp && action != forkserve) ) ) ) {
        show_help();
        return -1;
    }


    // (re)set program has to be done first
    reset_buffers();

    // process file(s) - this is the main function routine
    begin = clock();
    assert(file_cnt <= 2);
    if (action == forkserve) {
        g_use_seccomp = true; // do not allow forked mode without security in place
        fork_serve();
    } else {
        process_file(nullptr, nullptr);
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

    // show statistics
    fprintf( msgout,  "\n\n-> %i file(s) processed, %i error(s), %i warning(s)\n",
        file_cnt, error_cnt, warn_cnt );
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


/* ----------------------- Begin of main interface functions -------------------------- */

/* -----------------------------------------------
    reads in commandline arguments
    ----------------------------------------------- */
char g_dash[] = "-";
void initialize_options( int argc, char** argv )
{
    char** tmp_flp;
    int tmp_val;

    // get memory for filelist & preset with NULL
    filelist = (char**)custom_calloc(argc * sizeof(char*));

    // preset temporary fiolelist pointer
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
        else if ( strcmp((*argv), "-decode" ) == 0 ) {
            g_encoder.reset(new VP8ComponentEncoder);
        } else if ( strcmp((*argv), "-recode" ) == 0 ) {
            g_decoder.reset(new VP8ComponentDecoder);
        } else if ( strcmp((*argv), "-p" ) == 0 ) {
            err_tresh = 2;
        }
        else if ( strcmp((*argv), "-nostreaming" ) == 0)  {
            do_streaming = false;
        }
        else if ( strcmp((*argv), "-vec" ) == 0)  {
            g_vectorized_encode_block = true;
        }
        else if ( strcmp((*argv), "-singlethread" ) == 0)  {
            g_threaded = false;
        }
        else if ( strcmp((*argv), "-unjailed" ) == 0)  {
            g_use_seccomp = false;
        }
        else if ( strcmp((*argv), "-multithread" ) == 0 || strcmp((*argv), "-m") == 0)  {
            g_threaded = true;
        }
         else if ( strncmp((*argv), "-timing=", strlen("-timing=") ) == 0 ) {
            timing_log = fopen((*argv) + strlen("-timing="), "a");
        }
        else if ( strncmp((*argv), "-trunctiming=", strlen("-trunctiming=") ) == 0 ) {
            timing_log = fopen((*argv) + strlen("-trunctiming="), "w");
        }
        else if ( strcmp((*argv), "-s" ) == 0)  {
            do_streaming = true; // the default
        }
        else if ( strcmp((*argv), "-d" ) == 0 ) {
            disc_meta = true;
        }
        else if ( strcmp((*argv), "-dev") == 0 ) {
            developer = true;
        }
           else if ( ( strcmp((*argv), "-ujg") == 0 ) ||
                  ( strcmp((*argv), "-ujpg") == 0 )) {
            fprintf(stderr, "FOUND UJG ARG: using that as output\n");
            action = comp;
            ofiletype = UJG;
        } else if ( ( strcmp((*argv), "-comp") == 0) ) {
            action = comp;
        } else if ( ( strcmp((*argv), "-info") == 0) ) {
            action = info;
        } else if ( strcmp((*argv), "-fork") == 0 ) {    
            action = forkserve;
            // sets it up in serving mode
            msgout = stderr;
            // use "-" as placeholder for the socket
            *(tmp_flp++) = g_dash;
        }
        else if ( strcmp((*argv), "-") == 0 ) {    
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
}

/* -----------------------------------------------
    processes one file
    ----------------------------------------------- */
static void gen_nop(){}
void kill_workers(void * workers) {
    Sirikata::Array1d<GenericWorker, NUM_THREADS - 1> *generic_workers = 
        (Sirikata::Array1d<GenericWorker, NUM_THREADS - 1> *) workers;
    if (generic_workers) {
        for (size_t i = 0; i < generic_workers->size(); ++i){
            if (!generic_workers->at(i).has_ever_queued_work()){
                generic_workers->at(i).work = &gen_nop;
                generic_workers->at(i).activate_work();
                generic_workers->at(i).main_wait_for_done();
            }
        }
    }
}
void process_file(Sirikata::DecoderReader* reader, Sirikata::DecoderWriter *writer)
{
    clock_t begin = 0, end = 1;
    const char* actionmsg  = NULL;
    const char* errtypemsg = NULL;
    int speed, bpms;
    float cr;
    
    Sirikata::Array1d<GenericWorker, NUM_THREADS - 1> *generic_workers = nullptr;
    if (g_threaded) {
        generic_workers = new Sirikata::Array1d<GenericWorker,
                                                NUM_THREADS - 1>;
        custom_atexit(kill_workers, generic_workers);
    }
    // main function routine
    errorlevel.store(0);
    jpgfilesize = 0;
    ujgfilesize = 0;

    if (!reader) {
        // compare file name, set pipe if needed
        if ( ( strcmp( filelist[ file_no ], "-" ) == 0 ) && ( action == comp ) ) {
            reader = ujg_base_in = IOUtil::OpenFileOrPipe("STDIN", 2, 0, 1 ),
            pipe_on = true;
        }
        else {
            pipe_on = false;
        }
    }
    // check input file and determine filetype
    check_file(reader, writer);
    begin = clock();
#ifdef __linux
    if (g_use_seccomp) {
        if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT)) {
            custom_exit(36); // SECCOMP not allowed
        }        
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

    if ( verbosity < 2 ) {
        while (write(2, actionmsg , strlen(actionmsg)) < 0 && errno == EINTR) {}
    }



    if ( filetype == JPEG )
    {
        if (ofiletype == LEPTON) {
            if (!g_encoder) {
                g_encoder.reset(new VP8ComponentEncoder);
            }
            if (g_threaded) {//FIXME
                g_encoder->enable_threading(generic_workers->slice<0,NUM_THREADS - 1>());
            } else {
                g_encoder->disable_threading();
            }

        }else if (ofiletype == UJG) {
            g_encoder.reset(new SimpleComponentEncoder);
        }
        switch ( action )
        {
            case comp:
            case forkserve:
                timing_operation_start( 'c' );
                execute( read_jpeg );
                execute( decode_jpeg );
                execute( check_value_range );
                execute( write_ujpg ); // replace with compression function!
                timing_operation_complete( 'c' );
                break;

            case info:
                execute( read_jpeg );
                execute( write_info );
                break;
        }
    }
    else if ( filetype == UJG || filetype == LEPTON)
    {
        if (filetype == LEPTON) {
            if (!g_decoder) {
                g_decoder.reset(new VP8ComponentDecoder);
            }
            if (g_threaded) {//FIXME
                g_decoder->enable_threading(generic_workers->slice<0,NUM_THREADS - 1>());
            } else {
                g_decoder->disable_threading();
            }

        }else if (filetype == UJG) {
            g_decoder.reset(new SimpleComponentDecoder);
        }

        switch ( action )
        {
            case comp:
            case forkserve:
                if (!g_use_seccomp) {
                    overall_start = clock();
                }
                timing_operation_start( 'd' );
                execute( read_ujpg ); // replace with decompression function!
                if (!g_use_seccomp) {
                    read_done = clock();
                }
                execute( recode_jpeg );
                if (!do_streaming) {
                    execute( merge_jpeg );
                }
                timing_operation_complete( 'd' );
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
        if ( ( !pipe_on ) && ( ( errorlevel.load() >= err_tresh ) || ( action != comp && action != forkserve) ) ) {
            // FIXME: can't delete broken output--it's gone already
        }
    }
    if (!g_use_seccomp) {
        end = clock();
    }

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
                    fprintf( msgout,  "DONE" );
                }
            }
            else fprintf( msgout,  "ERROR" );
          if ( errorlevel.load() > 0 )
                fprintf( msgout,  "\n" );
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
        fprintf( stderr, " %s:\n", errtypemsg  );
        fprintf( stderr, " %s\n", errormessage.c_str() );
        if ( verbosity > 1 )
            fprintf( stderr, " (in file \"%s\")\n", filelist[ file_no ] );
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

    custom_exit(errorlevel.load()); // custom exit will delete generic_workers
    // reset buffers
    reset_buffers();
}


/* -----------------------------------------------
    main-function execution routine
    ----------------------------------------------- */

void execute( bool (*function)() )
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
        success = ( *function )();
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
    fprintf( msgout, "Usage: %s [switches] [filename(s)]", appname );
    fprintf( msgout, "\n" );
    fprintf( msgout, "\n" );
    fprintf( msgout, " [-ver]   verify files after processing\n" );
    fprintf( msgout, " [-v?]    set level of verbosity (max: 2) (def: 0)\n" );
    fprintf( msgout, " [-o]     overwrite existing files\n" );
    fprintf( msgout, " [-p]     proceed on warnings\n" );
    fprintf( msgout, " [-d]     discard meta-info\n" );
    if ( developer ) {
    fprintf( msgout, "\n" );
    fprintf( msgout, " [-test]  test algorithms, alert if error\n" );
    fprintf( msgout, " [-split] split jpeg (to header & image data)\n" );
    fprintf( msgout, " [-coll?] write collections (0=std,1=dhf,2=squ,3=unc)\n" );
    fprintf( msgout, " [-info]  write debug info to .nfo file\n" );
    fprintf( msgout, " [-pgm]   convert and write to pgm files\n" );
    }
    fprintf( msgout, "\n" );
    fprintf( msgout, "Examples: \"%s -v1 -o baboon.%s\"\n", appname, "lep");
    fprintf( msgout, "          \"%s -p *.%s\"\n", appname, "jpg" );
}

/* ----------------------- End of main interface functions -------------------------- */

/* ----------------------- Begin of main functions -------------------------- */


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
void nop (Sirikata::DecoderWriter*w, size_t) {
}
void static_cast_to_zlib_and_call (Sirikata::DecoderWriter*w, size_t size) {
    (static_cast<Sirikata::Zlib0Writer*>(w))->setFullFileSize(size);
}
/* -----------------------------------------------
    check file and determine filetype
    ----------------------------------------------- */

bool check_file(Sirikata::DecoderReader *reader ,Sirikata::DecoderWriter *writer)
{
    unsigned char fileid[ 2 ] = { 0, 0 };

    std::string ifilename;
    std::string ofilename;
    if (!reader) {
        assert(!pipe_on); // we should have filled the pipe here
        ifilename = filelist[ file_no++ ];
        reader = ujg_base_in = IOUtil::OpenFileOrPipe(ifilename.c_str(), 0, 0, 0);
    } else {
        pipe_on = true;
    }
    if (!reader) {
        const char *errormessage = "Read file not found\n";
        errorlevel.store(2);
        while (write(2, errormessage, strlen(errormessage)) < 0 && errno == EINTR) {

        }
        custom_exit(1);
    }
    // open input stream, check for errors
    str_in = reader;
    if ( str_in == NULL ) {
        fprintf( stderr, FRD_ERRMSG, filelist[ file_no ] );
        errorlevel.store(2);
        return false;
    }

    // immediately return error if 2 bytes can't be read
    if ( IOUtil::ReadFull(str_in, fileid, 2 ) != 2 ) {
        filetype = UNK;
        fprintf( stderr, "file doesn't contain enough data" );
        errorlevel.store(2);
        return false;
    }

    // check file id, determine filetype
    if ( ( fileid[0] == 0xFF ) && ( fileid[1] == 0xD8 ) ) {
        str_in = reader = new Sirikata::BufferedReader<JPG_READ_BUFFER_SIZE>(reader);
        // file is JPEG
        filetype = JPEG;
        // create filenames
        if ( !pipe_on ) {
            if (file_no < file_cnt && ofilename != ifilename) {
                ofilename = filelist[file_no];
            } else {
                ofilename = postfix_uniq(ifilename, (ofiletype == UJG ? ".ujg" : ".lep"));
            }
        }
        // open output stream, check for errors
        ujg_base = nullptr;
        if (!writer) {
            writer = ujg_base = IOUtil::OpenWriteFileOrPipe( ofilename.c_str(), ( !pipe_on ) ? 0 : 2, 0, 1 );
            if(!writer) {
                const char * errormessage = "Output file unable to be opened for writing\n";
                while(write(2, errormessage, strlen(errormessage)) == -1 && errno == EINTR) {

                }
                custom_exit(1);
            }
        }
        ujg_out = writer;
        if ( !ujg_out ) {
            fprintf( stderr, FWR_ERRMSG, ifilename.c_str() );
            errorlevel.store(2);
            return false;
        }
    }
    else if ( ( ( fileid[0] == ujg_header[0] ) && ( fileid[1] == ujg_header[1] ) )
              || ( ( fileid[0] == lepton_header[0] ) && ( fileid[1] == lepton_header[1] ) )
              || ( ( fileid[0] == zlepton_header[0] ) && ( fileid[1] == zlepton_header[1] ) ) ){
        bool compressed_output = (fileid[0] == zlepton_header[0]) && (fileid[1] == zlepton_header[1]);
        // file is UJG
        filetype = (( fileid[0] == ujg_header[0] ) && ( fileid[1] == ujg_header[1] ) ) ? UJG : LEPTON;
        // create filenames
        if ( !pipe_on ) {
            if (file_no < file_cnt && ofilename != ifilename) {
                ofilename = filelist[file_no];
            } else {
                if (compressed_output) {
                    ofilename = postfix_uniq(ifilename, ".jpg.z");
                } else {
                    ofilename = postfix_uniq(ifilename, ".jpg");
                }
            }
        }
        // open output stream, check for errors
        if (!writer) {
            writer = IOUtil::OpenWriteFileOrPipe( ofilename.c_str(), ( !pipe_on ) ? 0 : 2, 0, 1 );
        }
        std::function<void(Sirikata::DecoderWriter*, size_t file_size)> known_size_callback = &nop;
        if (compressed_output) {
            Sirikata::Zlib0Writer *zwriter = new Sirikata::Zlib0Writer(writer, 0);
            known_size_callback = &static_cast_to_zlib_and_call;
            writer = zwriter;
        }
        str_out = new bounded_iostream( writer,
                                        known_size_callback,
                                        Sirikata::JpegAllocator<uint8_t>());
        if ( str_out->chkerr() ) {
            fprintf( stderr, FWR_ERRMSG, ifilename.c_str() );
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


/* -----------------------------------------------
    Read in header & image data
    ----------------------------------------------- */
unsigned char EOI[ 2 ] = { 0xFF, 0xD9 }; // EOI segment
bool read_jpeg( void )
{
    std::vector<unsigned char> segment(1024); // storage for current segment
    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   crst = 0; // current rst marker counter
    unsigned int   cpos = 0; // rst marker counter
    unsigned char  tmp;

    abytewriter* huffw;
    abytewriter* hdrw;
    abytewriter* grbgw;
    unsigned int jpg_ident_offset = 2;
    ibytestream str_jpg_in(str_in, jpg_ident_offset, Sirikata::JpegAllocator<uint8_t>());
    ibytestream * jpg_in = &str_jpg_in;
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
                    }
                    else if ( tmp == 0xD0 + ( cpos & 7 ) ) { // restart marker
                        // increment rst counters
                        cpos++;
                        crst++;
                    }
                    else { // in all other cases leave it to the header parser routines
                        // store number of falsely set rst markers
                        if((int)rst_err.size() < scnc) {
                            rst_err.insert(rst_err.end(), scnc - rst_err.size(), 0);
                        }
                        rst_err.push_back(crst);
                        // end of current scan
                        scnc++;
                        assert(rst_err.size() == (size_t)scnc && "All reset errors must be accounted for");
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
                    if ( jpg_in->read( segment.data(), 1) != 2 ) break;
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
        hdrw->write_n( segment.data(), len );
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
    if ( !setup_imginfo_jpg() ) {
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
#if 1
    __m128i buf = _mm_load_si128((__m128i const*)local_huff_data);
    __m128i ff = _mm_set1_epi8(-1);
    __m128i res = _mm_cmpeq_epi8(buf, ff);
    uint32_t movmask = _mm_movemask_epi8(res);
    bool retval = movmask != 0x0;
    assert (retval == (memchr(local_huff_data, 0xff, 16) != NULL));
    return retval;
#endif
    return memchr(local_huff_data, 0xff, 16) != NULL;
}
MergeJpegStreamingStatus merge_jpeg_streaming(MergeJpegProgress *stored_progress, const unsigned char * local_huff_data, unsigned int max_byte_coded,
                                              bool flush) {
    if (!do_streaming) return STREAMING_DISABLED;
    //fprintf(stderr, "Running straming data until byte %d\n", max_byte_coded);
    MergeJpegProgress progress(stored_progress);
    unsigned char SOI[ 2 ] = { 0xFF, 0xD8 }; // SOI segment
    //unsigned char EOI[ 2 ] = { 0xFF, 0xD9 }; // EOI segment

    unsigned char  type = 0x00; // type of current marker segment

    if (progress.ipos == 0 && progress.hpos == 0 && progress.scan == 1 && progress.within_scan == false) {
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
                if ( ( int ) progress.hpos >= hdrs ) break;
                type = hdrdata[ progress.hpos + 1 ];
                int len = 2 + B_SHORT( hdrdata[ progress.hpos + 2 ], hdrdata[progress.hpos + 3 ] );
                progress.hpos += len;
            }
            // write header data to file
            str_out->write( hdrdata + tmp, ( progress.hpos - tmp ) );
            if ((!g_use_seccomp) && post_byte == 0) {
                post_byte = clock();
            }

            // get out if last marker segment type was not SOS
            if ( type != 0xDA ) break;

            // (re)set corrected rst pos
            progress.cpos = 0;
            progress.ipos = scnp[ progress.scan - 1 ];
        }
        if ((int)progress.scan > scnc + 1) { // don't want to go beyond our known number of scans (FIXME: danielrh@ is this > or >= )
            break;
        }
        if (progress.ipos < max_byte_coded) {
            timing_operation_first_byte( 'd' );
        }
        // write & expand huffman coded image data
        unsigned int progress_ipos = progress.ipos;
        unsigned int progress_scan = scnp[ progress.scan ];
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
                if (!rstp.empty()) {
                    const unsigned char rst = 0xD0 + ( progress.cpos & 7);
                    str_out->write_byte(mrk);
                    str_out->write_byte(rst);
                    progress.rpos++; progress.cpos++;
                    rstp_progress_rpos = rstp[ progress.rpos ];
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
                        if (!rstp.empty()) {
                            const unsigned char rst = 0xD0 + ( progress.cpos & 7);
                            str_out->write_byte(mrk);
                            str_out->write_byte(rst);
                            progress.rpos++; progress.cpos++;
                            rstp_progress_rpos = rstp[ progress.rpos ];
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
                if (!rstp.empty()) {
                    const unsigned char rst = 0xD0 + ( progress.cpos & 7);
                    str_out->write_byte(mrk);
                    str_out->write_byte(rst);
                    progress.rpos++; progress.cpos++;
                    rstp_progress_rpos = rstp[ progress.rpos ];
                }
            }
        }
        progress.ipos = progress_ipos;
        if (scnp[progress.scan] == 0 && !flush) {
            return STREAMING_NEED_DATA;
        }
        if (progress.ipos >= max_byte_coded && progress.ipos != scnp[progress.scan] && !flush) {
            return STREAMING_NEED_DATA;
        }
        // insert false rst markers at end if needed
        if (progress.scan - 1 < rst_err.size()) {
            while ( rst_err[ progress.scan - 1 ] > 0 ) {
                const unsigned char rst = 0xD0 + ( progress.cpos & 7 );
                str_out->write_byte(mrk);
                str_out->write_byte(rst);
                progress.cpos++;    rst_err[ progress.scan - 1 ]--;
            }
        }
        progress.within_scan = false;
        // proceed with next scan
        progress.scan++;
    }

    // write EOI (now EOI is stored in garbage of at least 2 bytes)
    // str_out->write( EOI, 1, 2 );
    str_out->set_bound(max_file_size);
    // write garbage if needed
    if ( grbs > 0 )
        str_out->write( grbgdata, grbs );
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

    return STREAMING_SUCCESS;

}


/* -----------------------------------------------
    Merges header & image data to jpeg
    ----------------------------------------------- */

bool merge_jpeg( void )
{
    unsigned char SOI[ 2 ] = { 0xFF, 0xD8 }; // SOI segment
    unsigned char EOI[ 2 ] = { 0xFF, 0xD9 }; // EOI segment
    unsigned char mrk = 0xFF; // marker start
    unsigned char stv = 0x00; // 0xFF stuff value
    unsigned char rst = 0xD0; // restart marker

    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   hpos = 0; // current position in header
    unsigned int   ipos = 0; // current position in imagedata
    unsigned int   rpos = 0; // current restart marker position
    unsigned int   cpos = 0; // in scan corrected rst marker position
    unsigned int   scan = 1; // number of current scan
    unsigned int   tmp  = 0;
    str_out->set_bound(max_file_size);
    // write SOI
    str_out->write( SOI, 2 );

    // JPEG writing loop
    while ( true )
    {
        // store current header position
        tmp = hpos;

        // seek till start-of-scan
        for ( type = 0x00; type != 0xDA; ) {
            if ( ( int ) hpos >= hdrs ) break;
            type = hdrdata[ hpos + 1 ];
            len = 2 + B_SHORT( hdrdata[ hpos + 2 ], hdrdata[ hpos + 3 ] );
            hpos += len;
        }

        // write header data to file
        str_out->write( hdrdata + tmp, ( hpos - tmp ) );

        // get out if last marker segment type was not SOS
        if ( type != 0xDA ) break;


        // (re)set corrected rst pos
        cpos = 0;
        timing_operation_first_byte( 'd' );

        // write & expand huffman coded image data
        for ( ipos = scnp[ scan - 1 ]; ipos < scnp[ scan ]; ipos++ ) {
            // write current byte
            str_out->write_byte( huffdata[ipos]);
            // check current byte, stuff if needed
            if ( huffdata[ ipos ] == 0xFF )
                str_out->write_byte(stv);
            // insert restart markers if needed
            if ( !rstp.empty() ) {
                if ( ipos == rstp[ rpos ] ) {
                    rst = 0xD0 + ( cpos & 7 );
                    str_out->write_byte(mrk);
                    str_out->write_byte(rst);
                    rpos++; cpos++;
                }
            }
        }
        // insert false rst markers at end if needed
        if ( !rst_err.empty() ) {
            while ( rst_err[ scan - 1 ] > 0 ) {
                rst = 0xD0 + ( cpos & 7 );
                str_out->write_byte(mrk);
                str_out->write_byte(rst);
                cpos++;    rst_err[ scan - 1 ]--;
            }
        }

        // proceed with next scan
        scan++;
    }

    // write EOI
    str_out->write( EOI, 2 );

    // write garbage if needed
    if ( grbs > 0 )
        str_out->write( grbgdata, grbs );

    // errormessage if write error
    if ( str_out->chkerr() ) {
        fprintf( stderr, "write error, possibly drive is full" );
        errorlevel.store(2);
        return false;
    }

    // get filesize
    jpgfilesize = str_out->getsize();


    return true;
}


/* -----------------------------------------------
    JPEG decoding routine
    ----------------------------------------------- */

bool decode_jpeg( void )
{
    abitreader* huffr; // bitwise reader for image data

    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   hpos = 0; // current position in header

    int lastdc[ 4 ]; // last dc for each component
    short block[ 64 ]; // store block for coeffs
    int peobrun; // previous eobrun
    unsigned int eobrun; // run of eobs
    int rstw; // restart wait counter

    int cmp, bpos, dpos;
    int mcu, sub, csc;
    int eob, sta;

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
            if ( ( int ) hpos >= hdrs ) break;
            type = hdrdata[ hpos + 1 ];
            len = 2 + B_SHORT( hdrdata[ hpos + 2 ], hdrdata[ hpos + 3 ] );
            if ( ( type == 0xC4 ) || ( type == 0xDA ) || ( type == 0xDD ) ) {
                if ( !parse_jfif_jpg( type, len, &( hdrdata[ hpos ] ) ) ) {
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
            if ( (( cs_sal == 0 ) && ( htset[ 0 ][ cmpnfo[cmp].huffdc ] == 0 )) ||
                 (( cs_sah >  0 ) && ( htset[ 1 ][ cmpnfo[cmp].huffac ] == 0 )) ) {
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

            // decoding for interleaved data
            if ( cs_cmpc > 1 )
            {
                if ( jpegtype == 1 ) {
                    // ---> sequential interleaved decoding <---
                    while ( sta == 0 ) {
                        if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                        // decode block
                        eob = decode_block_seq( huffr,
                            &(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
                            &(htrees[ 1 ][ cmpnfo[cmp].huffdc ]),
                            block );
                        if ( eob > 1 && !block[ eob - 1 ] ) {
                            fprintf( stderr, "cannot encode image with eob after last 0" );
                            errorlevel.store(1);
                        }

                        // fix dc
                        block[ 0 ] += lastdc[ cmp ];
                        lastdc[ cmp ] = block[ 0 ];

                        // copy to colldata
                        for ( bpos = 0; bpos < eob; bpos++ )
                            colldata.set( (BlockType)cmp , bpos , dpos ) = block[ bpos ];

                        // check for errors, proceed if no error encountered
                        if ( eob < 0 ) sta = -1;
                        else sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
                    }
                }
                else if ( cs_sah == 0 ) {
                    // ---> progressive interleaved DC decoding <---
                    // ---> succesive approximation first stage <---
                    while ( sta == 0 ) {
                        if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                        sta = decode_dc_prg_fs( huffr,
                            &(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
                            block );

                        // fix dc for diff coding
                        colldata.set((BlockType)cmp,0,dpos) = block[0] + lastdc[ cmp ];
                        lastdc[ cmp ] = colldata.set((BlockType)cmp,0,dpos);

                        // bitshift for succesive approximation
                        colldata.set((BlockType)cmp,0,dpos) <<= cs_sal;

                        // next mcupos if no error happened
                        if ( sta != -1 )
                            sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
                    }
                }
                else {
                    // ---> progressive interleaved DC decoding <---
                    // ---> succesive approximation later stage <---
                    while ( sta == 0 ) {
                        if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                        // decode next bit
                        sta = decode_dc_prg_sa( huffr,
                            block );

                        // shift in next bit
                        colldata.set((BlockType)cmp,0,dpos) += block[0] << cs_sal;

                        // next mcupos if no error happened
                        if ( sta != -1 )
                            sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
                    }
                }
            }
            else // decoding for non interleaved data
            {
                if ( jpegtype == 1 ) {
                    // ---> sequential non interleaved decoding <---
                    while ( sta == 0 ) {
                        if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                        // decode block
                        eob = decode_block_seq( huffr,
                            &(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
                            &(htrees[ 1 ][ cmpnfo[cmp].huffdc ]),
                            block );
                        if ( eob > 1 && !block[ eob - 1 ] ) {
                            fprintf( stderr, "cannot encode image with eob after last 0" );
                            errorlevel.store(1);
                        }
                        // fix dc
                        block[ 0 ] += lastdc[ cmp ];
                        lastdc[ cmp ] = block[ 0 ];

                        // copy to colldata
                        for ( bpos = 0; bpos < eob; bpos++ )
                            colldata.set((BlockType)cmp , bpos , dpos ) = block[ bpos ];

                        // check for errors, proceed if no error encountered
                        if ( eob < 0 ) sta = -1;
                        else sta = next_mcuposn( &cmp, &dpos, &rstw );
                    }
                }
                else if ( cs_to == 0 ) {
                    if ( cs_sah == 0 ) {
                        // ---> progressive non interleaved DC decoding <---
                        // ---> succesive approximation first stage <---
                        while ( sta == 0 ) {
                            if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                            sta = decode_dc_prg_fs( huffr,
                                &(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
                                block );

                            // fix dc for diff coding
                            colldata.set((BlockType)cmp,0,dpos) = block[0] + lastdc[ cmp ];
                            lastdc[ cmp ] = colldata.set((BlockType)cmp,0,dpos);

                            // bitshift for succesive approximation
                            colldata.set((BlockType)cmp,0,dpos) <<= cs_sal;

                            // check for errors, increment dpos otherwise
                            if ( sta != -1 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                        }
                    }
                    else {
                        // ---> progressive non interleaved DC decoding <---
                        // ---> succesive approximation later stage <---
                        while( sta == 0 ) {
                            if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                            // decode next bit
                            sta = decode_dc_prg_sa( huffr,
                                block );

                            // shift in next bit
                            colldata.set((BlockType)cmp,0,dpos) += block[0] << cs_sal;

                            // check for errors, increment dpos otherwise
                            if ( sta != -1 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
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
                                block, &eobrun, cs_from, cs_to );

                            // check for non optimal coding
                            if ( ( eob == cs_from ) && ( eobrun > 0 ) &&
                                ( peobrun > 0 ) && ( peobrun <
                                hcodes[ 1 ][ cmpnfo[cmp].huffac ].max_eobrun - 1 ) ) {
                                fprintf( stderr,
                                    "reconstruction of non optimal coding not supported" );
                                errorlevel.store(1);
                            }

                            // copy to colldata
                            for ( bpos = cs_from; bpos < eob; bpos++ )
                                colldata.set((BlockType)cmp , bpos , dpos ) = block[ bpos ] << cs_sal;

                            // check for errors
                            if ( eob < 0 ) sta = -1;
                            else sta = skip_eobrun( &cmp, &dpos, &rstw, &eobrun );

                            // proceed only if no error encountered
                            if ( sta == 0 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                        }
                    }
                    else {
                        // ---> progressive non interleaved AC decoding <---
                        // ---> succesive approximation later stage <---
                        while ( sta == 0 ) {
                            // copy from colldata
                            for ( bpos = cs_from; bpos <= cs_to; bpos++ )
                                block[ bpos ] = colldata.set((BlockType)cmp , bpos , dpos );

                            if ( eobrun == 0 ) {
                                if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                                // decode block (long routine)
                                eob = decode_ac_prg_sa( huffr,
                                    &(htrees[ 1 ][ cmpnfo[cmp].huffac ]),
                                    block, &eobrun, cs_from, cs_to );

                                // check for non optimal coding
                                if ( ( eob == cs_from ) && ( eobrun > 0 ) &&
                                    ( peobrun > 0 ) && ( peobrun <
                                    hcodes[ 1 ][ cmpnfo[cmp].huffac ].max_eobrun - 1 ) ) {
                                    fprintf( stderr,
                                        "reconstruction of non optimal coding not supported" );
                                    errorlevel.store(1);
                                }

                                // store eobrun
                                peobrun = eobrun;
                            }
                            else {
                                if(!huffr->eof) max_dpos[cmp] = std::max(dpos, max_dpos[cmp]); // record the max block serialized
                                // decode block (short routine)
                                eob = decode_eobrun_sa( huffr,
                                    block, &eobrun, cs_from, cs_to );
                                if ( eob > 1 && !block[ eob - 1 ] ) {
                                    fprintf( stderr, "cannot encode image with eob after last 0" );
                                    errorlevel.store(1);
                                }
                            }

                            // copy back to colldata
                            for ( bpos = cs_from; bpos <= cs_to; bpos++ )
                                colldata.set((BlockType)cmp , bpos , dpos ) += block[ bpos ] << cs_sal;

                            // proceed only if no error encountered
                            if ( eob < 0 ) sta = -1;
                            else sta = next_mcuposn( &cmp, &dpos, &rstw );
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
    // check for unneeded data
    if ( !huffr->eof ) {
        fprintf( stderr, "unneeded data found after coded image data" );
        errorlevel.store(1);
    }

    // clean up
    delete( huffr );


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
    short block[ 64 ]; // store block for coeffs
    unsigned int eobrun; // run of eobs
    int rstw; // restart wait counter

    int cmp, bpos, dpos;
    int mcu, sub, csc;
    int eob, sta;
    int tmp;

    // open huffman coded image data in abitwriter
    huffw = new abitwriter( 4096 * 1024 + 1024 );
    huffw->fillbit = padbit;

    // init storage writer
    storw = new abytewriter( 4096 * 1024 + 1024);

    // preset count of scans and restarts
    scnc = 0;
    rstc = 0;
    MergeJpegProgress streaming_progress;

    // JPEG decompression loop
    while ( true )
    {
        // seek till start-of-scan, parse only DHT, DRI and SOS
        for ( type = 0x00; type != 0xDA; ) {
            if ( ( int ) hpos >= hdrs ) break;
            type = hdrdata[ hpos + 1 ];
            len = 2 + B_SHORT( hdrdata[ hpos + 2 ], hdrdata[ hpos + 3 ] );
            if ( ( type == 0xC4 ) || ( type == 0xDA ) || ( type == 0xDD ) ) {
                if ( !parse_jfif_jpg( type, len, &( hdrdata[ hpos ] ) ) ) {
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
        scnp[ scnc ] = huffw->getpos();
        scnp[ scnc + 1 ] = 0; // danielrh@ avoid uninitialized memory when doing progressive writeout
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

            // encoding for interleaved data
            if ( cs_cmpc > 1 )
            {
                if ( jpegtype == 1 ) {
                    // ---> sequential interleaved encoding <---
                    while ( sta == 0 ) {
                        // copy from colldata
                        int16_t dc = block [ 0 ] = colldata.at((BlockType)cmp,
                                                               0 , dpos );
                        //fprintf(stderr, "Reading from cmp(%d) dpos %d\n", cmp, dpos);
                        for ( bpos = 1; bpos < 64; bpos++ )
                            block[ bpos ] = colldata.at_nosync((BlockType)cmp , bpos , dpos );

                        // diff coding for dc
                        block[ 0 ] -= lastdc[ cmp ];
                        lastdc[ cmp ] = dc;

                        // encode block
                        eob = encode_block_seq( huffw,
                            &(hcodes[ 0 ][ cmpnfo[cmp].huffac ]),
                            &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                            block );

                        // check for errors, proceed if no error encountered
                        if ( eob < 0 ) sta = -1;
                        else sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
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
                            block );

                        // next mcupos if no error happened
                        if ( sta != -1 )
                            sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
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
                        sta = encode_dc_prg_sa( huffw, block );

                        // next mcupos if no error happened
                        if ( sta != -1 )
                            sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                        }

                    }
                }
            }
            else // encoding for non interleaved data
            {
                if ( jpegtype == 1 ) {
                    // ---> sequential non interleaved encoding <---
                    while ( sta == 0 ) {
                        // copy from colldata
                        int16_t dc = block[ 0 ] = colldata.at((BlockType)cmp, 0, dpos);
                        for ( bpos = 0; bpos < 64; bpos++ )
                            block[ bpos ] = colldata.at_nosync((BlockType)cmp , bpos , dpos );

                        // diff coding for dc
                        block[ 0 ] -= lastdc[ cmp ];
                        lastdc[ cmp ] = dc;

                        // encode block
                        eob = encode_block_seq( huffw,
                            &(hcodes[ 0 ][ cmpnfo[cmp].huffac ]),
                            &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                            block );

                        // check for errors, proceed if no error encountered
                        if ( eob < 0 ) sta = -1;
                        else sta = next_mcuposn( &cmp, &dpos, &rstw );
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
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
                                block );

                            // check for errors, increment dpos otherwise
                            if ( sta != -1 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if (sta == 0 && huffw->no_remainder()) {
                                merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
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
                            sta = encode_dc_prg_sa( huffw, block );

                            // next mcupos if no error happened
                            if ( sta != -1 )
                                sta = next_mcuposn( &cmp, &dpos, &rstw );
                        }
                        if (sta == 0 && huffw->no_remainder()) {
                            merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
                        }
                    }
                }
                else {
                    if ( cs_sah == 0 ) {
                        // ---> progressive non interleaved AC encoding <---
                        // ---> succesive approximation first stage <---
                        while ( sta == 0 ) {
                            // copy from colldata
                            for ( bpos = cs_from; bpos <= cs_to; bpos++ )
                                block[ bpos ] =
                                    FDIV2( colldata.at((BlockType)cmp , bpos , dpos ), cs_sal );

                            // encode block
                            eob = encode_ac_prg_fs( huffw,
                                &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                                block, &eobrun, cs_from, cs_to );

                            // check for errors, proceed if no error encountered
                            if ( eob < 0 ) sta = -1;
                            else sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if (sta == 0 && huffw->no_remainder()) {
                                merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
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
                            // copy from colldata
                            for ( bpos = cs_from; bpos <= cs_to; bpos++ )
                                block[ bpos ] =
                                    FDIV2( colldata.at((BlockType)cmp , bpos , dpos ), cs_sal );

                            // encode block
                            eob = encode_ac_prg_sa( huffw, storw,
                                &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                                block, &eobrun, cs_from, cs_to );

                            // check for errors, proceed if no error encountered
                            if ( eob < 0 ) sta = -1;
                            else sta = next_mcuposn( &cmp, &dpos, &rstw );
                            if (sta == 0 && huffw->no_remainder()) {
                                merge_jpeg_streaming(&streaming_progress, huffw->peekptr(), huffw->getpos(), false);
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
                    rstp[ rstc++ ] = huffw->getpos() - 1;
            }
            huffw->flush_no_pad();
            assert(huffw->no_remainder() && "this should have been padded");
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
    assert(huffw->no_remainder() && "this should have been padded");
    merge_jpeg_streaming(&streaming_progress, huffdata, hufs, true);
    if (!fast_exit) {
        delete huffw;

        // remove storage writer
        delete storw;
    }
    // store last scan & restart positions
    scnp[ scnc ] = hufs;
    if ( !rstp.empty() )
        rstp[ rstc ] = hufs;


    return true;
}




/* -----------------------------------------------
    checks range of values, error if out of bounds
    ----------------------------------------------- */

bool check_value_range( void )
{
    int absmax;
    int cmp, bpos, dpos;
    int bad_cmp = 0, bad_bpos = 0, bad_dpos = 0;
    bool bad_colldata = false;
    // out of range should never happen with unmodified JPEGs
    for ( cmp = 0; cmp < cmpc; cmp++ ) {
    for ( bpos = 0; bpos < 64; bpos++ ) {
        absmax = MAX_V( cmp, bpos );
        for ( dpos = 0; dpos < cmpnfo[cmp].bc; dpos++ ) {
            if ( ( colldata.at_nosync((BlockType)cmp,bpos,dpos) > absmax ) ||
                 ( colldata.at_nosync((BlockType)cmp,bpos,dpos) < -absmax ) ) {
                if (!early_eof_encountered) {
                    fprintf( stderr, "value out of range error: cmp%i, frq%i, val %i, max %i",
                         cmp, bpos, colldata.at_nosync((BlockType)cmp,bpos,dpos), absmax );
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


/* -----------------------------------------------
    write uncompressed JPEG file
    ----------------------------------------------- */

bool write_ujpg( )
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

    Sirikata::MemReadWriter mrw((Sirikata::JpegAllocator<uint8_t>()));

    // write header to file
    // marker: "HDR" + [size of header]
    unsigned char hdr_mrk[] = {'H', 'D', 'R'};
    err = mrw.Write( hdr_mrk, sizeof(hdr_mrk) ).second;
    uint32toLE(hdrs, ujpg_mrk);
    err = mrw.Write( ujpg_mrk, 4).second;
    // data: data from header
    mrw.Write( hdrdata, hdrs );
    // beginning here: recovery information (needed for exact JPEG recovery)

    // write huffman coded data padbit
    // marker: "PAD"
    unsigned char pad_mrk[] = {'P', 'A', 'D'};
    err = mrw.Write( pad_mrk, sizeof(pad_mrk) ).second;
    // data: padbit
    err = mrw.Write( (unsigned char*) &padbit, 1 ).second;

    // write number of false set RST markers per scan (if available) to file
    if (!rst_err.empty()) {
        // marker: "FRS" + [number of scans]
        unsigned char frs_mrk[] = {'F', 'R', 'S'};
        err = mrw.Write( frs_mrk, 3 ).second;
        uint32toLE(scnc, ujpg_mrk);
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
    if ( grbs > 0 ) {
        // marker: "GRB" + [size of garbage]
        unsigned char grb_mrk[] = {'G', 'R', 'B'};
        err = mrw.Write( grb_mrk, sizeof(grb_mrk) ).second;
        uint32toLE(grbs, ujpg_mrk);
        err = mrw.Write( ujpg_mrk, 4 ).second;
        // data: garbage data
        err = mrw.Write( grbgdata, grbs ).second;
    }
    std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> > compressed_header;
    compressed_header =
            Sirikata::ZlibDecoderCompressionWriter::Compress(mrw.buffer().data(),
                                                             mrw.buffer().size(),
                                                             Sirikata::JpegAllocator<uint8_t>());
    unsigned char siz_mrk[] = {'Z'};
    err = ujg_out->Write( siz_mrk, sizeof(siz_mrk) ).second;
    err = ujg_out->Write( g_executable_md5, sizeof(g_executable_md5) ).second;
    uint32toLE(jpgfilesize, ujpg_mrk);
    err = ujg_out->Write( ujpg_mrk, 4).second;
    uint32toLE((uint32_t)compressed_header.size(), ujpg_mrk);
    err = ujg_out->Write( ujpg_mrk, 4).second;
    auto err2 = ujg_out->Write(compressed_header.data(),
                               compressed_header.size());
    if (err != Sirikata::JpegError::nil() || err2.second != Sirikata::JpegError::nil()) {
        fprintf( stderr, "write error, possibly drive is full" );
        errorlevel.store(2);
        return false;
    }
    unsigned char cmp_mrk[] = {'C', 'M', 'P'};
    err = ujg_out->Write( cmp_mrk, sizeof(cmp_mrk) ).second;
    while (g_encoder->encode_chunk(&colldata, ujg_out) == CODING_PARTIAL) {
    }

    // errormessage if write error
    if ( err != Sirikata::JpegError::nil() ) {
        fprintf( stderr, "write error, possibly drive is full" );
        errorlevel.store(2);
        return false;
    }

    // get filesize, if avail
    if (ujg_base) {
        ujgfilesize = ujg_base->getsize();
    }


    return true;
}


/* -----------------------------------------------
    read uncompressed JPEG file
    ----------------------------------------------- */
namespace {
void mem_nop (void *opaque, void *ptr){

}
void * mem_init_nop(size_t prealloc_size, uint8_t align){
    return NULL;
}
void* mem_realloc_nop(void * ptr, size_t size, size_t *actualSize, unsigned int movable, void *opaque){
    return NULL;
}
}
bool read_ujpg( void )
{
    using namespace Sirikata;
//    colldata.start_decoder_worker_thread(std::bind(&simple_decoder, &colldata, str_in));
    unsigned char ujpg_mrk[ 64 ];
    // this is where we will enable seccomp, before reading user data
    using namespace IOUtil;
    // check version number
    Sirikata::JpegError err = str_in->Read( ujpg_mrk, 1).second;
    if ( err != Sirikata::JpegError::nil() || ujpg_mrk[ 0 ] != ujgversion ) {
        fprintf( stderr, "incompatible file, use %s v%i.%i",
            appname, ujpg_mrk[ 0 ] / 10, ujpg_mrk[ 0 ] % 10 );
        errorlevel.store(2);
        return false;
    }
    ReadFull(str_in, ujpg_mrk, 1 );
    uint32_t compressed_file_size = 0;
    bool has_read_size = (memcmp( ujpg_mrk, "Z", 1 ) == 0);
    (void)has_read_size;
    assert(has_read_size && "Legacy prerelease format encountered\n");
    unsigned char md5[16];
    ReadFull(str_in, md5, sizeof(md5));
// full size of the original file
    ReadFull(str_in, ujpg_mrk, 8);
    max_file_size = LEtoUint32(ujpg_mrk);
    str_out->call_size_callback(max_file_size);
    compressed_file_size = LEtoUint32(ujpg_mrk + 4);
    if (compressed_file_size > 128 * 1024 * 1024 || max_file_size > 128 * 1024 * 1024) {
        assert(false && "Only support images < 128 megs");
        return false; // bool too big
    }
    std::vector<uint8_t, JpegAllocator<uint8_t> > compressed_header_buffer(compressed_file_size);
    IOUtil::ReadFull(str_in, compressed_header_buffer.data(), compressed_header_buffer.size());
    MemReadWriter header_reader((JpegAllocator<uint8_t>()));
    {
        JpegAllocator<uint8_t> no_free_allocator;
        no_free_allocator.setup_memory_subsystem(32 * 1024 * 1024,
                                                 16,
                                                 &mem_init_nop,
                                                 &MemMgrAllocatorMalloc,
                                                 &mem_nop,
                                                 &mem_realloc_nop,
                                                 &MemMgrAllocatorMsize);
        std::pair<std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> >,
                  JpegError> uncompressed_header_buffer;
        uncompressed_header_buffer
                = ZlibDecoderDecompressionReader::Decompress(compressed_header_buffer.data(),
                                                         compressed_header_buffer.size(),
                                                         no_free_allocator);
        if (uncompressed_header_buffer.second) {
            assert(false && "Data not properly zlib coded");
            return false;
        }
        header_reader.SwapIn(uncompressed_header_buffer.first, 0);
    }
    grbs = sizeof(EOI);
    grbgdata = EOI; // if we don't have any garbage, assume FFD9 EOI
    // read header from file
    ReadFull(&header_reader, ujpg_mrk, 3 ) ;
    // check marker
    if ( memcmp( ujpg_mrk, "HDR", 3 ) == 0 ) {
        // read size of header, alloc memory
        ReadFull(&header_reader, ujpg_mrk, 4 );
        hdrs = LEtoUint32(ujpg_mrk);
        hdrdata = (unsigned char*) aligned_alloc(hdrs);
        if ( hdrdata == NULL ) {
            fprintf( stderr, MEM_ERRMSG );
            errorlevel.store(2);
            return false;
        }
        // read hdrdata
        ReadFull(&header_reader, hdrdata, hdrs );
    }
    else {
        fprintf( stderr, "HDR marker not found" );
        errorlevel.store(2);
        return false;
    }

    // parse header for image-info
    if ( !setup_imginfo_jpg() )
        return false;

    // beginning here: recovery information (needed for exact JPEG recovery)

    // read padbit information from file
    ReadFull(&header_reader, ujpg_mrk, 3 );
    // check marker
    if ( memcmp( ujpg_mrk, "PAD", 3 ) == 0 ) {
        // read size of header, alloc memory
        header_reader.Read( reinterpret_cast<unsigned char*>(&padbit), 1 );
    }
    else {
        fprintf( stderr, "PAD marker not found" );
        errorlevel.store(2);
        return false;
    }

    // read further recovery information if any
    while ( ReadFull(&header_reader, ujpg_mrk, 3 ) == 3 ) {
        // check marker
        if ( memcmp( ujpg_mrk, "FRS", 3 ) == 0 ) {
            // read number of false set RST markers per scan from file
            ReadFull(&header_reader, ujpg_mrk, 4);
            scnc = LEtoUint32(ujpg_mrk);
            
            rst_err.insert(rst_err.end(), scnc - rst_err.size(), 0);
            // read data
            ReadFull(&header_reader, rst_err.data(), scnc );
        }
        else if ( memcmp( ujpg_mrk, "GRB", 3 ) == 0 ) {
            // read garbage (data after end of JPG) from file
            ReadFull(&header_reader, ujpg_mrk, 4);
            grbs = LEtoUint32(ujpg_mrk);
            grbgdata = aligned_alloc(grbs);
            if ( grbgdata == NULL ) {
                fprintf( stderr, MEM_ERRMSG );
                errorlevel.store(2);
                return false;
            }
            // read garbage data
            ReadFull(&header_reader, grbgdata, grbs );
        }
        else if ( memcmp( ujpg_mrk, "SIZ", 3 ) == 0 ) {
            // full size of the original file
            ReadFull(&header_reader, ujpg_mrk, 4);
            max_file_size = LEtoUint32(ujpg_mrk);
        }
        else if ( memcmp( ujpg_mrk, "EEE", 3) == 0) {
            ReadFull(&header_reader, ujpg_mrk, 28);
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
            if (memcmp(ujpg_mrk, "CMP", 3) == 0 ) {
                break;
            } else {
                fprintf( stderr, "unknown data found" );
                errorlevel.store(2);
            }
            return false;
        }
    }
    ReadFull(str_in, ujpg_mrk, 3 ) ;
    if (memcmp(ujpg_mrk, "CMP", 3) != 0) {
        assert(false && "CMP must be present (uncompressed) in the file");
        return false; // not a JPG
    }
    colldata.signal_worker_should_begin();
    g_decoder->initialize(str_in);
    colldata.start_decoder_worker_thread(g_decoder.get());


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
bool setup_imginfo_jpg( void )
{
    unsigned char  type = 0x00; // type of current marker segment
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   hpos = 0; // position in header

    int cmp;

    // header parser loop
    while ( ( int ) hpos < hdrs ) {
        type = hdrdata[ hpos + 1 ];
        len = 2 + B_SHORT( hdrdata[ hpos + 2 ], hdrdata[ hpos + 3 ] );
        // do not parse DHT & DRI
        if ( ( type != 0xDA ) && ( type != 0xC4 ) && ( type != 0xDD ) ) {
            if ( !parse_jfif_jpg( type, len, &( hdrdata[ hpos ] ) ) )
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

    // alloc memory for further operations
    colldata.init(cmpnfo, cmpc);

    return true;
}


/* -----------------------------------------------
    Parse routines for JFIF segments
    ----------------------------------------------- */
bool parse_jfif_jpg( unsigned char type, unsigned int len, unsigned char* segment )
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
                lval = LBITS( segment[ hpos ], 4 );
                rval = RBITS( segment[ hpos ], 4 );
                if ( ((lval < 0) || (lval >= 2)) || ((rval < 0) || (rval >= 4)) )
                    break;

                hpos++;
                // build huffman codes & trees
                build_huffcodes( &(segment[ hpos + 0 ]), &(segment[ hpos + 16 ]),
                    &(hcodes[ lval ][ rval ]), &(htrees[ lval ][ rval ]) );
                htset[ lval ][ rval ] = 1;

                skip = 16;
                for ( i = 0; i < 16; i++ )
                    skip += ( int ) segment[ hpos + i ];
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
                lval = LBITS( segment[ hpos ], 4 );
                rval = RBITS( segment[ hpos ], 4 );
                if ( (lval < 0) || (lval >= 2) ) break;
                if ( (rval < 0) || (rval >= 4) ) break;
                hpos++;
                if ( lval == 0 ) { // 8 bit precision
                    for ( i = 0; i < 64; i++ ) {
                        qtables[ rval ][ i ] = ( unsigned short ) segment[ hpos + i ];
                        if ( qtables[ rval ][ i ] == 0 ) break;
                    }
                    hpos += 64;
                }
                else { // 16 bit precision
                    for ( i = 0; i < 64; i++ ) {
                        qtables[ rval ][ i ] =
                            B_SHORT( segment[ hpos + (2*i) ], segment[ hpos + (2*i) + 1 ] );
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
            rsti = B_SHORT( segment[ hpos ], segment[ hpos + 1 ] );
            return true;

        case 0xDA: // SOS segment
            // prepare next scan
            cs_cmpc = segment[ hpos ];
            if ( cs_cmpc > cmpc ) {
                fprintf( stderr, "%i components in scan, only %i are allowed",
                            cs_cmpc, cmpc );
                errorlevel.store(2);
                return false;
            }
            hpos++;
            for ( i = 0; i < cs_cmpc; i++ ) {
                for ( cmp = 0; ( segment[ hpos ] != cmpnfo[ cmp ].jid ) && ( cmp < cmpc ); cmp++ );
                if ( cmp == cmpc ) {
                    fprintf( stderr, "component id mismatch in start-of-scan" );
                    errorlevel.store(2);
                    return false;
                }
                cs_cmp[ i ] = cmp;
                cmpnfo[ cmp ].huffdc = LBITS( segment[ hpos + 1 ], 4 );
                cmpnfo[ cmp ].huffac = RBITS( segment[ hpos + 1 ], 4 );
                if ( ( cmpnfo[ cmp ].huffdc < 0 ) || ( cmpnfo[ cmp ].huffdc >= 4 ) ||
                     ( cmpnfo[ cmp ].huffac < 0 ) || ( cmpnfo[ cmp ].huffac >= 4 ) ) {
                    fprintf( stderr, "huffman table number mismatch" );
                    errorlevel.store(2);
                    return false;
                }
                hpos += 2;
            }
            cs_from = segment[ hpos + 0 ];
            cs_to   = segment[ hpos + 1 ];
            cs_sah  = LBITS( segment[ hpos + 2 ], 4 );
            cs_sal  = RBITS( segment[ hpos + 2 ], 4 );
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
            lval = segment[ hpos ];
            if ( lval != 8 ) {
                fprintf( stderr, "%i bit data precision is not supported", lval );
                errorlevel.store(2);
                return false;
            }

            // image size, height & component count
            imgheight = B_SHORT( segment[ hpos + 1 ], segment[ hpos + 2 ] );
            imgwidth  = B_SHORT( segment[ hpos + 3 ], segment[ hpos + 4 ] );
            cmpc      = segment[ hpos + 5 ];
            if ( cmpc > 4 ) {
                cmpc = 4;
                fprintf( stderr, "image has %i components, max 4 are supported", cmpc );
                errorlevel.store(2);
                return false;
            }
            hpos += 6;
            // components contained in image
            for ( cmp = 0; cmp < cmpc; cmp++ ) {
                cmpnfo[ cmp ].jid = segment[ hpos ];
                cmpnfo[ cmp ].sfv = LBITS( segment[ hpos + 1 ], 4 );
                cmpnfo[ cmp ].sfh = RBITS( segment[ hpos + 1 ], 4 );
                cmpnfo[ cmp ].qtable = qtables[ segment[ hpos + 2 ] ];
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
    unsigned int   len  = 0; // length of current marker segment
    unsigned int   hpos = 0; // position in header


    // start headerwriter
    hdrw = new abytewriter( 4096 );

    // header parser loop
    while ( ( int ) hpos < hdrs ) {
        type = hdrdata[ hpos + 1 ];
        len = 2 + B_SHORT( hdrdata[ hpos + 2 ], hdrdata[ hpos + 3 ] );
        // discard any unneeded meta info
        if ( ( type == 0xDA ) || ( type == 0xC4 ) || ( type == 0xDB ) ||
             ( type == 0xC0 ) || ( type == 0xC1 ) || ( type == 0xC2 ) ||
             ( type == 0xDD ) ) {
            hdrw->write_n( &(hdrdata[ hpos ]), len );
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
    hc = next_huffcode( huffr, dctree );
    if ( hc < 0 ) return -1; // return error
    else s = ( unsigned char ) hc;
    n = huffr->read( s );
    block[ 0 ] = DEVLI( s, n );

    // decode ac
    for ( bpos = 1; bpos < 64; )
    {
        // decode next
        hc = next_huffcode( huffr, actree );
        // analyse code
        if ( hc > 0 ) {
            z = LBITS( hc, 4 );
            s = RBITS( hc, 4 );
            n = huffr->read( s );
            if ( ( z + bpos ) >= 64 )
                return -1; // run is to long
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


    // return position of eob
    return eob;
}


/* -----------------------------------------------
    sequential block encoding routine
    ----------------------------------------------- */
int encode_block_seq( abitwriter* huffw, huffCodes* dctbl, huffCodes* actbl, short* block )
{
    unsigned short n;
    unsigned char  s;
    int bpos;
    int hc;
    short tmp;


    // encode DC
    tmp = block[ 0 ];
    s = uint16bit_length(ABS(tmp));
    n = ENVLI( s, tmp );
    huffw->write( dctbl->cval[ s ], dctbl->clen[ s ] );
    huffw->write( n, s );
    signed z = -1;
    if (g_vectorized_encode_block) {
    uint64_t block_data;
    memcpy(&block_data, block, sizeof(block_data));
    block_data = htole64(block_data);
    constexpr uint64_t all_ones = 0xffffffffffffffffULL;
    block_data &= (all_ones ^ 0xffff); // zero out the 0th item
    for(int counter = 0; counter < 64; counter += 4) {
        int shorts_left = 4;
        do {
            if (block_data == 0) {
                z += shorts_left;
                break;
            } else {
                int ctz = __builtin_ctzl(block_data);
                uint8_t num_zeros = (ctz >> 4);
                z += num_zeros;
                shorts_left -= num_zeros + 1;
                block_data >>= num_zeros << 4;
                int16_t tmp = (int16_t)htole16(block_data & 0xffff);
                s = nonzero_bit_length(ABS(tmp));
                n = ENVLI(s, tmp);
                hc = ( ( (z & 0xf) << 4 ) + s );                
                if (__builtin_expect(z & 0xf0, 0)) {
                    // write remaining zeroes
                    do {
                        huffw->write( actbl->cval[ 0xF0 ], actbl->clen[ 0xF0 ] );
                        z -= 16;
                    } while ( z & 0xf0 );
                }
                // write to huffman writer
                //fprintf(stderr, "Writing %d %d %d\n", hc, n, s);
                huffw->write( actbl->cval[ hc ], actbl->clen[ hc ] );
                huffw->write( n, s );
                
                z = 0;
                block_data >>= 16;
            }
        } while (shorts_left);
        if (counter == 60) {
            break;
        }
        memcpy(&block_data, block + counter + 4, sizeof(block_data));
        block_data = htole64(block_data);
    }
    } else {
    // encode AC
    z = 0;
    for ( bpos = 1; bpos < 64; bpos++ )
    {
        // if nonzero is encountered
        tmp = block[bpos];
        if (tmp == 0) {
            ++z;
            continue;
        }
        // vli encode
        s = nonzero_bit_length(ABS(tmp));
        n = ENVLI(s, tmp);
        hc = ( ( (z & 0xf) << 4 ) + s );
        if (__builtin_expect(z & 0xf0, 0)) {
            // write remaining zeroes
            do {
                huffw->write( actbl->cval[ 0xF0 ], actbl->clen[ 0xF0 ] );
                z -= 16;
            } while ( z & 0xf0 );
        }
        // write to huffman writer
        huffw->write( actbl->cval[ hc ], actbl->clen[ hc ] );
        huffw->write( n, s );
        // reset zeroes
        z = 0;
    }
    }
    // write eob if needed
    if ( z > 0 )
        huffw->write( actbl->cval[ 0x00 ], actbl->clen[ 0x00 ] );


    return 64 - z;
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
    hc = next_huffcode( huffr, dctree );
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
        for ( bpos = from; bpos <= to; )
            block[ bpos ] = 0;
        (*eobrun)--;
        return from;
    }

    // decode ac
    for ( bpos = from; bpos <= to; )
    {
        // decode next
        hc = next_huffcode( huffr, actree );
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
        hc = next_huffcode( huffr, actree );
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
        assert(s && "actbl->max_eobrun needs to be > 0");
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
int next_huffcode( abitreader *huffw, huffTree *ctree )
{
    int node = 0;


    while ( node < 256 ) {
        node = ( huffw->read( 1 ) == 1 ) ?
                ctree->r[ node ] : ctree->l[ node ];
        if ( node == 0 ) break;
    }

    return ( node - 256 );
}


/* -----------------------------------------------
    calculates next position for MCU
    ----------------------------------------------- */
int next_mcupos( int* mcu, int* cmp, int* csc, int* sub, int* dpos, int* rstw )
{
    int sta = 0; // status
    unsigned int local_mcuh = mcuh;
    unsigned int local_mcu = *mcu;
    unsigned int local_cmp = *cmp;
    unsigned int local_sub;
    // increment all counts where needed
    if ( (local_sub = ++(*sub) ) >= (unsigned int)cmpnfo[local_cmp].mbs) {
        local_sub = (*sub) = 0;

        if ( ( ++(*csc) ) >= cs_cmpc ) {
            (*csc) = 0;
            local_cmp = (*cmp) = cs_cmp[ 0 ];
            local_mcu = ++(*mcu);
            if ( local_mcu >= (unsigned int)mcuc ) {
                sta = 2;
            } else if ( rsti > 0 ){
                if ( --(*rstw) == 0 ) {
                    sta = 1;
                }
            }
        }
        else {
            local_cmp = (*cmp) = cs_cmp[(*csc)];
        }
    }
    unsigned int sfh = cmpnfo[local_cmp].sfh;
    unsigned int sfv = cmpnfo[local_cmp].sfv;
    // get correct position in image ( x & y )
    if ( sfh > 1 ) { // to fix mcu order
        unsigned int mcu_o_mcuh = local_mcu / local_mcuh;
        unsigned int sub_o_sfv = local_sub / sfv;
        unsigned int mcu_mod_mcuh = local_mcu - mcu_o_mcuh * local_mcuh;
        unsigned int sub_mod_sfv = local_sub - sub_o_sfv * sfv;
        unsigned int local_dpos = mcu_o_mcuh * sfh + sub_o_sfv;
        local_dpos *= cmpnfo[local_cmp].bch;
        local_dpos += mcu_mod_mcuh * sfv + sub_mod_sfv;
        *dpos = local_dpos;
    }
    else if ( sfv > 1 ) {
        // simple calculation to speed up things if simple fixing is enough
        (*dpos) = local_mcu * cmpnfo[local_cmp].mbs + local_sub;
    }
    else {
        // no calculations needed without subsampling
        (*dpos) = (*mcu);
    }


    return sta;
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
void build_huffcodes( unsigned char *clen, unsigned char *cval,    huffCodes *hc, huffTree *ht )
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
        for( j = 0; j < (int) clen[ i ]; j++ ) {
            hc->clen[ (int) cval[k] ] = 1 + i;
            hc->cval[ (int) cval[k] ] = code;

            k++;
            code++;
        }
        code = code << 1;
    }

    // find out eobrun max value
    hc->max_eobrun = 0;
    for ( i = 14; i >= 0; i-- ) {
        if ( hc->clen[ i << 4 ] > 0 ) {
            hc->max_eobrun = ( 2 << i ) - 1;
            break;
        }
    }

    // 2nd -> part use codes to build the coding tree

    // initial value for next free place
    nextfree = 1;

    // work through every code creating links between the nodes (represented through ints)
    for ( i = 0; i < 256; i++ )    {
        // (re)set current node
        node = 0;
        // go through each code & store path
        for ( j = hc->clen[ i ] - 1; j > 0; j-- ) {
            if ( BITN( hc->cval[ i ], j ) == 1 ) {
                if ( ht->r[ node ] == 0 )
                     ht->r[ node ] = nextfree++;
                node = ht->r[ node ];
            }
            else{
                if ( ht->l[ node ] == 0 )
                    ht->l[ node ] = nextfree++;
                node = ht->l[ node ];
            }
        }
        // last link is number of targetvalue + 256
        if ( hc->clen[ i ] > 0 ) {
            if ( BITN( hc->cval[ i ], 0 ) == 1 )
                ht->r[ node ] = i + 256;
            else
                ht->l[ node ] = i + 256;
        }
    }
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
    for ( hpos = 0; (int) hpos < hdrs; hpos += len ) {
        type = hdrdata[ hpos + 1 ];
        len = 2 + B_SHORT( hdrdata[ hpos + 2 ], hdrdata[ hpos + 3 ] );
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
