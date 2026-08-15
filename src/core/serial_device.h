#ifndef PORTPILOT_CORE_SERIAL_DEVICE_H
#define PORTPILOT_CORE_SERIAL_DEVICE_H

#include "domain/device_port.h"
#include <string>
#include <mutex>

namespace portpilot::core {

class SerialDevice : public domain::DevicePort {
public:
    SerialDevice() = default;
    ~SerialDevice() override;

    SerialDevice(const SerialDevice&) = delete;
    SerialDevice& operator=(const SerialDevice&) = delete;

    domain::Result open(const domain::ConnectTarget& config) override;
    domain::Result close() override;
    domain::Result write(const domain::Bytes& data) override;
    domain::Bytes read() override;
    void onData(domain::DataCallback callback) override;
    bool isOpen() const override;
    domain::ProbeResult probe(const domain::ConnectTarget& params,
                              domain::ProbeCriteriaCallback onCriteria) override;

    int fd() const { return fd_; }

private:
    static domain::Result OpenErrorFromErrno(int err, const std::string& path);
    domain::Result ApplyTermios(const domain::ConnectTarget& config);

    mutable std::mutex mu_;
    int fd_{-1};
    std::string opened_path_;
    domain::DataCallback data_cb_;
};

} // namespace portpilot::core

#endif // PORTPILOT_CORE_SERIAL_DEVICE_H
