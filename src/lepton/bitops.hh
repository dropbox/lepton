/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <assert.h>
#include <cstring>
#define RBITS( c, n )		( c & ( 0xFF >> (8 - n) ) )
#define LBITS( c, n )		( c >> (8 - n) )
#define MBITS( c, l, r )	( RBITS( c,l ) >> r )
#define RBITS16( c, n )		( c & ( 0xFFFFFFFF >> (16 - n) ) )
#define LBITS16( c, n )		( c >> (16 - n) )
#define MBITS16( c, l, r )	( RBITS16( c,l ) >> r )
#define RBITS32( c, n )		( c & ( 0xFFFFFFFF >> (32 - n) ) )
#define LBITS32( c, n )		( c >> (32 - n) )
#define MBITS32( c, l, r )	( RBITS32( c,l ) >> r )

#define RBITS64( c, n )		( c & ( 0xFFFFFFFFFFFFFFFFULL >> (64 - n) ) )
#define LBITS64( c, n )		( c >> (64 - n) )
#define MBITS64( c, l, r )	( RBITS64( c,l ) >> r )

#define BITN( c, n )		( (c >> n) & 0x1 )
#define FDIV2( v, p )		( ( v < 0 ) ? -( (-v) >> p ) : ( v >> p ) )

#define BTST_BUFF			1024 * 1024

#include <stdio.h>
#include <functional>
#include "../io/Reader.hh"
#include "../io/ioutil.hh"
#include "../vp8/util/vpx_config.hh"
/* -----------------------------------------------
	class to read arrays bitwise
	----------------------------------------------- */
void compute_md5(const char * filename, unsigned char *result);
class abitreader
{
public:
	abitreader( unsigned char* array, int size );
	~abitreader( void );	
	unsigned int read( int nbits );
	unsigned char unpad( unsigned char fillbit );
	int getpos( void );	
	bool legacy_eof;
    bool eof;
private:
	unsigned char* data;
    unsigned char* data2;
    int cbyte2;
    int cbit2;
    uint64_t buf;
	int lbyte;
	int cbyte;
	int cbit;
};


/* -----------------------------------------------
	class to write arrays bitwise
	----------------------------------------------- */

class abitwriter
{
    unsigned char* data2;
    uint64_t buf;
    int dsize;
    int adds;
    int cbyte2;
    int cbit2;
    bool fmem;
public:
	abitwriter( int size );
	~abitwriter( void );
    
    void flush_no_pad() {
        assert(((64 - cbit2) & 7) == 0);
        buf = htobe64(buf);
        uint32_t bytes_to_write = (64 - cbit2) / 8;
        memcpy(data2 + cbyte2, &buf, bytes_to_write);
        cbyte2 += bytes_to_write;
        buf = 0;
        //assert(cbyte +1 == cbyte2 || cbyte == cbyte2 || cbyte == cbyte2 + 1 || cbyte == cbyte2 + 2 || cbyte == cbyte2 + 3);
        //assert(memcmp(data2, data, cbyte2) == 0);
        
        cbit2 = 64;
    }
    /* -----------------------------------------------
     writes n bits to abitwriter
     ----------------------------------------------- */
    
    void write( unsigned int val, int nbits )
    {

        int nbits2 = nbits;
        unsigned int val2 = val;
        assert(nbits <= 64);
        if ( __builtin_expect(cbyte2 > ( dsize - 16 ), false) ) {
            if (adds < 4096 * 1024) {
                adds <<= 1;
            }
            int new_size = dsize + adds;
            unsigned char * tmp = (unsigned char*)custom_malloc(new_size);
            if ( tmp == NULL ) {
                error = true;
                custom_exit(1);
                return;
            }
            memset(tmp + dsize, 0, adds);
            memcpy(tmp, data2, dsize);
            custom_free(data2);
            data2 = tmp;
            dsize = new_size;
        }

        // write data
        if ( nbits2 >= cbit2 ) {
            /*
            uint64_t tmp = val2;
            uint64_t mask = 1;
            mask <<= nbits2 - cbit2;
            mask -=1;
            tmp &= mask;
            buf <<= nbits2;
            buf |= tmp;
             */
            buf |= MBITS64(val2, nbits2, (nbits2-cbit2));
            nbits2 -= cbit2;
            cbit2 = 0;
            flush_no_pad();
        }
        if ( nbits2 > 0 ) {
            uint64_t tmp = (RBITS64(val2, nbits2));
            tmp <<= cbit2 - nbits2;
            buf |= tmp;
            cbit2 -= nbits2;
        }

        /*
        uint64_t to_print = htobe64(buf);
        to_print >>= (cbit2+nbitsbak)/8*8;
        to_print &= 255;
        fprintf(stderr, "%x & %d =>\n", val2, nbitsbak);
        for (int i = 0; i <= cbyte;++i) {
            fprintf(stderr, "%x", (int)data[i]);
        }
        fprintf(stderr, "\n");
        for (int i = 0; i <= cbyte2;++i) {
            fprintf(stderr, "%x", (int)data2[i]);
        }
        fprintf(stderr, "%07llx", buf);
        fprintf(stderr,"\n");
         */


    }
    void pad ( unsigned char fillbit ) {
        while ( cbit2 & 7 ) {
            write( fillbit, 1 );
        }
        flush_no_pad();
    }
    unsigned char* getptr( void ) {
        // data is padded here
        pad( fillbit );
        flush_no_pad();
        // forbid freeing memory
        fmem = false;
        // realloc data
        return data2;
    }
    const unsigned char* peekptr( void ) {
        flush_no_pad();
        return data2;
    }
    int getpos( void ) {
        return cbyte2;
    }
    bool no_remainder() const {
        return cbit2 == 64;
    }
	bool error;	
	unsigned char fillbit;
	
};


/* -----------------------------------------------
	class to write arrays bytewise
	----------------------------------------------- */
extern void aligned_dealloc(unsigned char*);
extern unsigned char * aligned_alloc(size_t);

class abytewriter
{
public:
	abytewriter( int size );
	~abytewriter( void );	
	void write( unsigned char byte );
	void write_n( unsigned char* byte, int n );
	unsigned char* getptr_aligned( void );
	unsigned char* peekptr_aligned( void );
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
    enum {
        buffer_size = 65536
    };
    uint8_t buffer[buffer_size];
    uint32_t buffer_position;
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
    unsigned int write_no_buffer( const void* from, size_t bytes_to_write );
    unsigned int write_byte(uint8_t byte) {
        assert(buffer_position < buffer_size && "Full buffer wasn't flushed");
        buffer[buffer_position++] = byte;
        if (__builtin_expect(buffer_position == buffer_size, 0)) {
            buffer_position = 0;
            write_no_buffer(buffer, buffer_size);
        }
        return 1;
    }
    unsigned int write(const void *from, unsigned int nbytes) {
        size_t bytes_to_write = nbytes;
        if (__builtin_expect(nbytes + buffer_position > buffer_size, 0)) {
            if (buffer_position) {
                write_no_buffer(buffer, buffer_position);
                buffer_position = 0;
            }
            if (bytes_to_write < 64) {
                memcpy(buffer + buffer_position, from, bytes_to_write);
                buffer_position += bytes_to_write;
            } else {
                return write_no_buffer(from, bytes_to_write);
            }
        } else {
            memcpy(buffer + buffer_position, from, bytes_to_write);
            buffer_position += bytes_to_write;
            if (__builtin_expect(buffer_position == buffer_size, 0)) {
                 buffer_position = 0;
                write_no_buffer(buffer, buffer_size);
            }
        }
        return bytes_to_write;
    }
    void flush();
    void close();
};
