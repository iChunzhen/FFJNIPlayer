
cmake_minimum_required(VERSION 3.4.1)

#引入头文件
include_directories(include)
#设置变量
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}")
#编译文件
file(GLOB src_files *.cpp)
#编译生成动态库
add_library(
        ffplayer
        SHARED
        ${src_files})
find_library(
        log-lib
        log)
#设置依赖的库
target_link_libraries(
        ffplayer
        -Wl,--start-group
        avfilter avformat avcodec avutil swresample swscale
        -Wl,--end-group
        ${log-lib}
        z
        OpenSLES
        android)