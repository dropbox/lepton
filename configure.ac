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
ARCH_FLAGS="-march=native"
AC_SUBST([ARCH_FLAGS])

# Allow sanitizing
AC_ARG_ENABLE([sanitize],
  [AS_HELP_STRING([--enable-sanitize],
    [Enable address and undefined-behavior sanitizers @<:@no@:>@])],
  [NODEBUG_CXXFLAGS="-fsanitize=address,undefined"],
  [NODEBUG_CXXFLAGS=""])
AC_SUBST(SANITIZE_FLAGS)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
# PKG_CHECK_MODULES([liblzma], [liblzma])
# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
