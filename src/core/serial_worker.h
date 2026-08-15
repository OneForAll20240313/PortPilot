#pragma once

#include "domain/device_port.h"
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <functional>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// SerialWorker：串口交互，实现 domain::DevicePort（Domain 层定义抽象/接口）
// - 真实使用 QSerialPort 的实现在 Service/UI 层派生本类（避免 Qt 依赖污染 Core 层）
// - 核心：提供 open/close/read/write/onData 的接口占位，默认行为与 Mock 类似，
//   便于装配根在无 Qt 环境下也能通过单测
// ---------------------------------------------------------------------------
class SerialWorker : public domain::DevicePort {
public:
    SerialWorker() = default;
    ~SerialWorker() override = default;

    SerialWorker(const SerialWorker&) = delete;
    SerialWorker& operator=(const SerialWorker&) = delete;

    domain::Result open(const domain::ConnectTarget& config) override {
        if (config.type != domain::PortType::Serial) {
            return domain::Result::Err("SESS_PARAM_INVALID", "SerialWorker 仅支持 type=serial");
        }
        if (config.port.empty()) {
            return domain::Result::Err("SESS_PARAM_INVALID", "串口路径为空");
        }
        if (config.baud == 0) {
            return domain::Result::Err("SESS_PARAM_INVALID", "波特率为 0");
        }
        // Core 层占位：真正的 QSerialPort::open 由 Service/UI 层派生类实现
        opened_ = true;
        cfg_ = config;
        return domain::Result::Ok();
    }

    domain::Result close() override {
        opened_ = false;
        return domain::Result::Ok();
    }

    domain::Result write(const domain::Bytes& data) override {
        if (!opened_) return domain::Result::Err("DEV_NOT_OPEN", "SerialWorker 未打开");
        tx_total_ += data.size();
        last_tx_ = data;
        if (write_observer_) {
            try { write_observer_(data); } catch (...) {}
        }
        return domain::Result::Ok();
    }

    domain::Bytes read() override {
        std::lock_guard<std::mutex> lk(mu_);
        if (!opened_) return {};
        // Core 层占位：真实 QSerialPort 读取在派生类
        if (pending_rx_.empty()) return {};
        domain::Bytes out = std::move(pending_rx_.front());
        pending_rx_.erase(pending_rx_.begin());
        return out;
    }

    void onData(domain::DataCallback cb) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (cb) cb_ = std::move(cb);
    }

    bool isOpen() const override { return opened_; }

    domain::ProbeResult probe(const domain::ConnectTarget& params,
                              domain::ProbeCriteriaCallback onCriteria) override {
        domain::ProbeResult r;
        r.confirmedTarget = params;
        r.success = false;
        if (params.port.empty()) {
            r.message = "设备路径为空";
            return r;
        }
        // Core 层占位：真实探测在派生类或 SerialDevice 中实现
        bool criteriaOk = !onCriteria || onCriteria(params);
        if (!criteriaOk) {
            r.message = "探测判定条件未满足";
            return r;
        }
        r.success = true;
        r.message = "SerialWorker 占位探测成功";
        return r;
    }

    // ---- Core 层测试辅助 / 模拟注入（Qt 派生类可覆盖这些行为）----
    void inject_rx(const domain::Bytes& data) {
        domain::DataCallback cb_copy;
        {
            std::lock_guard<std::mutex> lk(mu_);
            pending_rx_.push_back(data);
            cb_copy = cb_;
        }
        if (cb_copy) { try { cb_copy(data); } catch (...) {} }
    }

    using WriteObserver = std::function<void(const domain::Bytes&)>;
    void set_write_observer(WriteObserver o) { write_observer_ = std::move(o); }

    std::optional<domain::ConnectTarget> serial_config() const {
        return opened_ ? std::optional{cfg_} : std::nullopt;
    }

    std::size_t total_tx() const { return tx_total_; }
    domain::Bytes last_tx() const { return last_tx_; }

protected:
    bool opened_{false};
    domain::ConnectTarget cfg_;
    std::size_t tx_total_{0};
    domain::Bytes last_tx_;
    std::vector<domain::Bytes> pending_rx_;
    mutable std::mutex mu_;
    domain::DataCallback cb_;
    WriteObserver write_observer_;
};

}  // namespace portpilot::core
