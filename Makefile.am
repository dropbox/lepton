noinst_LIBRARIES = liblocalxz.a libtestdriver.a
#libio.a libdecoder.a libencoder.a libmodel.a libutil.a
bin_PROGRAMS = lepton test_invariants
lepton_CXXFLAGS = $(CXX11_FLAGS)  -I$(srcdir)/dependencies/xz/src/liblzma/api -I$(srcdir)/src/vp8/util -I$(srcdir)/src/vp8/model -I$(srcdir)/src/vp8/decoder -I$(srcdir)/src/vp8/encoder $(PICKY_CXXFLAGS)
lepton_LDADD = -lpthread liblocalxz.a
lepton_SOURCES = \
   src/lepton/base_coders.hh \
   src/lepton/simple_encoder.hh \
   src/lepton/bitops.cc \
   src/lepton/smalljpeg.hh \
   src/lepton/bitops.hh \
   src/lepton/test_driver.cc \
   src/lepton/component_info.hh \
   src/lepton/htables.hh \
   src/lepton/fork_serve.cc \
   src/lepton/fork_serve.hh \
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
   src/io/Compression.cc \
   src/io/Compression.hh \
   src/io/SwitchableCompression.hh \
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
   src/vp8/util/memory.cc \
   src/vp8/util/memory.hh \
   src/vp8/util/nd_array.hh \
   src/vp8/util/aligned_block.hh \
   src/vp8/util/block_based_image.hh \
   src/vp8/util/option.hh \
   src/vp8/model/block.hh \
   src/vp8/model/JpegArithmeticCoder.cc \
   src/vp8/model/JpegArithmeticCoder.hh \
   src/vp8/model/branch.hh \
   src/vp8/model/legacy.hh \
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
liblocalxz_a_SOURCES = dependencies/xz/src/liblzma/delta/delta_common.c \
dependencies/xz/src/liblzma/delta/delta_encoder.c \
dependencies/xz/src/liblzma/delta/delta_decoder.c \
dependencies/xz/src/liblzma/rangecoder/price_table.c \
dependencies/xz/src/liblzma/lz/lz_encoder_mf.c \
dependencies/xz/src/liblzma/lz/lz_decoder.c \
dependencies/xz/src/liblzma/lz/lz_encoder.c \
dependencies/xz/src/liblzma/check/sha256.c \
dependencies/xz/src/liblzma/check/crc32_table.c \
dependencies/xz/src/liblzma/check/check.c \
dependencies/xz/src/liblzma/check/crc64_table.c \
dependencies/xz/src/liblzma/check/crc64_fast.c \
dependencies/xz/src/liblzma/check/crc32_fast.c \
dependencies/xz/src/liblzma/lzma/lzma_encoder_optimum_fast.c \
dependencies/xz/src/liblzma/lzma/fastpos_table.c \
dependencies/xz/src/liblzma/lzma/lzma_encoder_presets.c \
dependencies/xz/src/liblzma/lzma/lzma_encoder_optimum_normal.c \
dependencies/xz/src/liblzma/lzma/lzma2_encoder.c \
dependencies/xz/src/liblzma/lzma/lzma_encoder.c \
dependencies/xz/src/liblzma/lzma/lzma_decoder.c \
dependencies/xz/src/liblzma/lzma/lzma2_decoder.c \
dependencies/xz/src/liblzma/simple/sparc.c \
dependencies/xz/src/liblzma/simple/arm.c \
dependencies/xz/src/liblzma/simple/ia64.c \
dependencies/xz/src/liblzma/simple/armthumb.c \
dependencies/xz/src/liblzma/simple/simple_decoder.c \
dependencies/xz/src/liblzma/simple/powerpc.c \
dependencies/xz/src/liblzma/simple/x86.c \
dependencies/xz/src/liblzma/simple/simple_coder.c \
dependencies/xz/src/liblzma/simple/simple_encoder.c \
dependencies/xz/src/liblzma/common/block_encoder.c \
dependencies/xz/src/liblzma/common/alone_encoder.c \
dependencies/xz/src/liblzma/common/easy_encoder.c \
dependencies/xz/src/liblzma/common/stream_buffer_decoder.c \
dependencies/xz/src/liblzma/common/filter_decoder.c \
dependencies/xz/src/liblzma/common/block_header_encoder.c \
dependencies/xz/src/liblzma/common/auto_decoder.c \
dependencies/xz/src/liblzma/common/easy_decoder_memusage.c \
dependencies/xz/src/liblzma/common/index.c \
dependencies/xz/src/liblzma/common/easy_encoder_memusage.c \
dependencies/xz/src/liblzma/common/stream_decoder.c \
dependencies/xz/src/liblzma/common/stream_encoder.c \
dependencies/xz/src/liblzma/common/vli_encoder.c \
dependencies/xz/src/liblzma/common/easy_buffer_encoder.c \
dependencies/xz/src/liblzma/common/hardware_cputhreads.c \
dependencies/xz/src/liblzma/common/easy_preset.c \
dependencies/xz/src/liblzma/common/filter_common.c \
dependencies/xz/src/liblzma/common/vli_size.c \
dependencies/xz/src/liblzma/common/vli_decoder.c \
dependencies/xz/src/liblzma/common/block_buffer_decoder.c \
dependencies/xz/src/liblzma/common/block_header_decoder.c \
dependencies/xz/src/liblzma/common/stream_buffer_encoder.c \
dependencies/xz/src/liblzma/common/index_hash.c \
dependencies/xz/src/liblzma/common/filter_flags_encoder.c \
dependencies/xz/src/liblzma/common/stream_flags_encoder.c \
dependencies/xz/src/liblzma/common/filter_buffer_encoder.c \
dependencies/xz/src/liblzma/common/outqueue.c \
dependencies/xz/src/liblzma/common/alone_decoder.c \
dependencies/xz/src/liblzma/common/filter_flags_decoder.c \
dependencies/xz/src/liblzma/common/common.c \
dependencies/xz/src/liblzma/common/block_util.c \
dependencies/xz/src/liblzma/common/block_buffer_encoder.c \
dependencies/xz/src/liblzma/common/hardware_physmem.c \
dependencies/xz/src/liblzma/common/block_decoder.c \
dependencies/xz/src/liblzma/common/stream_flags_decoder.c \
dependencies/xz/src/liblzma/common/index_encoder.c \
dependencies/xz/src/liblzma/common/stream_flags_common.c \
dependencies/xz/src/liblzma/common/index_decoder.c \
dependencies/xz/src/liblzma/common/filter_buffer_decoder.c \
dependencies/xz/src/liblzma/common/filter_encoder.c \
dependencies/xz/src/common/tuklib_cpucores.c \
dependencies/xz/src/common/tuklib_physmem.c

DEFAULT_INCLUDES=-I$(srcdir)/src



#noinst_LIBRARIES = libtestdriver.a
libtestdriver_a_SOURCES = src/lepton/test_driver.cc

test_invariants_SOURCES = src/lepton/test_invariants.cc    src/io/Allocator.hh \
   src/io/BufferedIO.hh \
   src/io/Compression.cc \
   src/io/Compression.hh \
   src/io/SwitchableCompression.hh \
   src/io/Error.hh \
   src/io/Reader.hh \
   src/io/MuxReader.hh \
   src/io/ioutil.hh \
   src/io/ioutil.cc \
   src/io/Zlib0.hh \
   src/io/Zlib0.cc \
   src/io/MemMgrAllocator.cc \
   src/io/MemMgrAllocator.hh \
   src/io/DecoderPlatform.hh
test_invariants_CXXFLAGS = $(CXX11_FLAGS) -I$(srcdir)/dependencies/xz/src/liblzma/api -I$(srcdir)/src/vp8/util -I$(srcdir)/src/vp8/model -I$(srcdir)/src/vp8/decoder -I$(srcdir)/src/vp8/encoder $(PICKY_CXXFLAGS)
test_invariants_LDADD = liblocalxz.a


check_PROGRAMS = test_invariants test_baseline_ujg test_baseline test_misc test_iphone test_truncate_ujg test_truncate test_slr test_progressive_ujg test_progressive test_arithmetic_failfast test_hq


test_baseline_ujg_SOURCES = src/lepton/test_harness.cc
test_baseline_ujg_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_baseline_SOURCES = src/lepton/test_harness.cc
test_baseline_CXXFLAGS = -DUSE_LEPTON
test_baseline_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_iphone_SOURCES = src/lepton/test_harness.cc
test_iphone_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=iphone -DTEST_FILE0=iphonecity -DTEST_FILE1=android
test_iphone_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_slr_SOURCES = src/lepton/test_harness.cc
test_slr_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=slrindoor -DTEST_FILE0=slrhills -DTEST_FILE1=slrcity
test_slr_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_hq_SOURCES = src/lepton/test_harness.cc
test_hq_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=hq
test_hq_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_misc_SOURCES = src/lepton/test_harness.cc
test_misc_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=androidcrop -DTEST_FILE0=androidcropoptions -DTEST_FILE1=iphonecrop -DTEST_FILE2=iphonecrop2
test_misc_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_arithmetic_failfast_SOURCES = src/lepton/test_harness.cc
test_arithmetic_failfast_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=arithmetic -DEXPECT_FAILURE
test_arithmetic_failfast_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_progressive_ujg_SOURCES = src/lepton/test_harness.cc
test_progressive_ujg_CXXFLAGS = -DTEST_FILE=iphoneprogressive -DTEST_FILE0=androidprogressive -DTEST_FILE1=iphoneprogressive2
test_progressive_ujg_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_progressive_SOURCES = src/lepton/test_harness.cc
test_progressive_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=iphoneprogressive -DTEST_FILE0=androidprogressive -DTEST_FILE1=iphoneprogressive2
test_progressive_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_truncate_ujg_SOURCES = src/lepton/test_harness.cc
test_truncate_ujg_CXXFLAGS = -DTEST_FILE=trunc
test_truncate_ujg_LDADD = libtestdriver.a liblocalxz.a -lpthread

test_truncate_SOURCES = src/lepton/test_harness.cc
test_truncate_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=trunc
test_truncate_LDADD = libtestdriver.a liblocalxz.a -lpthread

TESTS = test_invariants test_baseline_ujg test_baseline test_misc test_iphone test_truncate_ujg test_truncate test_slr test_progressive_ujg test_progressive test_arithmetic_failfast test_hq
dist_check_SCRIPTS = test_invariants test_baseline_ujg src/lepton/test_custom_table.sh test_baseline test_misc test_iphone test_truncate_ujg test_truncate test_slr test_progressive_ujg test_progressive test_arithmetic_failfast test_hq
