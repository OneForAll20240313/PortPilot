#include "serial_device.h"
#include "platform/serial_backend.h"
#include "domain/types.h"
#include <mutex>

namespace portpilot::core {

using domain::Result;
using domain::ConnectTarget;
using domain::Bytes;

SerialDevice::SerialDevice()
    : backend_(platform::SerialBackendFactory::Create()) {}

SerialDevice::SerialDevice(platform::ISerialBackendPtr backend)
    : backend_(std::move(backend)) {
    if (!backend_) {
        backend_ = platform::SerialBackendFactory::Create();
    }
}

SerialDevice::~SerialDevice() = default;

Result SerialDevice::open(const ConnectTarget& config) {
    return backend_->open(config);
}

Result SerialDevice::close() {
    return backend_->close();
}

Result SerialDevice::write(const Bytes& data) {
    return backend_->write(data);
}

Bytes SerialDevice::read() {
    Bytes buf = backend_->read();
    domain::DataCallback cb_copy;
    {
        std::lock_guard<std::mutex> lk(mu_);
        cb_copy = data_cb_;
    }
    if (cb_copy && !buf.empty()) {
        try {
            cb_copy(buf);
        } catch (...) {
        }
    }
    return buf;
}

void SerialDevice::onData(domain::DataCallback callback) {
    std::lock_guard<std::mutex> lk(mu_);
    data_cb_ = std::move(callback);
}

bool SerialDevice::isOpen() const {
    return backend_->isOpen();
}

domain::ProbeResult SerialDevice::probe(const ConnectTarget& params,
                                         domain::ProbeCriteriaCallback onCriteria) {
    return backend_->probe(params, std::move(onCriteria));
}

int64_t SerialDevice::nativeHandle() const {
    return backend_->nativeHandle();
}

} // namespace portpilot::core
