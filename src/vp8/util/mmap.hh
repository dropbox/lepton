#ifndef _MMAP_HH
#define _MMAP_HH
// mmap wrapper


#include <string>

#include "handle.hh"
#include "slice.hh"

class MMapFile
{
private:
  FHandle fd_;
  size_t size_;
  uint8_t * buffer_;
  Slice slice_;

public:
  MMapFile( const std::string & filename );
  ~MMapFile();

  const Slice & slice( void ) const { return slice_; }
  const Slice operator() ( const uint64_t & offset, const uint64_t & length ) const
  {
    return slice_( offset, length );
  }


  MMapFile(const MMapFile & other ) = delete;
  MMapFile & operator=(const MMapFile & other ) = delete;


};

#endif
