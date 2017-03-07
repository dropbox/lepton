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

## Bindings for other languages

* NodeJS: https://github.com/whitef0x0/node-lepton
* PHP: https://github.com/gtuk/php-lepton

## Acknowledgements

Many thanks to Matthias Stirner and HTW Aalen University for the development of the [uncmpJPG](http://packjpg.encode.ru/?page_id=178) library.

Thanks to the VPX authors for their highly tuned bool reader and bool writer implementations.

## Related Work

Be sure to read the groundbreaking research done by Matthias Stirner, Gehard Seelman and HTW Aalen University in their [2007 paper](http://www.elektronik.htw-aalen.de/packjpg/_notes/PCS2007_PJPG_paper_final.pdf) and check out their excellent [PackJPG](http://github.com/packjpg) repositories for compression of JPEG MP3 BMP and PMN formats.

Also the [paq](http://mattmahoney.net/dc/) algorithms combine for a highly compressed JPEG result when decompression time is less critical.

When the compressed JPEG needs to also be a JPEG, there is [mozjpg](https://github.com/mozilla/mozjpeg) to explore as well.

## License

Unless otherwise noted:

```
Copyright (c) 2016 Dropbox, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
