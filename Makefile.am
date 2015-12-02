noinst_LIBRARIES = liblocalzlib.a libtestdriver.a liblocalmd5.a
#libio.a libdecoder.a libencoder.a libmodel.a libutil.a
bin_PROGRAMS = lepton test_suite/test_invariants
lepton_CXXFLAGS = $(CXX11_FLAGS) -I$(srcdir)/dependencies/xz/src/liblzma/api -I$(srcdir)/src/vp8/util -I$(srcdir)/src/vp8/model -I$(srcdir)/src/vp8/decoder -I$(srcdir)/src/vp8/encoder $(PICKY_CXXFLAGS) -I$(srcdir)/dependencies -I$(srcdir)/dependencies -DGIT_REVISION=\"$(shell git rev-parse HEAD)\"
lepton_LDADD = -lpthread liblocalmd5.a liblocalzlib.a 
lepton_SOURCES = \
   src/io/MemReadWriter.cc \
   src/lepton/base_coders.hh \
   src/lepton/simple_encoder.hh \
   src/lepton/bitops.cc \
   src/lepton/smalljpeg.hh \
   src/lepton/bitops.hh \
   src/lepton/idct.cc \
   src/lepton/idct.hh \
   src/lepton/component_info.hh \
   src/lepton/htables.hh \
   src/lepton/fork_serve.cc \
   src/lepton/fork_serve.hh \
   src/lepton/socket_serve.cc \
   src/lepton/socket_serve.hh \
   src/lepton/jpgcoder.cc \
   src/lepton/uncompressed_components.cc \
   src/lepton/jpgcoder.hh \
   src/lepton/uncompressed_components.hh \
   src/lepton/vp8_decoder.cc \
   src/lepton/simple_decoder.cc \
   src/lepton/vp8_decoder.hh \
   src/lepton/simple_decoder.hh \
   src/lepton/vp8_encoder.cc \
   src/lepton/simple_encoder.cc \
   src/lepton/vp8_encoder.hh \
   src/io/Allocator.hh \
   src/io/MemMgrAllocator.hh \
   src/io/MemMgrAllocator.cc \
   src/io/BufferedIO.hh \
   src/io/ZlibCompression.cc \
   src/io/ZlibCompression.hh \
   src/io/Seccomp.hh \
   src/io/Seccomp.cc \
   src/io/seccomp-bpf.hh \
   src/io/Error.hh \
   src/io/Reader.hh \
   src/io/MuxReader.hh \
   src/io/ioutil.hh \
   src/io/ioutil.cc \
   src/io/Zlib0.hh \
   src/io/Zlib0.cc \
   src/io/DecoderPlatform.hh \
   src/vp8/util/generic_worker.hh \
   src/vp8/util/options.hh \
   src/vp8/util/generic_worker.cc \
   src/vp8/util/debug.cc \
   src/vp8/util/debug.h \
   src/vp8/util/memory.cc \
   src/vp8/util/memory.hh \
   src/vp8/util/nd_array.hh \
   src/vp8/util/aligned_block.hh \
   src/vp8/util/block_based_image.hh \
   src/vp8/util/option.hh \
   src/vp8/model/JpegArithmeticCoder.cc \
   src/vp8/model/JpegArithmeticCoder.hh \
   src/vp8/model/branch.hh \
   src/vp8/model/model.cc \
   src/vp8/model/model.hh \
   src/vp8/model/numeric.cc \
   src/vp8/model/numeric.hh \
   src/vp8/model/jpeg_meta.hh \
   src/vp8/encoder/encoder.cc \
   src/vp8/decoder/decoder.cc \
   src/vp8/encoder/bool_encoder.hh \
   src/vp8/decoder/bool_decoder.hh \
   src/vp8/encoder/boolwriter.hh \
   src/vp8/encoder/boolwriter.cc \
   src/vp8/decoder/boolreader.hh \
   src/vp8/decoder/boolreader.cc \
   src/vp8/encoder/vpx_bool_writer.hh \
   src/vp8/decoder/vpx_bool_reader.hh

AM_CXXFLAGS = $(CXX11_FLAGS) $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)
AM_CFLAGS = $(C99_FLAGS) $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)


liblocalxz_a_CFLAGS = -std=c99 -msse4.2 -DHAVE_CONFIG_H -I$(srcdir)/dependencies/xz/src/liblzma/common -I$(srcdir)/dependencies/xz/src/common -I$(srcdir)/dependencies -I$(srcdir)/dependencies/xz/src/liblzma/lzma -I$(srcdir)/dependencies/xz/src/liblzma/lz -I$(srcdir)/dependencies/xz/src/liblzma/check -I$(srcdir)/dependencies/xz/src/liblzma/rangecoder -I$(srcdir)/dependencies/xz/src/liblzma/api -I$(srcdir)/dependencies/xz/src/liblzma/simple -I$(srcdir)/dependencies/xz/src/liblzma/delta -I$(srcdir)/dependencies/xz/src/liblzma

liblocalmd5_a_CFLAGS = -std=c99 -msse4.2
liblocalmd5_a_SOURCES = dependencies/md5/md5.c
liblocalzlib_a_CFLAGS = -msse4.2 -include unistd.h
liblocalzlib_a_SOURCES = dependencies/zlib/inflate.c \
dependencies/zlib/inflate.h \
dependencies/zlib/gzguts.h \
dependencies/zlib/infback.c \
dependencies/zlib/trees.c \
dependencies/zlib/adler32.c \
dependencies/zlib/gzclose.c \
dependencies/zlib/inftrees.h \
dependencies/zlib/zconf.h \
dependencies/zlib/compress.c \
dependencies/zlib/crc32.c \
dependencies/zlib/crc32.h \
dependencies/zlib/trees.h \
dependencies/zlib/inftrees.c \
dependencies/zlib/zutil.c \
dependencies/zlib/gzwrite.c \
dependencies/zlib/zutil.h \
dependencies/zlib/zlib.h \
dependencies/zlib/inffixed.h \
dependencies/zlib/deflate.c \
dependencies/zlib/inffast.h \
dependencies/zlib/inffast.c \
dependencies/zlib/uncompr.c \
dependencies/zlib/gzread.c \
dependencies/zlib/deflate.h \
dependencies/zlib/gzlib.c

DEFAULT_INCLUDES=-I$(srcdir)/src


#noinst_LIBRARIES = libtestdriver.a
libtestdriver_a_SOURCES = test_suite/test_driver.cc

test_suite_test_invariants_SOURCES = test_suite/test_invariants.cc    src/io/Allocator.hh \
   src/vp8/util/memory.cc \
   src/io/BufferedIO.hh \
   src/io/Error.hh \
   src/io/Reader.hh \
   src/io/MuxReader.hh \
   src/io/ioutil.hh \
   src/io/ioutil.cc \
   src/io/Zlib0.hh \
   src/io/Zlib0.cc \
   src/io/MemReadWriter.cc \
   src/io/MemMgrAllocator.cc \
   src/io/MemMgrAllocator.hh \
   src/io/DecoderPlatform.hh
test_suite_test_invariants_CXXFLAGS = $(CXX11_FLAGS) -I$(srcdir)/dependencies/xz/src/liblzma/api -I$(srcdir)/src/vp8/util -I$(srcdir)/src/vp8/model -I$(srcdir)/src/vp8/decoder -I$(srcdir)/src/vp8/encoder $(PICKY_CXXFLAGS)
test_suite_test_invariants_LDADD = liblocalzlib.a


check_PROGRAMS = test_suite/test_invariants test_suite/test_baseline_ujg test_suite/test_baseline test_suite/test_misc test_suite/test_iphone test_suite/test_truncate_ujg test_suite/test_truncate test_suite/test_SLR test_suite/test_progressive_ujg test_suite/test_progressive test_suite/test_arithmetic_failfast test_suite/test_hq


test_suite_test_baseline_ujg_SOURCES = test_suite/test_harness.cc
test_suite_test_baseline_ujg_LDADD = libtestdriver.a -lpthread

test_suite_test_baseline_SOURCES = test_suite/test_harness.cc
test_suite_test_baseline_CXXFLAGS = -DUSE_LEPTON
test_suite_test_baseline_LDADD = libtestdriver.a -lpthread

test_suite_test_iphone_SOURCES = test_suite/test_harness.cc
test_suite_test_iphone_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=iphone -DTEST_FILE0=iphonecity -DTEST_FILE1=android
test_suite_test_iphone_LDADD = libtestdriver.a -lpthread

test_suite_test_SLR_SOURCES = test_suite/test_harness.cc
test_suite_test_SLR_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=slrindoor -DTEST_FILE0=slrhills -DTEST_FILE1=slrcity
test_suite_test_SLR_LDADD = libtestdriver.a -lpthread

test_suite_test_hq_SOURCES = test_suite/test_harness.cc
test_suite_test_hq_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=hq
test_suite_test_hq_LDADD = libtestdriver.a -lpthread

test_suite_test_misc_SOURCES = test_suite/test_harness.cc
test_suite_test_misc_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=androidcrop -DTEST_FILE0=androidcropoptions -DTEST_FILE1=iphonecrop -DTEST_FILE2=iphonecrop2
test_suite_test_misc_LDADD = libtestdriver.a -lpthread

test_suite_test_arithmetic_failfast_SOURCES = test_suite/test_harness.cc
test_suite_test_arithmetic_failfast_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=arithmetic -DEXPECT_FAILURE
test_suite_test_arithmetic_failfast_LDADD = libtestdriver.a -lpthread

test_suite_test_progressive_ujg_SOURCES = test_suite/test_harness.cc
test_suite_test_progressive_ujg_CXXFLAGS = -DTEST_FILE=iphoneprogressive -DTEST_FILE0=androidprogressive -DTEST_FILE1=iphoneprogressive2
test_suite_test_progressive_ujg_LDADD = libtestdriver.a -lpthread

test_suite_test_progressive_SOURCES = test_suite/test_harness.cc
test_suite_test_progressive_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=iphoneprogressive -DTEST_FILE0=androidprogressive -DTEST_FILE1=iphoneprogressive2
test_suite_test_progressive_LDADD = libtestdriver.a -lpthread

test_suite_test_truncate_ujg_SOURCES = test_suite/test_harness.cc
test_suite_test_truncate_ujg_CXXFLAGS = -DTEST_FILE=trunc
test_suite_test_truncate_ujg_LDADD = libtestdriver.a -lpthread

test_suite_test_truncate_SOURCES = test_suite/test_harness.cc
test_suite_test_truncate_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=trunc
test_suite_test_truncate_LDADD = libtestdriver.a -lpthread

TESTS = test_suite/test_invariants test_suite/test_baseline_ujg test_suite/test_baseline test_suite/test_misc test_suite/test_iphone test_suite/test_truncate_ujg test_suite/test_truncate test_suite/test_SLR test_suite/test_progressive_ujg test_suite/test_progressive test_suite/test_arithmetic_failfast test_suite/test_hq test_suite/forktester.py test_suite/sockettester.py src/lepton/test_custom_table.sh

dist_check_SCRIPTS = test_suite/test_invariants test_suite/test_baseline_ujg test_suite/test_baseline test_suite/test_misc test_suite/test_iphone test_suite/test_truncate_ujg test_suite/test_truncate test_suite/test_SLR test_suite/test_progressive_ujg test_suite/test_progressive test_suite/test_arithmetic_failfast test_suite/test_hq
