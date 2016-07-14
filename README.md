# Lepton

Lepton is a tool and file format for losslessly compressing JPEGs by an average of 22%.

This can be used to archive large photo collections, or to serve images live and save 22% banwdith.

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
must be kept in memory for the duration of the decompression, instead if just 2 rows of blocks.

    ./lepton -allowprogressive -memory=1024M -threadmemory=128M progressive.jpg compressedprogressive.lep

