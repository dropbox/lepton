#include <cstdlib>
#include <iostream>

#include "mmap.hh"
#include "decoder.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }
  
  if ( argc != 2  && argc != 3) {
    cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
  }

  MMapFile model_file { argv[ 1 ] };
  ProbabilityTables model_tables { model_file.slice() };
  if (argc > 2) {
      MMapFile orig_file { argv[ 2 ] };
      ProbabilityTables orig_tables { orig_file.slice() };
      model_tables.debug_print(&orig_tables);
  } else {
      model_tables.debug_print();
  }
  
  return EXIT_SUCCESS;
}
