#pragma once

#include "device_port.h"
#include <queue>
#include <mutex>
#include <vector>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// MockDevicePort（L2-002）：用于单元测试与装配根的设备模拟实现
// - 支持读写回环 / 预设错误 / 注入接收字节序列
// - 线程安全（最小化，足够支撑单元测试即可）
// ---------------------------------------------------------------------------
class MockDevicePort : public DevicePort {
public:
    MockDevicePort() = default;
    ~MockDevicePort() override = default;

    // ---- DevicePort 接口 ----
    Result<void> open(const ConnectTarget& config) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (fail_next_open_) {
            fail_next_open_ = false;
            return make_err_void(ErrorCode::DEV_OPEN_FAILED, "MockDevicePort 强制 open 失败");
        }
        last_config_ = config;
        opened_ = true;
        return make_ok();
    }

    Result<void> close() override {
        std::lock_guard<std::mutex> lk(mu_);
        opened_ = false;
        return make_ok();
    }

    Result<std::size_t> write(const Bytes& data) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (!opened_) return make_err<std::size_t>(ErrorCode::DEV_NOT_OPEN, "MockDevicePort 未打开");
        if (fail_next_write_) {
            fail_next_write_ = false;
            return make_err<std::size_t>(ErrorCode::DEV_WRITE_FAILED, "MockDevicePort 强制 write 失败");
        }
        tx_bytes_.insert(tx_bytes_.end(), data.begin(), data.end());
        tx_history_.push_back(data);
        // 如果启用回环，把写入的字节转成接收队列
        if (loopback_) {
            rx_queue_.push(data);
            notify_data_callbacks(data);
        }
        return make_ok(data.size());
    }

    Result<Bytes> read(std::size_t maxBytes = 4096) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (!opened_) return make_err<Bytes>(ErrorCode::DEV_NOT_OPEN, "MockDevicePort 未打开");
        Bytes out;
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
        return make_ok(std::move(out));
    }

    void onData(DataCallback cb) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (cb) data_callbacks_.push_back(std::move(cb));
    }

    bool isOpen() const override {
        std::lock_guard<std::mutex> lk(mu_);
        return opened_;
    }

    // ---- Mock 辅助接口 ----
    void set_loopback(bool on) {
        std::lock_guard<std::mutex> lk(mu_);
        loopback_ = on;
    }

    void set_fail_next_open(bool v) { fail_next_open_ = v; }
    void set_fail_next_write(bool v) { fail_next_write_ = v; }

    // 注入接收字节（可触发 onData 回调）
    void inject_rx(const Bytes& data) {
        std::lock_guard<std::mutex> lk(mu_);
        rx_queue_.push(data);
        notify_data_callbacks(data);
    }

    // 历史检查（测试断言用）
    std::size_t total_tx_bytes() const {
        std::lock_guard<std::mutex> lk(mu_);
        return tx_bytes_.size();
    }
    Bytes tx_snapshot() const {
        std::lock_guard<std::mutex> lk(mu_);
        return tx_bytes_;
    }
    std::vector<Bytes> tx_history() const {
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
    ConnectTarget last_config() const {
        std::lock_guard<std::mutex> lk(mu_);
        return last_config_;
    }

private:
    void notify_data_callbacks(const Bytes& data) {
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
    std::queue<Bytes> rx_queue_;
    Bytes tx_bytes_;
    std::vector<Bytes> tx_history_;
    std::vector<DataCallback> data_callbacks_;
    ConnectTarget last_config_;
};

}  // namespace portpilot::core
