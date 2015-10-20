#include "../../vp8/util/memory.hh"
#include <cstdlib>
#include <iostream>

#include "model.hh"
bool g_threaded = true;
using namespace std;

int main( int argc, char *argv[] )
{
    Sirikata::memmgr_init(768 * 1024 * 1024,
                          64 * 1024 * 1024,
                          3,
                          256);

  if ( argc <= 0 ) {
    abort();
  }
  Model::PrintabilitySpecification spec;
  spec.printability_bitmask = Model::CLOSE_TO_50 | Model::CLOSE_TO_ONE_ANOTHER;
  spec.tolerance = .25;
  spec.min_samples = 25;
  for (int i = 1; i < argc; ++i) {
      if (strstr(argv[i], "-t") == argv[i] || strstr(argv[i], "-s") == argv[i] || strcmp(argv[i], "-ok") == 0) {
          double arg = atof(argv[i] + 2);
          if (argv[i][1] == 't') {
              spec.tolerance = arg;
          } else if (argv[i][1] == 'o') {
              spec.printability_bitmask = Model::PRINTABLE_OK;
          }else {
              spec.min_samples = (int64_t)arg;
          }
          for (int j = i; j + 1 < argc; ++j) {
              argv[j] = argv[j + 1];
          }
          --argc;
          --i;
      }
  }
  if ( argc != 2  && argc != 3) {
    cerr << "Usage: " << argv[ 0 ] << " FILENAME" << endl;
  }

  Model model_tables;
  load_model(model_tables, argv[1]);
  if (argc > 2) {
    Model orig_tables;
    load_model(orig_tables, argv[2]);
    model_tables.debug_print(&orig_tables, spec);
  } else {
    model_tables.debug_print(nullptr, spec);
  }
  return EXIT_SUCCESS;
}
