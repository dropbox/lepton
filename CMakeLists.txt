cmake_minimum_required(VERSION 2.8)
project(lepton)
SET(CMAKE_CXX_FLAGS "-std=c++11 -msse4.2 -fno-exceptions -fno-rtti")
SET(CMAKE_C_FLAGS "-std=c99 -msse4.2 -DHAVE_CONFIG_H ")

SET(LIBLZMA_INCLUDE_DIRS)

IF(EXISTS ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/api/lzma.h)
SET(LIBLZMA_INCLUDE_DIRS
    ${LIBLZMA_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/common
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/common
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/lzma
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/lz
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/check
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/rangecoder
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/api
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/simple
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/delta
    ${PROJECT_SOURCE_DIR}/dependencies
    ${PROJECT_SOURCE_DIR}/dependencies/xz/src/liblzma/)
SET(LIBLZMA_LIBRARIES localxz)
ELSE()
IF(APPLE)
# not widely supported on linux yet
find_package(LibLZMA REQUIRED)
ELSE()
find_path(LIBLZMA_INCLUDE_DIRS lzma.h )
find_library(LIBLZMA_LIBRARIES lzma)
IF(NOT WIN32)
  SET(LIBLZMA_LIBRARIES ${LIBLZMA_LIBRARIES} pthread)
ENDIF()

ENDIF()
ENDIF()
add_library(localmd5 dependencies/md5/md5.c)
add_library(localxz
dependencies/xz/src/liblzma/delta/delta_common.c
dependencies/xz/src/liblzma/delta/delta_encoder.c
dependencies/xz/src/liblzma/delta/delta_decoder.c
dependencies/xz/src/liblzma/rangecoder/price_table.c
dependencies/xz/src/liblzma/lz/lz_encoder_mf.c
dependencies/xz/src/liblzma/lz/lz_decoder.c
dependencies/xz/src/liblzma/lz/lz_encoder.c
dependencies/xz/src/liblzma/check/sha256.c
dependencies/xz/src/liblzma/check/crc32_table.c
dependencies/xz/src/liblzma/check/check.c
dependencies/xz/src/liblzma/check/crc64_table.c
dependencies/xz/src/liblzma/check/crc64_fast.c
dependencies/xz/src/liblzma/check/crc32_fast.c
dependencies/xz/src/liblzma/lzma/lzma_encoder_optimum_fast.c
dependencies/xz/src/liblzma/lzma/fastpos_table.c
dependencies/xz/src/liblzma/lzma/lzma_encoder_presets.c
dependencies/xz/src/liblzma/lzma/lzma_encoder_optimum_normal.c
dependencies/xz/src/liblzma/lzma/lzma2_encoder.c
dependencies/xz/src/liblzma/lzma/lzma_encoder.c
dependencies/xz/src/liblzma/lzma/lzma_decoder.c
dependencies/xz/src/liblzma/lzma/lzma2_decoder.c
dependencies/xz/src/liblzma/simple/sparc.c
dependencies/xz/src/liblzma/simple/arm.c
dependencies/xz/src/liblzma/simple/ia64.c
dependencies/xz/src/liblzma/simple/armthumb.c
dependencies/xz/src/liblzma/simple/simple_decoder.c
dependencies/xz/src/liblzma/simple/powerpc.c
dependencies/xz/src/liblzma/simple/x86.c
dependencies/xz/src/liblzma/simple/simple_coder.c
dependencies/xz/src/liblzma/simple/simple_encoder.c
dependencies/xz/src/liblzma/common/block_encoder.c
dependencies/xz/src/liblzma/common/alone_encoder.c
dependencies/xz/src/liblzma/common/easy_encoder.c
dependencies/xz/src/liblzma/common/stream_buffer_decoder.c
dependencies/xz/src/liblzma/common/filter_decoder.c
dependencies/xz/src/liblzma/common/block_header_encoder.c
dependencies/xz/src/liblzma/common/auto_decoder.c
dependencies/xz/src/liblzma/common/easy_decoder_memusage.c
dependencies/xz/src/liblzma/common/index.c
dependencies/xz/src/liblzma/common/easy_encoder_memusage.c
dependencies/xz/src/liblzma/common/stream_decoder.c
dependencies/xz/src/liblzma/common/stream_encoder.c
dependencies/xz/src/liblzma/common/vli_encoder.c
dependencies/xz/src/liblzma/common/easy_buffer_encoder.c
dependencies/xz/src/liblzma/common/hardware_cputhreads.c
dependencies/xz/src/liblzma/common/easy_preset.c
dependencies/xz/src/liblzma/common/filter_common.c
dependencies/xz/src/liblzma/common/vli_size.c
dependencies/xz/src/liblzma/common/vli_decoder.c
dependencies/xz/src/liblzma/common/block_buffer_decoder.c
dependencies/xz/src/liblzma/common/block_header_decoder.c
dependencies/xz/src/liblzma/common/stream_buffer_encoder.c
dependencies/xz/src/liblzma/common/index_hash.c
dependencies/xz/src/liblzma/common/filter_flags_encoder.c
dependencies/xz/src/liblzma/common/stream_flags_encoder.c
dependencies/xz/src/liblzma/common/filter_buffer_encoder.c
dependencies/xz/src/liblzma/common/outqueue.c
dependencies/xz/src/liblzma/common/alone_decoder.c
dependencies/xz/src/liblzma/common/filter_flags_decoder.c
dependencies/xz/src/liblzma/common/common.c
dependencies/xz/src/liblzma/common/block_util.c
dependencies/xz/src/liblzma/common/block_buffer_encoder.c
dependencies/xz/src/liblzma/common/hardware_physmem.c
dependencies/xz/src/liblzma/common/block_decoder.c
dependencies/xz/src/liblzma/common/stream_flags_decoder.c
dependencies/xz/src/liblzma/common/index_encoder.c
dependencies/xz/src/liblzma/common/stream_flags_common.c
dependencies/xz/src/liblzma/common/index_decoder.c
dependencies/xz/src/liblzma/common/filter_buffer_decoder.c
dependencies/xz/src/liblzma/common/filter_encoder.c
dependencies/xz/src/common/tuklib_cpucores.c
dependencies/xz/src/common/tuklib_physmem.c)


include_directories(
    ${LIBLZMA_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/src/vp8/util
    ${PROJECT_SOURCE_DIR}/src/vp8/model
    ${PROJECT_SOURCE_DIR}/src/vp8/encoder
    ${PROJECT_SOURCE_DIR}/src/vp8/decoder)

add_executable(lepton
   src/lepton/base_coders.hh
   src/lepton/simple_encoder.hh
   src/lepton/bitops.cc
   src/lepton/smalljpeg.hh
   src/lepton/bitops.hh
   src/lepton/test_driver.cc
   src/lepton/component_info.hh
   src/lepton/htables.hh
   src/lepton/fork_serve.cc
   src/lepton/fork_serve.hh
   src/lepton/jpgcoder.cc
   src/lepton/idct.cc
   src/lepton/idct.hh
   src/lepton/uncompressed_components.cc
   src/lepton/jpgcoder.hh
   src/lepton/uncompressed_components.hh
   src/lepton/vp8_decoder.cc
   src/lepton/simple_decoder.cc
   src/lepton/vp8_decoder.hh
   src/lepton/simple_decoder.hh
   src/lepton/vp8_encoder.cc
   src/lepton/simple_encoder.cc
   src/lepton/vp8_encoder.hh
   src/io/Allocator.hh
   src/io/BufferedIO.hh
   src/io/Compression.cc
   src/io/Compression.hh
   src/io/SwitchableCompression.hh
   src/io/Error.hh
   src/io/Reader.hh
   src/io/MuxReader.hh
   src/io/ioutil.hh
   src/io/ioutil.cc
   src/io/Zlib0.hh
   src/io/Zlib0.cc
   src/io/DecoderPlatform.hh
   src/vp8/util/generic_worker.hh
   src/vp8/util/options.hh
   src/vp8/util/generic_worker.cc
   src/vp8/util/memory.cc
   src/vp8/util/memory.hh
   src/vp8/util/debug.cc
   src/vp8/util/debug.hh
   src/vp8/util/nd_array.hh
   src/vp8/util/aligned_block.hh
   src/vp8/util/block_based_image.hh
   src/vp8/model/block.hh
   src/vp8/model/JpegArithmeticCoder.cc
   src/vp8/model/JpegArithmeticCoder.hh
   src/vp8/model/branch.hh
   src/vp8/model/legacy.hh
   src/vp8/model/model.cc
   src/vp8/model/model.hh
   src/vp8/model/numeric.cc
   src/vp8/model/numeric.hh
   src/vp8/model/jpeg_meta.hh
   src/vp8/encoder/encoder.cc
   src/vp8/decoder/decoder.cc
   src/vp8/encoder/bool_encoder.hh
   src/vp8/decoder/bool_decoder.hh
   src/vp8/encoder/boolwriter.hh
   src/vp8/encoder/boolwriter.cc
   src/vp8/decoder/boolreader.hh
   src/vp8/decoder/boolreader.cc
   src/vp8/encoder/vpx_bool_writer.hh
   src/vp8/decoder/vpx_bool_reader.hh
   src/io/MemMgrAllocator.cc
   src/io/MemMgrAllocator.hh

               )
SET(ADDITIONAL_FLAGS)
IF(NOT APPLE)
    SET(ADDITIONAL_FLAGS -static-libstdc++ pthread -static -Wl,--no-export-dynamic -lstdc++)
ENDIF()
target_link_libraries(lepton ${LIBLZMA_LIBRARIES} localmd5 ${ADDITIONAL_FLAGS})


add_executable(print-model
   src/vp8/util/debug.cc
   src/vp8/util/debug.hh
   src/lepton/print-model.cc
   src/io/MemMgrAllocator.cc
   src/io/MemMgrAllocator.hh
   src/vp8/util/memory.cc
   src/vp8/util/memory.hh
   src/vp8/model/model.cc
   src/vp8/encoder/encoder.cc
   src/vp8/decoder/decoder.cc
   src/lepton/idct.cc
   src/lepton/idct.hh
   src/vp8/model/numeric.cc
   )
 target_link_libraries(print-model ${ADDITIONAL_FLAGS})
add_executable(test_invariants
   src/io/MemMgrAllocator.cc
   src/io/MemMgrAllocator.hh
   src/vp8/util/memory.cc
   src/lepton/test_invariants.cc
   src/io/Compression.cc
 )
target_link_libraries(test_invariants ${LIBLZMA_LIBRARIES} ${ADDITIONAL_FLAGS})
