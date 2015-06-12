#include <fcntl.h>
#include <sys/mman.h>

#include "mmap.hh"

using namespace std;

MMapFile::MMapFile( const std::string & filename )
  : fd_( open( filename.c_str(), O_RDONLY ) ),
    size_( fd_.size() ),
    buffer_( static_cast<uint8_t *>( mmap( nullptr, size_, PROT_READ, MAP_SHARED, fd_.fd(), 0 ) ) ),
    slice_( buffer_, size_ )
{
  if ( buffer_ == MAP_FAILED ) {
      abort();
  }
}

MMapFile::~MMapFile()
{
  if ( buffer_ ) { 
      if (munmap( buffer_, size_ ) < 0) {
          abort();
      }
  }
}
