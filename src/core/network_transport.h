#pragma once

#include "domain/device_port.h"
#include <string>
#include <mutex>
#include <optional>
#include <vector>
#include <cstdint>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// NetworkTransport（TCP/IP 预留实现 domain::DevicePort）
// - 对齐 session.schema.json connectTarget.type == network
// - 纯 C++ 层做接口与占位；真实 socket/SSL 实现由上层（Service/Qt）补齐
// ---------------------------------------------------------------------------
class NetworkTransport : public domain::DevicePort {
public:
    NetworkTransport() = default;
    ~NetworkTransport() override = default;

    NetworkTransport(const NetworkTransport&) = delete;
    NetworkTransport& operator=(const NetworkTransport&) = delete;

    domain::Result open(const domain::ConnectTarget& config) override {
        if (config.type != domain::PortType::Network) {
            return domain::Result::Err("SESS_PARAM_INVALID", "NetworkTransport 仅支持 type=network");
        }
        if (config.addr.empty() || config.tcpPort == 0) {
            return domain::Result::Err("SESS_PARAM_INVALID", "NetworkTransport 缺少 addr/tcpPort");
        }
        // Core 层占位：真实实现在 Service/Qt 层进行 socket 连接
        opened_ = true;
        last_ = config;
        return domain::Result::Ok();
    }

    domain::Result close() override {
        opened_ = false;
        return domain::Result::Ok();
    }

    domain::Result write(const domain::Bytes& data) override {
        if (!opened_) return domain::Result::Err("DEV_NOT_OPEN", "NetworkTransport 未连接");
        // 占位：写入会通过真实 socket 发出；此处仅累加计数
        tx_total_ += data.size();
        return domain::Result::Ok();
    }

    domain::Bytes read() override {
        // 占位：Core 层不实现真实 I/O，返回空字节
        return {};
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
        if (params.addr.empty() || params.tcpPort == 0) {
            r.message = "缺少 addr/tcpPort";
            return r;
        }
        bool criteriaOk = !onCriteria || onCriteria(params);
        if (!criteriaOk) {
            r.message = "探测判定条件未满足";
            return r;
        }
        // Core 层占位：真实 TCP ping/connect 探测在 Service/Qt 层
        r.success = true;
        r.message = "NetworkTransport 占位探测成功";
        return r;
    }

    // ---- 预留接口：获取最近一次 open 时的网络参数 / 累计发送（测试用）----
    std::optional<domain::ConnectTarget> last_config() const {
        return opened_ ? std::optional{last_} : std::nullopt;
    }
    std::size_t total_tx() const { return tx_total_; }

private:
    bool opened_{false};
    domain::ConnectTarget last_;
    std::size_t tx_total_{0};
    mutable std::mutex mu_;
    domain::DataCallback cb_;
};

}  // namespace portpilot::core
