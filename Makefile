# Project: uncmpJPG
# Makefile created by Matthias Stirner 28.03.2007
# Working with JFE & GCC (May 27 2004)

CC      = gcc
CPP     = g++-4.8
RC      = windres
CFLAGS  = -std=c++11 -Wno-write-strings -DUNIX -I. -g -Wall -O3 -pedantic -funroll-loops -ffast-math -fsched-spec-load -fomit-frame-pointer
LDFLAGS = -g -lpthread
DEPS    = bitops.h htables.h component_info.h uncompressed_components.h jpgcoder.h simple_decoder.h simple_encoder.h
OBJ     = bitops.o jpgcoder.o simple_decoder.o uncompressed_components.o simple_encoder.o
BIN     = uncmpJPG
#all: clean $(BIN)

%.o: %.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJ) $(RES)
	$(CPP) -o $@ $^ $(LDFLAGS)

clean:
	rm -f -- *.o $(BIN)

