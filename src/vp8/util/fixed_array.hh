#ifndef _FIXED_ARRAY_HH_
#define _FIXED_ARRAY_HH_
#include <array>
#include <assert.h>
#include <cstring>
template <class T, size_t dim> using FixedArray = std::array<T, dim>;
#endif
