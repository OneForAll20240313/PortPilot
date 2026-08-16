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

# --- Qt 探测：仅当找到 Qt 才启用 AUTOMOC / AUTORCC / AUTOUIC ---
# 无 Qt 环境（如 CI -DBUILD_GUI=OFF）只编译纯 C++ 核心层，
# 不开启 AUTOGEN，避免对每个 target 产生 "No valid Qt version found" 无意义告警。
find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

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

# ---- Doxygen API 文档（可选，-DBUILD_DOCS=OFF 关闭） ----
# 输出固定到独立构建目录 <build>/docs，禁止源码目录生成产物（见准则·源码路径禁止构建产物）。
option(BUILD_DOCS "Build Doxygen API documentation" ON)
if(BUILD_DOCS)
    find_package(Doxygen QUIET)
    if(Doxygen_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_SOURCE_BROWSER YES)
        doxygen_add_docs(doxygen-docs
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            COMMENT "Generate PortPilot API documentation (HTML)")
    endif()
endif()