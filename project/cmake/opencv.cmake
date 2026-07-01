#通过include的方式引入
#输出变量OpenCV_INCLUDE_DIRS 和 OpenCV_LIBS
if(NOT DEFINED OpenCV_LIBS)
    find_package(OpenCV)
    if(WITH_SOPHON_RUNTIME)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-variable")
        set(SOPHON_ROOT "/opt/sophon")
        list(APPEND OpenCV_LIBS "-L${SOPHON_ROOT}/libsophon-current/lib" bmlib bmrt bmcv yuv bmjpuapi bmjpulite -L"${SOPHON_ROOT}/sophon-ffmpeg-latest/lib")
        list(APPEND OpenCV_INCLUDE_DIRS ${SOPHON_ROOT}/libsophon-current/include/ ${SOPHON_ROOT}/sophon-ffmpeg-latest/include/)
    endif()
    message(STATUS "\${OpenCV_INCLUDE_DIRS}:${OpenCV_INCLUDE_DIRS}")
    message(STATUS "\${OpenCV_LIBS}:${OpenCV_LIBS}")
endif()
