# 领域层：@TODO 实现 Session / Protocol
# 对齐契约：产品/portpilot-design/contracts/session.schema.json、protocol.schema.json、route-state.schema.json
add_library(portpilot_domain STATIC
    domain.cpp
)

target_include_directories(portpilot_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(portpilot_domain PUBLIC portpilot_core)