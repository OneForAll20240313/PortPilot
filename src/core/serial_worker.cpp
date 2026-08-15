#include "serial_worker.h"
#include "domain/types.h"
#include <mutex>

namespace portpilot::core {

using domain::Result;
using domain::ConnectTarget;
using domain::Bytes;
using domain::PortType;

SerialWorker::SerialWorker()
    : dev_(std::make_unique<SerialDevice>()) {}

SerialWorker::SerialWorker(platform::ISerialBackendPtr backend)
    : dev_(std::make_unique<SerialDevice>(std::move(backend))) {}

Result SerialWorker::open(const ConnectTarget& config) {
    if (config.type != PortType::Serial) {
        return Result::Err("SESS_PARAM_INVALID", "SerialWorker 仅支持 type=serial");
    }
    if (config.port.empty()) {
        return Result::Err("SESS_PARAM_INVALID", "串口路径为空");
    }
    if (config.baud == 0) {
        return Result::Err("SESS_PARAM_INVALID", "波特率为 0");
    }
    Result r = dev_->open(config);
    if (!r.ok) return r;
    opened_ = true;
    cfg_ = config;
    return Result::Ok();
}

Result SerialWorker::close() {
    Result r = dev_->close();
    opened_ = false;
    cfg_ = ConnectTarget{};
    return r;
}

Result SerialWorker::write(const Bytes& data) {
    if (!opened_) return Result::Err("DEV_NOT_OPEN", "SerialWorker 未打开");
    Result r = dev_->write(data);
    if (r.ok) {
        tx_total_ += data.size();
        last_tx_ = data;
        if (write_observer_) {
            try { write_observer_(data); } catch (...) {}
        }
    }
    return r;
}

Bytes SerialWorker::read() {
    // 优先返回注入的 pending_rx_（测试辅助/上层转发场景）
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!pending_rx_.empty()) {
            Bytes out = std::move(pending_rx_.front());
            pending_rx_.erase(pending_rx_.begin());
            return out;
        }
    }
    // 否则从真实后端读取
    Bytes fromBackend = dev_->read();
    if (!fromBackend.empty()) {
        domain::DataCallback cb_copy;
        {
            std::lock_guard<std::mutex> lk(mu_);
            cb_copy = cb_;
        }
        if (cb_copy) {
            try { cb_copy(fromBackend); } catch (...) {}
        }
    }
    return fromBackend;
}

void SerialWorker::onData(domain::DataCallback cb) {
    std::lock_guard<std::mutex> lk(mu_);
    if (cb) cb_ = std::move(cb);
}

bool SerialWorker::isOpen() const { return opened_; }

domain::ProbeResult SerialWorker::probe(const ConnectTarget& params,
                                         domain::ProbeCriteriaCallback onCriteria) {
    return dev_->probe(params, std::move(onCriteria));
}

void SerialWorker::inject_rx(const Bytes& data) {
    domain::DataCallback cb_copy;
    {
        std::lock_guard<std::mutex> lk(mu_);
        pending_rx_.push_back(data);
        cb_copy = cb_;
    }
    if (cb_copy) { try { cb_copy(data); } catch (...) {} }
}

}  // namespace portpilot::core
