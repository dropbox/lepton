#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([lepton], [0.01], [danielrh@dropbox.com])
AM_INIT_AUTOMAKE([subdir-objects foreign])
AC_CONFIG_SRCDIR([src/lepton/vp8_encoder.hh])
AC_CONFIG_HEADERS([config.h])

# Add preprocessor flags
CXX11_FLAGS="-std=c++11"
AC_SUBST([CXX11_FLAGS])
C99_FLAGS="-std=c99"
AC_SUBST([C99_FLAGS])

# Add picky flags
PICKY_CFLAGS="-pedantic -Wall -Wextra -Wno-write-strings -Wno-unused-parameter"
AC_SUBST([PICKY_CFLAGS])

PICKY_CXXFLAGS="-pedantic -Wall -Wextra -Wno-write-strings -Wno-unused-parameter -fno-exceptions -fno-rtti"
AC_SUBST([PICKY_CXXFLAGS])

# Add -DNDEBUG by default
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug],
    [Enable assertions @<:@no@:>@])],
  [NODEBUG_CXXFLAGS=""],
  [NODEBUG_CXXFLAGS="-DNDEBUG"])
AC_SUBST(NODEBUG_CXXFLAGS)

# Add optimizer flags

AC_ARG_ENABLE([native-opt],
  [AS_HELP_STRING([--disable-native-opt])])
ARCH_FLAGS="-march=native"
AS_IF([test "x$enable_native_opt" = "xno"], [
  ARCH_FLAGS=""])

AC_ARG_ENABLE([vectorization],
  [AS_HELP_STRING([--disable-vectorization])])
AS_IF([test "x$enable_vectorization" = "xno"], [
  ARCH_FLAGS="$ARCH_FLAGS -DUSE_SCALAR"])
AC_SUBST([ARCH_FLAGS])

# Allow sanitizing
AC_ARG_ENABLE([sanitize],
  [AS_HELP_STRING([--enable-sanitize],
    [Enable address and undefined-behavior sanitizers @<:@no@:>@])],
  [SANITIZE_FLAGS="-fsanitize=address,undefined"],
  [SANITIZE_FLAGS=""])
AC_SUBST(SANITIZE_FLAGS)
SYSTEM_DEPENDENCIES_LDFLAGS="liblocalzlib.a"

AC_ARG_ENABLE([system-dependencies],
  [AS_HELP_STRING([--enable-system-dependencies],
    [Rely on system installed version of zlib @<:@no@:>@])],
  [SYSTEM_DEPENDENCIES_CFLAGS="-DUSE_SYSTEM_LIBRARIES " SYSTEM_DEPENDENCIES_LDFLAGS="-lz"],
  [SYSTEM_DEPENDENCIES_LDFLAGS="liblocalzlib.a"])
AC_SUBST([SYSTEM_DEPENDENCIES_LDFLAGS])
AC_SUBST([SYSTEM_DEPENDENCIES_CFLAGS])


AC_ARG_ENABLE([custom-allocator],
  [AS_HELP_STRING([--enable-custom-allocator]
    [Use custom allocators instead of the standard ones @<:@no@:>@])],
  [MEMORY_MANAGEMENT_CFLAGS=""],
  [MEMORY_MANAGEMENT_CFLAGS="-DUSE_STANDARD_MEMORY_ALLOCATORS"])
AC_SUBST([MEMORY_MANAGEMENT_CFLAGS])


AC_ARG_ENABLE([ans-experimental],
  [AS_HELP_STRING([--enable-ans-experimental]
    [Enable ANS arithmetic coder as an option instead of the VPX arithmetic coder @<:@no@:>@])],
  [ARITHMETIC_CODER_CFLAGS="-DENABLE_ANS_EXPERIMENTAL"],
  [ARITHMETIC_CODER_CFLAGS=""])
AC_SUBST([ARITHMETIC_CODER_CFLAGS])


AC_ARG_ENABLE([benchmark],
  [AS_HELP_STRING([--enable-benchmark]
    [Placeholder flag to enable longer/more in depth benchmark @<:@no@:>@])],
  [BENCHMARK_CFLAGS="-DREALISTIC_BENCHMARK"],
  [BENCHMARK_CFLAGS=""])
AC_SUBST([BENCHMARK_CFLAGS])


# Make single thread the default
AC_ARG_ENABLE([best-ratio-slow-decompression],
  [AS_HELP_STRING([--enable-best-ratio-slow-decompression],
    [Turn off multithreading to train priors better @<:@no@:>@])],
  [THREAD_FLAGS="-DDEFAULT_SINGLE_THREAD "],
  [THREAD_FLAGS=""])
AC_SUBST([THREAD_FLAGS])

# Make single thread the default
AC_ARG_ENABLE([billing],
  [AS_HELP_STRING([--enable-billing],
    [Print out a bill receipt @<:@no@:>@])],
  [BILLING_FLAGS="-DENABLE_BILLING "],
  [BILLING_FLAGS=""])
AC_SUBST([BILLING_FLAGS])

CODEC_FLAGS=""
# Disable advanced features
AC_ARG_ENABLE([advanced-jpeg-features],
  [AS_HELP_STRING([--disable-advanced-jpeg-features])])

AS_IF([test "x$enable_advanced_jpeg_features" != "xno"], [
  CODEC_FLAGS="-DDEFAULT_ALLOW_PROGRESSIVE -DHIGH_MEMORY"])
AC_SUBST([CODEC_FLAGS])


if test -z "$CXXFLAGS"; then
    CXXFLAGS='-O3 -g -DNDEBUG'
fi

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
