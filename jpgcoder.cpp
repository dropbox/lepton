#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <ctime>

#if defined(UNIX) || defined (__LINUX__)
	#include <unistd.h>
#else
	#include <io.h>
#endif 

#include "bitops.h"
#include "htables.h"



#define QUANT(cmp,bpos) ( cmpnfo[cmp].qtable[ bpos ] )
#define MAX_V(cmp,bpos) ( ( freqmax[bpos] + QUANT(cmp,bpos) - 1 ) /  QUANT(cmp,bpos) )

#define ENVLI(s,v)		( ( v > 0 ) ? v : ( v - 1 ) + ( 1 << s ) )
#define DEVLI(s,n)		( ( n >= ( 1 << (s - 1) ) ) ? n : n + 1 - ( 1 << s ) )
#define E_ENVLI(s,v)	( v - ( 1 << s ) )
#define E_DEVLI(s,n)	( n + ( 1 << s ) )

#define COS_DCT(l,s,n)  ( cos( ( ( 2 * l + 1 ) * s * M_PI ) / ( 2 * n ) ) )
#define C_DCT(n)		( ( n == 0 ) ? ( 1 ) : ( sqrt( 2 ) ) )
#define DCT_SCALE		sqrt( 8 )

#define ABS(v1)			( (v1 < 0) ? -v1 : v1 )
#define ABSDIFF(v1,v2)	( (v1 > v2) ? (v1 - v2) : (v2 - v1) )
#define IPOS(w,v,h)		( ( v * w ) + h )
#define NPOS(n1,n2,p)	( ( ( p / n1 ) * n2 ) + ( p % n1 ) )
#define ROUND_F(v1)		( (v1 < 0) ? (int) (v1 - 0.5) : (int) (v1 + 0.5) )
#define B_SHORT(v1,v2)	( ( ((int) v1) << 8 ) + ((int) v2) )
#define CLAMPED(l,h,v)	( ( v < l ) ? l : ( v > h ) ? h : v )

#define MEM_ERRMSG	"out of memory error"
#define FRD_ERRMSG	"could not read file / file not found: %s"
#define FWR_ERRMSG	"could not write file / file write-protected: %s"


/* -----------------------------------------------
	struct & enum declarations
	----------------------------------------------- */

enum ACTION { 	comp  =  1, split =  2, coll  =  3,
				info  =  4,	pgm   =  8	};
				
enum F_TYPE {   JPEG = 0, UJG = 1, UNK = 2		};

struct componentInfo {
	unsigned short* qtable; // quantization table
	int huffdc; // no of huffman table (DC)
	int huffac; // no of huffman table (AC)
	int sfv; // sample factor vertical
	int sfh; // sample factor horizontal	
	int mbs; // blocks in mcu		
	int bcv; // block count vertical (interleaved)
	int bch; // block count horizontal (interleaved)
	int bc;  // block count (all) (interleaved)
	int ncv; // block count vertical (non interleaved)
	int nch; // block count horizontal (non interleaved)
	int nc;  // block count (all) (non interleaved)
	int sid; // statistical identity
	int jid; // jpeg internal id
};

struct huffCodes {
	unsigned short cval[ 256 ];
	unsigned short clen[ 256 ];
	unsigned short max_eobrun;
};

struct huffTree {
	unsigned short l[ 256 ];
	unsigned short r[ 256 ];
};

	
/* -----------------------------------------------
	function declarations: main interface
	----------------------------------------------- */

void initialize_options( int argc, char** argv );
void process_file( void );
void execute( bool (*function)() );
void get_status( bool (*function)() );
void show_help( void );


/* -----------------------------------------------
	function declarations: main functions
	----------------------------------------------- */
	
bool check_file( void );
bool read_jpeg( void );
bool merge_jpeg( void );
bool decode_jpeg( void );
bool recode_jpeg( void );
bool adapt_icos( void );
bool check_value_range( void );
bool write_ujpg( void );
bool read_ujpg( void );
bool swap_streams( void );
bool compare_output( void );
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
						int* eobrun, int from, int to );
int encode_ac_prg_fs( abitwriter* huffw, huffCodes* actbl, short* block,
						int* eobrun, int from, int to );

int decode_dc_prg_sa( abitreader* huffr, short* block );
int encode_dc_prg_sa( abitwriter* huffw, short* block );
int decode_ac_prg_sa( abitreader* huffr, huffTree* actree, short* block,
						int* eobrun, int from, int to );
int encode_ac_prg_sa( abitwriter* huffw, abytewriter* storw, huffCodes* actbl,
						short* block, int* eobrun, int from, int to );

int decode_eobrun_sa( abitreader* huffr, short* block, int* eobrun, int from, int to );
int encode_eobrun( abitwriter* huffw, huffCodes* actbl, int* eobrun );
int encode_crbits( abitwriter* huffw, abytewriter* storw );

int next_huffcode( abitreader *huffw, huffTree *ctree );
int next_mcupos( int* mcu, int* cmp, int* csc, int* sub, int* dpos, int* rstw );
int next_mcuposn( int* cmp, int* dpos, int* rstw );
int skip_eobrun( int* cmp, int* dpos, int* rstw, int* eobrun );

void build_huffcodes( unsigned char *clen, unsigned char *cval,
				huffCodes *hc, huffTree *ht );


/* -----------------------------------------------
	function declarations: DCT
	----------------------------------------------- */

bool prepare_dct( int nx, int ny, float* icos_idct_fst, float* icos_fdct_fst );
bool prepare_dct_base( int nx, int ny, float* dct_base );
float idct_2d_fst_8x8( signed short* F, int ix, int iy );
float fdct_2d_fst_8x8( unsigned char* f, int iu, int iv );
float fdct_2d_fst_8x8( float* f, int iu, int iv );
float idct_1d_fst_8( signed short* F, int ix );
float fdct_1d_fst_8( unsigned char* f, int iu );
float fdct_1d_fst_8( float* f, int iu );
float idct_2d_fst_8x8( int cmp, int dpos, int ix, int iy );
float idct_2d_fst_1x8( int cmp, int dpos, int ix, int iy );
float idct_2d_fst_8x1( int cmp, int dpos, int ix, int iy );


/* -----------------------------------------------
	function declarations: miscelaneous helpers
	----------------------------------------------- */

char* create_filename( char* base, char* extension );
void set_extension( char* destination, char* origin, char* extension );
void add_underscore( char* filename );


/* -----------------------------------------------
	function declarations: developers functions
	----------------------------------------------- */

// these are developers functions, they are not needed
// in any way to compress jpg or decompress ujg
bool write_hdr( void );
bool write_huf( void );
bool write_coll( void );
bool write_file( char* base, char* ext, void* data, int bpv, int size );
bool write_errfile( void );
bool write_info( void );
bool write_pgm( void );


/* -----------------------------------------------
	global variables: data storage
	----------------------------------------------- */

unsigned short qtables[4][64];				// quantization tables
huffCodes      hcodes[2][4];				// huffman codes
huffTree       htrees[2][4];				// huffman decoding trees
unsigned char  htset[2][4];					// 1 if huffman table is set

unsigned char* grbgdata			= 	NULL;	// garbage data
unsigned char* hdrdata          =   NULL;   // header data
unsigned char* huffdata         =   NULL;   // huffman coded data
int            hufs             =    0  ;   // size of huffman data
int            hdrs             =    0  ;   // size of header
int            grbs             =    0  ;   // size of garbage

unsigned int*  rstp             =   NULL;   // restart markers positions in huffdata
unsigned int*  scnp             =   NULL;   // scan start positions in huffdata
int            rstc             =    0  ;   // count of restart markers
int            scnc             =    0  ;   // count of scans
int            rsti             =    0  ;   // restart interval
char           padbit           =    -1 ;   // padbit (for huffman coding)
unsigned char* rst_err			=   NULL;   // number of wrong-set RST markers per scan

signed short*  colldata[4][64]  = { NULL }; // collection sorted DCT coefficients

float icos_base_8x8[ 8 * 8 ];				// precalculated base dct elements (8x1)

float icos_fdct_8x8[ 8 * 8 * 8 * 8 ];		// precalculated values for fdct (8x8)
float icos_idct_8x8[ 8 * 8 * 8 * 8 ];		// precalculated values for idct (8x8)
float icos_fdct_1x8[ 1 * 1 * 8 * 8 ];		// precalculated values for fdct (1x8)
float icos_idct_1x8[ 1 * 1 * 8 * 8 ];		// precalculated values for idct (1x8)

float adpt_idct_8x8[ 4 ][ 8 * 8 * 8 * 8 ];	// precalculated values for idct (8x8)
float adpt_idct_1x8[ 4 ][ 1 * 1 * 8 * 8 ];	// precalculated values for idct (1x8)
float adpt_idct_8x1[ 4 ][ 8 * 8 * 1 * 1 ];	// precalculated values for idct (8x1)


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
int mcuh        = 0; // mcus per collumn
int mcuc        = 0; // count of mcus


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
	
char*  jpgfilename;			// name of JPEG file
char*  ujgfilename;			// name of UJG file
char*  tmpfilename;			// temporary file name
char*  basfilename;			// base file name

int    jpgfilesize;			// size of JPEG file
int    ujgfilesize;			// size of UJG file
int    jpegtype = 0;		// type of JPEG coding: 0->unknown, 1->sequential, 2->progressive
F_TYPE filetype;			// type of current file

iostream* str_in  = NULL;	// input stream
iostream* str_out = NULL;	// output stream
iostream* str_str = NULL;	// storage stream

char** filelist = NULL;		// list of files to process 
int    file_cnt = 0;		// count of files in list
int    file_no  = 0;		// number of current file


/* -----------------------------------------------
	global variables: messages
	----------------------------------------------- */

char statusmessage[ 128 ];
char errormessage [ 128 ];
bool (*errorfunction)();
int  errorlevel;
// meaning of errorlevel:
// -1 -> wrong input
// 0 -> no error
// 1 -> warning
// 2 -> fatal error


/* -----------------------------------------------
	global variables: settings
	----------------------------------------------- */

int  verbosity  = 0;		// level of verbosity
bool overwrite  = false;	// overwrite files yes / no
int  verify_lv  = 0;		// verification level ( none (0), simple (1), detailed output (2) )
int  err_tresh  = 1;		// error threshold ( proceed on warnings yes (2) / no (1) )
bool disc_meta  = false;	// discard meta-info yes / no

bool developer  = false;	// allow developers functions yes/no
ACTION action   = comp;		// what to do with JPEG/UJG files
int  collmode   = 0;		// write mode for collections: 0 -> std, 1 -> dhf, 2 -> squ, 3 -> unc

FILE*  msgout   = stdout;	// stream for output of messages
bool   pipe_on  = false;	// use stdin/stdout instead of filelist


/* -----------------------------------------------
	global variables: info about program
	----------------------------------------------- */

const unsigned char ujgversion   = 24;
static const char*  subversion   = "a";
static const char*  appname      = "uncmpJPG";
static const char*  versiondate  = "10/20/2011";
static const char*  author       = "Matthias Stirner";
static const char*  website      = "http://www.elektronik.htw-aalen.de/packjpg/";
static const char*  email        = "packjpg@htw-aalen.de";
static const char*  ujg_ext      = "ujg";
static const char*  jpg_ext      = "jpg";
static const char   ujg_header[] = { 'U', 'J' };


/* -----------------------------------------------
	main-function
	----------------------------------------------- */

int main( int argc, char** argv )
{	
	sprintf( statusmessage, "no statusmessage specified" );
	sprintf( errormessage, "no errormessage specified" );
	
	clock_t begin, end;
	
	int error_cnt = 0;
	int warn_cnt  = 0;
	
	int acc_jpgsize = 0;
	int acc_ujgsize = 0;
	
	int speed, bpms;
	float cr;
	
	errorlevel = 0;
	
	
	// read options from command line
	initialize_options( argc, argv );
	
	// write program info to screen
	fprintf( msgout,  "\n--> %s v%i.%i%s (%s) by %s <--\n\n",
			appname, ujgversion / 10, ujgversion % 10, subversion, versiondate, author );
	
	// check if user input is wrong, show help screen if it is
	if ( ( file_cnt == 0 ) ||
		( ( !developer ) && ( (action != comp) || (verify_lv > 1) ) ) ) {
		show_help();
		return -1;
	}
	
	// init tables for dct
	prepare_dct( 8, 8, icos_idct_8x8, icos_fdct_8x8 );
	prepare_dct( 1, 8, icos_idct_1x8, icos_fdct_1x8 );
	prepare_dct_base( 8, 8, icos_base_8x8 );
	
	// (re)set program has to be done first
	reset_buffers();
	
	// process file(s) - this is the main function routine
	begin = clock();
	for ( file_no = 0; file_no < file_cnt; file_no++ ) {		
		process_file();
		if ( errorlevel >= err_tresh ) error_cnt++;
		else {
			if ( errorlevel == 1 ) warn_cnt++;
			if ( errorlevel < err_tresh ) {
				acc_jpgsize += jpgfilesize;
				acc_ujgsize += ujgfilesize;
			}
		}
	}
	end = clock();
		
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
	
	
	return file_cnt;
}


/* ----------------------- Begin of main interface functions -------------------------- */

/* -----------------------------------------------
	reads in commandline arguments
	----------------------------------------------- */
	
void initialize_options( int argc, char** argv )
{
	char** tmp_flp;
	int tmp_val;
	int i;
	
	
	// get memory for filelist & preset with NULL
	filelist = new char*[ argc ];
	for ( i = 0; i < argc; i++ )
		filelist[ i ] = NULL;
	
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
		else if ( strcmp((*argv), "-p" ) == 0 ) {
			err_tresh = 2;
		}
		else if ( strcmp((*argv), "-d" ) == 0 ) {
			disc_meta = true;
		}		
		else if ( strcmp((*argv), "-ver" ) == 0 ) {
			verify_lv = ( verify_lv < 1 ) ? 1 : verify_lv;
		}
		else if ( strcmp((*argv), "-dev") == 0 ) {
			developer = true;
		}
		else if ( strcmp((*argv), "-test") == 0 ) {
			verify_lv = 2;
		}
		else if ( sscanf( (*argv), "-coll%i", &tmp_val ) == 1 ) {
			tmp_val = ( tmp_val < 0 ) ? 0 : tmp_val;
			tmp_val = ( tmp_val > 3 ) ? 3 : tmp_val;
			collmode = tmp_val;
			action = coll;
		}
		else if ( strcmp((*argv), "-split") == 0 ) {
			action = split;
		}
		else if ( strcmp((*argv), "-info") == 0 ) {
			action = info;
		}
		else if ( strcmp((*argv), "-pgm") == 0 ) {
			action = pgm;
		}
	   	else if ( ( strcmp((*argv), "-ujg") == 0 ) ||
				  ( strcmp((*argv), "-ujg") == 0 ) ||
				  ( strcmp((*argv), "-comp") == 0) ) {
			action = comp;
		}
		else if ( strcmp((*argv), "-") == 0 ) {			
			msgout = stderr;
			// set binary mode for stdin & stdout
			#if !defined( unix )				
				setmode( fileno( stdin ), O_BINARY );
				setmode( fileno( stdout ), O_BINARY );
			#endif
			// use "-" as placeholder for stdin
			*(tmp_flp++) = "-";
		}
		else {
			// if argument is not switch, it's a filename
			*(tmp_flp++) = *argv;
		}		
	}
	
	// count number of files (or filenames) in filelist
	for ( file_cnt = 0; filelist[ file_cnt ] != NULL; file_cnt++ );	
}


/* -----------------------------------------------
	processes one file
	----------------------------------------------- */

void process_file( void )
{
	clock_t begin, end;
	char* actionmsg  = NULL;
	char* errtypemsg = NULL;
	int speed, bpms;
	float cr;	
	
	
	errorfunction = NULL;
	errorlevel = 0;
	jpgfilesize = 0;
	ujgfilesize = 0;	
	
	
	// compare file name, set pipe if needed
	if ( ( strcmp( filelist[ file_no ], "-" ) == 0 ) && ( action == comp ) ) {
		pipe_on = true;
		filelist[ file_no ] = "STDIN";
	}
	else {		
		pipe_on = false;
	}
		
	fprintf( msgout,  "\nProcessing file %i of %i \"%s\" -> ",
				file_no + 1, file_cnt, filelist[ file_no ] );
	if ( verbosity > 1 )
		fprintf( msgout,  "\n----------------------------------------" );
	
	// check input file and determine filetype
	execute( check_file );
	
	// get specific action message
	if ( filetype == UNK ) actionmsg = "unknown filetype";
	else switch ( action )
	{
		case comp:
			if ( filetype == JPEG )
				actionmsg = "Expanding";
			else
				actionmsg = "Compressing";
			break;
			
		case split:
			actionmsg = "Splitting";
			break;
			
		case coll:
			actionmsg = "Extracting Colls";
			break;
			
		case info:
			actionmsg = "Extracting info";
			break;
		
		case pgm:
			actionmsg = "Converting";
			break;
	}
	
	if ( verbosity < 2 ) fprintf( msgout, "%s -> ", actionmsg );
	
	
	// main function routine
	begin = clock();
	
	if ( filetype == JPEG )
	{
		switch ( action )
		{
			case comp:
				execute( read_jpeg );
				execute( decode_jpeg );
				execute( check_value_range );
				execute( adapt_icos );
				execute( write_ujpg ); // replace with compression function!
				if ( verify_lv > 0 ) { // verifcation
					execute( reset_buffers );
					execute( swap_streams );
					execute( read_ujpg );
					execute( adapt_icos );
					execute( recode_jpeg );
					execute( merge_jpeg );
					execute( compare_output );
				}
				break;
			
			case split:
				execute( read_jpeg );
				execute( write_hdr );
				execute( write_huf );
				break;
				
			case coll:
				execute( read_jpeg );
				execute( decode_jpeg );
				execute( write_coll );
				break;
				
			case info:
				execute( read_jpeg );
				execute( write_info );
				break;
			
			case pgm:
				execute( read_jpeg );
				execute( decode_jpeg );
				execute( adapt_icos );
				execute( write_pgm );
				break;
		}
	}
	else if ( filetype == UJG )
	{
		switch ( action )
		{
			case comp:
				execute( read_ujpg ); // replace with decompression function!
				execute( adapt_icos );
				execute( recode_jpeg );
				execute( merge_jpeg );
				if ( verify_lv > 0 ) { // verify
					execute( reset_buffers );
					execute( swap_streams );
					execute( read_jpeg );
					execute( decode_jpeg );
					execute( check_value_range );
					execute( adapt_icos );
					execute( write_ujpg );
					execute( compare_output );
				}
				break;
			
			case split:
				execute( read_ujpg );
				execute( adapt_icos );
				execute( recode_jpeg );
				execute( write_hdr );
				execute( write_huf );
				break;
				
			case coll:
				execute( read_ujpg );
				execute( adapt_icos );	
				execute( write_coll );
				break;
			
			case info:
				execute( read_ujpg );
				execute( write_info );
				break;
			
			case pgm:
				execute( read_ujpg );
				execute( adapt_icos );
				execute( write_pgm );
				break;
		}
	}
	// write error file if verify lv > 1
	if ( ( verify_lv > 1 ) && ( errorlevel >= err_tresh ) )
		write_errfile();
	// reset buffers
	reset_buffers();
	
	// close iostreams
	if ( str_in  != NULL ) delete( str_in  ); str_in  = NULL;
	if ( str_out != NULL ) delete( str_out ); str_out = NULL;
	if ( str_str != NULL ) delete( str_str ); str_str = NULL;
	// delete if broken or if output not needed
	if ( ( !pipe_on ) && ( ( errorlevel >= err_tresh ) || ( action != comp ) ) ) {
		if ( filetype == JPEG )
			if ( access( ujgfilename, 0 ) == 0 ) remove( ujgfilename );
		else if ( filetype == UJG )
			if ( access( jpgfilename, 0 ) == 0 ) remove( jpgfilename );
	}
	// remove temp file
	if ( ( access( tmpfilename, 0 ) == 0 ) &&
		!( ( verify_lv > 1 ) && ( errorlevel >= err_tresh ) && ( errorfunction == compare_output ) ) )
		remove( tmpfilename );
	
	end = clock();
	
	
	// speed and compression ratio calculation
	speed = (int) ( (double) (( end - begin ) * 1000) / CLOCKS_PER_SEC );
	bpms  = ( speed > 0 ) ? ( jpgfilesize / speed ) : jpgfilesize;
	cr    = ( jpgfilesize > 0 ) ? ( 100.0 * ujgfilesize / jpgfilesize ) : 0;

	
	switch ( verbosity )
	{
		case 0:
			if ( errorlevel < err_tresh ) {
				if ( action == comp )
					fprintf( msgout,  "%.2f%%", cr );
				else fprintf( msgout,  "DONE" );
			}
			else fprintf( msgout,  "ERROR" );
			if ( errorlevel > 0 )
				fprintf( msgout,  "\n" );
			break;
		
		case 1:
			if ( errorlevel < err_tresh ) fprintf( msgout,  "DONE\n" );
			else fprintf( msgout,  "ERROR\n" );
			break;
		
		case 2:
			fprintf( msgout,  "\n----------------------------------------\n" );
			if ( errorlevel < err_tresh ) fprintf( msgout,  "-> %s OK\n", actionmsg );
			break;
	}
	
	switch ( errorlevel )
	{
		case 0:
			errtypemsg = "none";
			break;
			
		case 1:
			if ( errorlevel < err_tresh )
				errtypemsg = "warning (ignored)";
			else
				errtypemsg = "warning (skipped file)";
			break;
		
		case 2:
			errtypemsg = "fatal error";
			break;
	}
	
	if ( errorlevel > 0 )
	{
		get_status( errorfunction );
		fprintf( stderr, " %s -> %s:\n", statusmessage, errtypemsg  );
		fprintf( stderr, " %s\n", errormessage );
		if ( verbosity > 1 )
			fprintf( stderr, " (in file \"%s\")\n", filelist[ file_no ] );
	}
	if ( (verbosity > 0) && (errorlevel < err_tresh) )
	if ( action == comp )
	{
		fprintf( msgout,  " time taken  : %7i msec\n", speed );
		fprintf( msgout,  " byte per ms : %7i byte\n", bpms );
		fprintf( msgout,  " comp. ratio : %7.2f %%\n", cr );		
	}
	
	if ( ( verbosity > 1 ) && ( action == comp ) )
		fprintf( msgout,  "\n" );
}


/* -----------------------------------------------
	main-function execution routine
	----------------------------------------------- */

void execute( bool (*function)() )
{	
	clock_t begin, end;
	bool success;
	int i;
	
	
	if ( errorlevel < err_tresh )
	{
		// get statusmessage
		get_status( function );
		// write statusmessage
		if ( verbosity == 2 ) {
			fprintf( msgout,  "\n%s ", statusmessage );
			for ( i = strlen( statusmessage ); i <= 30; i++ )
				fprintf( msgout,  " " );			
		}
		
		// set starttime
		begin = clock();
		// call function
		success = ( *function )();
		// set endtime
		end = clock();
		
		if ( ( errorlevel > 0 ) && ( errorfunction == NULL ) )
			errorfunction = function;
		
		// write statusmessage
		if ( success ) {
			if ( verbosity == 2 ) fprintf( msgout,  "%6ims",
				(int) ( (double) (( end - begin ) * 1000) / CLOCKS_PER_SEC ) );
		}
		else {
			errorfunction = function;
			if ( verbosity == 2 ) fprintf( msgout,  "%8s", "ERROR" );
		}
	}
}


/* -----------------------------------------------
	gets statusmessage for function
	----------------------------------------------- */
	
void get_status( bool (*function)() )
{	
	if ( function == NULL ) {
		sprintf( statusmessage, "unknown action" );
	}
	else if ( function == *check_file ) {
		sprintf( statusmessage, "Determining filetype" );
	}
	else if ( function == *read_jpeg ) {
		sprintf( statusmessage, "Reading header & image data" );
	}
	else if ( function == *merge_jpeg ) {
		sprintf( statusmessage, "Merging header & image data" );
	}
	else if ( function == *decode_jpeg ) {
		sprintf( statusmessage, "Decompressing JPEG image data" );
	}
	else if ( function == *recode_jpeg ) {
		sprintf( statusmessage, "Recompressing JPEG image data" );
	}
	else if ( function == *adapt_icos ) {
		sprintf( statusmessage, "Adapting DCT precalc. tables" );
	}
	else if ( function == *check_value_range ) {
		sprintf( statusmessage, "Checking values range" );
	}
	else if ( function == *write_ujpg ) {
		sprintf( statusmessage, "Expanding data to UJPG" );
	}
	else if ( function == *read_ujpg ) {
		sprintf( statusmessage, "Compressing data to JPEG" );
	}
	else if ( function == *swap_streams ) {
		sprintf( statusmessage, "Swapping input/output streams" );
	}
	else if ( function == *compare_output ) {
		sprintf( statusmessage, "Verifying output stream" );
	}
	else if ( function == *reset_buffers ) {
		sprintf( statusmessage, "Resetting program" );
	}
	else if ( function == *write_hdr ) {
		sprintf( statusmessage, "Writing header data to file" );
	}
	else if ( function == *write_huf ) {
		sprintf( statusmessage, "Writing huffman data to file" );
	}
	else if ( function == *write_coll ) {
		sprintf( statusmessage, "Writing collections to files" );
	}
	else if ( function == *write_errfile ) {
		sprintf( statusmessage, "Writing error info to file" );
	}
	else if ( function == *write_info ) {
		sprintf( statusmessage, "Writing info to files" );
	}
	else if ( function == *write_pgm ) {
		sprintf( statusmessage, "Writing converted image to pgm" );
	}
}


/* -----------------------------------------------
	shows help in case of wrong input
	----------------------------------------------- */
	
void show_help( void )
{	
	fprintf( msgout, "\n" );
	fprintf( msgout, "Website: %s\n", website );
	fprintf( msgout, "Email  : %s\n", email );
	fprintf( msgout, "\n" );
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
	fprintf( msgout, "Examples: \"%s -v1 -o baboon.%s\"\n", appname, ujg_ext );
	fprintf( msgout, "          \"%s -p *.%s\"\n", appname, jpg_ext );	
}

/* ----------------------- End of main interface functions -------------------------- */

/* ----------------------- Begin of main functions -------------------------- */


/* -----------------------------------------------
	check file and determine filetype
	----------------------------------------------- */
	
bool check_file( void )
{	
	unsigned char fileid[ 2 ] = { 0, 0 };
	int namelen = strlen( filelist[ file_no ] ) + 16;
	
	
	// open input stream, check for errors
	str_in = new iostream( (void*) filelist[ file_no ], ( !pipe_on ) ? 0 : 2, 0, 0 );
	if ( str_in->chkerr() ) {
		sprintf( errormessage, FRD_ERRMSG, filelist[ file_no ] );
		errorlevel = 2;
		return false;
	}
	
	// free memory from filenames if needed
	if ( jpgfilename != NULL ) free( jpgfilename );
	if ( ujgfilename != NULL ) free( ujgfilename );
	if ( tmpfilename != NULL ) free( tmpfilename );
	if ( basfilename != NULL ) free( basfilename );
	// alloc memory for filenames
	jpgfilename = (char*) calloc( namelen, sizeof( char ) );
	ujgfilename = (char*) calloc( namelen, sizeof( char ) );
	tmpfilename = (char*) calloc( namelen, sizeof( char ) );
	basfilename = (char*) calloc( namelen, sizeof( char ) );
	
	// immediately return error if 2 bytes can't be read
	if ( str_in->read( fileid, 1, 2 ) != 2 ) { 
		filetype = UNK;
		sprintf( errormessage, "file doesn't contain enough data" );
		errorlevel = 2;
		return false;
	}
	
	// set temp filename & base filename
	if ( !pipe_on ) {
		set_extension( basfilename, filelist[ file_no ], "bin" );
		set_extension( tmpfilename, filelist[ file_no ], "tmp" );
	}
	else {
		strcpy( basfilename, "BINFILE" );
		strcpy( tmpfilename, "TMPFILE" );
	}
	while ( access( tmpfilename, 0 ) == 0 ) {
		namelen += sizeof( char );
		tmpfilename = (char*) realloc( tmpfilename, namelen );
		add_underscore( tmpfilename );
	}
	
	// check file id, determine filetype
	if ( ( fileid[0] == 0xFF ) && ( fileid[1] == 0xD8 ) ) {
		// file is JPEG
		filetype = JPEG;
		// create filenames
		if ( !pipe_on ) {
			strcpy( jpgfilename, filelist[ file_no ] );
			set_extension( ujgfilename, filelist[ file_no ], (char*) ujg_ext );
			if ( !overwrite ) {
				while ( access( ujgfilename, 0 ) == 0 ) {
					namelen += sizeof( char );
					ujgfilename = (char*) realloc( ujgfilename, namelen );
					add_underscore( ujgfilename );
				}
			}
		}
		else {
			strcpy( jpgfilename, "STDIN" );
			strcpy( ujgfilename, "STDOUT" );
		}
		// open output stream, check for errors
		str_out = new iostream( (void*) ujgfilename, ( !pipe_on ) ? 0 : 2, 0, 1 );
		if ( str_out->chkerr() ) {
			sprintf( errormessage, FWR_ERRMSG, ujgfilename );
			errorlevel = 2;
			return false;
		}
	}
	else if ( ( fileid[0] == ujg_header[0] ) && ( fileid[1] == ujg_header[1] ) ) {
		// file is UJG
		filetype = UJG;
		// create filenames
		if ( !pipe_on ) {
			strcpy( ujgfilename, filelist[ file_no ] );
			set_extension( jpgfilename, filelist[ file_no ], (char*) jpg_ext );			
			if ( !overwrite ) {
				while ( access( jpgfilename, 0 ) == 0 ) {
					namelen += sizeof( char );
					jpgfilename = (char*) realloc( jpgfilename, namelen );
					add_underscore( jpgfilename );
				}
			}
		}
		else {
			strcpy( jpgfilename, "STDOUT" );
			strcpy( ujgfilename, "STDIN" );
		}
		// open output stream, check for errors
		str_out = new iostream( (void*) jpgfilename, ( !pipe_on ) ? 0 : 2, 0, 1 );
		if ( str_out->chkerr() ) {
			sprintf( errormessage, FWR_ERRMSG, jpgfilename );
			errorlevel = 2;
			return false;
		}
	}
	else {
		// file is neither
		filetype = UNK;
		sprintf( errormessage, "filetype of file \"%s\" is unknown", filelist[ file_no ] );
		errorlevel = 2;
		return false;		
	}
	
	
	return true;
}


/* -----------------------------------------------
	Read in header & image data
	----------------------------------------------- */
	
bool read_jpeg( void )
{
	unsigned char* segment = NULL; // storage for current segment
	unsigned int   ssize = 1024; // current size of segment array
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
	
	// alloc memory for segment data first
	segment = ( unsigned char* ) calloc( ssize, sizeof( char ) );
	if ( segment == NULL ) {
		sprintf( errormessage, MEM_ERRMSG );
		errorlevel = 2;
		return false;
	}
	
	// JPEG reader loop
	while ( true ) {		
		if ( type == 0xDA ) { // if last marker was sos
			// switch to huffman data reading mode
			cpos = 0;
			crst = 0;
			while ( true ) {
				// read byte from imagedata
				if ( str_in->read( &tmp, 1, 1 ) == 0 )
					break;
					
				// non-0xFF loop
				if ( tmp != 0xFF ) {
					crst = 0;
					while ( tmp != 0xFF ) {
						huffw->write( tmp );
						if ( str_in->read( &tmp, 1, 1 ) == 0 )
							break;
					}
				}
				
				// treatment of 0xFF
				if ( tmp == 0xFF ) {
					if ( str_in->read( &tmp, 1, 1 ) == 0 )
						break; // read next byte & check
					if ( tmp == 0x00 ) {
						crst = 0;
						// no zeroes needed -> ignore 0x00. write 0xFF
						huffw->write( 0xFF );
					}
					else if ( tmp == 0xD0 + ( cpos % 8 ) ) { // restart marker
						// increment rst counters
						cpos++;
						crst++;
					}
					else { // in all other cases leave it to the header parser routines
						// store number of falsely set rst markers
						if ( crst > 0 ) {
							if ( rst_err == NULL ) {
								rst_err = (unsigned char*) calloc( scnc + 1, sizeof( char ) );
								if ( rst_err == NULL ) {
									sprintf( errormessage, MEM_ERRMSG );
									errorlevel = 2;
									return false;
								}
							}
						}
						if ( rst_err != NULL ) {
							// realloc and set only if needed
							rst_err = ( unsigned char* ) realloc( rst_err, ( scnc + 1 ) * sizeof( char ) );
							if ( rst_err == NULL ) {
								sprintf( errormessage, MEM_ERRMSG );
								errorlevel = 2;
								return false;
							}
							if ( crst > 255 ) {
								sprintf( errormessage, "Severe false use of RST markers (%i)", crst );
								errorlevel = 1;
								crst = 255;
							}
							rst_err[ scnc ] = crst;							
						}
						// end of current scan
						scnc++;
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
			if ( str_in->read( segment, 1, 2 ) != 2 ) break;
			if ( segment[ 0 ] != 0xFF ) {
				// ugly fix for incorrect marker segment sizes
				sprintf( errormessage, "size mismatch in marker segment FF %2X", type );
				errorlevel = 2;
				if ( type == 0xFE ) { //  if last marker was COM try again
					if ( str_in->read( segment, 1, 2 ) != 2 ) break;
					if ( segment[ 0 ] == 0xFF ) errorlevel = 1;
				}
				if ( errorlevel == 2 ) {
					delete ( hdrw );
					delete ( huffw );
					free ( segment );
					return false;
				}
			}
		}
		
		// read segment type
		type = segment[ 1 ];
		
		// if EOI is encountered make a quick exit
		if ( type == 0xD9 ) {
			// get pointer for header data & size
			hdrdata  = hdrw->getptr();
			hdrs     = hdrw->getpos();
			// get pointer for huffman data & size
			huffdata = huffw->getptr();
			hufs     = huffw->getpos();
			// everything is done here now
			break;			
		}
		
		// read in next segments' length and check it
		if ( str_in->read( segment + 2, 1, 2 ) != 2 ) break;
		len = 2 + B_SHORT( segment[ 2 ], segment[ 3 ] );
		if ( len < 4 ) break;
		
		// realloc segment data if needed
		if ( ssize < len ) {
			segment = ( unsigned char* ) realloc( segment, len );
			if ( segment == NULL ) {
				sprintf( errormessage, MEM_ERRMSG );
				errorlevel = 2;
				delete ( hdrw );
				delete ( huffw );
				return false;
			}
			ssize = len;
		}
		
		// read rest of segment, store back in header writer
		if ( str_in->read( ( segment + 4 ), 1, ( len - 4 ) ) !=
			( unsigned short ) ( len - 4 ) ) break;
		hdrw->write_n( segment, len );
	}
	// JPEG reader loop end
	
	// free writers
	delete ( hdrw );
	delete ( huffw );
	
	// check if everything went OK
	if ( ( hdrs == 0 ) || ( hufs == 0 ) ) {
		sprintf( errormessage, "unexpected end of data encountered" );
		errorlevel = 2;
		return false;
	}
	
	// store garbage after EOI if needed
	grbs = str_in->read( &tmp, 1, 1 );	
	if ( grbs > 0 ) {
		// sprintf( errormessage, "data after EOI - last bytes: FF D9 %2X", tmp );
		// errorlevel = 1;		
		grbgw = new abytewriter( 1024 );
		grbgw->write( tmp );
		while( true ) {
			len = str_in->read( segment, 1, ssize );
			if ( len == 0 ) break;
			grbgw->write_n( segment, len );
		}
		grbgdata = grbgw->getptr();
		grbs     = grbgw->getpos();
		delete ( grbgw );
	}
	
	// free segment
	free( segment );
	
	// get filesize
	jpgfilesize = str_in->getsize();	
		
	// parse header for image info
	if ( !setup_imginfo_jpg() ) {
		return false;
	}
	
	
	return true;
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
	unsigned int   tmp; // temporary storage variable
	
	
	// write SOI
	str_out->write( SOI, 1, 2 );
	
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
		str_out->write( hdrdata + tmp, 1, ( hpos - tmp ) );
		
		// get out if last marker segment type was not SOS
		if ( type != 0xDA ) break;
		
		
		// (re)set corrected rst pos
		cpos = 0;
		
		// write & expand huffman coded image data
		for ( ipos = scnp[ scan - 1 ]; ipos < scnp[ scan ]; ipos++ ) {
			// write current byte
			str_out->write( huffdata + ipos, 1, 1 );
			// check current byte, stuff if needed
			if ( huffdata[ ipos ] == 0xFF )
				str_out->write( &stv, 1, 1 );
			// insert restart markers if needed
			if ( rstp != NULL ) {
				if ( ipos == rstp[ rpos ] ) {
					rst = 0xD0 + ( cpos % 8 );
					str_out->write( &mrk, 1, 1 );
					str_out->write( &rst, 1, 1 );
					rpos++; cpos++;
				}
			}
		}
		// insert false rst markers at end if needed
		if ( rst_err != NULL ) {
			while ( rst_err[ scan - 1 ] > 0 ) {
				rst = 0xD0 + ( cpos % 8 );
				str_out->write( &mrk, 1, 1 );
				str_out->write( &rst, 1, 1 );
				cpos++;	rst_err[ scan - 1 ]--;
			}
		}

		// proceed with next scan
		scan++;
	}
	
	// write EOI
	str_out->write( EOI, 1, 2 );
	
	// write garbage if needed
	if ( grbs > 0 )
		str_out->write( grbgdata, 1, grbs );
	
	// errormessage if write error
	if ( str_out->chkerr() ) {
		sprintf( errormessage, "write error, possibly drive is full" );
		errorlevel = 2;		
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
	int eobrun; // run of eobs
	int rstw; // restart wait counter
	
	int cmp, bpos, dpos;
	int mcu, sub, csc;
	int eob, sta;
	
	
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
			if ( ( cs_sal == 0 ) && ( htset[ 0 ][ cmpnfo[cmp].huffdc ] == 0 ) ||
				 ( cs_sah >  0 ) && ( htset[ 1 ][ cmpnfo[cmp].huffac ] == 0 ) ) {
				sprintf( errormessage, "huffman table missing in scan%i", scnc );
				delete huffr;
				errorlevel = 2;
				return false;
			}
		}
		
		
		// intial variables set for decoding
		cmp  = cs_cmp[ 0 ];
		csc  = 0;
		mcu  = 0;
		sub  = 0;
		dpos = 0;
		
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
						// decode block
						eob = decode_block_seq( huffr,
							&(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
							&(htrees[ 1 ][ cmpnfo[cmp].huffdc ]),
							block );
						
						// fix dc
						block[ 0 ] += lastdc[ cmp ];
						lastdc[ cmp ] = block[ 0 ];
						
						// copy to colldata
						for ( bpos = 0; bpos < eob; bpos++ )
							colldata[ cmp ][ bpos ][ dpos ] = block[ bpos ];
						
						// check for errors, proceed if no error encountered
						if ( eob < 0 ) sta = -1;
						else sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
					}
				}
				else if ( cs_sah == 0 ) {
					// ---> progressive interleaved DC decoding <---
					// ---> succesive approximation first stage <---
					while ( sta == 0 ) {
						sta = decode_dc_prg_fs( huffr,
							&(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
							block );
						
						// fix dc for diff coding
						colldata[cmp][0][dpos] = block[0] + lastdc[ cmp ];
						lastdc[ cmp ] = colldata[cmp][0][dpos];
						
						// bitshift for succesive approximation
						colldata[cmp][0][dpos] <<= cs_sal;
						
						// next mcupos if no error happened
						if ( sta != -1 )
							sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
					}
				}
				else {
					// ---> progressive interleaved DC decoding <---
					// ---> succesive approximation later stage <---					
					while ( sta == 0 ) {
						// decode next bit
						sta = decode_dc_prg_sa( huffr,
							block );
						
						// shift in next bit
						colldata[cmp][0][dpos] += block[0] << cs_sal;
						
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
						// decode block
						eob = decode_block_seq( huffr,
							&(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
							&(htrees[ 1 ][ cmpnfo[cmp].huffdc ]),
							block );
						
						// fix dc
						block[ 0 ] += lastdc[ cmp ];
						lastdc[ cmp ] = block[ 0 ];
						
						// copy to colldata
						for ( bpos = 0; bpos < eob; bpos++ )
							colldata[ cmp ][ bpos ][ dpos ] = block[ bpos ];
						
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
							sta = decode_dc_prg_fs( huffr,
								&(htrees[ 0 ][ cmpnfo[cmp].huffdc ]),
								block );
								
							// fix dc for diff coding
							colldata[cmp][0][dpos] = block[0] + lastdc[ cmp ];
							lastdc[ cmp ] = colldata[cmp][0][dpos];
							
							// bitshift for succesive approximation
							colldata[cmp][0][dpos] <<= cs_sal;
							
							// check for errors, increment dpos otherwise
							if ( sta != -1 )
								sta = next_mcuposn( &cmp, &dpos, &rstw );
						}
					}
					else {
						// ---> progressive non interleaved DC decoding <---
						// ---> succesive approximation later stage <---
						while( sta == 0 ) {
							// decode next bit
							sta = decode_dc_prg_sa( huffr,
								block );
							
							// shift in next bit
							colldata[cmp][0][dpos] += block[0] << cs_sal;
							
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
							// decode block
							eob = decode_ac_prg_fs( huffr,
								&(htrees[ 1 ][ cmpnfo[cmp].huffac ]),
								block, &eobrun, cs_from, cs_to );
							
							// check for non optimal coding
							if ( ( eob == cs_from ) && ( eobrun > 0 ) &&
								( peobrun > 0 ) && ( peobrun <
								hcodes[ 1 ][ cmpnfo[cmp].huffac ].max_eobrun - 1 ) ) {
								sprintf( errormessage,
									"reconstruction of non optimal coding not supported" );
								errorlevel = 1;
							}
							
							// copy to colldata
							for ( bpos = cs_from; bpos < eob; bpos++ )
								colldata[ cmp ][ bpos ][ dpos ] = block[ bpos ] << cs_sal;
							
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
								block[ bpos ] = colldata[ cmp ][ bpos ][ dpos ];
							
							if ( eobrun == 0 ) {
								// decode block (long routine)
								eob = decode_ac_prg_sa( huffr,
									&(htrees[ 1 ][ cmpnfo[cmp].huffac ]),
									block, &eobrun, cs_from, cs_to );
								
								// check for non optimal coding
								if ( ( eob == cs_from ) && ( eobrun > 0 ) &&
									( peobrun > 0 ) && ( peobrun <
									hcodes[ 1 ][ cmpnfo[cmp].huffac ].max_eobrun - 1 ) ) {
									sprintf( errormessage,
										"reconstruction of non optimal coding not supported" );
									errorlevel = 1;
								}
								
								// store eobrun
								peobrun = eobrun;
							}
							else {
								// decode block (short routine)
								eob = decode_eobrun_sa( huffr,
									block, &eobrun, cs_from, cs_to );
							}
								
							// copy back to colldata
							for ( bpos = cs_from; bpos <= cs_to; bpos++ )
								colldata[ cmp ][ bpos ][ dpos ] += block[ bpos ] << cs_sal;
							
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
					sprintf( errormessage, "inconsistent use of padbits" );
					padbit = 1;
					errorlevel = 1;
				}
			}
			else {
				padbit = huffr->unpad( padbit );
			}
			
			// evaluate status
			if ( sta == -1 ) { // status -1 means error
				sprintf( errormessage, "decode error in scan%i / mcu%i",
					scnc, ( cs_cmpc > 1 ) ? mcu : dpos );
				delete huffr;
				errorlevel = 2;
				return false;
			}
			else if ( sta == 2 ) { // status 2/3 means done
				scnc++; // increment scan counter
				break; // leave decoding loop, everything is done here
			}
			// else if ( sta == 1 ); // status 1 means restart - so stay in the loop
		}
	}
	
	// check for unneeded data
	if ( !huffr->eof ) {
		sprintf( errormessage, "unneeded data found after coded image data" );
		errorlevel = 1;
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
	abitwriter*  huffw; // bitwise writer for image data
	abytewriter* storw; // bytewise writer for storage of correction bits
	
	unsigned char  type = 0x00; // type of current marker segment
	unsigned int   len  = 0; // length of current marker segment
	unsigned int   hpos = 0; // current position in header
		
	int lastdc[ 4 ]; // last dc for each component
	short block[ 64 ]; // store block for coeffs
	int eobrun; // run of eobs
	int rstw; // restart wait counter
	
	int cmp, bpos, dpos;
	int mcu, sub, csc;
	int eob, sta;
	int tmp;
	
	
	// open huffman coded image data in abitwriter
	huffw = new abitwriter( 0 );
	huffw->fillbit = padbit;
	
	// init storage writer
	storw = new abytewriter( 0 );
	
	// preset count of scans and restarts
	scnc = 0;
	rstc = 0;
	
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
		if ( scnp == NULL ) scnp = ( unsigned int* ) calloc( scnc + 2, sizeof( int ) );
		else scnp = ( unsigned int* ) realloc( scnp, ( scnc + 2 ) * sizeof( int ) );
		if ( scnp == NULL ) {
			sprintf( errormessage, MEM_ERRMSG );
			errorlevel = 2;
			return false;
		}
		
		// (re)alloc restart marker positons array if needed
		if ( rsti > 0 ) {
			tmp = rstc + ( ( cs_cmpc > 1 ) ?
				( mcuc / rsti ) : ( cmpnfo[ cs_cmp[ 0 ] ].bc / rsti ) );
			if ( rstp == NULL ) rstp = ( unsigned int* ) calloc( tmp + 1, sizeof( int ) );
			else rstp = ( unsigned int* ) realloc( rstp, ( tmp + 1 ) * sizeof( int ) );
			if ( rstp == NULL ) {
				sprintf( errormessage, MEM_ERRMSG );
				errorlevel = 2;
				return false;
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
						for ( bpos = 0; bpos < 64; bpos++ )
							block[ bpos ] = colldata[ cmp ][ bpos ][ dpos ];
						
						// diff coding for dc
						block[ 0 ] -= lastdc[ cmp ];
						lastdc[ cmp ] = colldata[ cmp ][ 0 ][ dpos ];
						
						// encode block
						eob = encode_block_seq( huffw,
							&(hcodes[ 0 ][ cmpnfo[cmp].huffac ]),
							&(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
							block );
						
						// check for errors, proceed if no error encountered
						if ( eob < 0 ) sta = -1;
						else sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
					}
				}
				else if ( cs_sah == 0 ) {
					// ---> progressive interleaved DC encoding <---
					// ---> succesive approximation first stage <---
					while ( sta == 0 ) {
						// diff coding & bitshifting for dc 
						tmp = colldata[ cmp ][ 0 ][ dpos ] >> cs_sal;
						block[ 0 ] = tmp - lastdc[ cmp ];
						lastdc[ cmp ] = tmp;
						
						// encode dc
						sta = encode_dc_prg_fs( huffw,
							&(hcodes[ 0 ][ cmpnfo[cmp].huffdc ]),
							block );
						
						// next mcupos if no error happened
						if ( sta != -1 )
							sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
					}
				}
				else {
					// ---> progressive interleaved DC encoding <---
					// ---> succesive approximation later stage <---
					while ( sta == 0 ) {
						// fetch bit from current bitplane
						block[ 0 ] = BITN( colldata[ cmp ][ 0 ][ dpos ], cs_sal );
						
						// encode dc correction bit
						sta = encode_dc_prg_sa( huffw, block );
						
						// next mcupos if no error happened
						if ( sta != -1 )
							sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
					}
				}
			}
			else // encoding for non interleaved data
			{
				if ( jpegtype == 1 ) {
					// ---> sequential non interleaved encoding <---
					while ( sta == 0 ) {
						// copy from colldata
						for ( bpos = 0; bpos < 64; bpos++ )
							block[ bpos ] = colldata[ cmp ][ bpos ][ dpos ];
						
						// diff coding for dc
						block[ 0 ] -= lastdc[ cmp ];
						lastdc[ cmp ] = colldata[ cmp ][ 0 ][ dpos ];
						
						// encode block
						eob = encode_block_seq( huffw,
							&(hcodes[ 0 ][ cmpnfo[cmp].huffac ]),
							&(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
							block );
						
						// check for errors, proceed if no error encountered
						if ( eob < 0 ) sta = -1;
						else sta = next_mcuposn( &cmp, &dpos, &rstw );	
					}
				}
				else if ( cs_to == 0 ) {
					if ( cs_sah == 0 ) {
						// ---> progressive non interleaved DC encoding <---
						// ---> succesive approximation first stage <---
						while ( sta == 0 ) {
							// diff coding & bitshifting for dc 
							tmp = colldata[ cmp ][ 0 ][ dpos ] >> cs_sal;
							block[ 0 ] = tmp - lastdc[ cmp ];
							lastdc[ cmp ] = tmp;
							
							// encode dc
							sta = encode_dc_prg_fs( huffw,
								&(hcodes[ 0 ][ cmpnfo[cmp].huffdc ]),
								block );							
							
							// check for errors, increment dpos otherwise
							if ( sta != -1 )
								sta = next_mcuposn( &cmp, &dpos, &rstw );
						}
					}
					else {
						// ---> progressive non interleaved DC encoding <---
						// ---> succesive approximation later stage <---
						while ( sta == 0 ) {
							// fetch bit from current bitplane
							block[ 0 ] = BITN( colldata[ cmp ][ 0 ][ dpos ], cs_sal );
							
							// encode dc correction bit
							sta = encode_dc_prg_sa( huffw, block );
							
							// next mcupos if no error happened
							if ( sta != -1 )
								sta = next_mcuposn( &cmp, &dpos, &rstw );
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
									FDIV2( colldata[ cmp ][ bpos ][ dpos ], cs_sal );
							
							// encode block
							eob = encode_ac_prg_fs( huffw,
								&(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
								block, &eobrun, cs_from, cs_to );
							
							// check for errors, proceed if no error encountered
							if ( eob < 0 ) sta = -1;
							else sta = next_mcuposn( &cmp, &dpos, &rstw );
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
									FDIV2( colldata[ cmp ][ bpos ][ dpos ], cs_sal );
							
							// encode block
							eob = encode_ac_prg_sa( huffw, storw,
								&(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
								block, &eobrun, cs_from, cs_to );
							
							// check for errors, proceed if no error encountered
							if ( eob < 0 ) sta = -1;
							else sta = next_mcuposn( &cmp, &dpos, &rstw );
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
				sprintf( errormessage, "encode error in scan%i / mcu%i",
					scnc, ( cs_cmpc > 1 ) ? mcu : dpos );
				delete huffw;
				errorlevel = 2;
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
		}
	}
	
	// safety check for error in huffwriter
	if ( huffw->error ) {
		delete huffw;
		sprintf( errormessage, MEM_ERRMSG );
		errorlevel = 2;
		return false;
	}
	
	// get data into huffdata
	huffdata = huffw->getptr();
	hufs = huffw->getpos();	
	delete huffw;
	
	// remove storage writer
	delete storw;
	
	// store last scan & restart positions
	scnp[ scnc ] = hufs;
	if ( rstp != NULL )
		rstp[ rstc ] = hufs;
	
	
	return true;
}


/* -----------------------------------------------
	adapt ICOS tables for quantizer tables
	----------------------------------------------- */
	
bool adapt_icos( void )
{
	int ipos;
	int cmp;
	
	
	for ( cmp = 0; cmp < cmpc; cmp++ ) {
		// adapt idct 8x8 table
		for ( ipos = 0; ipos < 64 * 64; ipos++ )
			adpt_idct_8x8[ cmp ][ ipos ] = icos_idct_8x8[ ipos ] * QUANT( cmp, zigzag[ ipos % 64 ] );
		// adapt idct 1x8 table
		for ( ipos = 0; ipos < 8 * 8; ipos++ )
			adpt_idct_1x8[ cmp ][ ipos ] = icos_idct_1x8[ ipos ] * QUANT( cmp, zigzag[ ( ipos % 8 ) * 8 ] );
		// adapt idct 8x1 table
		for ( ipos = 0; ipos < 8 * 8; ipos++ )
			adpt_idct_8x1[ cmp ][ ipos ] = icos_idct_1x8[ ipos ] * QUANT( cmp, zigzag[ ipos % 8 ] );
	}
	
	
	return true;
}


/* -----------------------------------------------
	checks range of values, error if out of bounds
	----------------------------------------------- */

bool check_value_range( void )
{
	int absmax;
	int cmp, bpos, dpos;
	
	
	// out of range should never happen with unmodified JPEGs
	for ( cmp = 0; cmp < cmpc; cmp++ )
	for ( bpos = 0; bpos < 64; bpos++ ) {		
		absmax = MAX_V( cmp, bpos );
		for ( dpos = 0; dpos < cmpnfo[cmp].bc; dpos++ )
		if ( ( colldata[cmp][bpos][dpos] > absmax ) ||
			 ( colldata[cmp][bpos][dpos] < -absmax ) ) {
			sprintf( errormessage, "value out of range error: cmp%i, frq%i, val %i, max %i",
					cmp, bpos, colldata[cmp][bpos][dpos], absmax );
			errorlevel = 2;
			return false;
		}
	}
	
	
	return true;
}


/* -----------------------------------------------
	write uncompressed JPEG file
	----------------------------------------------- */
	
bool write_ujpg( void )
{
	char ujpg_mrk[ 64 ];
	int cmp, bpos;
	
	
	// UJG-Header
	str_out->write( (void*) ujg_header, 1, 2 );
	
	// store version number
	ujpg_mrk[ 0 ] = ujgversion;
	str_out->write( ujpg_mrk, 1, 1 );
	
	// discard meta information from header if needed
	if ( disc_meta )
		if ( !rebuild_header_jpg() )
			return false;
	
	// write header to file
	// marker: "HDR" + [size of header]
	sprintf( ujpg_mrk, "HDR" );
	str_out->write( (void*) ujpg_mrk, 1, 3 );
	str_out->write( (void*) &hdrs, sizeof( int ), 1 );
	// data: data from header
	str_out->write( (void*) hdrdata, sizeof( char ), hdrs );
	
	// write actual decompressed coefficient data to file
	for ( cmp = 0; cmp < cmpc; cmp++ ) {
		// marker: "CMP" + [number of component]
		sprintf( ujpg_mrk, "CMP%i", cmp );
		str_out->write( (void*) ujpg_mrk, 1, 4 );
		// data: coefficient data in zigzag collection order
		for ( bpos = 0; bpos < 64; bpos++ )
			str_out->write( (void*) colldata[ cmp ][ bpos ], sizeof( short ), cmpnfo[ cmp ].bc );
	}

	// beginning here: recovery information (needed for exact JPEG recovery)
	
	// write huffman coded data padbit
	// marker: "PAD"
	sprintf( ujpg_mrk, "PAD" );
	str_out->write( (void*) ujpg_mrk, 1, 3 );
	// data: padbit
	str_out->write( (void*) &padbit, sizeof( char ), 1 );
	
	// write number of false set RST markers per scan (if available) to file
	if ( rst_err != NULL ) {
		// marker: "FRS" + [number of scans]
		sprintf( ujpg_mrk, "FRS" );
		str_out->write( (void*) ujpg_mrk, 1, 3 );
		str_out->write( (void*) &scnc, sizeof( int ), 1 );
		// data: numbers of false set markers		
		str_out->write( (void*) rst_err, sizeof( char ), scnc );
	}
	
	// write garbage (data after EOI) (if any) to file
	if ( grbs > 0 ) {
		// marker: "GRB" + [size of garbage]
		sprintf( ujpg_mrk, "GRB" );
		str_out->write( (void*) ujpg_mrk, 1, 3 );
		str_out->write( (void*) &grbs, sizeof( int ), 1 );
		// data: garbage data
		str_out->write( (void*) grbgdata, sizeof( char ), grbs );
	}
	
	// errormessage if write error
	if ( str_out->chkerr() ) {
		sprintf( errormessage, "write error, possibly drive is full" );
		errorlevel = 2;		
		return false;
	}
	
	// get filesize
	ujgfilesize = str_out->getsize();
	
	
	return true;
}


/* -----------------------------------------------
	read uncompressed JPEG file
	----------------------------------------------- */
	
bool read_ujpg( void )
{
	char ujpg_mrk[ 64 ];
	int cmp, bpos;
	
	
	// check version number
	str_in->read( ujpg_mrk, 1, 1 );
	if ( ujpg_mrk[ 0 ] != ujgversion ) {
		sprintf( errormessage, "incompatible file, use %s v%i.%i",
			appname, ujpg_mrk[ 0 ] / 10, ujpg_mrk[ 0 ] % 10 );
		errorlevel = 2;
		return false;
	}
	
	
	// read header from file
	str_in->read( ujpg_mrk, 1, 3 );
	// check marker
	if ( strncmp( ujpg_mrk, "HDR", 3 ) == 0 ) {
		// read size of header, alloc memory
		str_in->read( &hdrs, sizeof( int ), 1 );
		hdrdata = (unsigned char*) calloc( hdrs, sizeof( char ) );
		if ( hdrdata == NULL ) {
			sprintf( errormessage, MEM_ERRMSG );
			errorlevel = 2;
			return false;
		}
		// read hdrdata
		str_in->read( hdrdata, sizeof( char ), hdrs );
	}
	else {
		sprintf( errormessage, "HDR marker not found" );
		errorlevel = 2;
		return false;
	}
	
	// parse header for image-info
	if ( !setup_imginfo_jpg() )
		return false;
	
	// read actual decompressed coefficient data from file
	for ( cmp = 0; cmp < cmpc; cmp++ ) {
		str_in->read( ujpg_mrk, 1, 4 );
		// check marker
		if ( strncmp( ujpg_mrk, "CMP", 3 ) == 0 ) {
			// read coefficient data from file
			for ( bpos = 0; bpos < 64; bpos++ ) {
				if ( str_in->read( colldata[ cmp ][ bpos ], sizeof( short ), cmpnfo[ cmp ].bc ) != cmpnfo[ cmp ].bc ) {
					sprintf( errormessage, "unexpected end of file" );
					errorlevel = 2;
					return false;
				}
			}
		}
		else {
			sprintf( errormessage, "CMP%i marker not found", cmp );
			errorlevel = 2;
			return false;
		}
	}
	
	// beginning here: recovery information (needed for exact JPEG recovery)
	
	// read padbit information from file
	str_in->read( ujpg_mrk, 1, 3 );
	// check marker
	if ( strncmp( ujpg_mrk, "PAD", 3 ) == 0 ) {
		// read size of header, alloc memory
		str_in->read( &padbit, sizeof( char ), 1 );
	}
	else {
		sprintf( errormessage, "PAD marker not found" );
		errorlevel = 2;
		return false;
	}
	
	// read further recovery information if any
	while ( str_in->read( ujpg_mrk, 1, 3 ) == 3 ) {
		// check marker
		if ( strncmp( ujpg_mrk, "FRS", 3 ) == 0 ) {
			// read number of false set RST markers per scan from file
			str_in->read( &scnc, sizeof( int ), 1 ); // # of scans
			rst_err = (unsigned char*) calloc( scnc, sizeof( char ) );
			if ( rst_err == NULL ) {
				sprintf( errormessage, MEM_ERRMSG );
				errorlevel = 2;
				return false;
			}
			// read data
			str_in->read( rst_err, sizeof( char ), scnc );
		}
		else if ( strncmp( ujpg_mrk, "GRB", 3 ) == 0 ) {
			// read garbage (data after end of JPG) from file
			str_in->read( &grbs, sizeof( int ), 1 );
			grbgdata = (unsigned char*) calloc( grbs, sizeof( char ) );
			if ( grbgdata == NULL ) {
				sprintf( errormessage, MEM_ERRMSG );
				errorlevel = 2;
				return false;
			}
			// read garbage data
			str_in->read( grbgdata, sizeof( char ), grbs );
		}
		else {
			sprintf( errormessage, "unknown data found" );
			errorlevel = 2;
			return false;
		}
	}
	
	// get filesize
	ujgfilesize = str_in->getsize();
	
	
	return true;
}


/* -----------------------------------------------
	swap streams / init verification
	----------------------------------------------- */
bool swap_streams( void )	
{
	char dmp[ 2 ];
	
	// store input stream
	str_str = str_in;
	str_str->rewind();
	
	// replace input stream by output stream / switch mode for reading / read first bytes
	str_in = str_out;
	str_in->switch_mode();
	str_in->read( dmp, 1, 2 );
	
	// open new stream for output / check for errors
	str_out = new iostream( (void*) tmpfilename, 0, 0, 1 );
	if ( str_out->chkerr() ) {
		sprintf( errormessage, "error opening comparison stream" );
		errorlevel = 2;
		return false;
	}
	
	
	return true;
}


/* -----------------------------------------------
	comparison between input & output
	----------------------------------------------- */

bool compare_output( void )
{
	unsigned char* buff_ori;
	unsigned char* buff_cmp;
	int bsize = 1024;
	int dsize;
	int i, b;
	
	
	// init buffer arrays
	buff_ori = ( unsigned char* ) calloc( bsize, sizeof( char ) );
	buff_cmp = ( unsigned char* ) calloc( bsize, sizeof( char ) );
	if ( ( buff_ori == NULL ) || ( buff_cmp == NULL ) ) {
		sprintf( errormessage, MEM_ERRMSG );
		errorlevel = 2;
		return false;
	}
	
	// switch output stream mode / check for stream errors
	str_out->switch_mode();
	while ( true ) {
		if ( str_out->chkerr() )
			sprintf( errormessage, "error in comparison stream" );
		else if ( str_in->chkerr() )
			sprintf( errormessage, "error in output stream" );
		else if ( str_str->chkerr() )
			sprintf( errormessage, "error in input stream" );
		else break;
		errorlevel = 2;
		return false;
	}
	
	// compare sizes
	dsize = str_str->getsize();
	if ( str_out->getsize() != dsize ) {
		sprintf( errormessage, "file sizes do not match" );
		errorlevel = 2;
		return false;
	}
	
	// compare files byte by byte
	for ( i = 0; i < dsize; i++ ) {
		b = i % bsize;
		if ( b == 0 ) {
			str_str->read( buff_ori, sizeof( char ), bsize );
			str_out->read( buff_cmp, sizeof( char ), bsize );
		}
		if ( buff_ori[ b ] != buff_cmp[ b ] ) {
			sprintf( errormessage, "difference found at 0x%X", i );
			errorlevel = 2;
			return false;
		}
	}
	
	
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
	if ( hdrdata  != NULL ) free ( hdrdata );
	if ( huffdata != NULL ) free ( huffdata );
	if ( grbgdata != NULL ) free ( grbgdata );
	if ( rst_err  != NULL ) free ( rst_err );
	if ( rstp     != NULL ) free ( rstp );
	if ( scnp     != NULL ) free ( scnp );
	hdrdata   = NULL;
	huffdata  = NULL;
	grbgdata  = NULL;
	rst_err   = NULL;
	rstp      = NULL;
	scnp      = NULL;
	
	// free image arrays
	for ( cmp = 0; cmp < 4; cmp++ ) {
		for ( bpos = 0; bpos < 64; bpos++ ){
			if (colldata[ cmp ][ bpos ] != NULL) free( colldata[cmp][bpos] );
			colldata[ cmp ][ bpos ] = NULL;
		}		
	}
	
	
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
	
	int cmp, bpos;
	
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
		sprintf( errormessage, "header contains incomplete information" );
		errorlevel = 2;
		return false;
	}
	for ( cmp = 0; cmp < cmpc; cmp++ ) {
		if ( ( cmpnfo[cmp].sfv == 0 ) ||
			 ( cmpnfo[cmp].sfh == 0 ) ||
			 ( cmpnfo[cmp].qtable == NULL ) ||
			 ( cmpnfo[cmp].qtable[0] == 0 ) ||
			 ( jpegtype == 0 ) ) {
			sprintf( errormessage, "header information is incomplete" );
			errorlevel = 2;
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
	}
	
	// decide components' statistical ids
	if ( cmpc <= 3 ) {
		for ( cmp = 0; cmp < cmpc; cmp++ ) cmpnfo[ cmp ].sid = cmp;
	}
	else {
		for ( cmp = 0; cmp < cmpc; cmp++ ) cmpnfo[ cmp ].sid = 0;
	}
	
	// alloc memory for further operations
	for ( cmp = 0; cmp < cmpc; cmp++ )
	{
		// alloc memory for colls
		for ( bpos = 0; bpos < 64; bpos++ ) {
			colldata[cmp][bpos] = (short int*) calloc ( cmpnfo[cmp].bc, sizeof( short ) );
			if (colldata[cmp][bpos] == NULL) {
				sprintf( errormessage, MEM_ERRMSG );
				errorlevel = 2;
				return false;
			}
		}
	}
	
	
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
				sprintf( errormessage, "size mismatch in dht marker" );
				errorlevel = 2;
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
				sprintf( errormessage, "size mismatch in dqt marker" );
				errorlevel = 2;
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
				sprintf( errormessage, "%i components in scan, only %i are allowed",
							cs_cmpc, cmpc );
				errorlevel = 2;
				return false;
			}
			hpos++;
			for ( i = 0; i < cs_cmpc; i++ ) {
				for ( cmp = 0; ( segment[ hpos ] != cmpnfo[ cmp ].jid ) && ( cmp < cmpc ); cmp++ );
				if ( cmp == cmpc ) {
					sprintf( errormessage, "component id mismatch in start-of-scan" );
					errorlevel = 2;
					return false;
				}
				cs_cmp[ i ] = cmp;
				cmpnfo[ cmp ].huffdc = LBITS( segment[ hpos + 1 ], 4 );
				cmpnfo[ cmp ].huffac = RBITS( segment[ hpos + 1 ], 4 );
				if ( ( cmpnfo[ cmp ].huffdc < 0 ) || ( cmpnfo[ cmp ].huffdc >= 4 ) ||
					 ( cmpnfo[ cmp ].huffac < 0 ) || ( cmpnfo[ cmp ].huffac >= 4 ) ) {
					sprintf( errormessage, "huffman table number mismatch" );
					errorlevel = 2;
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
				sprintf( errormessage, "spectral selection parameter out of range" );
				errorlevel = 2;
				return false;
			}
			if ( ( cs_sah >= 12 ) || ( cs_sal >= 12 ) ) {
				sprintf( errormessage, "successive approximation parameter out of range" );
				errorlevel = 2;
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
				sprintf( errormessage, "%i bit data precision is not supported", lval );
				errorlevel = 2;
				return false;
			}
			
			// image size, height & component count
			imgheight = B_SHORT( segment[ hpos + 1 ], segment[ hpos + 2 ] );
			imgwidth  = B_SHORT( segment[ hpos + 3 ], segment[ hpos + 4 ] );
			cmpc      = segment[ hpos + 5 ];
			if ( cmpc > 4 ) {
				sprintf( errormessage, "image has %i components, max 4 are supported", cmpc );
				errorlevel = 2;
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
			sprintf( errormessage, "sof3 marker found, image is coded lossless" );
			errorlevel = 2;
			return false;
		
		case 0xC5: // SOF5 segment
			// coding process: differential sequential DCT
			sprintf( errormessage, "sof5 marker found, image is coded diff. sequential" );
			errorlevel = 2;
			return false;
		
		case 0xC6: // SOF6 segment
			// coding process: differential progressive DCT
			sprintf( errormessage, "sof6 marker found, image is coded diff. progressive" );
			errorlevel = 2;
			return false;
		
		case 0xC7: // SOF7 segment
			// coding process: differential lossless
			sprintf( errormessage, "sof7 marker found, image is coded diff. lossless" );
			errorlevel = 2;
			return false;
			
		case 0xC9: // SOF9 segment
			// coding process: arithmetic extended sequential DCT
			sprintf( errormessage, "sof9 marker found, image is coded arithm. sequential" );
			errorlevel = 2;
			return false;
			
		case 0xCA: // SOF10 segment
			// coding process: arithmetic extended sequential DCT
			sprintf( errormessage, "sof10 marker found, image is coded arithm. progressive" );
			errorlevel = 2;
			return false;
			
		case 0xCB: // SOF11 segment
			// coding process: arithmetic extended sequential DCT
			sprintf( errormessage, "sof11 marker found, image is coded arithm. lossless" );
			errorlevel = 2;
			return false;
			
		case 0xCD: // SOF13 segment
			// coding process: arithmetic differntial sequential DCT
			sprintf( errormessage, "sof13 marker found, image is coded arithm. diff. sequential" );
			errorlevel = 2;
			return false;
			
		case 0xCE: // SOF14 segment
			// coding process: arithmetic differential progressive DCT
			sprintf( errormessage, "sof14 marker found, image is coded arithm. diff. progressive" );
			errorlevel = 2;
			return false;
		
		case 0xCF: // SOF15 segment
			// coding process: arithmetic differntial lossless
			sprintf( errormessage, "sof15 marker found, image is coded arithm. diff. lossless" );
			errorlevel = 2;
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
			sprintf( errormessage, "rst marker found out of place" );
			errorlevel = 2;
			return false;
		
		case 0xD8: // SOI segment
			// return errormessage - start-of-image is out of place here
			sprintf( errormessage, "soi marker found out of place" );
			errorlevel = 2;
			return false;
		
		case 0xD9: // EOI segment
			// return errormessage - end-of-image is out of place here
			sprintf( errormessage, "eoi marker found out of place" );
			errorlevel = 2;
			return false;
			
		default: // unknown marker segment
			// return warning
			sprintf( errormessage, "unknown marker found: FF %2X", type );
			errorlevel = 1;
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
	free( hdrdata );
	hdrdata = hdrw->getptr();
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
			//	block[ bpos++ ] = 0;
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
	unsigned char  z;
	int bpos;
	int hc;
	int tmp;
	
	
	// encode DC	
	tmp = ABS( block[ 0 ] );
	BITLEN( s, tmp );
	n = ENVLI( s, block[ 0 ] );
	huffw->write( dctbl->cval[ s ], dctbl->clen[ s ] );
	huffw->write( n, s );
	
	// encode AC
	z = 0;
	for ( bpos = 1; bpos < 64; bpos++ )
	{
		// if nonzero is encountered
		if ( block[ bpos ] != 0 ) {
			// write remaining zeroes
			while ( z >= 16 ) {
				huffw->write( actbl->cval[ 0xF0 ], actbl->clen[ 0xF0 ] );
				z -= 16;
			}			
			// vli encode
			tmp = ABS( block[ bpos ] );
			BITLEN( s, tmp );
			n = ENVLI( s, block[ bpos ] );
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
	tmp = ABS( block[ 0 ] );
	BITLEN( s, tmp );
	n = ENVLI( s, block[ 0 ] );
	huffw->write( dctbl->cval[ s ], dctbl->clen[ s ] );
	huffw->write( n, s );
	
	
	// return 0 if everything is ok
	return 0;
}


/* -----------------------------------------------
	progressive AC decoding routine
	----------------------------------------------- */
int decode_ac_prg_fs( abitreader* huffr, huffTree* actree, short* block, int* eobrun, int from, int to )
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
			//	block[ bpos++ ] = 0;
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
int encode_ac_prg_fs( abitwriter* huffw, huffCodes* actbl, short* block, int* eobrun, int from, int to )
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
		if ( block[ bpos ] != 0 ) {
			// encode eobrun
			encode_eobrun( huffw, actbl, eobrun );
			// write remaining zeroes
			while ( z >= 16 ) {
				huffw->write( actbl->cval[ 0xF0 ], actbl->clen[ 0xF0 ] );
				z -= 16;
			}			
			// vli encode
			tmp = ABS( block[ bpos ] );
			BITLEN( s, tmp );
			n = ENVLI( s, block[ bpos ] );
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
int decode_ac_prg_sa( abitreader* huffr, huffTree* actree, short* block, int* eobrun, int from, int to )
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
int encode_ac_prg_sa( abitwriter* huffw, abytewriter* storw, huffCodes* actbl, short* block, int* eobrun, int from, int to )
{
	unsigned short n;
	unsigned char  s;
	unsigned char  z;
	int eob = from;
	int bpos;
	int hc;
	int tmp;
	
	// check if block contains any newly nonzero coefficients and find out position of eob
	for ( bpos = to; bpos >= from; bpos-- )	{
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
		// if zero is encountered
		if ( block[ bpos ] == 0 ) {
			z++; // increment zero counter
			if ( z == 16 ) { // write zeroes if needed
				huffw->write( actbl->cval[ 0xF0 ], actbl->clen[ 0xF0 ] );
				encode_crbits( huffw, storw );
				z = 0;
			}
		}
		// if nonzero is encountered
		else if ( ( block[ bpos ] == 1 ) || ( block[ bpos ] == -1 ) ) {
			// vli encode
			tmp = ABS( block[ bpos ] );
			BITLEN( s, tmp );
			n = ENVLI( s, block[ bpos ] );
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
int decode_eobrun_sa( abitreader* huffr, short* block, int* eobrun, int from, int to )
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
int encode_eobrun( abitwriter* huffw, huffCodes* actbl, int* eobrun )
{
	unsigned short n;
	unsigned char  s;
	int hc;
	
	
	if ( (*eobrun) > 0 ) {
		while ( (*eobrun) > actbl->max_eobrun ) {
			huffw->write( actbl->cval[ 0xE0 ], actbl->clen[ 0xE0 ] );
			huffw->write( E_ENVLI( 14, 32767 ), 14 );
			(*eobrun) -= actbl->max_eobrun;
		}
		BITLEN( s, (*eobrun) );
		s--;
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
	data = storw->peekptr();
	
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
	
	
	// increment all counts where needed
	if ( ( ++(*sub) ) >= cmpnfo[(*cmp)].mbs ) {
		(*sub) = 0;
		
		if ( ( ++(*csc) ) >= cs_cmpc ) {
			(*csc) = 0;
			(*cmp) = cs_cmp[ 0 ];
			(*mcu)++;
			if ( (*mcu) >= mcuc ) sta = 2;
			else if ( rsti > 0 )
				if ( --(*rstw) == 0 ) sta = 1;
		}
		else {
			(*cmp) = cs_cmp[(*csc)];
		}
	}
	
	// get correct position in image ( x & y )
	if ( cmpnfo[(*cmp)].sfh > 1 ) { // to fix mcu order
		(*dpos)  = ( (*mcu) / mcuh ) * cmpnfo[(*cmp)].sfh + ( (*sub) / cmpnfo[(*cmp)].sfv );
		(*dpos) *= cmpnfo[(*cmp)].bch;
		(*dpos) += ( (*mcu) % mcuh ) * cmpnfo[(*cmp)].sfv + ( (*sub) % cmpnfo[(*cmp)].sfv );
	}
	else if ( cmpnfo[(*cmp)].sfv > 1 ) {
		// simple calculation to speed up things if simple fixing is enough
		(*dpos) = ( (*mcu) * cmpnfo[(*cmp)].mbs ) + (*sub);
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
int skip_eobrun( int* cmp, int* dpos, int* rstw, int* eobrun )
{
	if ( (*eobrun) > 0 ) // error check for eobrun
	{		
		// compare rst wait counter if needed
		if ( rsti > 0 ) {
			if ( (*eobrun) > (*rstw) )
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
void build_huffcodes( unsigned char *clen, unsigned char *cval,	huffCodes *hc, huffTree *ht )
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
	for ( i = 0; i < 256; i++ )	{
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

/* ----------------------- Begin ofDCT specific functions -------------------------- */


/* -----------------------------------------------
	precalculate some values for FDCT/IDCT
	----------------------------------------------- */
bool prepare_dct( int nx, int ny, float* icos_idct_fst, float* icos_fdct_fst )
{
	float* icos_idct_nx;
	float* icos_idct_ny;
	float* icos_fdct_nx;
	float* icos_fdct_ny;
	
	int iu, iv;
	int iy, ix;
	int sx, sy;
	int su, sv;
	int ti;
	
	// alloc memory for precalculated tables
	icos_idct_nx = (float*) calloc( nx * nx, sizeof( float ) );
	icos_idct_ny = (float*) calloc( ny * ny, sizeof( float ) );
	icos_fdct_nx = (float*) calloc( nx * nx, sizeof( float ) );
	icos_fdct_ny = (float*) calloc( ny * ny, sizeof( float ) );	
	
	// check for out of memory
	if ( ( icos_idct_nx == NULL ) || ( icos_idct_ny == NULL ) ||
		 ( icos_fdct_nx == NULL ) || ( icos_fdct_ny == NULL ) ) 
	{
		return false;
	}
	
	// precalculate tables
	// idct / nx table
	for ( ix = 0; ix < nx; ix++ ) {
		for ( iu = 0; iu < nx; iu++ ) {
			icos_idct_nx [ iu + ( ix * nx ) ] = ( C_DCT ( iu ) * COS_DCT( ix, iu, nx ) ) / DCT_SCALE;
		}
	}
	
	// idct / ny table
	for ( iy = 0; iy < ny; iy++ ) {
		for ( iv = 0; iv < ny; iv++ ) {
			icos_idct_ny [ iv + ( iy * ny ) ] = ( C_DCT ( iv ) * COS_DCT( iy, iv, ny ) ) / DCT_SCALE;
		}
	}
	
	// fdct / nx table
	for ( iu = 0; iu < nx; iu++ ) {
		for ( ix = 0; ix < nx; ix++ ) {
			icos_fdct_nx [ ix + ( iu * nx ) ] = ( C_DCT ( iu ) * COS_DCT( ix, iu, nx ) * DCT_SCALE ) / nx;
		}
	}
	
	// fdct / ny table
	for ( iv = 0; iv < ny; iv++ ) {
		for ( iy = 0; iy < ny; iy++ ) {
			icos_fdct_ny [ iy + ( iv * ny ) ] = ( C_DCT ( iv ) * COS_DCT( iy, iv, ny ) * DCT_SCALE ) / ny;
		}
	}
	
	// precalculation of fast DCT tables...	
	// idct / fast table
	ti = 0;
	for ( iy = 0; iy < ny; iy++ ) {
		for ( ix = 0; ix < nx; ix++ ) {
			sx = ( ix * nx );
			sy = ( iy * ny );
			for ( iv = 0; iv < ny; iv++ ) {
				for ( iu = 0; iu < nx; iu++ ) {
					icos_idct_fst[ ti++ ] = icos_idct_nx[ sx + iu ] * icos_idct_ny[ sy + iv ]; 
				}
			}
		}
	}
	
	// fdct / fast table
	ti = 0;
	for ( iv = 0; iv < ny; iv++ ) {
		for ( iu = 0; iu < nx; iu++ ) {
			su = ( iu * nx );
			sv = ( iv * ny );
			for ( iy = 0; iy < ny; iy++ ) {
				for ( ix = 0; ix < nx; ix++ ) {
					icos_fdct_fst[ ti++ ] = icos_fdct_nx[ su + ix ] * icos_fdct_ny[ sv + iy ]; 
				}
			}
		}
	}
	
	//free helper tables
	free( icos_idct_nx );
	free( icos_idct_ny );
	free( icos_fdct_nx );
	free( icos_fdct_ny );
	
	
	return true;
}


/* -----------------------------------------------
	precalculate base elements of the DCT
	----------------------------------------------- */
bool prepare_dct_base( int nx, int ny, float* dct_base )
{
	int ix, iy;
	int i = 0;
	
	for ( iy = 0; iy < ny; iy++ )
		for ( ix = 0; ix < nx; ix++ )
			dct_base[ i++ ] = ( C_DCT ( iy ) * COS_DCT( ix, iy, nx ) );
		
	return true;
}


/* -----------------------------------------------
	inverse DCT transform using precalc tables (fast)
	----------------------------------------------- */
float idct_2d_fst_8x8( signed short* F, int ix, int iy )
{
	float idct;
	int ixy;
	int i;
	
	
	// calculate start index
	ixy = ( ( iy * 8 ) + ix ) * 64;
	
	// begin transform
	idct = 0;
	for ( i = 0; i < 64; i++ )
		// idct += F[ i ] * icos_idct_fst[ ixy + i ];
		idct += F[ i ] * icos_idct_8x8[ ixy++ ];
	
	
	return idct;
}


/* -----------------------------------------------
	forward DCT transform using precalc tables (fast)
	----------------------------------------------- */
float fdct_2d_fst_8x8( unsigned char* f, int iu, int iv )
{
	float fdct;
	int iuv;
	int i;
	
	
	// calculate start index
	iuv = ( ( iv * 8 ) + iu ) * 64;
	
	// begin transform
	fdct = 0;
	for ( i = 0; i < 64; i++ )
		// fdct += f[ i ] * icos_fdct_fst[ iuv + i ];
		fdct += f[ i ] * icos_fdct_8x8[ iuv++ ];
	
	
	return fdct;
}


/* -----------------------------------------------
	forward DCT transform using precalc tables (fast)
	----------------------------------------------- */
float fdct_2d_fst_8x8( float* f, int iu, int iv )
{
	float fdct;
	int iuv;
	int i;
	
	
	// calculate start index
	iuv = ( ( iv * 8 ) + iu ) * 64;
	
	// begin transform
	fdct = 0;
	for ( i = 0; i < 64; i++ )
		// fdct += f[ i ] * icos_fdct_fst[ iuv + i ];
		fdct += f[ i ] * icos_fdct_8x8[ iuv++ ];
	
	
	return fdct;
}


/* -----------------------------------------------
	inverse DCT transform using precalc tables (fast)
	----------------------------------------------- */
float idct_1d_fst_8( signed short* F, int ix )
{
	float idct;
	int i;
	
	
	// calculate start index
	ix *= 8;
	
	// begin transform
	idct = 0;
	for ( i = 0; i < 8; i++ )
		// idct += F[ i ] * icos_idct_fst[ ix + i ];
		idct += F[ i ] * icos_idct_1x8[ ix++ ];
	
	
	return idct;
}


/* -----------------------------------------------
	forward DCT transform using precalc tables (fast)
	----------------------------------------------- */
float fdct_1d_fst_8( unsigned char* f, int iu )
{
	float fdct;
	int i;
	
	
	// calculate start index
	iu *= 8;
	
	// begin transform
	fdct = 0;
	for ( i = 0; i < 8; i++ )
		// fdct += f[ i ] * icos_fdct_fst[ iu + i ];
		fdct += f[ i ] * icos_fdct_1x8[ iu++ ];
	
	
	return fdct;
}


/* -----------------------------------------------
	forward DCT transform using precalc tables (fast)
	----------------------------------------------- */
float fdct_1d_fst_8( float* f, int iu )
{
	float fdct;
	int i;
	
	
	// calculate start index
	iu *= 8;
	
	// begin transform
	fdct = 0;
	for ( i = 0; i < 8; i++ )
		// fdct += f[ i ] * icos_fdct_fst[ iu + i ];
		fdct += f[ i ] * icos_fdct_1x8[ iu++ ];
	
	
	return fdct;
}


/* -----------------------------------------------
	inverse DCT transform using precalc tables (fast)
	----------------------------------------------- */
float idct_2d_fst_8x8( int cmp, int dpos, int ix, int iy )
{
	float idct = 0;
	int ixy;
	
	
	// calculate start index
	ixy = ( ( iy * 8 ) + ix ) * 64;
	
	// begin transform
	idct += colldata[ cmp ][  0 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 0 ];
	idct += colldata[ cmp ][  1 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 1 ];
	idct += colldata[ cmp ][  5 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 2 ];
	idct += colldata[ cmp ][  6 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 3 ];
	idct += colldata[ cmp ][ 14 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 4 ];
	idct += colldata[ cmp ][ 15 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 5 ];
	idct += colldata[ cmp ][ 27 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 6 ];
	idct += colldata[ cmp ][ 28 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 7 ];
	idct += colldata[ cmp ][  2 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 8 ];
	idct += colldata[ cmp ][  4 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 9 ];
	idct += colldata[ cmp ][  7 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 10 ];
	idct += colldata[ cmp ][ 13 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 11 ];
	idct += colldata[ cmp ][ 16 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 12 ];
	idct += colldata[ cmp ][ 26 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 13 ];
	idct += colldata[ cmp ][ 29 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 14 ];
	idct += colldata[ cmp ][ 42 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 15 ];
	idct += colldata[ cmp ][  3 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 16 ];
	idct += colldata[ cmp ][  8 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 17 ];
	idct += colldata[ cmp ][ 12 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 18 ];
	idct += colldata[ cmp ][ 17 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 19 ];
	idct += colldata[ cmp ][ 25 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 20 ];
	idct += colldata[ cmp ][ 30 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 21 ];
	idct += colldata[ cmp ][ 41 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 22 ];
	idct += colldata[ cmp ][ 43 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 23 ];
	idct += colldata[ cmp ][  9 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 24 ];
	idct += colldata[ cmp ][ 11 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 25 ];
	idct += colldata[ cmp ][ 18 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 26 ];
	idct += colldata[ cmp ][ 24 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 27 ];
	idct += colldata[ cmp ][ 31 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 28 ];
	idct += colldata[ cmp ][ 40 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 29 ];
	idct += colldata[ cmp ][ 44 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 30 ];
	idct += colldata[ cmp ][ 53 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 31 ];
	idct += colldata[ cmp ][ 10 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 32 ];
	idct += colldata[ cmp ][ 19 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 33 ];
	idct += colldata[ cmp ][ 23 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 34 ];
	idct += colldata[ cmp ][ 32 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 35 ];
	idct += colldata[ cmp ][ 39 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 36 ];
	idct += colldata[ cmp ][ 45 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 37 ];
	idct += colldata[ cmp ][ 52 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 38 ];
	idct += colldata[ cmp ][ 54 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 39 ];
	idct += colldata[ cmp ][ 20 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 40 ];
	idct += colldata[ cmp ][ 22 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 41 ];
	idct += colldata[ cmp ][ 33 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 42 ];
	idct += colldata[ cmp ][ 38 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 43 ];
	idct += colldata[ cmp ][ 46 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 44 ];
	idct += colldata[ cmp ][ 51 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 45 ];
	idct += colldata[ cmp ][ 55 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 46 ];
	idct += colldata[ cmp ][ 60 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 47 ];
	idct += colldata[ cmp ][ 21 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 48 ];
	idct += colldata[ cmp ][ 34 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 49 ];
	idct += colldata[ cmp ][ 37 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 50 ];
	idct += colldata[ cmp ][ 47 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 51 ];
	idct += colldata[ cmp ][ 50 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 52 ];
	idct += colldata[ cmp ][ 56 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 53 ];
	idct += colldata[ cmp ][ 59 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 54 ];
	idct += colldata[ cmp ][ 61 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 55 ];
	idct += colldata[ cmp ][ 35 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 56 ];
	idct += colldata[ cmp ][ 36 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 57 ];
	idct += colldata[ cmp ][ 48 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 58 ];
	idct += colldata[ cmp ][ 49 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 59 ];
	idct += colldata[ cmp ][ 57 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 60 ];
	idct += colldata[ cmp ][ 58 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 61 ];
	idct += colldata[ cmp ][ 62 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 62 ];
	idct += colldata[ cmp ][ 63 ][ dpos ] * adpt_idct_8x8[ cmp ][ ixy + 63 ];
	
	
	return idct;
}


/* -----------------------------------------------
	inverse DCT transform using precalc tables (fast)
	----------------------------------------------- */
float idct_2d_fst_8x1( int cmp, int dpos, int ix, int iy )
{
	float idct = 0;
	int ixy;
	
	
	// calculate start index
	ixy = ix * 8;
	
	// begin transform
	idct += colldata[ cmp ][  0 ][ dpos ] * adpt_idct_8x1[ cmp ][ ixy + 0 ];
	idct += colldata[ cmp ][  1 ][ dpos ] * adpt_idct_8x1[ cmp ][ ixy + 1 ];
	idct += colldata[ cmp ][  5 ][ dpos ] * adpt_idct_8x1[ cmp ][ ixy + 2 ];
	idct += colldata[ cmp ][  6 ][ dpos ] * adpt_idct_8x1[ cmp ][ ixy + 3 ];
	idct += colldata[ cmp ][ 14 ][ dpos ] * adpt_idct_8x1[ cmp ][ ixy + 4 ];
	idct += colldata[ cmp ][ 15 ][ dpos ] * adpt_idct_8x1[ cmp ][ ixy + 5 ];
	idct += colldata[ cmp ][ 27 ][ dpos ] * adpt_idct_8x1[ cmp ][ ixy + 6 ];
	idct += colldata[ cmp ][ 28 ][ dpos ] * adpt_idct_8x1[ cmp ][ ixy + 7 ];
	
	
	return idct;
}


/* -----------------------------------------------
	inverse DCT transform using precalc tables (fast)
	----------------------------------------------- */
float idct_2d_fst_1x8( int cmp, int dpos, int ix, int iy )
{
	float idct = 0;
	int ixy;
	
	
	// calculate start index
	ixy = iy * 8;
	
	// begin transform
	idct += colldata[ cmp ][  0 ][ dpos ] * adpt_idct_1x8[ cmp ][ ixy + 0 ];
	idct += colldata[ cmp ][  2 ][ dpos ] * adpt_idct_1x8[ cmp ][ ixy + 1 ];
	idct += colldata[ cmp ][  3 ][ dpos ] * adpt_idct_1x8[ cmp ][ ixy + 2 ];
	idct += colldata[ cmp ][  9 ][ dpos ] * adpt_idct_1x8[ cmp ][ ixy + 3 ];
	idct += colldata[ cmp ][ 10 ][ dpos ] * adpt_idct_1x8[ cmp ][ ixy + 4 ];
	idct += colldata[ cmp ][ 20 ][ dpos ] * adpt_idct_1x8[ cmp ][ ixy + 5 ];
	idct += colldata[ cmp ][ 21 ][ dpos ] * adpt_idct_1x8[ cmp ][ ixy + 6 ];
	idct += colldata[ cmp ][ 35 ][ dpos ] * adpt_idct_1x8[ cmp ][ ixy + 7 ];
	
	
	return idct;
}


/* ----------------------- End of DCT specific functions -------------------------- */

/* ----------------------- Begin of miscellaneous helper functions -------------------------- */


/* -----------------------------------------------
	creates filename, callocs memory for it
	----------------------------------------------- */	
char* create_filename( char* base, char* extension )
{
	int len = strlen(base);
	int tol = 8;
	char* filename = (char*) calloc( len + tol, sizeof( char ) );
	
	set_extension( filename, base, extension );
	
	return filename;
}

/* -----------------------------------------------
	changes extension of filename
	----------------------------------------------- */	
void set_extension( char* destination, char* origin, char* extension )
{
	int i;
	
	int dotpos = 0;
	int length = strlen( origin );

	// find position of dot in filename
	for ( i = 0; i < length; i++ ) {
		if ( origin[i] == '.' ) {
			dotpos = i;
		}
	}
	
	if ( !dotpos ){
		dotpos = length;
	}
	
	strncpy( destination, origin, dotpos );
	destination[ dotpos ] = '.';
	strcpy( destination + dotpos + 1, extension );
}

/* -----------------------------------------------
	adds underscore after filename
	----------------------------------------------- */	
void add_underscore( char* filename )
{
	int i;
	
	int dotpos = 0;
	int length = strlen( filename );
	char* tmpname;
	
	// copy filename to tmpname
	tmpname = new char[ length + 1 ];
	strcpy( tmpname, filename );

	// find position of dot in filename
	for ( i = 0; i < length; i++ ) {
		if ( filename[ i ] == '.' ) {
			dotpos = i;
		}
	}
	
	// add underscore before extension
	if ( !dotpos ) {
		sprintf( filename, "%s_", tmpname );
	}
	else {
		strncpy( filename, tmpname, dotpos );
		sprintf( filename + dotpos, "_%s", tmpname + dotpos );
	}
}

/* ----------------------- End of miscellaneous helper functions -------------------------- */

/* ----------------------- Begin of developers functions -------------------------- */


/* -----------------------------------------------
	Writes header file
	----------------------------------------------- */
bool write_hdr( void )
{
	char* ext = "hdr";
	char* basename = basfilename;
	
	if ( !write_file( basename, ext, hdrdata, 1, hdrs ) )
		return false;	
	
	return true;
}


/* -----------------------------------------------
	Writes huffman coded file
	----------------------------------------------- */
bool write_huf( void )
{
	char* ext = "huf";
	char* basename = basfilename;
	
	if ( !write_file( basename, ext, huffdata, 1, hufs ) )
		return false;
	
	return true;
}


/* -----------------------------------------------
	Writes collections of DCT coefficients
	----------------------------------------------- */
bool write_coll( void )
{
	FILE* fp;
	
	char* fn;
	char* ext[4];
	char* base;
	int cmp, bpos, dpos;
	int i, j;
	
	ext[0] = "coll0";
	ext[1] = "coll1";
	ext[2] = "coll2";
	ext[3] = "coll3";
	base = basfilename;
	
	
	for ( cmp = 0; cmp < cmpc; cmp++ ) {
		
		// create filename
		fn = create_filename( base, ext[ cmp ] );
		
		// open file for output
		fp = fopen( fn, "wb" );
		if ( fp == NULL ){
			sprintf( errormessage, FWR_ERRMSG, fn);
			errorlevel = 2;
			return false;
		}
		free( fn );
		
		switch ( collmode ) {
			
			case 0: // standard collections
				for ( bpos = 0; bpos < 64; bpos++ )
					fwrite( colldata[cmp][bpos], sizeof( short ), cmpnfo[cmp].bc, fp );
				break;
				
			case 1: // sequential order collections, 'dhufs'
				for ( dpos = 0; dpos < cmpnfo[cmp].bc; dpos++ )
				for ( bpos = 0; bpos < 64; bpos++ )
					fwrite( &(colldata[cmp][bpos][dpos]), sizeof( short ), 1, fp );
				break;
				
			case 2: // square collections
				dpos = 0;
				for ( i = 0; i < 64; ) {
					bpos = zigzag[ i++ ];
					fwrite( &(colldata[cmp][bpos][dpos]), sizeof( short ),
						cmpnfo[cmp].bch, fp );
					if ( ( i % 8 ) == 0 ) {
						dpos += cmpnfo[cmp].bch;
						if ( dpos >= cmpnfo[cmp].bc ) {
							dpos = 0;
						}
						else {
							i -= 8;
						}
					}
				}
				break;
			
			case 3: // uncollections
				for ( i = 0; i < ( cmpnfo[cmp].bcv * 8 ); i++ )			
				for ( j = 0; j < ( cmpnfo[cmp].bch * 8 ); j++ ) {
					bpos = zigzag[ ( ( i % 8 ) * 8 ) + ( j % 8 ) ];
					dpos = ( ( i / 8 ) * cmpnfo[cmp].bch ) + ( j / 8 );
					fwrite( &(colldata[cmp][bpos][dpos]), sizeof( short ), 1, fp );
				}
				break;
		}
		
		fclose( fp );
	}
	
	return true;
}


/* -----------------------------------------------
	Writes to file
	----------------------------------------------- */
bool write_file( char* base, char* ext, void* data, int bpv, int size )
{	
	FILE* fp;
	char* fn;
	
	// create filename
	fn = create_filename( base, ext );
	
	// open file for output
	fp = fopen( fn, "wb" );	
	if ( fp == NULL ) {
		sprintf( errormessage, FWR_ERRMSG, fn);
		errorlevel = 2;
		return false;
	}
	free( fn );
	
	// write & close
	fwrite( data, bpv, size, fp );
	fclose( fp );
	
	return true;
}


/* -----------------------------------------------
	Writes error info file
	----------------------------------------------- */
bool write_errfile( void )
{
	FILE* fp;
	char* fn;
	
	
	// return immediately if theres no error
	if ( errorlevel == 0 ) return true;
	
	// create filename based on errorlevel
	if ( errorlevel == 1 ) {
		fn = create_filename( basfilename, "wrn.nfo" );
	}
	else {
		fn = create_filename( basfilename, "err.nfo" );
	}
	
	// open file for output
	fp = fopen( fn, "w" );
	if ( fp == NULL ){
		sprintf( errormessage, FWR_ERRMSG, fn);
		errorlevel = 2;
		return false;
	}
	free( fn );
	
	// write status and errormessage to file
	fprintf( fp, "--> error (level %i) in file \"%s\" <--\n", errorlevel, basfilename );
	fprintf( fp, "\n" );
	// write error specification to file
	get_status( errorfunction );
	fprintf( fp, " %s -> %s:\n", statusmessage,
			( errorlevel == 1 ) ? "warning" : "error" );
	fprintf( fp, " %s\n", errormessage );
	
	// done, close file
	fclose( fp );
	
	
	return true;
}


/* -----------------------------------------------
	Writes info to textfile
	----------------------------------------------- */
bool write_info( void )
{	
	FILE* fp;
	char* fn;
	
	unsigned char  type = 0x00; // type of current marker segment
	unsigned int   len  = 0; // length of current marker segment
	unsigned int   hpos = 0; // position in header		
	
	int cmp, bpos;
	int i;
	
	
	// create filename
	fn = create_filename( basfilename, "nfo" );
	
	// open file for output
	fp = fopen( fn, "w" );
	if ( fp == NULL ){
		sprintf( errormessage, FWR_ERRMSG, fn);
		errorlevel = 2;
		return false;
	}
	free( fn );

	// info about image
	fprintf( fp, "<Infofile for JPEG image %s>\n\n\n", jpgfilename );
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

/* -----------------------------------------------
	Do inverse DCT and write pgms
	----------------------------------------------- */
bool write_pgm( void )
{	
	unsigned char* imgdata;
	
	FILE* fp;
	char* fn;
	char* ext[4];
	
	int cmp, dpos;
	int pix_v;
	int xpos, ypos, dcpos;
	int x, y;
	
	
	ext[0] = "cmp0.pgm";
	ext[1] = "cmp1.pgm";
	ext[2] = "cmp2.pgm";
	ext[3] = "cmp3.pgm";
	
	
	for ( cmp = 0; cmp < cmpc; cmp++ )
	{
		// create filename
		fn = create_filename( basfilename, ext[ cmp ] );
		
		// open file for output
		fp = fopen( fn, "wb" );		
		if ( fp == NULL ){
			sprintf( errormessage, FWR_ERRMSG, fn );
			errorlevel = 2;
			return false;
		}
		free( fn );
		
		// alloc memory for image data
		imgdata = (unsigned char*) calloc ( cmpnfo[cmp].bc * 64, sizeof( char ) );
		if ( imgdata == NULL ) {
			sprintf( errormessage, MEM_ERRMSG );
			errorlevel = 2;
			return false;
		}
		
		for ( dpos = 0; dpos < cmpnfo[cmp].bc; dpos++ )	{	
			// do inverse DCT, store in imgdata
			dcpos  = ( ( ( dpos / cmpnfo[cmp].bch ) * cmpnfo[cmp].bch ) << 6 ) +
					   ( ( dpos % cmpnfo[cmp].bch ) << 3 );
			for ( y = 0; y < 8; y++ ) {
				ypos = dcpos + ( y * ( cmpnfo[cmp].bch << 3 ) );
				for ( x = 0; x < 8; x++ ) {
					xpos = ypos + x;
					pix_v = ROUND_F( idct_2d_fst_8x8( cmp, dpos, x, y ) + 128 );
					imgdata[ xpos ] = ( unsigned char ) CLAMPED( 0, 255, pix_v );
				}
			}			
		}
		
		// write PGM header
		fprintf( fp, "P5\n" );
		fprintf( fp, "# created by %s v%i.%i%s (%s) by %s\n",
			appname, ujgversion / 10, ujgversion % 10, subversion, versiondate, author );
		fprintf( fp, "%i %i\n", cmpnfo[cmp].bch * 8, cmpnfo[cmp].bcv * 8 );
		fprintf( fp, "255\n" );
		
		// write image data
		fwrite( imgdata, sizeof( char ), cmpnfo[cmp].bc * 64, fp );
		
		// free memory
		free( imgdata );
		
		// close file
		fclose( fp );
	}
	
	return true;
}

/* ----------------------- End of developers functions -------------------------- */

/* ----------------------- End of file -------------------------- */
