cmake_minimum_required(VERSION 3.0)

# 项目名称
project(ThreadPoolProject)


# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)


# 添加源文件
set(SOURCES
        main.cpp
        threadpool.cpp
)


# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR})


# 生成可执行文件
add_executable(main ${SOURCES})


# 正确链接 pthread 库
target_link_libraries(main pthread)