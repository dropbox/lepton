cmake_minimum_required(VERSION 2.8)
project(lepton)

FIND_PACKAGE(Git)

SET(CMAKE_CXX_FLAGS "-std=c++11 -fno-exceptions -fno-rtti")
SET(CMAKE_C_FLAGS "-std=c99 -DHAVE_CONFIG_H ")

add_library(localmd5 dependencies/md5/md5.c)
add_library(localzlib
dependencies/zlib/inflate.c
dependencies/zlib/inflate.h
dependencies/zlib/gzguts.h
dependencies/zlib/infback.c
dependencies/zlib/trees.c
dependencies/zlib/adler32.c
dependencies/zlib/gzclose.c
dependencies/zlib/inftrees.h
dependencies/zlib/zconf.h
dependencies/zlib/compress.c
dependencies/zlib/crc32.c
dependencies/zlib/crc32.h
dependencies/zlib/trees.h
dependencies/zlib/inftrees.c
dependencies/zlib/zutil.c
dependencies/zlib/gzwrite.c
dependencies/zlib/zutil.h
dependencies/zlib/zlib.h
dependencies/zlib/inffixed.h
dependencies/zlib/deflate.c
dependencies/zlib/inffast.h
dependencies/zlib/inffast.c
dependencies/zlib/uncompr.c
dependencies/zlib/gzread.c
dependencies/zlib/deflate.h
dependencies/zlib/gzlib.c)


include_directories(
    ${CMAKE_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src/vp8/util
    ${PROJECT_SOURCE_DIR}/src/vp8/model
    ${PROJECT_SOURCE_DIR}/src/vp8/encoder
    ${PROJECT_SOURCE_DIR}/src/vp8/decoder)

SET(LEPTON_SOURCES
   src/lepton/base_coders.hh
   src/lepton/simple_encoder.hh
   src/lepton/bitops.cc
   src/lepton/bitops.hh
   src/lepton/component_info.hh
   src/lepton/htables.hh
   src/lepton/fork_serve.cc
   src/lepton/fork_serve.hh
   src/lepton/thread_handoff.cc
   src/lepton/thread_handoff.hh
   src/lepton/socket_serve.cc
   src/lepton/socket_serve.hh
   src/lepton/jpgcoder.cc
   src/lepton/validation.cc
   src/lepton/validation.hh
   src/lepton/recoder.cc
   src/lepton/recoder.hh
   src/lepton/idct.cc
   src/lepton/idct.hh
   src/lepton/uncompressed_components.cc
   src/lepton/jpgcoder.hh
   src/lepton/uncompressed_components.hh
   src/lepton/lepton_codec.cc
   src/lepton/lepton_codec.hh
   src/lepton/vp8_decoder.cc
   src/lepton/simple_decoder.cc
   src/lepton/vp8_decoder.hh
   src/lepton/simple_decoder.hh
   src/lepton/vp8_encoder.cc
   src/lepton/simple_encoder.cc
   src/lepton/vp8_encoder.hh
   src/io/Allocator.hh
   src/io/BufferedIO.hh
   src/io/ZlibCompression.cc
   src/io/ZlibCompression.hh
   src/io/Seccomp.hh
   src/io/Seccomp.cc
   src/io/seccomp-bpf.hh
   src/io/MemReadWriter.cc
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
   src/vp8/model/JpegArithmeticCoder.cc
   src/vp8/model/JpegArithmeticCoder.hh
   src/vp8/model/branch.hh
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
add_executable(lepton ${LEPTON_SOURCES})
add_executable(lepton-avx ${LEPTON_SOURCES})
SET(ADDITIONAL_FLAGS)
IF(NOT APPLE)
    SET(ADDITIONAL_FLAGS -static-libstdc++ pthread -Wl,--no-export-dynamic -lstdc++)
ENDIF()
SET(ADDITIONAL_COMPILE_FLAGS)
SET(ADDITIONAL_DEFINES)
option(UNSAFE_SKIP_VALIDATION "Don't check roundtrip: can cause data corruption unless you have a following validation step" OFF)
IF(UNSAFE_SKIP_VALIDATION)
SET(ADDITIONAL_DEFINES "${ADDITIONAL_DEFINES} -DSKIP_VALIDATION")
ENDIF()
option(ASAN "ASAN" OFF)
IF(ASAN)
SET(ADDITIONAL_COMPILE_FLAGS "-fsanitize=address,undefined ${ADDITIONAL_COMPILE_FLAGS}")
SET(ADDITIONAL_FLAGS "-fsanitize=address,undefined ${ADDITIONAL_FLAGS} -Wl,-rpath,/srv/lepton-qualified/lib,-rpath,/srv/lepton-candidate/lib")
ELSE()
IF(NOT APPLE)
    SET(ADDITIONAL_FLAGS "-static ${ADDITIONAL_FLAGS}")
ENDIF()
ENDIF()
target_link_libraries(lepton localzlib localmd5 ${ADDITIONAL_FLAGS})
target_link_libraries(lepton-avx localzlib localmd5 ${ADDITIONAL_FLAGS})
set_target_properties(localmd5 PROPERTIES COMPILE_FLAGS "-msse4.2 ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES}")
set_target_properties(localzlib PROPERTIES COMPILE_FLAGS "-msse4.2 -include unistd.h ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES}")
set_target_properties(lepton PROPERTIES COMPILE_FLAGS "-msse4.2 ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES}")
set_target_properties(lepton-avx PROPERTIES COMPILE_FLAGS "-march=core-avx2 ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES}")

#add_executable(print-model
#   src/vp8/util/debug.cc
#   src/vp8/util/debug.hh
#   src/lepton/print-model.cc
#   src/io/MemMgrAllocator.cc
#   src/io/MemMgrAllocator.hh
#   src/vp8/util/memory.cc
#   src/vp8/util/memory.hh
#   src/vp8/model/model.cc
#   src/vp8/encoder/encoder.cc
#   src/vp8/decoder/decoder.cc
#   src/lepton/idct.cc
#   src/lepton/idct.hh
#   src/vp8/model/numeric.cc
#   )
# target_link_libraries(print-model ${ADDITIONAL_FLAGS})
add_executable(test_invariants
   src/io/MemMgrAllocator.cc
   src/io/MemMgrAllocator.hh
   src/io/MemReadWriter.cc
   src/lepton/thread_handoff.cc
   src/lepton/thread_handoff.hh
   src/vp8/util/memory.cc
   test_suite/test_invariants.cc
 )
target_link_libraries(test_invariants localzlib ${ADDITIONAL_FLAGS})
#set_target_properties(print-model PROPERTIES COMPILE_FLAGS "-msse4.2")
set_target_properties(test_invariants PROPERTIES COMPILE_FLAGS "-msse4.2")

FILE(WRITE ${CMAKE_BINARY_DIR}/version.hh.in
"\#define GIT_REVISION \"@VERSION@\"\n"
)

FILE(WRITE ${CMAKE_BINARY_DIR}/version.cmake
"EXECUTE_PROCESS(
     COMMAND ${GIT_EXECUTABLE} --git-dir=${PROJECT_SOURCE_DIR}/.git --work-tree=${PROJECT_SOURCE_DIR} rev-parse HEAD
     OUTPUT_VARIABLE VERSION
     OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 CONFIGURE_FILE(\${SRC} \${DST} @ONLY)
")

INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR})

ADD_CUSTOM_TARGET(
    version
    ${CMAKE_COMMAND} -D SRC=${CMAKE_BINARY_DIR}/version.hh.in
                     -D DST=${CMAKE_BINARY_DIR}/version.hh
                     -P ${CMAKE_BINARY_DIR}/version.cmake
)

ADD_DEPENDENCIES(lepton version)
