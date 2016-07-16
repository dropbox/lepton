#ifndef VPX_CONFIG_H_
#define VPX_CONFIG_H_

//#define DEBUG_ARICODER
#ifdef DEBUG_ARICODER
#include <cstdio>
#endif
#define INLINE inline
#endif

#ifdef _WIN32
#include <intrin.h>
// FIXME: this assumes windows platforms are little endian
#define htobe64 _byteswap_uint64
#define be64toh _byteswap_uint64
#define htobe32 _byteswap_ulong
#define be32toh _byteswap_ulong
#define htobe16 _byteswap_ushort
#define be16toh _byteswap_ushort
#define htole64(x) (x)
#define htole32(x) (x)
#define htole16(x) (x)
#define le64toh(x) (x)
#define le32toh(x) (x)
#define le16toh(x) (x)

#else
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
#ifdef BSD
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#endif
#endif
