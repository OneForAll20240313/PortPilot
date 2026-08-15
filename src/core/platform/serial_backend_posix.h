#ifndef PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_POSIX_H
#define PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_POSIX_H

#include "serial_backend.h"
#include <mutex>
#include <string>

namespace portpilot::core::platform {

// ---------------------------------------------------------------------------
// SerialBackendPosix：POSIX 串口后端（Linux / macOS / BSD）
//   - 使用 termios 配置串口参数
//   - 非阻塞 I/O（O_NONBLOCK）+ read/write 循环
//   - 对齐契约 §9：VMIN=1 / VTIME=0 阻塞至少 1 字节
// ---------------------------------------------------------------------------
class SerialBackendPosix : public ISerialBackend {
public:
    SerialBackendPosix() = default;
    ~SerialBackendPosix() override;

    SerialBackendPosix(const SerialBackendPosix&) = delete;
    SerialBackendPosix& operator=(const SerialBackendPosix&) = delete;

    domain::Result open(const domain::ConnectTarget& config) override;
    domain::Result close() override;
    domain::Result write(const domain::Bytes& data) override;
    domain::Bytes read() override;
    bool isOpen() const override;
    domain::ProbeResult probe(const domain::ConnectTarget& params,
                              domain::ProbeCriteriaCallback onCriteria) override;
    int64_t nativeHandle() const override;
    bool isBusy(const std::string& port) const override;

private:
    static domain::Result OpenErrorFromErrno(int err, const std::string& path);
    domain::Result ApplyTermios(const domain::ConnectTarget& config);

    mutable std::mutex mu_;
    int fd_{-1};
    std::string opened_path_;
};

} // namespace portpilot::core::platform

#endif // PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_POSIX_H
