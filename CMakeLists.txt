cmake_minimum_required(VERSION 2.8)
project(lepton)

find_package(Git)
option(USE_SYSTEM_DEPENDENCIES "Use system dependencies for libz and openssl" OFF)

option(USE_CUSTOM_ALLOCATOR "Use a custom allocator on linux and OSX to bound the memory usage" ON)
set(ALLOCATOR_FLAGS)
if(NOT USE_CUSTOM_ALLOCATOR)
set(ALLOCATOR_FLAGS "-DUSE_STANDARD_MEMORY_ALLOCATORS")
endif()

set(BILLING_FLAGS)
option(ENABLE_BILLING "Always print out a receipt of which parts of the jpeg took how many bits" OFF)
if(ENABLE_BILLING)
set(BILLING_FLAGS "-DENABLE_BILLING")
endif()

set(ANS_FLAGS)
option(ENABLE_ANS_EXPERIMENTAL "Enable ANS arithmetic coder option (trigger with -ans during encode) (experimental)" OFF)
if(ENABLE_ANS_EXPERIMENTAL)
set(ANS_FLAGS "-DENABLE_ANS_EXPERIMENTAL")
endif()


if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "ppc")
option(SSE_VECTORIZATION "SSE instructions" OFF)
else()
option(SSE_VECTORIZATION "SSE instructions" ON)
endif()
option(BENCHMARK "Include a test file for benchmarking lepton" OFF)
set(flags_configs "")
if("${CMAKE_CONFIGURATION_TYPES}" STREQUAL "")
    if("${CMAKE_BUILD_TYPE}" STREQUAL "")
         list(APPEND flags_configs CMAKE_C_FLAGS)
         list(APPEND flags_configs CMAKE_CXX_FLAGS)
    else()
            string(TOUPPER ${CMAKE_BUILD_TYPE} config)
            list(APPEND flags_configs CMAKE_C_FLAGS_${config})
            list(APPEND flags_configs CMAKE_CXX_FLAGS_${config})
    endif()
else()
        # handle multi config generators (like msvc, xcode
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${config} config)
            list(APPEND flags_configs CMAKE_C_FLAGS_${config})
            list(APPEND flags_configs CMAKE_CXX_FLAGS_${config})
    endforeach()
endif()
if(WIN32)
    foreach(flags ${flags_configs})
        if(${flags} MATCHES "/MD")
             string(REGEX REPLACE "/MD" "/MT" ${flags} "${${flags}}")
        endif()
    endforeach()
endif()
if(SSE_VECTORIZATION)
  set(VECTOR_FLAGS "-mssse3 -msse4.2")
else()
  set(VECTOR_FLAGS "")
endif()
if(EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "-std=c++11 -fno-exceptions -fno-rtti -s DEMANGLE_SUPPORT=1 -s TOTAL_MEMORY=184549376 --pre-js pre.js --post-js post.js -O2")
else()
    set(CMAKE_CXX_FLAGS "-std=c++11 -fno-exceptions -fno-rtti")
endif()
set(CMAKE_C_FLAGS "-std=c99 -DHAVE_CONFIG_H")

if(WIN32)
SET(CMAKE_CXX_FLAGS "-D_HAS_EXCEPTIONS=0 -GR-")
else()
SET(CMAKE_CXX_FLAGS "-std=c++11 -fno-exceptions -fno-rtti")
endif()
if(WIN32)
SET(CMAKE_C_FLAGS "-DHAVE_CONFIG_H")
else()
SET(CMAKE_C_FLAGS "-std=c99 -DHAVE_CONFIG_H ")
endif()
if(NOT USE_SYSTEM_DEPENDENCIES)
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
    dependencies/zlib/zutil.h
    dependencies/zlib/zlib.h
    dependencies/zlib/inffixed.h
    dependencies/zlib/deflate.c
    dependencies/zlib/inffast.h
    dependencies/zlib/inffast.c
    dependencies/zlib/uncompr.c
    dependencies/zlib/deflate.h)
endif()
add_library(localbrotli
    dependencies/brotli/c/include/brotli/encode.h
    dependencies/brotli/c/include/brotli/types.h
    dependencies/brotli/c/include/brotli/decode.h
    dependencies/brotli/c/include/brotli/port.h
    dependencies/brotli/c/common/constants.h
    dependencies/brotli/c/common/version.h
    dependencies/brotli/c/common/dictionary.h
    dependencies/brotli/c/common/dictionary.c
    dependencies/brotli/c/enc/block_encoder_inc.h
    dependencies/brotli/c/enc/memory.c
    dependencies/brotli/c/enc/bit_cost.h
    dependencies/brotli/c/enc/ringbuffer.h
    dependencies/brotli/c/enc/entropy_encode.c
    dependencies/brotli/c/enc/quality.h
    dependencies/brotli/c/enc/utf8_util.h
    dependencies/brotli/c/enc/block_splitter_inc.h
    dependencies/brotli/c/enc/block_splitter.h
    dependencies/brotli/c/enc/compress_fragment.c
    dependencies/brotli/c/enc/utf8_util.c
    dependencies/brotli/c/enc/cluster_inc.h
    dependencies/brotli/c/enc/cluster.h
    dependencies/brotli/c/enc/literal_cost.c
    dependencies/brotli/c/enc/static_dict.c
    dependencies/brotli/c/enc/backward_references_hq.c
    dependencies/brotli/c/enc/hash.h
    dependencies/brotli/c/enc/literal_cost.h
    dependencies/brotli/c/enc/compress_fragment_two_pass.c
    dependencies/brotli/c/enc/dictionary_hash.c
    dependencies/brotli/c/enc/entropy_encode.h
    dependencies/brotli/c/enc/command.h
    dependencies/brotli/c/enc/metablock_inc.h
    dependencies/brotli/c/enc/context.h
    dependencies/brotli/c/enc/metablock.h
    dependencies/brotli/c/enc/hash_longest_match_quickly_inc.h
    dependencies/brotli/c/enc/hash_longest_match_inc.h
    dependencies/brotli/c/enc/hash_to_binary_tree_inc.h
    dependencies/brotli/c/enc/backward_references.h
    dependencies/brotli/c/enc/find_match_length.h
    dependencies/brotli/c/enc/prefix.h
    dependencies/brotli/c/enc/static_dict.h
    dependencies/brotli/c/enc/cluster.c
    dependencies/brotli/c/enc/brotli_bit_stream.h
    dependencies/brotli/c/enc/bit_cost_inc.h
    dependencies/brotli/c/enc/metablock.c
    dependencies/brotli/c/enc/backward_references_hq.h
    dependencies/brotli/c/enc/write_bits.h
    dependencies/brotli/c/enc/entropy_encode_static.h
    dependencies/brotli/c/enc/histogram.c
    dependencies/brotli/c/enc/encode.c
    dependencies/brotli/c/enc/port.h
    dependencies/brotli/c/enc/compress_fragment.h
    dependencies/brotli/c/enc/static_dict_lut.h
    dependencies/brotli/c/enc/histogram.h
    dependencies/brotli/c/enc/hash_forgetful_chain_inc.h
    dependencies/brotli/c/enc/block_splitter.c
    dependencies/brotli/c/enc/brotli_bit_stream.c
    dependencies/brotli/c/enc/compress_fragment_two_pass.h
    dependencies/brotli/c/enc/backward_references_inc.h
    dependencies/brotli/c/enc/bit_cost.c
    dependencies/brotli/c/enc/backward_references.c
    dependencies/brotli/c/enc/histogram_inc.h
    dependencies/brotli/c/enc/hash_longest_match64_inc.h
    dependencies/brotli/c/enc/fast_log.h
    dependencies/brotli/c/enc/memory.h
    dependencies/brotli/c/enc/dictionary_hash.h
    dependencies/brotli/c/dec/state.h
    dependencies/brotli/c/dec/huffman.h
    dependencies/brotli/c/dec/transform.h
    dependencies/brotli/c/dec/state.c
    dependencies/brotli/c/dec/bit_reader.c
    dependencies/brotli/c/dec/huffman.c
    dependencies/brotli/c/dec/context.h
    dependencies/brotli/c/dec/bit_reader.h
    dependencies/brotli/c/dec/prefix.h
    dependencies/brotli/c/dec/port.h
    dependencies/brotli/c/dec/decode.c)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src/vp8/util
    ${PROJECT_SOURCE_DIR}/dependencies/brotli/c/include
    ${PROJECT_SOURCE_DIR}/src/vp8/model
    ${PROJECT_SOURCE_DIR}/src/vp8/encoder
    ${PROJECT_SOURCE_DIR}/src/vp8/decoder)

set(LEPTON_SOURCES
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
   src/lepton/concat.cc
   src/lepton/smalljpg.hh
   src/lepton/benchmark.cc
   src/lepton/main.cc
   src/lepton/validation.cc
   src/lepton/validation.hh
   src/lepton/generic_compress.cc
   src/lepton/generic_compress.hh
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
   src/io/BrotliCompression.cc
   src/io/BrotliCompression.hh
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
   src/vp8/util/billing.cc
   src/vp8/util/billing.hh
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
if(SSE_VECTORIZATION)
add_executable(lepton ${LEPTON_SOURCES})
add_executable(lepton-slow-best-ratio ${LEPTON_SOURCES})
add_executable(lepton-avx ${LEPTON_SOURCES})
endif()
add_executable(lepton-scalar ${LEPTON_SOURCES})
set(ADDITIONAL_FLAGS)

if(NOT APPLE)
if(NOT WIN32)
if(EMSCRIPTEN)
    if(USE_SYSTEM_DEPENDENCIES)
        set(ADDITIONAL_FLAGS -static-libstdc++ pthread -Wl,--no-whole-archive -Wl,--no-export-dynamic -O2)
    else()
        set(ADDITIONAL_FLAGS -lstdc++ -static-libstdc++ pthread -Wl,--no-export-dynamic -O2)
    endif()
else()
    if(USE_SYSTEM_DEPENDENCIES)
        set(ADDITIONAL_FLAGS -static-libstdc++  -Wl,--whole-archive -lpthread  -Wl,--no-whole-archive -Wl,--no-export-dynamic)
    else()
        set(ADDITIONAL_FLAGS -lstdc++ -static-libstdc++  -Wl,--whole-archive -lpthread  -Wl,--no-whole-archive -Wl,--no-export-dynamic)
    endif()
endif()
endif()
endif()

set(ADDITIONAL_COMPILE_FLAGS)
if(NOT CMAKE_BUILD_TYPE)
  set(ADDITIONAL_COMPILE_FLAGS "${ADDITIONAL_COMPILE_FLAGS} -DNDEBUG -O3 -g")
endif()

set(ADDITIONAL_DEFINES)
if(BENCHMARK)
  set(ADDITIONAL_DEFINES "${ADDITIONAL_DEFINES} -DREALISTIC_BENCHMARK")
endif()
if(USE_SYSTEM_DEPENDENCIES)
    set(ADDITIONAL_DEFINES "${ADDITIONAL_DEFINES} -DUSE_SYSTEM_LIBRARIES -DUSE_SYSTEM_MD5_DEPENDENCY")
endif()

if(SSE_VECTORIZATION)
IF(WIN32)
IF("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
SET(ARCH_SSE2_FLAGS "/D__SSE2__")
ELSE()
SET(ARCH_SSE2_FLAGS "/arch:SSE2")
ENDIF()
ELSE()
set(ARCH_SSE2_FLAGS "-msse4.2")
ENDIF()

IF(WIN32)
SET(ARCH_AVX2_FLAGS "/arch:AVX2 /D__SSE2__")
ELSE()
set(ARCH_AVX2_FLAGS "-march=core-avx2")
ENDIF()
else()
set(ARCH_SSE2_FLAGS "")
set(ARCH_AVX2_FLAGS "")
endif()
option(BEST_RATIO_SLOW_DECOMPRESSION "Turn on single threaded decode in exchange for more compression ratio " OFF)

if(BEST_RATIO_SLOW_DECOMPRESSION)
    set(ADDITIONAL_DEFINES "${ADDITIONAL_DEFINES} -DDEFAULT_SINGLE_THREAD")
endif()

option(BASELINE_JPEG_ONLY "Only support small < 4MB baseline jpegs, instead of progressive and large JPEGS." OFF)
if(NOT BASELINE_JPEG_ONLY)
    set(ADDITIONAL_DEFINES "${ADDITIONAL_DEFINES} -DDEFAULT_ALLOW_PROGRESSIVE -DHIGH_MEMORY")
endif()

option(UNSAFE_SKIP_VALIDATION "Don't check roundtrip: can cause data corruption unless you have a following validation step" OFF)
if(UNSAFE_SKIP_VALIDATION)
    set(ADDITIONAL_DEFINES "${ADDITIONAL_DEFINES} -DSKIP_VALIDATION")
endif()
option(ASAN "ASAN" OFF)
if(ASAN)
    set(ADDITIONAL_COMPILE_FLAGS "-fsanitize=address,undefined ${ADDITIONAL_COMPILE_FLAGS}")
    set(ADDITIONAL_FLAGS "-fsanitize=address,undefined ${ADDITIONAL_FLAGS} -Wl,-rpath,/srv/lepton-qualified/lib,-rpath,/srv/lepton-candidate/lib")
else()
    if(NOT APPLE)
    if(NOT WIN32)
    if(NOT USE_SYSTEM_DEPENDENCIES)
        set(ADDITIONAL_FLAGS "-static ${ADDITIONAL_FLAGS}")
    endif()
    endif()
    endif()
endif()

if(USE_SYSTEM_DEPENDENCIES)
    find_package(ZLIB)
    include_directories(${ZLIB_INCLUDE_DIRS})
    find_package(OpenSSL)
    include_directories(${OPENSSL_INCLUDE_DIRS})
    if(SSE_VECTORIZATION)
      target_link_libraries(lepton localbrotli ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES} ${ADDITIONAL_FLAGS})
      target_link_libraries(lepton-slow-best-ratio localbrotli ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES} ${ADDITIONAL_FLAGS})
      target_link_libraries(lepton-avx localbrotli ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES} ${ADDITIONAL_FLAGS})
    endif()
    target_link_libraries(lepton-scalar localbrotli ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES} ${ADDITIONAL_FLAGS})
else()
    if(SSE_VECTORIZATION)
      target_link_libraries(lepton localzlib localbrotli localmd5 ${ADDITIONAL_FLAGS})
      target_link_libraries(lepton-slow-best-ratio localzlib localbrotli localmd5 ${ADDITIONAL_FLAGS})
      target_link_libraries(lepton-avx localzlib localbrotli localmd5 ${ADDITIONAL_FLAGS})
    endif()
    target_link_libraries(lepton-scalar localzlib localbrotli localmd5 ${ADDITIONAL_FLAGS})
    set_target_properties(localmd5 PROPERTIES COMPILE_FLAGS "${VECTOR_FLAGS} ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES}")
    if(WIN32)
        set(ZLIB_EXTRA_INCLUDE_DIRS)
    else()
        set(ZLIB_EXTRA_INCLUDE_DIRS " -include unistd.h")
    endif()
    set_target_properties(localzlib PROPERTIES COMPILE_FLAGS "${VECTOR_FLAGS} ${ZLIB_EXTRA_INCLUDE_DIRS} ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES}")
endif()
if(SSE_VECTORIZATION)
set_target_properties(lepton PROPERTIES COMPILE_FLAGS "${VECTOR_FLAGS} ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES} ${ALLOCATOR_FLAGS} ${ANS_FLAGS} ${BILLING_FLAGS}")
set_target_properties(lepton-slow-best-ratio PROPERTIES COMPILE_FLAGS "${VECTOR_FLAGS} ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES} ${ALLOCATOR_FLAGS} ${ANS_FLAGS} ${BILLING_FLAGS} -DDEFAULT_SINGLE_THREAD")
set_target_properties(lepton-avx PROPERTIES COMPILE_FLAGS "${ARCH_AVX2_FLAGS} ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES} ${ALLOCATOR_FLAGS} ${ANS_FLAGS} ${BILLING_FLAGS}")
endif()
set_target_properties(lepton-scalar PROPERTIES COMPILE_FLAGS "${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES} ${ALLOCATOR_FLAGS} ${ANS_FLAGS} ${BILLING_FLAGS} -DUSE_SCALAR")

set_target_properties(localzlib PROPERTIES COMPILE_FLAGS "${ARCH_SSE2_FLAGS} ${ZLIB_EXTRA_INCLUDE_DIRS} ${ADDITIONAL_COMPILE_FLAGS} ${ADDITIONAL_DEFINES} ${ALLOCATOR_FLAGS} ${ANS_FLAGS} ${BILLING_FLAGS}")

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
if(SSE_VECTORIZATION)
add_executable(test_invariants
   src/io/MemMgrAllocator.cc
   src/io/MemMgrAllocator.hh
   src/io/MemReadWriter.cc
   src/lepton/thread_handoff.cc
   src/lepton/thread_handoff.hh
   src/vp8/util/memory.cc
   test_suite/test_invariants.cc
   src/vp8/util/billing.cc
   src/vp8/util/billing.hh
   src/vp8/model/numeric.cc
   src/vp8/model/numeric.hh
   )
if(USE_SYSTEM_DEPENDENCIES)
    target_link_libraries(test_invariants ${ZLIB_LIBRARIES} ${ADDITIONAL_FLAGS})
else()
    target_link_libraries(test_invariants localzlib ${ADDITIONAL_FLAGS})
endif()

#set_target_properties(print-model PROPERTIES COMPILE_FLAGS "-msse4.2")
set_target_properties(test_invariants PROPERTIES COMPILE_FLAGS "${VECTOR_FLAGS}")
endif()
file(WRITE ${CMAKE_BINARY_DIR}/version.hh.in
"\#define GIT_REVISION \"@VERSION@\"\n"
)

file(WRITE ${CMAKE_BINARY_DIR}/version.cmake
"EXECUTE_PROCESS(
     COMMAND ${GIT_EXECUTABLE} --git-dir=${PROJECT_SOURCE_DIR}/.git --work-tree=${PROJECT_SOURCE_DIR} rev-parse HEAD
     OUTPUT_VARIABLE VERSION
     OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 CONFIGURE_FILE(\${SRC} \${DST} @ONLY)
")

include_directories(${CMAKE_BINARY_DIR})

add_custom_target(
    version ALL
    ${CMAKE_COMMAND} -D SRC=${CMAKE_BINARY_DIR}/version.hh.in
                     -D DST=${CMAKE_BINARY_DIR}/version.hh
                     -P ${CMAKE_BINARY_DIR}/version.cmake
)
file(GLOB JS_FILES "src/js/*")
file(COPY ${JS_FILES} DESTINATION ${CMAKE_BINARY_DIR})
if(SSE_VECTORIZATION)
add_dependencies(lepton version)
add_dependencies(lepton-avx version)
add_dependencies(lepton-slow-best-ratio version)
endif()
add_dependencies(lepton-scalar version)
if(SSE_VECTORIZATION)
install (TARGETS lepton lepton-slow-best-ratio lepton-avx lepton-scalar DESTINATION bin)
else()
install (TARGETS lepton-scalar DESTINATION bin)
endif()
