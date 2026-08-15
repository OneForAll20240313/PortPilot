#pragma once

#include "device_port.h"

namespace portpilot::core {

// ---------------------------------------------------------------------------
// NetworkTransport（TCP/IP 预留实现 DevicePort）
// - 对齐 session.schema.json connectTarget.type == network
// - 纯 C++ 层做接口与占位；真实 socket/SSL 实现由上层（Service/Qt）补齐
// ---------------------------------------------------------------------------
class NetworkTransport : public DevicePort {
public:
    NetworkTransport() = default;
    ~NetworkTransport() override = default;

    Result<void> open(const ConnectTarget& config) override {
        if (config.type != ConnectionType::Network) {
            return make_err_void(ErrorCode::SESS_PARAM_INVALID, "NetworkTransport 仅支持 type=network");
        }
        if (!config.network || config.network->addr.empty() || config.network->tcpPort == 0) {
            return make_err_void(ErrorCode::SESS_PARAM_INVALID, "NetworkTransport 缺少 addr/tcpPort");
        }
        // Core 层占位：真实实现在 Service/Qt 层进行 socket 连接
        opened_ = true;
        last_ = *config.network;
        return make_ok();
    }

    Result<void> close() override {
        opened_ = false;
        return make_ok();
    }

    Result<std::size_t> write(const Bytes& data) override {
        if (!opened_) return make_err<std::size_t>(ErrorCode::DEV_NOT_OPEN, "NetworkTransport 未连接");
        // 占位：写入会通过真实 socket 发出；此处仅累加计数
        tx_total_ += data.size();
        return make_ok(data.size());
    }

    Result<Bytes> read(std::size_t /*maxBytes*/ = 4096) override {
        if (!opened_) return make_err<Bytes>(ErrorCode::DEV_NOT_OPEN, "NetworkTransport 未连接");
        // 占位：Core 层不实现真实 I/O，返回空字节
        return make_ok(Bytes{});
    }

    void onData(DataCallback cb) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (cb) cb_ = std::move(cb);
    }

    bool isOpen() const override { return opened_; }

    // ---- 预留接口：获取最近一次 open 时的网络参数 / 累计发送（测试用）----
    std::optional<NetworkConfig> last_config() const { return opened_ ? std::optional{last_} : std::nullopt; }
    std::size_t total_tx() const { return tx_total_; }

private:
    bool opened_{false};
    NetworkConfig last_;
    std::size_t tx_total_{0};
    mutable std::mutex mu_;
    DataCallback cb_;
};

}  // namespace portpilot::core
