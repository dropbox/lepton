#ifndef _FRAMEWORK_HPP_
#define _FRAMEWORK_HPP_
#include <assert.h>
#include "memory.hh"
#define AVOID_ARRAY_BOUNDS_CHECKS

#include "nd_array.hh"
extern Sirikata::Array3d<float (*)(const Sirikata::Array1d<int16_t, 79>&, int), 17, 16, 64> global_gen_lepton_prior;
#endif
