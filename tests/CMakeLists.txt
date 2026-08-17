# PortPilot 单元测试
# 统一使用 GoogleTest + GMock（见准则「自测开发环境规范」）
# 通过 FetchContent 拉取 gtest，避免依赖系统包

include(FetchContent)
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 单元测试可执行目标
add_executable(portpilot_tests
    unit/core_sample_test.cpp
    unit/domain_device_port_test.cpp
    unit/core_serial_device_test.cpp
    unit/plugin_test.cpp
    unit/port_enumerator_test.cpp
    unit/buffer_service_test.cpp
)

target_include_directories(portpilot_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
# gtest 头以 SYSTEM 方式引入：第三方头文件的告警不参与 -Werror 判定
target_include_directories(portpilot_tests SYSTEM PRIVATE
    ${googletest_SOURCE_DIR}/googletest/include
    ${googletest_SOURCE_DIR}/googlemock/include
)
# 编码规范硬性要求：测试代码同样禁止 error / warning / note
target_compile_options(portpilot_tests PRIVATE -Wall -Wextra -Werror)
# 确保 glibc 暴露 posix_openpt / ptsname_r / grantpt / unlockpt 等声明
target_compile_definitions(portpilot_tests PRIVATE
    _DEFAULT_SOURCE
    _XOPEN_SOURCE=600
    _GNU_SOURCE
)
target_link_libraries(portpilot_tests PRIVATE
    portpilot_core
    portpilot_domain
    portpilot_service
    portpilot_plugin
    GTest::gtest_main
    GTest::gmock
)
target_compile_features(portpilot_tests PRIVATE cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(portpilot_tests PRIVATE Threads::Threads)

include(GoogleTest)
gtest_discover_tests(portpilot_tests)
