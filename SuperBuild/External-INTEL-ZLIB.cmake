set(INTEL_BRANCH master) # INTEL zlib branch

ExternalProject_Add(zlib
    GIT_REPOSITORY "${git_protocol}://github.com/neurolabusc/zlib"
    GIT_TAG "${INTEL_BRANCH}"
    SOURCE_DIR intel-zlib
    BINARY_DIR intel-zlib-build
    CMAKE_ARGS
        -Wno-dev
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=${DEP_INSTALL_DIR}
        -DBUILD_SHARED_LIBS:STRING=OFF
)

set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set(ZLIB_ROOT ${DEP_INSTALL_DIR})
