# 核心层（Core）：无 Qt 依赖的纯 C++，可独立测试
# 对齐契约：产品/portpilot-design/contracts/ （service-api.md / events.md / schema）
# 架构原则：Core 层实现 Domain 层定义的接口，因此依赖 Domain
#
# Issue #50 交付项 —— 跨平台串口实现（Qt-free，自写轻量平台抽象层）：
#   - platform/serial_backend.h              纯虚接口 ISerialBackend + Factory
#   - platform/serial_backend_posix.{h,cpp}  POSIX 后端（termios，Linux/macOS/BSD）
#   - platform/serial_backend_windows.{h,cpp} Windows 后端（CreateFile + DCB）
#   - serial_device.{h,cpp}                  委托 backend，保留锁/回调
#   - serial_worker.{h,cpp}                  通过 SerialDevice 真实调用
#
# M1 Issue #17 其余交付项：
#   - Logger（分级日志）
#   - EventBus（统一事件派发，对齐 events.md）
#   - MockDevicePort（L2-002 测试装配根）
#   - NetworkTransport（TCP/IP 预留 DevicePort 实现）
#   - FileRepository（SQLite/JSON 配置/协议持久化抽象 + InMemory 实现）
#   - ProtocolEngine（帧切分 / 字段提取 / 组帧，对齐 protocol.schema.json）
#   - Result<T> / ErrorCode（错误码系统，对齐 service-api.md §错误码约定）

set(PORTPILOT_CORE_SOURCES
    common_types.cpp
    core.cpp
    serial_device.cpp
    serial_worker.cpp
    protocol_engine.cpp
    platform/serial_backend_factory.cpp
)

# 按编译期平台选择后端源文件（Linux 无头环境下不包含 Windows 源）
if(WIN32)
    list(APPEND PORTPILOT_CORE_SOURCES
        platform/serial_backend_windows.cpp
    )
else()
    list(APPEND PORTPILOT_CORE_SOURCES
        platform/serial_backend_posix.cpp
        port_enumerator.cpp
    )
endif()

add_library(portpilot_core STATIC ${PORTPILOT_CORE_SOURCES})

target_include_directories(portpilot_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(portpilot_core PUBLIC portpilot_domain)
target_compile_features(portpilot_core PUBLIC cxx_std_17)

# POSIX 环境：暴露 posix_openpt / ptsname_r / grantpt / unlockpt 声明
if(UNIX)
    target_compile_definitions(portpilot_core PRIVATE
        _DEFAULT_SOURCE
        _XOPEN_SOURCE=600
        _GNU_SOURCE
    )
endif()

# Windows 后端链接系统库（SetCommState 等均属 kernel32，默认已链；保留显式声明以防万一）
if(WIN32)
    target_link_libraries(portpilot_core PRIVATE kernel32)
endif()
