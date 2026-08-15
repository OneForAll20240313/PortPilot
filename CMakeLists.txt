cmake_minimum_required(VERSION 3.16)

# PortPilot - 串口调试工具
# 遵循 DDD 五层架构：core -> domain -> service -> ui
project(PortPilot
    VERSION 0.1.0
    DESCRIPTION "PortPilot 串口调试工具"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 编译产物统一输出到构建目录，禁止在源码目录编译
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# GUI（Qt）开关：默认开启；CI 无 Qt 环境时用 -DBUILD_GUI=OFF 仅测核心层
option(BUILD_GUI "Build Qt GUI layer" ON)

# 五层架构子目录（按依赖顺序）
add_subdirectory(src)

# 测试（默认开启，可用 -DBUILD_TESTING=OFF 关闭）
include(CTest)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()