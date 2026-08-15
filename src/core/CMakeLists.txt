# 核心层（Core）：无 Qt 依赖的纯 C++，可独立测试
# 对齐契约：产品/portpilot-design/contracts/ （service-api.md / events.md / schema）
# M1 Issue #17 交付项：
#   - Logger（分级日志）
#   - EventBus（统一事件派发，对齐 events.md）
#   - DevicePort 抽象接口（service-api §9 L2-001）
#   - MockDevicePort（L2-002 测试装配根）
#   - SerialWorker（串口交互 DevicePort 实现抽象）
#   - NetworkTransport（TCP/IP 预留 DevicePort 实现）
#   - FileRepository（SQLite/JSON 配置/协议持久化抽象 + InMemory 实现）
#   - ProtocolEngine（帧切分 / 字段提取 / 组帧，对齐 protocol.schema.json）
#   - Result<T> / ErrorCode（错误码系统，对齐 service-api.md §错误码约定）
add_library(portpilot_core STATIC
    common_types.cpp
    core.cpp
    protocol_engine.cpp
)

target_include_directories(portpilot_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(portpilot_core PUBLIC cxx_std_17)
