set(CHROMIUM_BRANCH developChromium) # Chromium zlib branch

ExternalProject_Add(zlib
    GIT_REPOSITORY "${git_protocol}://github.com/rogiedodgie/zlib"
    GIT_TAG "${CHROMIUM_BRANCH}"
    SOURCE_DIR chromium-zlib
    BINARY_DIR chromium-zlib-build
    CMAKE_ARGS
        -Wno-dev
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=${DEP_INSTALL_DIR}
        -DBUILD_SHARED_LIBS:STRING=OFF
)

set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set(ZLIB_ROOT ${DEP_INSTALL_DIR})
