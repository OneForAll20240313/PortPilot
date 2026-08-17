/**
 * @file main.cpp
 * @brief 程序入口（装配根，与 GUI 解耦）
 * @copyright 此文件为 PortPilot 项目一部分，遵循项目编码规范
 * @details 遵循架构约定 13.4 §4.1：main.cpp 不依赖 Qt，入口只做装配根；
 * BUILD_GUI=OFF 时仍产出 portpilot 可执行文件（CLI/服务模式）。
 */

#include <cstdio>
#include <cstdlib>

// 声明 GUI 入口（仅 BUILD_GUI=ON 时链接）
#if BUILD_GUI
extern int gui_main(int argc, char* argv[]);
#else
// 服务模式/CLI 入口，当前为最小演示：后续扩展 MCP 服务启动
static int service_main(int /*argc*/, char* /*argv*/[])
{
    std::printf("PortPilot service mode (no GUI)\n");
    std::printf("Core layers built successfully: core + domain + service + plugin\n");
    // TODO: #57 MCP 服务器启动入口
    return EXIT_SUCCESS;
}
#endif

int main(int argc, char* argv[])
{
#if BUILD_GUI
    // BUILD_GUI=ON: 进入 GUI 宿主
    return gui_main(argc, argv);
#else
    // BUILD_GUI=OFF: 进入服务/CLI 模式
    return service_main(argc, argv);
#endif
}