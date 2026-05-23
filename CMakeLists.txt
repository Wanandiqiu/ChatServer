cmake_minimum_required(VERSION 3.10)
project(chat)

# 配置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# 配置最终的可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 解决 macOS 新版 CMake 的 CMP0167 警告
cmake_policy(SET CMP0167 OLD)

# 指定 Boost 路径（macOS Homebrew Apple Silicon）
set(CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(Boost REQUIRED COMPONENTS thread)

if(Boost_FOUND)
    message(STATUS "Found Boost: ${Boost_VERSION_STRING}")

    add_executable(chat src/main.cpp)

    target_include_directories(chat PRIVATE ${Boost_INCLUDE_DIRS})

    target_link_libraries(chat PRIVATE ${Boost_LIBRARIES})

else()
    message(FATAL_ERROR "Boost not found")
endif()