# cmake_minimum_required(VERSION 3.0)

# # 项目名称
# project(ThreadPoolProject)


# # 设置 C++ 标准
# set(CMAKE_CXX_STANDARD 20)


# # 添加源文件
# set(SOURCES
#         main.cpp
#         threadpool.cc
# )


# # 包含头文件目录
# include_directories(${CMAKE_SOURCE_DIR})


# # 生成可执行文件
# add_executable(main ${SOURCES})


# # 正确链接 pthread 库
# target_link_libraries(main pthread)


cmake_minimum_required(VERSION 3.0)


# 项目名称
project(ThreadPoolProject)


# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)


# 添加源文件
set(SOURCE_FILES
        threadpool.cc
)


# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR})


# 生成动态库
add_library(threadpool SHARED ${SOURCE_FILES})


# 安装动态库到 /usr/lib 目录
install(TARGETS threadpool LIBRARY DESTINATION /usr/local/lib)


# 正确链接 pthread 库
target_link_libraries(threadpool pthread)