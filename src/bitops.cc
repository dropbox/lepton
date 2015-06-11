/*
This file contains special classes for bitwise
reading and writing of arrays
*/

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "bitops.hh"

#define BUFFER_SIZE 1024 * 1024


/* -----------------------------------------------
	constructor for abitreader class
	----------------------------------------------- */	

abitreader::abitreader( unsigned char* array, int size )
{
	cbyte = 0;	
	cbit = 8;
	eof = false;
	
	data = array;
	lbyte = size;	
}

/* -----------------------------------------------
	destructor for abitreader class
	----------------------------------------------- */	

abitreader::~abitreader( void )
{
}

/* -----------------------------------------------
	reads n bits from abitreader
	----------------------------------------------- */	

unsigned int abitreader::read( int nbits )
{
	unsigned int retval = 0;
	
	// safety check for eof
	if (eof) return 0;
	
	while ( nbits >= cbit ) {
		nbits -= cbit;
		retval |= ( RBITS( data[cbyte], cbit ) << nbits );		
		cbit = 8;
		if ( ++cbyte >= lbyte ) {
			eof = true;
			return retval;
		}
	}
	
	if ( nbits > 0 ) {		
		retval |= ( MBITS( data[cbyte], cbit, (cbit-nbits) ) );
		cbit -= nbits;		
	}
	
	return retval;
}

/* -----------------------------------------------
	to skip padding from current byte
	----------------------------------------------- */

unsigned char abitreader::unpad( unsigned char fillbit )
{
	if ( ( cbit == 8 ) || eof ) return fillbit;
	else {
		fillbit = read( 1 );
		while ( cbit != 8 ) read( 1 );
	}
	
	return fillbit;
}

/* -----------------------------------------------
	get current position in array
	----------------------------------------------- */	

int abitreader::getpos( void )
{
	return cbyte;
}


/* -----------------------------------------------
	constructor for abitwriter class
	----------------------------------------------- */	

abitwriter::abitwriter( int size )
{
	fillbit = 1;
	adds    = 65536;
	cbyte   = 0;
	cbit    = 8;
	
	error = false;
	fmem  = true;
	
	dsize = ( size > 0 ) ? size : adds;
	data = ( unsigned char* ) malloc ( dsize );
	if ( data == NULL ) {
		error = true;
		return;
	}
	
	// fill buffer with zeroes
	memset( data, 0, dsize * sizeof( char ) );
	// for ( int i = 0; i < dsize; i++ ) data[i] = 0;
}

/* -----------------------------------------------
	destructor for abitwriter class
	----------------------------------------------- */	

abitwriter::~abitwriter( void )
{
	// free memory if pointer was not given out
	if ( fmem )	free( data );
}

/* -----------------------------------------------
	writes n bits to abitwriter
	----------------------------------------------- */	

void abitwriter::write( unsigned int val, int nbits )
{
	// safety check for error
	if ( error ) return;
	
	// test if pointer beyond flush treshold
	if ( cbyte > ( dsize - 5 ) ) {
		dsize += adds;
		data = (unsigned char*) realloc( data, dsize );
		if ( data == NULL ) {
			error = true;
			return;
		}
		memset( ( data + cbyte + 1 ), 0, ( dsize - ( cbyte + 1 ) ) * sizeof( char ) );
		// for ( int i = cbyte + 1; i < dsize; i++ ) data[i] = 0;
	}
	
	// write data
	while ( nbits >= cbit ) {
		data[cbyte] |= ( MBITS32(val, nbits, (nbits-cbit)) );		
		nbits -= cbit;		
		cbyte++;
		cbit = 8;
	}
	
	if ( nbits > 0 ) {		
		data[cbyte] |= ( (RBITS32(val, nbits)) << (cbit - nbits) );
		cbit -= nbits;		
	}	
}

/* -----------------------------------------------
	pads data using fillbit
	----------------------------------------------- */
	
void abitwriter::pad( unsigned char fillbit )
{
	while ( cbit < 8 )
		write( fillbit, 1 );
}

/* -----------------------------------------------
	gets data array from abitwriter
	----------------------------------------------- */	

unsigned char* abitwriter::getptr( void )
{
	// data is padded here
	pad( fillbit );
	// forbid freeing memory
	fmem = false;
	// realloc data
	data = (unsigned char*) realloc( data, cbyte );
	
	return data;
}

/* -----------------------------------------------
	gets data array from abitwriter
	----------------------------------------------- */	

const unsigned char* abitwriter::peekptr( void ) const
{
	return data;
}

/* -----------------------------------------------
	gets size of data array from abitwriter
	----------------------------------------------- */	

int abitwriter::getpos( void )
{
	return cbyte;
}


/* -----------------------------------------------
	constructor for abytewriter class
	----------------------------------------------- */	

abytewriter::abytewriter( int size )
{
	adds  = 65536;
	cbyte = 0;
	
	error = false;
	fmem  = true;
	
	dsize = ( size > 0 ) ? size : adds;
	data = (unsigned char*) malloc( dsize );
	if ( data == NULL ) {
		error = true;
		return;
	}
}

/* -----------------------------------------------
	destructor for abytewriter class
	----------------------------------------------- */	

abytewriter::~abytewriter( void )
{
	// free data if pointer is not read
	if ( fmem )	free( data );
}

/* -----------------------------------------------
	writes 1 byte to abytewriter
	----------------------------------------------- */	

void abytewriter::write( unsigned char byte )
{
	// safety check for error
	if ( error ) return;
	
	// test if pointer beyond flush threshold
	if ( cbyte >= ( dsize - 2 ) ) {
		dsize += adds;
		data = (unsigned char*) realloc( data, dsize );
		if ( data == NULL ) {
			error = true;
			return;
		}
	}
	
	// write data
	data[ cbyte++ ] = byte;
}

/* -----------------------------------------------
	writes n byte to abytewriter
	----------------------------------------------- */
	
void abytewriter::write_n( unsigned char* byte, int n )
{
	// safety check for error
	if ( error ) return;
	
	// make sure that pointer doesn't get beyond flush threshold
	while ( ( cbyte + n ) >= ( dsize - 2 ) ) {
		dsize += adds;
		data = (unsigned char*) realloc( data, dsize );
		if ( data == NULL ) {
			error = true;
			return;
		}
	}
	
	// copy data from array
	while ( n-- > 0 )
		data[ cbyte++ ] = *(byte++);
}

/* -----------------------------------------------
	gets data array from abytewriter
	----------------------------------------------- */

unsigned char* abytewriter::getptr( void )
{
	// forbid freeing memory
	fmem = false;
	// realloc data
	data = (unsigned char*) realloc( data, cbyte );
	
	return data;
}

/* -----------------------------------------------
	peeks into data array from abytewriter
	----------------------------------------------- */
	
unsigned char* abytewriter::peekptr( void )
{
	return data;
}

/* -----------------------------------------------
	gets size of data array from abytewriter
	----------------------------------------------- */	

int abytewriter::getpos( void )
{
	return cbyte;
}

/* -----------------------------------------------
	reset without realloc
	----------------------------------------------- */	
	
void abytewriter::reset( void )
{
	// set position of current byte
	cbyte = 0;
}


/* -----------------------------------------------
	constructor for abytewriter class
	----------------------------------------------- */

abytereader::abytereader( unsigned char* array, int size )
{
	cbyte = 0;
	eof = false;
	
	data = array;
	lbyte = size;
	
	if ( ( data == NULL ) || ( lbyte == 0 ) )
		eof = true;
}

/* -----------------------------------------------
	destructor for abytewriter class
	----------------------------------------------- */

abytereader::~abytereader( void )
{
}

/* -----------------------------------------------
	reads 1 byte from abytereader
	----------------------------------------------- */

int abytereader::read( unsigned char* byte )
{
	if ( cbyte >= lbyte ) {
		cbyte = lbyte;
		eof = true;
		return 0;
	}
	else {
		*byte = data[ cbyte++ ];
		return 1;
	}
}

/* -----------------------------------------------
	reads n bytes from abytereader
	----------------------------------------------- */
	
int abytereader::read_n( unsigned char* byte, int n )
{
	int nl = lbyte - cbyte;
	int i;
	
	if ( nl < n ) {
		for ( i = 0; i < nl; i++ )
			byte[ i ] = data[ cbyte + i ];
		cbyte = lbyte;
		eof = true;
		return nl;
	}
	else {
		for ( i = 0; i < n; i++ )
			byte[ i ] = data[ cbyte + i ];
		cbyte += n;
		return n;
	}
}

/* -----------------------------------------------
	go to position in data
	----------------------------------------------- */
	
void abytereader::seek( int pos )
{
	if ( pos >= lbyte ) {
		cbyte = lbyte;
		eof = true;
	}
	else {
		cbyte = pos;
		eof = false;
	}
}

/* -----------------------------------------------
	gets size of current data
	----------------------------------------------- */
	
int abytereader::getsize( void )
{
	return lbyte;
}

/* -----------------------------------------------
	gets current position from abytereader
	----------------------------------------------- */	

int abytereader::getpos( void )
{
	return cbyte;
}


/* -----------------------------------------------
	constructor for iostream class
	----------------------------------------------- */

iostream::iostream( void* src, int srctype, int srcsize, int iomode )
{
	// locally copy source, source type # and io mode #
	source = src;
	srct   = srctype;
	srcs   = srcsize;
	mode   = iomode;
	
	// don't free memory when reading - this will be useful if switching occurs
	free_mem_sw = false;
	
	// set binary mode for streams
	#ifdef _WIN32
		setmode( fileno( stdin ), O_BINARY );
		setmode( fileno( stdout ), O_BINARY );
	#endif
	
	// open file/mem/stream
	switch ( srct )
	{
		case 0:
			open_file();
			break;
		
		case 1:
			open_mem();
			break;
		
		case 2:
			open_stream();
			break;
		
		default:			
			break;
	}
}

/* -----------------------------------------------
	destructor for iostream class
	----------------------------------------------- */

iostream::~iostream( void )
{
	// if needed, write memory to stream or free memory from buffered stream
	if ( srct == 2 ) {		
		if ( mode == 1 ) {
			if ( !(mwrt->error) ) {
				srcs   = mwrt->getpos();
				source = mwrt->getptr();
				fwrite( source, sizeof( char ), srcs, stdout );
			}
		}
	}
	
	// free all buffers
	if ( srct == 0 ) {
		if ( fptr != NULL )
			fclose( fptr );
	}
	else if ( mode == 0 ) {
		if ( free_mem_sw )
			free( source );
		delete( mrdr );
	}
	else
		delete( mwrt );
}

/* -----------------------------------------------
	switches mode from reading to writing and vice versa
	----------------------------------------------- */
	
void iostream::switch_mode( void )
{	
	// return immediately if there's an error
	if ( chkerr() ) return;
	
	
	if ( mode == 0 ) {
		// WARNING: when switching from reading to writing, information might be lost forever
		switch ( srct ) {
			case 0:
				fclose( fptr );
				fptr = fopen( ( char* ) source, "wb" );
				break;
			case 1:
			case 2:
				delete( mrdr );
				if ( free_mem_sw )
					free( source ); // see? I've told you so :-)
				mwrt = new abytewriter( srcs );
				break;
			default:
				break;
		}
		mode = 1;
	}
	else {
		// switching from writing to reading is a bit more complicated
		switch ( srct ) {
			case 0:
				fclose( fptr );
				fptr = fopen( ( char* ) source, "rb" );
				break;
			case 1:
			case 2:
				source = mwrt->getptr();
				srcs   = mwrt->getpos();
				delete( mwrt );
				mrdr = new abytereader( ( unsigned char* ) source, srcs );
				free_mem_sw = true;
				break;
			default:
				break;
		}
		mode = 0;
	}
}

/* -----------------------------------------------
	generic read function
	----------------------------------------------- */
	
int iostream::read( void* to, int tpsize, int dtsize )
{
	return ( srct == 0 ) ? read_file( to, tpsize, dtsize ) : read_mem( to, tpsize, dtsize );
}

/* -----------------------------------------------
	generic write function
	----------------------------------------------- */

int iostream::write( const void* from, int tpsize, int dtsize )
{
	return ( srct == 0 ) ? write_file( from, tpsize, dtsize ) : write_mem( from, tpsize, dtsize );
}

/* -----------------------------------------------
	flush function 
	----------------------------------------------- */

int iostream::flush( void )
{
	if ( srct == 0 )
		fflush( fptr );
	
	return getpos();
}

/* -----------------------------------------------
	rewind to beginning of stream
	----------------------------------------------- */

int iostream::rewind( void )
{
	// WARNING: when writing, rewind might lose all your data
	if ( srct == 0 )
		fseek( fptr, 0, SEEK_SET );
	else if ( mode == 0 )
		mrdr->seek( 0 );
	else
		mwrt->reset();
	
	return getpos();
}

/* -----------------------------------------------
	get current position in stream
	----------------------------------------------- */

int iostream::getpos( void )
{
	int pos;
	
	if ( srct == 0 )
		pos = ftell( fptr );
	else if ( mode == 0 )
		pos = mrdr->getpos();
	else
		pos = mwrt->getpos();

	return pos;
}

/* -----------------------------------------------
	get size of file
	----------------------------------------------- */

int iostream::getsize( void )
{
	int pos;
	int siz;
	
	if ( mode == 0 ) {
		if ( srct == 0 ) {
			pos = ftell( fptr );
			fseek( fptr, 0, SEEK_END );
			siz = ftell( fptr );
			fseek( fptr, pos, SEEK_SET );
		}
		else {
			siz = mrdr->getsize();
		}
	}
	else {
		siz = getpos();
	}

	return siz;
}

/* -----------------------------------------------
	get data pointer (for mem io only)
	----------------------------------------------- */

unsigned char* iostream::getptr( void )
{
	if ( srct == 1 )
		return ( mode == 0 ) ? ( unsigned char* ) source : mwrt->getptr();
	else
		return NULL;
}

/* -----------------------------------------------
	check for errors
	----------------------------------------------- */
	
bool iostream::chkerr( void )
{
	bool error = false;
	
	// check for user input errors
	if ( ( mode != 0 ) && ( mode != 1 ) )
		error = true;
	if ( ( srct != 0 ) && ( srct != 1 ) && ( srct != 2 ) )
		error = true;
	
	// check for io errors
	if ( srct == 0 ) {
		if ( fptr == NULL )
			error = true;
		else if ( ferror( fptr ) )
			error = true;
	}
	else if ( mode == 0 ) {
		if ( mrdr == NULL )			
			error = true;
		else if ( mrdr->getsize() == 0 )
			error = true;
	}
	else {		
		if ( mwrt == NULL )
			error = true;
		else if ( mwrt->error )
			error = true;
	}
	
	return error;
}

/* -----------------------------------------------
	check for eof (read only)
	----------------------------------------------- */
	
bool iostream::chkeof( void )
{
	if ( mode == 0 )
		return ( srct == 0 ) ? feof( fptr ) : mrdr->eof;
	else
		return false;
}

/* -----------------------------------------------
	open function for files
	----------------------------------------------- */

void iostream::open_file( void )
{
	char* fn = (char*) source;
	
	// open file for reading / writing
	fptr = fopen( fn, ( mode == 0 ) ? "rb" : "wb" );
}

/* -----------------------------------------------
	open function for memory
	----------------------------------------------- */

void iostream::open_mem( void )
{
	if ( mode == 0 )
		mrdr = new abytereader( ( unsigned char* ) source, srcs );
	else
		mwrt = new abytewriter( srcs );
}

/* -----------------------------------------------
	open function for streams
	----------------------------------------------- */

void iostream::open_stream( void )
{	
	abytewriter* strwrt;
	unsigned char* buffer;
	int i;
	
	if ( mode == 0 ) {
		// read whole stream into memory buffer
		strwrt = new abytewriter( 0 );
		buffer = ( unsigned char* ) calloc( BUFFER_SIZE, sizeof( char ) );
		if ( buffer != NULL ) {
			while ( ( i = fread( buffer, sizeof( char ), BUFFER_SIZE, stdin ) ) > 0 )
				strwrt->write_n( buffer, i );
		}
		if ( strwrt->error ) {
			source = NULL;
			srcs   = 0;
		}
		else {
			source = strwrt->getptr();
			srcs   = strwrt->getpos();
		}
		delete ( strwrt );
		free( buffer );
		// free memory after done
		free_mem_sw = true;
	}
	
	// for writing: simply open new stream in mem writer
	// writing to stream will be done later
	open_mem();
}

/* -----------------------------------------------
	write function for files
	----------------------------------------------- */

int iostream::write_file( const void* from, int tpsize, int dtsize )
{
	int retval = fwrite( from, tpsize, dtsize, fptr );
	static int status = fflush(fptr);
	(void)status;
	return retval;
}

/* -----------------------------------------------
	read function for files
	----------------------------------------------- */

int iostream::read_file( void* to, int tpsize, int dtsize )
{
	return fread( to, tpsize, dtsize, fptr );
}

/* -----------------------------------------------
	write function for memory
	----------------------------------------------- */
	
int iostream::write_mem( const void* from, int tpsize, int dtsize )
{
	int n = tpsize * dtsize;
	
	mwrt->write_n( ( unsigned char* ) from, n );
	
	return ( mwrt->error ) ? 0 : n;
}

/* -----------------------------------------------
	read function for memory
	----------------------------------------------- */

int iostream::read_mem( void* to, int tpsize, int dtsize )
{
	int n = tpsize * dtsize;
	
	return ( mrdr->read_n( ( unsigned char* ) to, n ) ) / tpsize;
}
