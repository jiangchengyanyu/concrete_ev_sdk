cmake_minimum_required(VERSION 3.10)
project(wkt_parser)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-fext-numeric-literals ${CMAKE_CXX_FLAGS}")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include(opencv)

find_package(Boost REQUIRED)

# libwkt_parser.so
add_library(${PROJECT_NAME} STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/wkt_parser.cpp)
target_link_libraries(${PROJECT_NAME}
PRIVATE
        ${OpenCV_LIBS}
        boost_system
        boost_filesystem
        boost_thread
        boost_coroutine
        boost_context
        boost_regex
)

target_include_directories(${PROJECT_NAME}
PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
PRIVATE
        ${Boost_INCLUDE_DIRS}
)

target_compile_options(${PROJECT_NAME}
PRIVATE
        -std=c++11
        -Wall
        -O3
        -fPIC
)

# install(TARGETS ${PROJECT_NAME} DESTINATION lib)

if (BUILD_EV_SDK_TESTS)
    # testWkt
    add_executable(testWktParser ${CMAKE_CURRENT_SOURCE_DIR}/src/test-wkt-parser.cpp)
    target_link_libraries(testWktParser 
    PRIVATE
            ${OpenCV_LIBS}
            ${PROJECT_NAME}
    )
    target_include_directories(testWktParser
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    install(TARGETS testWktParser DESTINATION bin)
endif ()