cmake_minimum_required(VERSION 3.10)
project(ji)

set(CMAKE_CXX_STANDARD 11)
set(EV_SDK_PATH "/usr/local/ev_sdk")
set(CMAKE_INSTALL_PREFIX ${EV_SDK_PATH})

option(BUILD_EV_SDK_TESTS "whether to build ev_sdk test" OFF)
option(WITH_ASCENDCL_RUNTIME "whether to build with ascendcl/atlas" ON)

if(WITH_ASCENDCL_RUNTIME)
    add_definitions("-DWITH_ASCENDCL_RUNTIME")
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "release")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(opencv)

if(WITH_ASCENDCL_RUNTIME)
    if(DEFINED ENV{CANN_INC_PATH})
        set(EDGE_INC_PATH $ENV{CANN_INC_PATH})
    else()
        set(EDGE_INC_PATH "/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/include")
    endif()

    if(DEFINED ENV{CANN_LIB_PATH})
        set(EDGE_LIB_PATH $ENV{CANN_LIB_PATH})
    else()
        set(EDGE_LIB_PATH "/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64")
    endif()
endif()

set(libji_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ji.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sample_algorithm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sample_detector.cpp
)

add_library(${PROJECT_NAME} SHARED ${libji_SOURCES})

target_include_directories(${PROJECT_NAME}
PUBLIC
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EDGE_INC_PATH}
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    ${OpenCV_LIBS}
    glog
    pthread
    ${EDGE_LIB_PATH}/libascendcl.so
)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}:${EV_SDK_PATH}/lib:${EDGE_LIB_PATH}")

install(TARGETS ${PROJECT_NAME} DESTINATION lib)

if(BUILD_EV_SDK_TESTS)
    add_subdirectory(test)
endif()
