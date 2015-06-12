#ifndef _SLICE_HH
#define _SLICE_HH

#include <string>
#include <stdexcept>
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
# define htobe64 OSSwapHostToBigInt64
# define be64toh OSSwapBigToHostInt64
# define htobe32 OSSwapHostToBigInt32
# define be32toh OSSwapBigToHostInt32
# define htobe16 OSSwapHostToBigInt16
# define be16toh OSSwapBigToHostInt16

# define htole64 OSSwapHostToLittleInt64
# define le64toh OSSwapLittleToHostInt64
# define htole32 OSSwapHostToLittleInt32
# define le32toh OSSwapLittleToHostInt32
# define htole16 OSSwapHostToLittleInt16
# define le16toh OSSwapLittleToHostInt16
#endif

class Slice
{
private:
  const uint8_t *buffer_;
  uint64_t size_;

  void bounds_check( const uint64_t & length ) const
  {
    if ( length > size_ ) {
      throw std::out_of_range( "attempted to read past end of chunk" );
    }
  }

  static uint64_t bit_mask( const uint64_t & n )
  {
    if ( n > 63 ) {
      throw std::out_of_range( "bit mask size is unsupported" );
    }
    return ( 1 << n ) - 1;
  }

public:
  Slice( const uint8_t *s_buffer, const uint64_t & s_size )
    : buffer_( s_buffer ),
      size_( s_size )
  {}

  const uint8_t * buffer( void ) const { return buffer_; }
  const uint64_t & size( void ) const { return size_; }

  Slice operator() ( const uint64_t & offset ) const
  {
    return operator() ( offset, size_ - offset );
  }

  Slice operator() ( const uint64_t & offset, const uint64_t & length ) const
  {
    bounds_check( offset );
    bounds_check( offset + length );
    return Slice( buffer_ + offset, length );
  }

  std::string to_string( void ) const
  {
    return std::string( reinterpret_cast<const char *>( buffer_ ), size_ );
  }

  const uint8_t & octet( void ) const
  {
    bounds_check( sizeof( uint8_t ) );
    return *buffer_;
  }

  uint16_t le16( void ) const
  {
    bounds_check( sizeof( uint16_t ) );
    return le16toh( *reinterpret_cast<const uint16_t *>( buffer_ ) );
  }

  uint64_t le32( void ) const
  {
    bounds_check( sizeof( uint32_t ) );
    return le32toh( *reinterpret_cast<const uint32_t *>( buffer_ ) );
  }

  uint64_t le64( void ) const
  {
    bounds_check( sizeof( uint64_t ) );
    return le64toh( *reinterpret_cast<const uint64_t *>( buffer_ ) );
  }

  uint64_t bits( const uint64_t & bit_offset, const uint64_t bit_length ) const
  {
    const uint64_t byte_len = 1 + ( bit_offset + bit_length - 1 ) / 8;
    bounds_check( byte_len );
    if ( byte_len > sizeof( uint64_t ) ) {
      throw std::out_of_range( "bit offset and length not supported" );
    }

    uint64_t val = 0;
    for ( uint64_t i = 0; i < byte_len; i++ ) {
      val |= buffer_[ i ] << ( i * 8 );
    }

    return ( val >> bit_offset ) & bit_mask( bit_length );
  }
};

#endif
