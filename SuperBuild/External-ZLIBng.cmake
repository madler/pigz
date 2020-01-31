set(NG_BRANCH develop) # zlib-ng branch

ExternalProject_Add(zlib
    GIT_REPOSITORY "${git_protocol}://github.com/zlib-ng/zlib-ng.git"
    GIT_TAG "${NG_BRANCH}"
    SOURCE_DIR ng-zlib
    BINARY_DIR ng-zlib-build
    BUILD_BYPRODUCTS ${ZLIB_STATIC_LIBRARIES}
	CMAKE_ARGS
        -Wno-dev
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=${DEP_INSTALL_DIR}
        -DZLIB_COMPAT:STRING=ON
        -DBUILD_SHARED_LIBS:STRING=OFF
)

set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set(ZLIB_ROOT ${DEP_INSTALL_DIR})
