includes = -I$(srcdir)/dependencies/xz/src/liblzma/api -I$(srcdir)/src/vp8/util -I$(srcdir)/src/vp8/model -I$(srcdir)/src/vp8/decoder -I$(srcdir)/src/vp8/encoder -I$(srcdir)/dependencies -I$(srcdir)/dependencies -I$(srcdir)/dependencies/brotli/c/include -I$(srcdir)/dependencies/xz/src/liblzma/common -I$(srcdir)/dependencies/xz/src/common -I$(srcdir)/dependencies/xz/src/liblzma/lzma -I$(srcdir)/dependencies/xz/src/liblzma/lz -I$(srcdir)/dependencies/xz/src/liblzma/check -I$(srcdir)/dependencies/xz/src/liblzma/rangecoder -I$(srcdir)/dependencies/xz/src/liblzma/api -I$(srcdir)/dependencies/xz/src/liblzma/simple -I$(srcdir)/dependencies/xz/src/liblzma/delta -I$(srcdir)/dependencies/xz/src/liblzma



liblocalmd5_a_SOURCES = dependencies/md5/md5.c

AM_CXXFLAGS = $(CXX11_FLAGS) $(CODEC_FLAGS) $(SYSTEM_DEPENDENCIES_CFLAGS) $(ARITHMETIC_CODER_CFLAGS) $(MEMORY_MANAGEMENT_CFLAGS) $(THREAD_FLAGS) $(BILLING_FLAGS) $(PICKY_CXXFLAGS) $(BENCHMARK_CFLAGS) $(ARCH_FLAGS) $(SANITIZE_FLAGS) $(NODEBUG_CXXFLAGS) -DGIT_REVISION=\"$(shell git describe --dirty --always 2> /dev/null || basename `pwd`)\" $(includes)

AM_CFLAGS = $(C99_FLAGS) $(CODEC_FLAGS) $(THREAD_FLAGS) $(BILLING_FLAGS) $(BENCHMARK_CFLAGS) $(PICKY_CFLAGS) $(ARCH_FLAGS) $(SANITIZE_FLAGS) $(NODEBUG_CXXFLAGS) -I$(srcdir)/dependencies/brotli/c/include

noinst_LIBRARIES = liblocalzlib.a liblocalmd5.a libtestdriver.a liblocalbrotli.a

bin_PROGRAMS = lepton
noinst_PROGRAMS = test_suite/test_invariants

lepton_LDADD = liblocalmd5.a liblocalbrotli.a $(SYSTEM_DEPENDENCIES_LDFLAGS) -lpthread

lepton_SOURCES = \
   src/io/MemReadWriter.cc \
   src/lepton/base_coders.hh \
   src/lepton/simple_encoder.hh \
   src/lepton/bitops.cc \
   src/lepton/smalljpg.hh \
   src/lepton/bitops.hh \
   src/lepton/benchmark.cc \
   src/lepton/idct.cc \
   src/lepton/idct.hh \
   src/lepton/component_info.hh \
   src/lepton/htables.hh \
   src/lepton/fork_serve.cc \
   src/lepton/fork_serve.hh \
   src/lepton/thread_handoff.cc \
   src/lepton/thread_handoff.hh \
   src/lepton/socket_serve.cc \
   src/lepton/socket_serve.hh \
   src/lepton/jpgcoder.cc \
   src/lepton/concat.cc \
   src/lepton/main.cc \
   src/lepton/validation.hh \
   src/lepton/validation.cc \
   src/lepton/generic_compress.hh \
   src/lepton/generic_compress.cc \
   src/lepton/recoder.cc \
   src/lepton/uncompressed_components.cc \
   src/lepton/jpgcoder.hh \
   src/lepton/recoder.hh \
   src/lepton/uncompressed_components.hh \
   src/lepton/lepton_codec.cc \
   src/lepton/lepton_codec.hh \
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
   src/io/BrotliCompression.cc \
   src/io/BrotliCompression.hh \
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
   src/vp8/util/billing.cc \
   src/vp8/util/billing.hh \
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


liblocalbrotli_a_SOURCES =     dependencies/brotli/c/include/brotli/encode.h \
    dependencies/brotli/c/include/brotli/types.h \
    dependencies/brotli/c/include/brotli/decode.h \
    dependencies/brotli/c/include/brotli/port.h \
    dependencies/brotli/c/common/constants.h \
    dependencies/brotli/c/common/version.h \
    dependencies/brotli/c/common/dictionary.h \
    dependencies/brotli/c/common/dictionary.c \
    dependencies/brotli/c/enc/block_encoder_inc.h \
    dependencies/brotli/c/enc/memory.c \
    dependencies/brotli/c/enc/bit_cost.h \
    dependencies/brotli/c/enc/ringbuffer.h \
    dependencies/brotli/c/enc/entropy_encode.c \
    dependencies/brotli/c/enc/quality.h \
    dependencies/brotli/c/enc/utf8_util.h \
    dependencies/brotli/c/enc/block_splitter_inc.h \
    dependencies/brotli/c/enc/block_splitter.h \
    dependencies/brotli/c/enc/compress_fragment.c \
    dependencies/brotli/c/enc/utf8_util.c \
    dependencies/brotli/c/enc/cluster_inc.h \
    dependencies/brotli/c/enc/cluster.h \
    dependencies/brotli/c/enc/literal_cost.c \
    dependencies/brotli/c/enc/static_dict.c \
    dependencies/brotli/c/enc/backward_references_hq.c \
    dependencies/brotli/c/enc/hash.h \
    dependencies/brotli/c/enc/literal_cost.h \
    dependencies/brotli/c/enc/compress_fragment_two_pass.c \
    dependencies/brotli/c/enc/dictionary_hash.c \
    dependencies/brotli/c/enc/entropy_encode.h \
    dependencies/brotli/c/enc/command.h \
    dependencies/brotli/c/enc/metablock_inc.h \
    dependencies/brotli/c/enc/context.h \
    dependencies/brotli/c/enc/metablock.h \
    dependencies/brotli/c/enc/hash_longest_match_quickly_inc.h \
    dependencies/brotli/c/enc/hash_longest_match_inc.h \
    dependencies/brotli/c/enc/hash_to_binary_tree_inc.h \
    dependencies/brotli/c/enc/backward_references.h \
    dependencies/brotli/c/enc/find_match_length.h \
    dependencies/brotli/c/enc/prefix.h \
    dependencies/brotli/c/enc/static_dict.h \
    dependencies/brotli/c/enc/cluster.c \
    dependencies/brotli/c/enc/brotli_bit_stream.h \
    dependencies/brotli/c/enc/bit_cost_inc.h \
    dependencies/brotli/c/enc/metablock.c \
    dependencies/brotli/c/enc/backward_references_hq.h \
    dependencies/brotli/c/enc/write_bits.h \
    dependencies/brotli/c/enc/entropy_encode_static.h \
    dependencies/brotli/c/enc/histogram.c \
    dependencies/brotli/c/enc/encode.c \
    dependencies/brotli/c/enc/port.h \
    dependencies/brotli/c/enc/compress_fragment.h \
    dependencies/brotli/c/enc/static_dict_lut.h \
    dependencies/brotli/c/enc/histogram.h \
    dependencies/brotli/c/enc/hash_forgetful_chain_inc.h \
    dependencies/brotli/c/enc/block_splitter.c \
    dependencies/brotli/c/enc/brotli_bit_stream.c \
    dependencies/brotli/c/enc/compress_fragment_two_pass.h \
    dependencies/brotli/c/enc/backward_references_inc.h \
    dependencies/brotli/c/enc/bit_cost.c \
    dependencies/brotli/c/enc/backward_references.c \
    dependencies/brotli/c/enc/histogram_inc.h \
    dependencies/brotli/c/enc/hash_longest_match64_inc.h \
    dependencies/brotli/c/enc/fast_log.h \
    dependencies/brotli/c/enc/memory.h \
    dependencies/brotli/c/enc/dictionary_hash.h \
    dependencies/brotli/c/dec/state.h \
    dependencies/brotli/c/dec/huffman.h \
    dependencies/brotli/c/dec/transform.h \
    dependencies/brotli/c/dec/state.c \
    dependencies/brotli/c/dec/bit_reader.c \
    dependencies/brotli/c/dec/huffman.c \
    dependencies/brotli/c/dec/context.h \
    dependencies/brotli/c/dec/bit_reader.h \
    dependencies/brotli/c/dec/prefix.h \
    dependencies/brotli/c/dec/port.h \
    dependencies/brotli/c/dec/decode.c


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
dependencies/zlib/zutil.h \
dependencies/zlib/zlib.h \
dependencies/zlib/inffixed.h \
dependencies/zlib/deflate.c \
dependencies/zlib/inffast.h \
dependencies/zlib/inffast.c \
dependencies/zlib/uncompr.c \
dependencies/zlib/deflate.h

libtestdriver_a_SOURCES = test_suite/timing_driver.cc

test_suite_test_invariants_SOURCES = test_suite/test_invariants.cc    src/io/Allocator.hh \
   src/vp8/util/memory.cc \
   src/io/BufferedIO.hh \
   src/io/Error.hh \
   src/io/Reader.hh \
   src/io/MuxReader.hh \
   src/io/ioutil.hh \
   src/vp8/util/billing.cc \
   src/vp8/util/billing.hh \
   src/vp8/model/numeric.cc \
   src/vp8/model/numeric.hh \
   src/io/ioutil.cc \
   src/io/Zlib0.hh \
   src/io/Zlib0.cc \
   src/lepton/thread_handoff.cc \
   src/lepton/thread_handoff.hh \
   src/io/MemReadWriter.cc \
   src/io/MemMgrAllocator.cc \
   src/io/MemMgrAllocator.hh \
   src/io/DecoderPlatform.hh

test_suite_test_invariants_LDADD = $(SYSTEM_DEPENDENCIES_LDFLAGS) liblocalmd5.a

check_PROGRAMS = test_suite/test_recode_memory_bound test_suite/test_truncate_lowmem test_suite/test_android_lowmem test_suite/test_invariants test_suite/test_baseline_ujg test_suite/test_baseline test_suite/test_misc test_suite/test_iphone test_suite/test_phone_outdoor test_suite/test_truncate test_suite/test_single_row_truncate test_suite/test_SLR test_suite/test_progressive_ujg test_suite/test_progressive_disallowed test_suite/test_progressive test_suite/test_arithmetic_failfast test_suite/test_hq test_suite/test_baseline_unjailed test_suite/test_baseline_unjailed_thread test_suite/test_baseline_unjailed_decode test_suite/test_baseline_unjailed_decode_thread test_suite/test_seccomp_encode_main test_suite/test_seccomp_encode_thread  test_suite/test_seccomp_decode_main test_suite/test_seccomp_decode_thread test_suite/test_nofsync test_suite/test_colorswap test_suite/test_odd_rst test_suite/test_trailing_header test_suite/test_trailing_rst test_suite/test_gray2sf test_suite/test_truncated_zero_run test_suite/test_bad_zero_run

test_suite_test_baseline_ujg_SOURCES = test_suite/test_harness.cc
test_suite_test_baseline_ujg_LDADD = libtestdriver.a -lpthread

test_suite_test_baseline_SOURCES = test_suite/test_harness.cc
test_suite_test_baseline_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON
test_suite_test_baseline_LDADD = libtestdriver.a -lpthread

test_suite_test_baseline_unjailed_SOURCES = test_suite/test_harness.cc
test_suite_test_baseline_unjailed_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DUNJAILED -DINJECT_SYSCALL=1
test_suite_test_baseline_unjailed_LDADD = libtestdriver.a -lpthread

test_suite_test_baseline_unjailed_thread_SOURCES = test_suite/test_harness.cc
test_suite_test_baseline_unjailed_thread_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DUNJAILED -DINJECT_SYSCALL=2
test_suite_test_baseline_unjailed_thread_LDADD = libtestdriver.a -lpthread

test_suite_test_baseline_unjailed_decode_SOURCES = test_suite/test_harness.cc
test_suite_test_baseline_unjailed_decode_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DUNJAILED -DINJECT_SYSCALL=3
test_suite_test_baseline_unjailed_decode_LDADD = libtestdriver.a -lpthread

test_suite_test_baseline_unjailed_decode_thread_SOURCES = test_suite/test_harness.cc
test_suite_test_baseline_unjailed_decode_thread_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DUNJAILED -DINJECT_SYSCALL=4
test_suite_test_baseline_unjailed_decode_thread_LDADD = libtestdriver.a -lpthread

test_suite_test_seccomp_encode_main_SOURCES = test_suite/test_harness.cc
test_suite_test_seccomp_encode_main_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DINJECT_SYSCALL=1 -DEXPECT_LINUX_FAILURE
test_suite_test_seccomp_encode_main_LDADD = libtestdriver.a -lpthread

test_suite_test_seccomp_encode_thread_SOURCES = test_suite/test_harness.cc
test_suite_test_seccomp_encode_thread_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DINJECT_SYSCALL=2 -DEXPECT_LINUX_FAILURE
test_suite_test_seccomp_encode_thread_LDADD = libtestdriver.a -lpthread

test_suite_test_seccomp_decode_main_SOURCES = test_suite/test_harness.cc
test_suite_test_seccomp_decode_main_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DINJECT_SYSCALL=3 -DEXPECT_LINUX_DECODE_FAILURE
test_suite_test_seccomp_decode_main_LDADD = libtestdriver.a -lpthread

test_suite_test_seccomp_decode_thread_SOURCES = test_suite/test_harness.cc
test_suite_test_seccomp_decode_thread_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DINJECT_SYSCALL=4 -DEXPECT_LINUX_DECODE_FAILURE
test_suite_test_seccomp_decode_thread_LDADD = libtestdriver.a -lpthread

test_suite_test_phone_outdoor_SOURCES = test_suite/test_harness.cc
test_suite_test_phone_outdoor_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=iphonecity -DZLIB_HEADER
test_suite_test_phone_outdoor_LDADD = libtestdriver.a -lpthread

test_suite_test_trailing_header_SOURCES = test_suite/test_harness.cc
test_suite_test_trailing_header_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=androidtrail
test_suite_test_trailing_header_LDADD = libtestdriver.a -lpthread

test_suite_test_iphone_SOURCES = test_suite/test_harness.cc
test_suite_test_iphone_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=iphone
test_suite_test_iphone_LDADD = libtestdriver.a -lpthread

test_suite_test_SLR_SOURCES = test_suite/test_harness.cc
test_suite_test_SLR_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=slrindoor -DTEST_FILE0=slrhills -DTEST_FILE1=slrcity -DMORE_MEMORY=\"256M\"
test_suite_test_SLR_LDADD = libtestdriver.a -lpthread

test_suite_test_hq_SOURCES = test_suite/test_harness.cc
test_suite_test_hq_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=hq -DMORE_MEMORY=\"360M\" -DTHREAD_MEMORY=\"24M\" -DSINGLETHREAD_MEMORY=\"87M\"
test_suite_test_hq_LDADD = libtestdriver.a -lpthread

test_suite_test_misc_SOURCES = test_suite/test_harness.cc
test_suite_test_misc_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=androidcrop -DTEST_FILE0=androidcropoptions -DTEST_FILE1=iphonecrop -DTEST_FILE2=iphonecrop2 -DZLIB_HEADER

test_suite_test_misc_LDADD = libtestdriver.a -lpthread

test_suite_test_arithmetic_failfast_SOURCES = test_suite/test_harness.cc
test_suite_test_arithmetic_failfast_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=arithmetic -DEXPECT_FAILURE
test_suite_test_arithmetic_failfast_LDADD = libtestdriver.a -lpthread

test_suite_test_recode_memory_bound_SOURCES = test_suite/test_harness.cc
test_suite_test_recode_memory_bound_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=android -DEXPECT_FAILURE -DSINGLETHREAD_MEMORY=\"6M\"
test_suite_test_recode_memory_bound_LDADD = libtestdriver.a -lpthread

test_suite_test_progressive_ujg_SOURCES = test_suite/test_harness.cc
test_suite_test_progressive_ujg_CXXFLAGS = $(AM_CXXFLAGS) -DTEST_FILE=iphoneprogressive -DTEST_FILE0=androidprogressive -DTEST_FILE1=iphoneprogressive2 -DALLOW_PROGRESSIVE
test_suite_test_progressive_ujg_LDADD = libtestdriver.a -lpthread

test_suite_test_progressive_SOURCES = test_suite/test_harness.cc
test_suite_test_progressive_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=iphoneprogressive -DTEST_FILE0=androidprogressive -DTEST_FILE1=iphoneprogressive2 -DALLOW_PROGRESSIVE
test_suite_test_progressive_LDADD = libtestdriver.a -lpthread

test_suite_test_progressive_disallowed_SOURCES = test_suite/test_harness.cc
test_suite_test_progressive_disallowed_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=iphoneprogressive -DEXPECT_FAILURE -DDISALLOW_PROGRESSIVE
test_suite_test_progressive_disallowed_LDADD = libtestdriver.a -lpthread

test_suite_test_truncate_SOURCES = test_suite/test_harness.cc
test_suite_test_truncate_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=trunc
test_suite_test_truncate_LDADD = libtestdriver.a -lpthread

test_suite_test_single_row_truncate_SOURCES = test_suite/test_harness.cc
test_suite_test_single_row_truncate_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=singlerowtrunc
test_suite_test_single_row_truncate_LDADD = libtestdriver.a -lpthread

test_suite_test_truncate_lowmem_SOURCES = test_suite/test_harness.cc
test_suite_test_truncate_lowmem_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=trunc -DSINGLETHREAD -DDECODE_MEMORY=\"18M\" -DTHREAD_MEMORY=\"0M\"
test_suite_test_truncate_lowmem_LDADD = libtestdriver.a -lpthread

test_suite_test_android_lowmem_SOURCES = test_suite/test_harness.cc
test_suite_test_android_lowmem_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=android -DTEST_FILE0=grayscale -DSINGLETHREAD -DDECODE_MEMORY=\"18M\" -DTHREAD_MEMORY=\"0M\"
test_suite_test_android_lowmem_LDADD = libtestdriver.a -lpthread

test_suite_test_nofsync_SOURCES = test_suite/test_harness.cc
test_suite_test_nofsync_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=nofsync -DTEST_FILE0=grayscale
test_suite_test_nofsync_LDADD = libtestdriver.a -lpthread

test_suite_test_gray2sf_SOURCES = test_suite/test_harness.cc
test_suite_test_gray2sf_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=gray2sf
test_suite_test_gray2sf_LDADD = libtestdriver.a -lpthread

test_suite_test_colorswap_SOURCES = test_suite/test_harness.cc
test_suite_test_colorswap_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=colorswap -DFORCE_NO_ANS
test_suite_test_colorswap_LDADD = libtestdriver.a -lpthread

test_suite_test_odd_rst_SOURCES = test_suite/test_harness.cc
test_suite_test_odd_rst_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=narrowrst
test_suite_test_odd_rst_LDADD = libtestdriver.a -lpthread
test_suite_test_truncated_zero_run_SOURCES = test_suite/test_harness.cc
test_suite_test_truncated_zero_run_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=truncatedzerorun
test_suite_test_truncated_zero_run_LDADD = libtestdriver.a -lpthread

test_suite_test_bad_zero_run_SOURCES = test_suite/test_harness.cc
test_suite_test_bad_zero_run_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=badzerorun -DEXPECT_FAILURE
test_suite_test_bad_zero_run_LDADD = libtestdriver.a -lpthread
test_suite_test_trailing_rst_SOURCES = test_suite/test_harness.cc
test_suite_test_trailing_rst_CXXFLAGS = $(AM_CXXFLAGS) -DUSE_LEPTON -DTEST_FILE=trailingrst -DTEST_FILE0=trailingrst2
test_suite_test_trailing_rst_LDADD = libtestdriver.a -lpthread

TESTS = test_suite/test_recode_memory_bound test_suite/test_invariants test_suite/test_baseline_ujg test_suite/test_baseline test_suite/test_misc test_suite/test_iphone test_suite/test_phone_outdoor test_suite/test_truncate test_suite/test_single_row_truncate test_suite/test_android_lowmem test_suite/test_SLR test_suite/test_progressive_ujg test_suite/test_progressive_disallowed test_suite/test_progressive test_suite/test_arithmetic_failfast test_suite/test_hq test_suite/forktester.py test_suite/sockettester.py src/lepton/test_custom_table.sh test_suite/test_baseline_unjailed test_suite/test_baseline_unjailed_thread test_suite/test_baseline_unjailed_decode test_suite/test_baseline_unjailed_decode_thread test_suite/test_seccomp_encode_main test_suite/test_seccomp_encode_thread  test_suite/test_seccomp_decode_main test_suite/test_seccomp_decode_thread test_suite/test_truncate_lowmem test_suite/test_nofsync test_suite/test_colorswap test_suite/test_odd_rst test_suite/test_trailing_header test_suite/test_trailing_rst test_suite/test_legacy.sh test_suite/test_roundtrip.sh test_suite/test_embedded.sh test_suite/test_16threads.sh test_suite/test_future_compat.sh test_suite/test_gray2sf test_suite/test_2nd_block.sh test_suite/test_3rd_block.sh test_suite/test_last_block.sh test_suite/test_truncated_zero_run test_suite/test_bad_zero_run test_suite/test_concat.sh test_suite/test_permissive.sh

dist_check_SCRIPTS = test_suite/test_recode_memory_bound test_suite/test_invariants test_suite/test_baseline_ujg test_suite/test_baseline test_suite/test_misc test_suite/test_iphone test_suite/test_phone_outdoor test_suite/test_truncate test_suite/test_single_row_truncate test_suite/test_android_lowmem test_suite/test_SLR test_suite/test_progressive_ujg test_suite/test_progressive_disallowed test_suite/test_progressive test_suite/test_arithmetic_failfast test_suite/test_hq test_suite/test_baseline_unjailed test_suite/test_baseline_unjailed_thread test_suite/test_baseline_unjailed_decode test_suite/test_baseline_unjailed_decode_thread test_suite/test_seccomp_encode_main test_suite/test_seccomp_encode_thread  test_suite/test_seccomp_decode_main test_suite/test_seccomp_decode_thread test_suite/test_truncate_lowmem test_suite/test_nofsync test_suite/test_colorswap test_suite/test_odd_rst test_suite/test_trailing_header test_suite/test_trailing_rst test_suite/test_legacy.sh test_suite/test_roundtrip.sh test_suite/test_embedded.sh test_suite/test_16threads.sh test_suite/test_future_compat.sh test_suite/test_gray2sf test_suite/test_2nd_block.sh test_suite/test_3rd_block.sh test_suite/test_last_block.sh test_suite/test_truncated_zero_run test_suite/test_bad_zero_run test_suite/test_concat.sh test_suite/test_permissive.sh

test:
	$(MAKE) check
