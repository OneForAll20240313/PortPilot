#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "domain/device_port.h"
#include "core/event_bus.h"

namespace portpilot::service {

// ---------------------------------------------------------------------------
// BufferService：字节流模块 Qt-free 内核（对齐 service-api.md §2 A-301 / A-302）
//
// 职责：承接 DevicePort（串口如 SerialWorker / 网络如 NetworkTransport）的接收
// 数据，通过 Core EventBus 发射 `buffer.dataReceived` 事件，供 Service/UI 层
// 订阅消费；并承载发送路径，发射 `buffer.sent` 事件。
//
// 架构对齐（13.7 协作约束）：
//   - Service 层只依赖 Domain 抽象（domain::DevicePort）与 Core 事件总线，
//     不 new 具体 Core 设备、不直接触碰 Core 设备方法，符合单向依赖。
//   - UI 层经本服务（而非直接订阅设备）获取数据，维持 UI → Service →
//     Domain/Core 的单向依赖，规避跨线程 UI 直连设备。
//
// #36 交付范围：SerialWorker → Qt 信号桥接的 Qt-free 内核部分。
//   onData（A-301）→ buffer.dataReceived 事件 → 供 Qt 薄壳订阅。
// ---------------------------------------------------------------------------
class BufferService {
public:
    BufferService() = default;
    ~BufferService() = default;

    BufferService(const BufferService&) = delete;
    BufferService& operator=(const BufferService&) = delete;

    // 订阅 buffer.dataReceived（收到包装后的连接 id 与字节）。
    // 返回订阅 id，可用于 off() 取消；返回 0 表示 handler 为空。
    std::uint64_t onDataReceived(core::EventHandler handler);

    // 直接投递数据：把 DevicePort 收到的字节连同连接 id 发射为 dataReceived 事件。
    // 对齐 A-301 onData(ConnectionId, bytes) → 发射 buffer.dataReceived。
    void pushData(const std::string& connectionId, const domain::Bytes& data);

    // A-301 契约方法名别名：语义与 pushData 相同（承接 DevicePort 接收数据，
    // 发射 buffer.dataReceived），提供与 service-api.md §2 A-301 一致的入口。
    void onData(const std::string& connectionId, const domain::Bytes& data) {
        pushData(connectionId, data);
    }

    // 取消事件订阅
    void off(std::uint64_t id);

    // 当前 dataReceived 订阅数（测试辅助）
    std::size_t dataReceivedListeners() const;

private:
    core::EventBus& bus_{core::EventBus::instance()};
};

}  // namespace portpilot::service