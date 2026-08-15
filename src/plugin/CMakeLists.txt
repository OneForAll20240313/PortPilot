# Plugin 扩展层：协议模板、自定义命令扩展点
# 架构定位：DDD 五层架构之 Plugin 横向扩展层（13.1），挂载到 Service 层
# 对齐契约：service-api.md A-413 manageTemplates / A-715 manageScripts
#           protocol.schema.json / command-group.schema.json / macro.schema.json
add_library(portpilot_plugin STATIC
    plugin_manager.cpp
)

target_include_directories(portpilot_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
