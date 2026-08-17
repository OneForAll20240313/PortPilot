/**
 * @file main_gui.cpp
 * @brief Qt GUI 宿主入口（仅 BUILD_GUI=ON 编译链接）
 * @copyright 此文件为 PortPilot 项目一部分，遵循项目编码规范
 * @details 遵循架构约定 13.4 §4.1：GUI 启动逻辑独立于此文件，
 * main.cpp 不依赖 Qt，入口与 GUI 解耦。
 */

#include <QApplication>
#include <QByteArray>
#include <QString>
#include <QTimer>

#include <cstdio>

#include "core/event_bus.h"
#include "core/serial_worker.h"
#include "service/buffer_service.h"
#include "ui/qt_signal_bridge.h"

// #36 + #56 桥接演示：SerialWorker → BufferService → QtSignalBridge(dataReceived)
// 证明串口数据经 Service 事件总线到达 UI，并经 Qt 信号/槽在 UI 线程触发。
int gui_main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PortPilot"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    namespace ppc = portpilot::core;
    namespace pps = portpilot::service;
    namespace ppu = portpilot::ui;

    // 1) 数据源：SerialWorker（Qt-free 核心设备）
    auto worker = ppc::SerialWorker();

    // 2) 内核：BufferService（Qt-free，仅经 Service 抽象承接设备数据）
    auto buffer = pps::BufferService();

    // 3) 薄壳：QtSignalBridge 订阅 buffer.dataReceived → Qt 信号
    auto bridge = ppu::QtSignalBridge(&buffer);

    // 4) 设备数据上行：SerialWorker.onData → BufferService.pushData
    worker.onData([&](const portpilot::domain::Bytes& data) {
        buffer.pushData("demo-conn", data);
    });

    // 5) UI 侧订阅 Qt 信号：数据实时到达 UI（无跨线程，Qt 信号/槽机制保证）
    int received = 0;
    QObject::connect(&bridge, &ppu::QtSignalBridge::dataReceived,
                     [&](const QString& connId, const QByteArray& bytes) {
        ++received;
        std::printf("[bridge] conn=%s bytes=%d received=%d\n",
                    connId.toUtf8().constData(), static_cast<int>(bytes.size()), received);
    });

    // 6) 注入模拟串口字节，驱动数据链路上行
    QTimer::singleShot(0, [&]() {
        worker.inject_rx({0x48, 0x65, 0x6C, 0x6C, 0x6F});
    });
    QTimer::singleShot(50, [&]() {
        worker.inject_rx({0x21});
    });
    // 事件处理一轮后退出（等两帧数据都被送达 UI）
    QTimer::singleShot(120, &app, &QCoreApplication::quit);

    return app.exec();
}
