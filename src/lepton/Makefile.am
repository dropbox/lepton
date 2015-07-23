AM_CPPFLAGS = $(CXX11_FLAGS) -I$(srcdir)/../vp8/util -I$(srcdir)/../vp8/model -I$(srcdir)/../vp8/decoder -I$(srcdir)/../vp8/encoder $(shell pkg-config liblzma --cflags || echo -DNO_XZ)
AM_CXXFLAGS = $(PICKY_CXXFLAGS)
LDADD = -lpthread ../vp8/encoder/libleptonencoder.a ../vp8/decoder/libleptondecoder.a ../vp8/encoder/libleptonencoder.a ../vp8/model/libleptonmodel.a ../io/libio.a ../vp8/util/libleptonutil.a $(liblzma_LIBS)

lepton_CXXFLAGS=$(liblzma_CFLAGS)
lepton_LDFLAGS=

#$(shell uname -s | grep -q Darwin || echo -static)
common_source = bitops.cc bitops.hh \
	component_info.hh htables.hh jpgcoder.cc jpgcoder.hh \
	uncompressed_components.cc uncompressed_components.hh \
	simple_decoder.hh simple_decoder.cc \
	simple_encoder.hh simple_encoder.cc \
	vp8_decoder.hh vp8_decoder.cc \
	vp8_encoder.hh vp8_encoder.cc

bin_PROGRAMS = lepton print-model
noinst_LIBRARIES = libtestdriver.a
libtestdriver_a_SOURCES = test_driver.cc

check_PROGRAMS = test_invariants test_baseline_ujg test_baseline test_misc_ujg test_misc test_iphone_ujg test_iphone test_truncate_ujg test_truncate test_slr_ujg test_slr test_progressive_ujg test_progressive test_arithmetic_failfast_ujg test_arithmetic_failfast test_hq_ujg test_hq

test_invariants_SOURCES = test_invariants.cc

test_baseline_ujg_SOURCES = test_harness.cc
test_baseline_ujg_LDADD = libtestdriver.a -lpthread

test_baseline_SOURCES = test_harness.cc
test_baseline_CXXFLAGS = -DUSE_LEPTON
test_baseline_LDADD = libtestdriver.a -lpthread

test_iphone_ujg_SOURCES = test_harness.cc
test_iphone_ujg_CXXFLAGS = -DTEST_FILE=iphone -DTEST_FILE0=iphonecity -DTEST_FILE1=android
test_iphone_ujg_LDADD = libtestdriver.a -lpthread

test_iphone_SOURCES = test_harness.cc
test_iphone_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=iphone -DTEST_FILE0=iphonecity -DTEST_FILE1=android
test_iphone_LDADD = libtestdriver.a -lpthread

test_slr_ujg_SOURCES = test_harness.cc
test_slr_ujg_CXXFLAGS = -DTEST_FILE=slrindoor -DTEST_FILE0=slrhills -DTEST_FILE1=slrcity
test_slr_ujg_LDADD = libtestdriver.a -lpthread

test_hq_ujg_SOURCES = test_harness.cc
test_hq_ujg_CXXFLAGS = -DTEST_FILE=hq
test_hq_ujg_LDADD = libtestdriver.a -lpthread

test_slr_SOURCES = test_harness.cc
test_slr_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=slrindoor -DTEST_FILE0=slrhills -DTEST_FILE1=slrcity
test_slr_LDADD = libtestdriver.a -lpthread

test_hq_SOURCES = test_harness.cc
test_hq_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=hq
test_hq_LDADD = libtestdriver.a -lpthread

test_misc_ujg_SOURCES = test_harness.cc
test_misc_ujg_CXXFLAGS = -DTEST_FILE=androidcrop -DTEST_FILE0=androidcropoptions -DTEST_FILE1=iphonecrop -DTEST_FILE2=iphonecrop2
test_misc_ujg_LDADD = libtestdriver.a -lpthread

test_misc_SOURCES = test_harness.cc
test_misc_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=androidcrop -DTEST_FILE0=androidcropoptions -DTEST_FILE1=iphonecrop -DTEST_FILE2=iphonecrop2
test_misc_LDADD = libtestdriver.a -lpthread

test_arithmetic_failfast_ujg_SOURCES = test_harness.cc
test_arithmetic_failfast_ujg_CXXFLAGS = -DTEST_FILE=arithmetic -DEXPECT_FAILURE
test_arithmetic_failfast_ujg_LDADD = libtestdriver.a -lpthread

test_arithmetic_failfast_SOURCES = test_harness.cc
test_arithmetic_failfast_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=arithmetic -DEXPECT_FAILURE
test_arithmetic_failfast_LDADD = libtestdriver.a -lpthread

test_progressive_ujg_SOURCES = test_harness.cc
test_progressive_ujg_CXXFLAGS = -DTEST_FILE=iphoneprogressive -DTEST_FILE0=androidprogressive -DTEST_FILE1=iphoneprogressive2
test_progressive_ujg_LDADD = libtestdriver.a -lpthread

test_progressive_SOURCES = test_harness.cc
test_progressive_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=iphoneprogressive -DTEST_FILE0=androidprogressive -DTEST_FILE1=iphoneprogressive2
test_progressive_LDADD = libtestdriver.a -lpthread

test_truncate_ujg_SOURCES = test_harness.cc
test_truncate_ujg_CXXFLAGS = -DTEST_FILE=trunc
test_truncate_ujg_LDADD = libtestdriver.a -lpthread

test_truncate_SOURCES = test_harness.cc
test_truncate_CXXFLAGS = -DUSE_LEPTON -DTEST_FILE=trunc
test_truncate_LDADD = libtestdriver.a -lpthread

lepton_SOURCES = $(common_source)

print_model_SOURCES = print-model.cc

TESTS = test_baseline_ujg test_baseline ./test_custom_table.sh test_misc_ujg test_misc test_iphone_ujg test_iphone test_truncate_ujg test_truncate test_slr_ujg test_slr test_progressive_ujg test_progressive test_arithmetic_failfast_ujg test_arithmetic_failfast test_hq_ujg test_hq


dist_check_SCRIPTS = test_baseline_ujg ./test_custom_table.sh test_baseline test_misc_ujg test_misc test_iphone_ujg test_iphone test_truncate_ujg test_truncate test_slr_ujg test_slr test_progressive_ujg test_progressive test_arithmetic_failfast_ujg test_arithmetic_failfast test_hq_ujg test_hq

