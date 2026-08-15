#pragma once

#include "domain/device_port.h"
#include <queue>
#include <mutex>
#include <vector>
#include <string>
#include <optional>
#include <functional>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// MockDevicePort（L2-002）：用于单元测试与装配根的设备模拟实现
// - 实现 domain::DevicePort（Domain 层定义的统一接口）
// - 支持读写回环 / 预设错误 / 注入接收字节序列
// - 线程安全（最小化，足够支撑单元测试即可）
// ---------------------------------------------------------------------------
class MockDevicePort : public domain::DevicePort {
public:
    MockDevicePort() = default;
    ~MockDevicePort() override = default;

    MockDevicePort(const MockDevicePort&) = delete;
    MockDevicePort& operator=(const MockDevicePort&) = delete;

    // ---- domain::DevicePort 接口 ----
    domain::Result open(const domain::ConnectTarget& config) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (fail_next_open_) {
            fail_next_open_ = false;
            return domain::Result::Err("DEV_OPEN_FAILED", "MockDevicePort 强制 open 失败");
        }
        last_config_ = config;
        opened_ = true;
        return domain::Result::Ok();
    }

    domain::Result close() override {
        std::lock_guard<std::mutex> lk(mu_);
        opened_ = false;
        return domain::Result::Ok();
    }

    domain::Result write(const domain::Bytes& data) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (!opened_) return domain::Result::Err("DEV_NOT_OPEN", "MockDevicePort 未打开");
        if (fail_next_write_) {
            fail_next_write_ = false;
            return domain::Result::Err("DEV_WRITE_FAILED", "MockDevicePort 强制 write 失败");
        }
        tx_bytes_.insert(tx_bytes_.end(), data.begin(), data.end());
        tx_history_.push_back(data);
        // 如果启用回环，把写入的字节转成接收队列
        if (loopback_) {
            rx_queue_.push(data);
            notify_data_callbacks(data);
        }
        return domain::Result::Ok();
    }

    domain::Bytes read() override {
        std::lock_guard<std::mutex> lk(mu_);
        if (!opened_) return {};
        // domain::DevicePort::read 无参数；内部使用 4096 作为单次上限
        constexpr std::size_t maxBytes = 4096;
        domain::Bytes out;
        out.reserve(maxBytes);
        while (out.size() < maxBytes && !rx_queue_.empty()) {
            auto& front = rx_queue_.front();
            const auto need = maxBytes - out.size();
            if (front.size() <= need) {
                out.insert(out.end(), front.begin(), front.end());
                rx_queue_.pop();
            } else {
                out.insert(out.end(), front.begin(), front.begin() + need);
                front.erase(front.begin(), front.begin() + need);
                break;
            }
        }
        return out;
    }

    void onData(domain::DataCallback cb) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (cb) data_callbacks_.push_back(std::move(cb));
    }

    bool isOpen() const override {
        std::lock_guard<std::mutex> lk(mu_);
        return opened_;
    }

    domain::ProbeResult probe(const domain::ConnectTarget& params,
                              domain::ProbeCriteriaCallback onCriteria) override {
        domain::ProbeResult r;
        r.confirmedTarget = params;
        r.success = false;
        if (fail_next_open_) {
            fail_next_open_ = false;
            r.message = "MockDevicePort 强制 probe 失败";
            return r;
        }
        bool criteriaOk = !onCriteria || onCriteria(params);
        if (!criteriaOk) {
            r.message = "探测判定条件未满足";
            return r;
        }
        r.success = true;
        r.message = "MockDevicePort 探测成功";
        return r;
    }

    // ---- Mock 辅助接口 ----
    void set_loopback(bool on) {
        std::lock_guard<std::mutex> lk(mu_);
        loopback_ = on;
    }

    void set_fail_next_open(bool v) { fail_next_open_ = v; }
    void set_fail_next_write(bool v) { fail_next_write_ = v; }

    // 注入接收字节（可触发 onData 回调）
    void inject_rx(const domain::Bytes& data) {
        std::vector<domain::DataCallback> callbacks_copy;
        {
            std::lock_guard<std::mutex> lk(mu_);
            rx_queue_.push(data);
            callbacks_copy = data_callbacks_;
        }
        for (const auto& cb : callbacks_copy) {
            try { cb(data); } catch (...) {}
        }
    }

    // 历史检查（测试断言用）
    std::size_t total_tx_bytes() const {
        std::lock_guard<std::mutex> lk(mu_);
        return tx_bytes_.size();
    }
    domain::Bytes tx_snapshot() const {
        std::lock_guard<std::mutex> lk(mu_);
        return tx_bytes_;
    }
    std::vector<domain::Bytes> tx_history() const {
        std::lock_guard<std::mutex> lk(mu_);
        return tx_history_;
    }
    std::size_t rx_pending() const {
        std::lock_guard<std::mutex> lk(mu_);
        std::size_t n = 0;
        auto q = rx_queue_;
        while (!q.empty()) { n += q.front().size(); q.pop(); }
        return n;
    }
    domain::ConnectTarget last_config() const {
        std::lock_guard<std::mutex> lk(mu_);
        return last_config_;
    }

private:
    void notify_data_callbacks(const domain::Bytes& data) {
        // mu_ 已由调用方持有
        for (const auto& cb : data_callbacks_) {
            try { cb(data); } catch (...) {}
        }
    }

    mutable std::mutex mu_;
    bool opened_{false};
    bool loopback_{false};
    bool fail_next_open_{false};
    bool fail_next_write_{false};
    std::queue<domain::Bytes> rx_queue_;
    domain::Bytes tx_bytes_;
    std::vector<domain::Bytes> tx_history_;
    std::vector<domain::DataCallback> data_callbacks_;
    domain::ConnectTarget last_config_;
};

}  // namespace portpilot::core
