#ifndef _FRAMEWORK_HPP_
#define _FRAMEWORK_HPP_
#include <assert.h>
#define AVOID_ARRAY_BOUNDS_CHECKS

#include "nd_array.hh"
extern Sirikata::Array3d<float (*)(const Sirikata::Array1d<int16_t, 110>&, int), 17, 16, 64> global_gen_lepton_prior;
#endif
