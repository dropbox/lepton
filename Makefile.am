SUBDIRS = src
DEFAULT_INCLUDES=-Isrc
liblocalxz_a_CFLAGS = -std=c99 -msse4.2 -DHAVE_CONFIG_H -Idependencies/xz/src/liblzma/common -Idependencies/xz/src/common -Idependencies -Idependencies/xz/src/liblzma/lzma -Idependencies/xz/src/liblzma/lz -Idependencies/xz/src/liblzma/check -Idependencies/xz/src/liblzma/rangecoder -Idependencies/xz/src/liblzma/api -Idependencies/xz/src/liblzma/simple -Idependencies/xz/src/liblzma/delta -Idependencies/xz/src/liblzma

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
noinst_LIBRARIES = liblocalxz.a

liblzma_LIBS=../../liblocalxz.a
liblzma_CFLAGS=-I../../dependencies/xz/src/liblzma/api
