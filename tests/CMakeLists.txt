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
    unit/domain_device_port_test.cpp
    unit/core_serial_device_test.cpp
)

target_include_directories(portpilot_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
# 确保 glibc 暴露 posix_openpt / ptsname_r / grantpt / unlockpt 等声明
target_compile_definitions(portpilot_tests PRIVATE
    _DEFAULT_SOURCE
    _XOPEN_SOURCE=600
    _GNU_SOURCE
)
target_link_libraries(portpilot_tests PRIVATE
    portpilot_core
    portpilot_domain
    GTest::gtest_main
)

find_package(Threads REQUIRED)
target_link_libraries(portpilot_tests PRIVATE Threads::Threads)

include(GoogleTest)
gtest_discover_tests(portpilot_tests)