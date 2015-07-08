#ifndef _HANDLE_HH
#define _HANDLE_HH

#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <system_error>
#include <iostream>

class FHandle
{
private:
  int fd_;

public:
  FHandle( const int s_fd ) : fd_( s_fd ) {}

  ~FHandle() 
  { 
    if ( fd_ >= 0 ) {
        if (close( fd_ ) < 0) {
            abort();
        }
    }
  }

  uint64_t size( void ) const
  {
    struct stat file_info;
    if (fstat( fd_, &file_info ) < 0) {
        abort();
    }
    return file_info.st_size;
  }

  int fd( void ) const { return fd_; }

  FHandle( const FHandle & other ) = delete;
  const FHandle & operator=( const FHandle & other ) = delete;

};

#endif
