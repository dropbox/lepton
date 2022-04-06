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

/*
This file contains special classes for bitwise
reading and writing of arrays
*/
#include "../../vp8/util/memory.hh"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <algorithm>
#include <assert.h>
#include "bitops.hh"

#define BUFFER_SIZE 1024 * 1024
/* -----------------------------------------------
	constructor for abitreader class
	----------------------------------------------- */	

abitreader::abitreader( unsigned char* array, int size )
{
    cbyte2 = 0;
    cbit2 = 0;
    data2 = array;
	eof = false;
	lbyte = size;
    buf = 0;
}

/* -----------------------------------------------
	destructor for abitreader class
	----------------------------------------------- */	

abitreader::~abitreader( void )
{
}



/* -----------------------------------------------
	constructor for abitwriter class
	----------------------------------------------- */	

abitwriter::abitwriter( int size , int max_file_size)
{
    size_bound = max_file_size;
    if (size_bound) {
        size_bound += 8; // 64 bits of padding on the end
    }
    fillbit = 1;
    adds    = 65536;
    cbyte2   = 0;
    cbit2    = 64;
    buf = 0;

    error = false;
    fmem  = true;
    dsize = ( size > 0 ) ? size : adds;
    data2 = aligned_alloc(dsize);
    if ( data2 == NULL ) {
        error = true;
        custom_exit(ExitCode::MALLOCED_NULL);
        return;
    }
	// for ( int i = 0; i < dsize; i++ ) data[i] = 0;
}

/* -----------------------------------------------
	destructor for abitwriter class
	----------------------------------------------- */	

abitwriter::~abitwriter( void )
{
	// free memory if pointer was not given out
    if ( fmem )	aligned_dealloc( data2 );
}


void aligned_dealloc(unsigned char *data) {
    if (!data) return;
    always_assert(((size_t)(data - 0) & 0xf) == 0);
    always_assert(data[-1] <= 0x10);
    data -= data[-1];
    custom_free(data);
}
unsigned char *aligned_alloc(size_t dsize) {
    unsigned char*data = (unsigned char*) custom_malloc( dsize + 16);
    if (data) {
        size_t rem = (size_t)(data - 0) & 0xf;
        if (rem) {
            data += rem;
            data[-1] = rem;
        } else {
            data += 0x10;
            data[-1] = 0x10;
        }
    }
    return data;
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
    data = aligned_alloc(dsize);
	if ( data == NULL ) {
		error = true;
        custom_exit(ExitCode::MALLOCED_NULL);
		return;
	}
}

/* -----------------------------------------------
	destructor for abytewriter class
	----------------------------------------------- */	

abytewriter::~abytewriter( void )
{
	// free data if pointer is not read
	if (fmem && data) aligned_dealloc(data);
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
        if (data) {
            unsigned char * newData = aligned_alloc(dsize *  2);
            memcpy(newData, data, dsize);
            dsize *= 2;
            aligned_dealloc(data);
            data = newData;
        }
		if ( data == NULL ) {
			error = true;
            custom_exit(ExitCode::MALLOCED_NULL);
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
        unsigned char * newData = aligned_alloc(dsize *  2);
        memcpy(newData, data, dsize);
        dsize *= 2;
        aligned_dealloc(data);
        data = newData;
		if ( data == NULL ) {
            error = true;
            custom_exit(ExitCode::MALLOCED_NULL);
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

unsigned char* abytewriter::getptr_aligned( void )
{
	// forbid freeing memory
	fmem = false;
	return data;
}

/* -----------------------------------------------
	peeks into data array from abytewriter
	----------------------------------------------- */
	
unsigned char* abytewriter::peekptr_aligned( void )
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

bounded_iostream::bounded_iostream(Sirikata::DecoderWriter *w,
                                   const std::function<void(Sirikata::DecoderWriter*, size_t)> &size_callback,
                                   const Sirikata::JpegAllocator<uint8_t> &alloc) 
    : parent(w), err(Sirikata::JpegError::nil()) {
    this->size_callback = size_callback;
    bookkeeping_bytes_written = 0;
    buffer_position = 0;
    byte_position = 0;
    byte_bound = 0x7FFFFFFF;
    num_bytes_attempted_to_write = 0;
    set_bound(0);
}
void bounded_iostream::call_size_callback(size_t size) {
    size_callback(parent, size);
}
bool bounded_iostream::chkerr() {
    return err != Sirikata::JpegError::nil();
}
void bounded_iostream::prep_for_new_file() {
    buffer_position = 0;
    byte_position = 0;
    byte_bound = 0x7FFFFFFF;;
    num_bytes_attempted_to_write = 0;
    set_bound(0);
    
}
void bounded_iostream::set_bound(size_t bound) {
    flush();
    if (num_bytes_attempted_to_write > byte_bound) {
        num_bytes_attempted_to_write = byte_bound;
    }
    byte_bound = bound;
}
void bounded_iostream::flush() {
    if (buffer_position) {
        write_no_buffer(buffer, buffer_position);
        buffer_position = 0;
    }
}
void bounded_iostream::close() {
    flush();
    parent->Close();
}

uint32_t bounded_iostream::write_no_buffer(const void *from, size_t bytes_to_write) {
    //return iostream::write(from,tpsize,dtsize);
    std::pair<unsigned int, Sirikata::JpegError> retval;
    if (byte_bound != 0 && byte_position + bytes_to_write > byte_bound) {
        always_assert(byte_position <= byte_bound); // otherwise we already wrote too much
        size_t real_bytes_to_write = byte_bound - byte_position;
        byte_position += real_bytes_to_write;
        retval = parent->Write(reinterpret_cast<const unsigned char*>(from), real_bytes_to_write);
        if (retval.first < real_bytes_to_write) {
            err = retval.second;
            return retval.first;
        }
        return bytes_to_write; // pretend we wrote it all
    }
    size_t total = bytes_to_write;
    retval = parent->Write(reinterpret_cast<const unsigned char*>(from), total);
    unsigned int written = retval.first;
    byte_position += written;
    if (written < total ) {
        err = retval.second;
        return written;
    }
    return bytes_to_write;
}

unsigned int bounded_iostream::getsize() {
    return byte_position;
}

bounded_iostream::~bounded_iostream(){
}

ibytestreamcopier::ibytestreamcopier(Sirikata::DecoderReader *p, unsigned int byte_offset,
                                     unsigned int max_file_size,
                                     const Sirikata::JpegAllocator<uint8_t> &alloc)
    : ibytestream(p, byte_offset, alloc), side_channel(alloc) {
    if (max_file_size) {
        side_channel.reserve(max_file_size);
    }
}
bool ibytestreamcopier::read_byte(unsigned char *output) {
    bool retval = ibytestream::read_byte(output);
    if (retval) {
        side_channel.push_back(*output);
    }
    return retval;
}

unsigned int ibytestreamcopier::read(unsigned char *output, unsigned int size) {
    unsigned int retval = ibytestream::read(output, size);
    if (retval > 0) {
        side_channel.insert(side_channel.end(), output, output + retval);
    }
    return retval;
}
ibytestream::ibytestream(Sirikata::DecoderReader *p, unsigned int byte_offset,
                         const Sirikata::JpegAllocator<uint8_t> &alloc) 
    : parent(p) {
    bytes_read = byte_offset;
}

unsigned int ibytestream::read(unsigned char*output, unsigned int size) {
    dev_assert(size);
    if (size == 1) {
        return read_byte(output) ? 1 : 0;
    }
    int retval = IOUtil::ReadFull(parent, output, size);
    bytes_read += retval;
    static_assert(sizeof(last_read) == 2, "Last read must hold full jpeg huffman");
    if (retval >= 2) {
        memcpy(last_read, output + size - sizeof(last_read), sizeof(last_read));
    } else if (retval) {
        last_read[0] = last_read[1];
        last_read[1] = *output;
    }
    return retval;
}

bool ibytestream::read_byte(unsigned char *output) {
    unsigned int retval = parent->Read(output, 1).first;
    if (retval != 0) {
        last_read[0] = last_read[1];
        last_read[1] = *output;
        bytes_read += 1;
        return true;
    }
    return false;
}
