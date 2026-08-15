# 服务层：@TODO 实现 EventBus / ViewManager / SerialWorker
# 对齐契约：产品/portpilot-design/contracts/service-api.md、events.md
add_library(portpilot_service STATIC
    service.cpp
)

target_include_directories(portpilot_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(portpilot_service PUBLIC
    portpilot_core
    portpilot_domain
)