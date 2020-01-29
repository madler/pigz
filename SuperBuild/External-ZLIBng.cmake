set(NG_BRANCH develop) # zlib-ng branch

ExternalProject_Add(zlib
    GIT_REPOSITORY "${git_protocol}://github.com/rordenlab/zlib-ng.git"
    GIT_TAG "${NG_BRANCH}"
    SOURCE_DIR ng-zlib
    BINARY_DIR ng-zlib-build
    CMAKE_ARGS
        -Wno-dev
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=${DEP_INSTALL_DIR}
)

set(ZLIB_ROOT ${DEP_INSTALL_DIR})
