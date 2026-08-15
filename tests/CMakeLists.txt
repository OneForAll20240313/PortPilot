# PortPilot 测试
# 统一使用 GoogleTest + GMock（见准则「自测开发环境规范」）
# 通过 FetchContent 拉取 gtest，避免依赖系统包

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 单元测试可执行目标
add_executable(portpilot_tests
    unit/core_sample_test.cpp
    unit/plugin_test.cpp
)

target_include_directories(portpilot_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(portpilot_tests PRIVATE
    portpilot_core
    portpilot_plugin
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(portpilot_tests)