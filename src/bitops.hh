#define RBITS( c, n )		( c & ( 0xFF >> (8 - n) ) )
#define LBITS( c, n )		( c >> (8 - n) )
#define MBITS( c, l, r )	( RBITS( c,l ) >> r )
#define RBITS16( c, n )		( c & ( 0xFFFFFFFF >> (16 - n) ) )
#define LBITS16( c, n )		( c >> (16 - n) )
#define MBITS16( c, l, r )	( RBITS16( c,l ) >> r )
#define RBITS32( c, n )		( c & ( 0xFFFFFFFF >> (32 - n) ) )
#define LBITS32( c, n )		( c >> (32 - n) )
#define MBITS32( c, l, r )	( RBITS32( c,l ) >> r )
#define BITN( c, n )		( (c >> n) & 0x1 )
#define BITLEN( l, v )		for ( l = 0; ( v >> l ) > 0; l++ )
#define FDIV2( v, p )		( ( v < 0 ) ? -( (-v) >> p ) : ( v >> p ) )

#define BTST_BUFF			1024 * 1024

#include <stdio.h>

/* -----------------------------------------------
	class to read arrays bitwise
	----------------------------------------------- */

class abitreader
{
public:
	abitreader( unsigned char* array, int size );
	~abitreader( void );	
	unsigned int read( int nbits );
	unsigned char unpad( unsigned char fillbit );
	int getpos( void );	
	bool eof;
	
private:
	unsigned char* data;
	int lbyte;
	int cbyte;
	int cbit;
};


/* -----------------------------------------------
	class to write arrays bitwise
	----------------------------------------------- */

class abitwriter
{
public:
	abitwriter( int size );
	~abitwriter( void );	
	void write( unsigned int val, int nbits );
	void pad ( unsigned char fillbit );
	unsigned char* getptr( void );
	const unsigned char* peekptr( void )const;
	int getpos( void );
    bool no_remainder() const {
        return cbit == 8;
    }
	bool error;	
	unsigned char fillbit;
	
private:
	unsigned char* data;
	int dsize;
	int adds;
	int lbyte;
	int cbyte;
	int cbit;
	bool fmem;
};


/* -----------------------------------------------
	class to write arrays bytewise
	----------------------------------------------- */

class abytewriter
{
public:
	abytewriter( int size );
	~abytewriter( void );	
	void write( unsigned char byte );
	void write_n( unsigned char* byte, int n );
	unsigned char* getptr( void );
	unsigned char* peekptr( void );
	int getpos( void );
	void reset( void );
	bool error;	
	
private:
	unsigned char* data;
	int dsize;
	int adds;
	int lbyte;
	int cbyte;
	bool fmem;
};


/* -----------------------------------------------
	class to read arrays bytewise
	----------------------------------------------- */

class abytereader
{
public:
	abytereader( unsigned char* array, int size );
	~abytereader( void );	
	int read( unsigned char* byte );
	int read_n( unsigned char* byte, int n );
	void seek( int pos );
	int getsize( void );
	int getpos( void );
	bool eof;	
	
private:
	unsigned char* data;
	int lbyte;
	int cbyte;
};


/* -----------------------------------------------
	class for input and output from file or memory
	----------------------------------------------- */

class iostream
{
public:
	iostream( void* src, int srctype, int srcsize, int iomode );
	~iostream( void );
	void switch_mode( void );
	int read( void* to, int tpsize, int dtsize );
	int write( const void* from, int tpsize, int dtsize );
	int flush( void );
	int rewind( void );
	int getpos( void );
	int getsize( void );
	unsigned char* getptr( void );
	bool chkerr( void );
	bool chkeof( void );
	
private:
	void open_file( void );
	void open_mem( void );
	void open_stream( void );
	
	int write_file( const void* from, int tpsize, int dtsize );
	int read_file( void* to, int tpsize, int dtsize );
	int write_mem( const void* from, int tpsize, int dtsize );
	int read_mem( void* to, int tpsize, int dtsize );
	
	FILE* fptr;
	abytewriter* mwrt;
	abytereader* mrdr;
	
	bool free_mem_sw;
	void* source;
	int mode;
	int srct;
	int srcs;
};
