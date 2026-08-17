# PortPilot 源码 - DDD 五层架构（UI / Service / Domain / Core + Plugin）
# 依赖方向：core <- domain <- service <- ui（单向依赖，禁止反向）
# Plugin 为横向扩展层，挂载到 Service 层（见 13.1）

# 编码规范硬性要求：编译期间禁止出现 error / warning / note。
# -Wall -Wextra 开启全部常规告警；-Werror 将告警升级为错误，任何 warning 直接阻断构建。
# 仅作用于本项目源码（src 目录），不影响 tests / 第三方（gtest 等）头文件。
add_compile_options(-Wall -Wextra -Werror)

# 核心层（无 Qt 依赖的纯 C++，可独立测试）
add_subdirectory(core)

# 领域层（依赖 core）
add_subdirectory(domain)

# 服务层（依赖 core/domain）
add_subdirectory(service)

# Plugin 扩展层（横向扩展，挂载到 Service 层；协议模板/自定义命令扩展点）
add_subdirectory(plugin)

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
    # 与 UI 层一致禁用 Qt 关键字宏，避免 core::EventBus::emit 冲突（qt_signal_bridge 亦依赖）
    target_compile_definitions(portpilot PRIVATE QT_NO_KEYWORDS)
endif()