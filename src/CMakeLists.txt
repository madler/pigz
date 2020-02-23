cmake_minimum_required(VERSION 3.1.0)

project(pigz)

# Option Choose whether to use static runtime
include(ucm.cmake)
option(USE_STATIC_RUNTIME "Use static runtime" ON)
if(USE_STATIC_RUNTIME)
    ucm_set_runtime(STATIC)
else()
    ucm_set_runtime(DYNAMIC)
endif()

# Basic CMake build settings
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS  "Debug;Release;RelWithDebInfo;MinSizeRel")
endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    # using Clang
    add_definitions(-fno-caret-diagnostics)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    # using GCC
    if(NOT (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 4.5.0))
        add_definitions(-Wno-unused-result)    # available since GCC 4.5.0
    endif()
    if(NOT (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 4.8.0))
        add_definitions(-fno-diagnostics-show-caret)    # available since GCC 4.8.0
    endif()
elseif(MSVC)
    # using Visual Studio C++
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4018")   # '<': signed/unsigned mismatch
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4068")   # unknown pragma
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4101")   # unreferenced local variable
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244")   # 'initializing': conversion from 'double' to 'int', possible loss of data
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267")   # 'initializing': conversion from 'size_t' to 'int', possible loss of data
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4305")   # 'argument': truncation from 'double' to 'float'
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4308")   # negative integral constant converted to unsigned type
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4334")   # '<<': result of 32-bit shift implicitly converted to 64 bits
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")   # 'uint32_t' : forcing value to bool 'true' or 'false'
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4819")   # The file contains a character that cannot be represented in the current code page
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")   # 'access': The POSIX name for this item is deprecated
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8388608")  # set "Stack Reserve Size" to 8MB (default value is 1MB)
    include_directories(${CMAKE_SOURCE_DIR}/win/dirent)
endif()

# Compiler dependent flags
include (CheckCXXCompilerFlag)
if(UNIX)
    check_cxx_compiler_flag(-msse2 HAS_SSE2)
    if(HAS_SSE2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2 -mfpmath=sse")
    endif()
endif()

set(PROGRAMS ${PROJECT_NAME})

set(PIGZ_SRCS
    pigz.c
    yarn.c
    try.c
    zopfli/src/zopfli/deflate.c
    zopfli/src/zopfli/blocksplitter.c
    zopfli/src/zopfli/tree.c
    zopfli/src/zopfli/lz77.c
    zopfli/src/zopfli/cache.c
    zopfli/src/zopfli/hash.c
    zopfli/src/zopfli/util.c
    zopfli/src/zopfli/squeeze.c
    zopfli/src/zopfli/katajainen.c)

add_executable(${PROJECT_NAME} ${PIGZ_SRCS})

if(MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PTHREADS4W_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${PTHREADS4W_LIBRARIES})
else()
    target_link_libraries(${PROJECT_NAME} m)
    if(MINGW)
        target_link_libraries(${PROJECT_NAME} "-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(${PROJECT_NAME} Threads::Threads)
    endif()
endif()
set(THREADS_PREFER_PTHREAD_FLAG ON)

if(MINGW)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS -municode)
endif()

set(ZLIB_IMPLEMENTATION "System" CACHE STRING "Choose zlib implementation.")
set_property(CACHE ZLIB_IMPLEMENTATION PROPERTY STRINGS  "System;Custom")
if(NOT ${ZLIB_IMPLEMENTATION} STREQUAL "System")
    set(ZLIB_ROOT ${ZLIB_ROOT} CACHE PATH "Specify custom zlib root directory.")
    if(NOT ZLIB_ROOT)
        message(FATAL_ERROR "ZLIB_ROOT needs to be set to locate custom zlib!")
    endif()
endif()
find_package(ZLIB REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES})

install(TARGETS ${PROGRAMS} DESTINATION bin)
