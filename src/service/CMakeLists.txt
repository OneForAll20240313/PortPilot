# 服务层：@TODO 实现 EventBus / ViewManager / SerialWorker
# 对齐契约：产品/portpilot-design/contracts/service-api.md、events.md
#
# #36 交付：BufferService（字节流 Qt-free 内核，A-301/A-302）
#   - 承接 DevicePort 接收数据 → 发射 buffer.dataReceived 事件（供 UI Qt 薄壳订阅）
#   - 发送路径 → 发射 buffer.sent 事件
add_library(portpilot_service STATIC
    service.cpp
    buffer_service.cpp
)

target_include_directories(portpilot_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(portpilot_service PUBLIC
    portpilot_core
    portpilot_domain
)