#ifndef VPX_CONFIG_H_
#define VPX_CONFIG_H_

//#define DEBUG_ARICODER
#ifdef DEBUG_ARICODER
#include <cstdio>
#endif
#define INLINE
#endif


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
#else
#ifndef _BSD_SOURCE
#define _BSD_SOURCE       /* See feature_test_macros(7) */
#endif
#include <endian.h>
#endif
