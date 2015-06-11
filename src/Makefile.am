AM_CPPFLAGS = $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS)
LDADD = -lpthread

common_source = bitops.cc bitops.hh \
	component_info.hh htables.hh jpgcoder.cc jpgcoder.hh \
	uncompressed_components.cc uncompressed_components.hh \
	simple_decoder.hh simple_decoder.cc \
	simple_encoder.hh simple_encoder.cc \
	vp8_decoder.hh vp8_decoder.cc \
	vp8_encoder.hh vp8_encoder.cc

bin_PROGRAMS = lepton
lepton_SOURCES = $(common_source)
