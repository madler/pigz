cmake_minimum_required(VERSION 3.1.0)

if(COMMAND CMAKE_POLICY)
    CMAKE_POLICY(SET CMP0003 NEW)
endif()

project(pigz)

include(${CMAKE_SOURCE_DIR}/SuperBuild/SuperBuild.cmake)
