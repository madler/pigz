set(PTHREADS4W_TAG cmake)

set(CLEANUP_STYLE VC)
set(PTW32_VER 3)

ExternalProject_Add(pthreads4w
    GIT_REPOSITORY "${git_protocol}://github.com/ningfei/pthreads4w.git"
    GIT_TAG "${PTHREADS4W_TAG}"
    SOURCE_DIR pthreads4w
    BINARY_DIR pthreads4w-build
    CMAKE_ARGS
        -Wno-dev
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=${DEP_INSTALL_DIR}
        -DCLEANUP_STYLE:STRING=${CLEANUP_STYLE}
)

set(PTHREADS4W_INCLUDE_DIRS ${DEP_INSTALL_DIR}/include)
set(PTHREADS4W_LIBRARIES ${DEP_INSTALL_DIR}/lib/libpthread${CLEANUP_STYLE}${PTW32_VER}.lib)
