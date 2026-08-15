# 领域层：定义 Session / Protocol / DevicePort 等接口与基础类型
# 对齐契约：产品/portpilot-design/contracts/session.schema.json、protocol.schema.json、route-state.schema.json
# 架构原则：Domain 层不依赖 Core 层；接口定义在 Domain，实现在 Core
add_library(portpilot_domain STATIC
    domain.cpp
)

target_include_directories(portpilot_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(portpilot_domain PUBLIC cxx_std_17)
