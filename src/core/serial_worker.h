#pragma once

#include "domain/device_port.h"
#include "platform/serial_backend.h"
#include "serial_device.h"
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <functional>
#include <memory>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// SerialWorker：串口交互工作者（Issue #50 重构）
//
// 旧设计（pre-#50）：Core 层占位 mock，Service/UI 层通过 QSerialPort 派生实现。
// 新设计（#50 方案 A，Qt-free）：
//   - SerialWorker 直接通过轻量平台抽象层（platform::ISerialBackend）完成真实串口操作
//   - Core 层自管双后端：POSIX / Windows，不依赖 Qt / QSerialPort
//   - 同时保留 inject_rx / set_write_observer 等测试辅助钩子，便于单测和调试
//   - 仍继承 domain::DevicePort，对齐契约 §9
// ---------------------------------------------------------------------------
class SerialWorker : public domain::DevicePort {
public:
    SerialWorker();
    // 测试友好：注入后端
    explicit SerialWorker(platform::ISerialBackendPtr backend);
    ~SerialWorker() override = default;

    SerialWorker(const SerialWorker&) = delete;
    SerialWorker& operator=(const SerialWorker&) = delete;

    // ---- domain::DevicePort 接口实现 ----
    domain::Result open(const domain::ConnectTarget& config) override;
    domain::Result close() override;
    domain::Result write(const domain::Bytes& data) override;
    domain::Bytes read() override;
    void onData(domain::DataCallback cb) override;
    bool isOpen() const override;
    domain::ProbeResult probe(const domain::ConnectTarget& params,
                              domain::ProbeCriteriaCallback onCriteria) override;

    // ---- 测试辅助 / 模拟注入（Qt-free 场景下的调试钩子）----
    // 注入接收字节：推送到 pending_rx_ 并触发 onData 回调
    void inject_rx(const domain::Bytes& data);

    using WriteObserver = std::function<void(const domain::Bytes&)>;
    void set_write_observer(WriteObserver o) { write_observer_ = std::move(o); }

    std::optional<domain::ConnectTarget> serial_config() const {
        return opened_ ? std::optional{cfg_} : std::nullopt;
    }

    std::size_t total_tx() const { return tx_total_; }
    domain::Bytes last_tx() const { return last_tx_; }

private:
    std::unique_ptr<SerialDevice> dev_;   // 委托给真实跨平台 SerialDevice

    // 状态与统计
    bool opened_{false};
    domain::ConnectTarget cfg_;
    std::size_t tx_total_{0};
    domain::Bytes last_tx_;

    // 注入/队列能力（测试辅助 + 作为 read() 的补充缓冲区）
    std::vector<domain::Bytes> pending_rx_;
    mutable std::mutex mu_;
    domain::DataCallback cb_;
    WriteObserver write_observer_;
};

}  // namespace portpilot::core
