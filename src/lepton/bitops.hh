/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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
#define FDIV2( v, p )		( ( v < 0 ) ? -( (-v) >> p ) : ( v >> p ) )

#define BTST_BUFF			1024 * 1024

#include <stdio.h>
#include <functional>
#include "../io/Reader.hh"
#include "../io/ioutil.hh"
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

class ibytestream {
    Sirikata::DecoderReader* parent;
    unsigned int bytes_read;
public:
	unsigned char get_last_read();
	unsigned char get_penultimate_read();
    ibytestream(Sirikata::DecoderReader *p,
                unsigned int starting_byte_offset,
                const Sirikata::JpegAllocator<uint8_t> &alloc);
    unsigned int getsize();
    bool read_byte(unsigned char *output);
    unsigned int read(unsigned char *output, unsigned int size);
    // the biggest allowed huffman code (that may get damaged by truncation)
    unsigned char last_read[2];
};


class bounded_iostream
{
    Sirikata::DecoderWriter *parent;
    unsigned int byte_bound;
    unsigned int byte_position;
    Sirikata::JpegError err;
    std::function<void(Sirikata::DecoderWriter*, size_t)> size_callback;
public:
	bounded_iostream( Sirikata::DecoderWriter * parent,
                      const std::function<void(Sirikata::DecoderWriter*, size_t)> &size_callback,
                      const Sirikata::JpegAllocator<uint8_t> &alloc);
	~bounded_iostream( void );
    void call_size_callback(size_t size);
    bool chkerr();
    unsigned int getsize();
    void set_bound(size_t bound); // bound of zero = fine
	unsigned int write( const void* from, unsigned int tpsize, unsigned int dtsize );
    void close();
};
