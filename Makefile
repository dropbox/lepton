# Project: uncmpJPG
# Makefile created by Matthias Stirner 28.03.2007
# Working with JFE & GCC (May 27 2004)

CC      = gcc
CPP     = g++
RC      = windres
CFLAGS  = -I. -O3 -Wall -pedantic -funroll-loops -ffast-math -fsched-spec-load -fomit-frame-pointer
LDFLAGS = -s
DEPS    = bitops.h htables.h
OBJ     = bitops.o jpgcoder.o
BIN     = uncmpJPG

%.o: %.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJ) $(RES)
	$(CPP) -o $@ $^ $(LDFLAGS)
