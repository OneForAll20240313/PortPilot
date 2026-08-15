#pragma once

#include "device_port.h"

namespace portpilot::core {

// ---------------------------------------------------------------------------
// SerialWorker：串口交互，实现 DevicePort（Core 层定义抽象/接口）
// - 真实使用 QSerialPort 的实现在 Service/UI 层派生本类（避免 Qt 依赖污染 Core 层）
// - 核心：提供 open/close/read/write/onData 的接口占位，默认行为与 Mock 类似，
//   便于装配根在无 Qt 环境下也能通过单测
// ---------------------------------------------------------------------------
class SerialWorker : public DevicePort {
public:
    SerialWorker() = default;
    ~SerialWorker() override = default;

    Result<void> open(const ConnectTarget& config) override {
        if (config.type != ConnectionType::Serial) {
            return make_err_void(ErrorCode::SESS_PARAM_INVALID, "SerialWorker 仅支持 type=serial");
        }
        if (!config.serial) {
            return make_err_void(ErrorCode::SESS_PARAM_INVALID, "SerialWorker 缺少 serial 配置");
        }
        const auto& s = *config.serial;
        if (s.port.empty()) {
            return make_err_void(ErrorCode::DEV_NOT_FOUND, "串口路径为空");
        }
        if (s.baud == 0) {
            return make_err_void(ErrorCode::SESS_PARAM_INVALID, "波特率为 0");
        }
        // Core 层占位：真正的 QSerialPort::open 由 Service/UI 层派生类实现
        opened_ = true;
        cfg_ = s;
        return make_ok();
    }

    Result<void> close() override {
        opened_ = false;
        return make_ok();
    }

    Result<std::size_t> write(const Bytes& data) override {
        if (!opened_) return make_err<std::size_t>(ErrorCode::DEV_NOT_OPEN, "SerialWorker 未打开");
        tx_total_ += data.size();
        last_tx_ = data;
        // 可选回调（便于测试断言）
        if (write_observer_) {
            try { write_observer_(data); } catch (...) {}
        }
        return make_ok(data.size());
    }

    Result<Bytes> read(std::size_t /*maxBytes*/ = 4096) override {
        if (!opened_) return make_err<Bytes>(ErrorCode::DEV_NOT_OPEN, "SerialWorker 未打开");
        // Core 层占位：真实 QSerialPort 读取在派生类
        if (pending_rx_.empty()) return make_ok(Bytes{});
        Bytes out = std::move(pending_rx_.front());
        pending_rx_.erase(pending_rx_.begin());
        return make_ok(std::move(out));
    }

    void onData(DataCallback cb) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (cb) cb_ = std::move(cb);
    }

    bool isOpen() const override { return opened_; }

    // ---- Core 层测试辅助 / 模拟注入（Qt 派生类可覆盖这些行为）----
    void inject_rx(const Bytes& data) {
        std::lock_guard<std::mutex> lk(mu_);
        pending_rx_.push_back(data);
        if (cb_) { try { cb_(data); } catch (...) {} }
    }

    using WriteObserver = std::function<void(const Bytes&)>;
    void set_write_observer(WriteObserver o) { write_observer_ = std::move(o); }

    std::optional<SerialConfig> serial_config() const {
        return opened_ ? std::optional{cfg_} : std::nullopt;
    }

    std::size_t total_tx() const { return tx_total_; }
    Bytes last_tx() const { return last_tx_; }

protected:
    bool opened_{false};
    SerialConfig cfg_;
    std::size_t tx_total_{0};
    Bytes last_tx_;
    std::vector<Bytes> pending_rx_;
    mutable std::mutex mu_;
    DataCallback cb_;
    WriteObserver write_observer_;
};

}  // namespace portpilot::core
