#通过include的方式引入
#输出变量EVDEPLOY_INCLUDE_DIRS 和 EVDEPLOY__LIBS EVDEPLOY_LIB_PATH

if(NOT DEFINED EVDEPLOY__LIBS)
    set(EVDEPLOY_LIB_PATH "/usr/local/evdeploy/lib")
    set(EVDEPLOY_INCLUDE_DIRS /usr/local/evdeploy/include)
    set(EVDEPLOY_LIBS ${EVDEPLOY_LIB_PATH}/libevdeploy.so)
endif()