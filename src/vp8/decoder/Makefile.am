AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../model $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libleptondecoder.a

libleptondecoder_a_SOURCES = block.hh bool_decoder.hh decoder.hh \
	coefs.cc coefs.hh \
	jpeg_meta.hh vp8_prob_data.hh
