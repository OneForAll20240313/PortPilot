#pragma once

#include <QObject>
#include <QByteArray>

#include <cstdint>
#include <string>

namespace portpilot::service {
class BufferService;
}  // namespace portpilot::service

namespace portpilot::ui {

// ---------------------------------------------------------------------------
// QtSignalBridge：Qt 信号薄壳（#36 SerialWorker → Qt 信号桥接的 UI 层部分）
//
// 职责：订阅 Service 层 BufferService 的 `buffer.dataReceived` 事件，将其桥接
// 为 Qt 信号 `dataReceived`，供 Qt 宿主（终端/可视化/状态栏）直接 connect。
//
// 架构对齐（13.7 协作约束）：
//   - UI 层只经 Service 抽象（BufferService）获取数据，不 new Domain、不直接
//     触碰 Core 设备方法；桥接仅订阅事件、发射 Qt 信号，无设备 I/O。
//   - 通过 Qt 事件循环（信号/槽）承接事件，规避跨线程直连设备带来的线程
//     安全问题：数据到达 → 薄壳在订阅回调中发射 Qt 信号 → 槽在所属线程触发。
// ---------------------------------------------------------------------------
class QtSignalBridge : public QObject {
    Q_OBJECT

public:
    explicit QtSignalBridge(service::BufferService* buffer, QObject* parent = nullptr);
    ~QtSignalBridge() override;

    QtSignalBridge(const QtSignalBridge&) = delete;
    QtSignalBridge& operator=(const QtSignalBridge&) = delete;

Q_SIGNALS:
    void dataReceived(const QString& connectionId, const QByteArray& bytes);

private:
    service::BufferService* buffer_;
    std::uint64_t subId_{0};
};

}  // namespace portpilot::ui