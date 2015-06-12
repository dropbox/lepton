#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([lepton], [0.01], [danielrh@dropbox.com])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/lepton/vp8_encoder.hh])
AC_CONFIG_HEADERS([config.h])

# Add picky CXXFLAGS
CXX11_FLAGS="-std=c++11 -pthread"
PICKY_CXXFLAGS="-pedantic -Wall -Wextra -Wno-write-strings -Werror" # -Weffc++
AC_SUBST([CXX11_FLAGS])
AC_SUBST([PICKY_CXXFLAGS])

# Checks for programs.
AC_PROG_CXX
AC_PROG_RANLIB

# Checks for libraries.
PKG_CHECK_MODULES([liblzma], [liblzma])
# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 src/vp8/Makefile
		 src/vp8/util/Makefile
		 src/vp8/decoder/Makefile
		 src/vp8/encoder/Makefile
		 src/io/Makefile
		 src/lepton/Makefile])
AC_OUTPUT
