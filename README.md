# Lepton

Lepton is a tool and file format for losslessly compressing JPEGs by an average of 22%.

This can be used to archive large photo collections, or to serve images live and save 22% bandwidth.


[![Build Status](https://travis-ci.org/dropbox/lepton.svg?branch=master)](https://travis-ci.org/dropbox/lepton)


## Build directions
Using a single core

    ./autogen.sh
    ./configure
    make
    make check

For multiprocessor machines:

    ./autogen.sh
    ./configure
    make -j8
    make check -j8

Using CMAKE:

    mkdir -p build
    cd build
    cmake ..
    make -j8

On Windows

    mkdir -p build
    cd build
    "c:\Program Files\CMake\bin\cmake" ..
    start .
    REM Double click the Visual Studio project

## Usage

To roundtrip (compress and decompress) an image, `original.jpg`, do the following:

    ./lepton original.jpg compressed.lep
    ./lepton compressed.lep restored_original.jpg

Or all at once:

    ./lepton original.jpg compressed.lep && ./lepton compressed.lep restored_original.jpg && diff restored_original.jpg original.jpg && echo no differences


Lepton may also be used with pipes -- be sure to check the exit code when using pipes
as if compression fails lepton will produce 0 bytes and return a nonzero exit code
(failure). In this case do not assume the 0 byte file is representative of the original.

    ./lepton - < original.jpg > compressed.lep
    ./lepton - < compressed.lep > restored_original.jpg


You may specify higher memory limits than the default for lepton to handle bigger images:

    ./lepton -memory=1024M -threadmemory=128M input_file output_file

Additionally you can configure lepton to process progressive jpegs.
Warning: these take more memory to decode than normal JPEGs since the entire framebuffer
must be kept in memory for the duration of the decompression, instead of just 2 rows of blocks.

    ./lepton -allowprogressive -memory=1024M -threadmemory=128M progressive.jpg compressedprogressive.lep


## Submitting pull requests to lepton

Please begin by filling out the contributor form and asserting that

    The code I'm contributing is mine, and I have the right to license it.
    I'm granting you a license to distribute said code under the terms of this agreement.

at this page:
https://opensource.dropbox.com/cla/

Then create a new pull request through the github interface

## Debugging

Lepton is designed to be easy to debug, but a command line requirement is necessary to prevent
the standard forks that let it do a secure verification in a separate process.

To avoid setting follow fork flags, please pass -skipverify to the command line.
This will stop verification and let you debug the app as a single process application.
If the bug happens in single threaded mode, also you can pass -singlethread which makes
it easier to step through the code without other threads hitting breakpoints.

## Wrapper Libraries

* NodeJS: https://github.com/whitef0x0/node-lepton
