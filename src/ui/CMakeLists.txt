# UI 适配层：@TODO 实现 Qt 界面（串口、终端、可视化）
add_library(portpilot_ui STATIC
)

target_include_directories(portpilot_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(portpilot_ui PUBLIC
    portpilot_core
    portpilot_domain
    portpilot_service
)