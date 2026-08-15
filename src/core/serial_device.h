#ifndef PORTPILOT_CORE_SERIAL_DEVICE_H
#define PORTPILOT_CORE_SERIAL_DEVICE_H

#include "domain/device_port.h"
#include "platform/serial_backend.h"
#include <string>
#include <mutex>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// SerialDevice：跨平台串口实现（domain::DevicePort 契约实现方）
//
// 架构（Issue #50）：
//   - 所有平台相关 I/O 委托给 platform::ISerialBackend
//   - 双后端：POSIX（termios）/ Windows（CreateFile+DCB）
//   - Core 层 Qt-free：不依赖 QSerialPort；串口是 OS 能力
//   - 本类负责：线程安全锁 + onData 回调分发 + 与 domain::DevicePort 对齐
// ---------------------------------------------------------------------------
class SerialDevice : public domain::DevicePort {
public:
    SerialDevice();
    // 允许注入后端（便于测试时使用 Mock 后端）
    explicit SerialDevice(platform::ISerialBackendPtr backend);
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

    // 调试/扩展：底层后端 native handle
    int64_t nativeHandle() const;

private:
    mutable std::mutex mu_;
    platform::ISerialBackendPtr backend_;
    domain::DataCallback data_cb_;
};

} // namespace portpilot::core

#endif // PORTPILOT_CORE_SERIAL_DEVICE_H
