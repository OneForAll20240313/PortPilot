# PortPilot 源码 - 五层架构
# 依赖方向：core <- domain <- service <- ui（单向依赖，禁止反向）

# 核心层（无 Qt 依赖的纯 C++，可独立测试）
add_subdirectory(core)

# 领域层（依赖 core）
add_subdirectory(domain)

# 服务层（依赖 core/domain）
add_subdirectory(service)

# UI 适配层（依赖 core/domain/service，Qt 界面；无 Qt 环境时跳过）
if(BUILD_GUI)
    add_subdirectory(ui)
endif()

# 主程序入口（无 Qt 环境时仍可编译核心层，但不产出 GUI 主程序）
if(BUILD_GUI)
    add_executable(portpilot
        main.cpp
    )
    target_link_libraries(portpilot PRIVATE
        portpilot_core
        portpilot_domain
        portpilot_service
        portpilot_ui
    )
endif()