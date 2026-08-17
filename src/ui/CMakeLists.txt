# UI 适配层：#36 Qt 信号桥接 + @TODO 完整 Qt 界面（串口、终端、可视化）
#
# #36 交付：QtSignalBridge（SerialWorker → Qt 信号桥接的 UI 薄壳）
#   - 订阅 Service BufferService.buffer.dataReceived → 发射 Qt 信号 dataReceived
#   - 仅经 Service 抽象取数，不触碰 Core/Domain 设备实现（13.7 协作约束）
add_library(portpilot_ui STATIC
    qt_signal_bridge.cpp
)

target_include_directories(portpilot_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(portpilot_ui PUBLIC
    portpilot_core
    portpilot_domain
    portpilot_service
)

# Qt 桥接依赖 Qt 信号/槽机制（Q_OBJECT → moc），在 Qt 环境下链接 Qt6::Core
if(Qt6_FOUND)
    target_link_libraries(portpilot_ui PUBLIC Qt6::Widgets)
    # 定义 QT_NO_KEYWORDS：禁用 Qt 把 emit/signals/slots 定义为宏，
    # 避免与 core::EventBus::emit 方法名冲突（core 为 Qt-free 纯 C++）。
    target_compile_definitions(portpilot_ui PUBLIC QT_NO_KEYWORDS)
endif()