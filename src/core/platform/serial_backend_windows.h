#ifndef PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_WINDOWS_H
#define PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_WINDOWS_H

#include "serial_backend.h"
#include <mutex>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
// 非 Windows 平台兜底：保证头文件可被 #include（factory 需要两个类的完整类型可见）
// 这些 typedef 仅用于类型声明；真实实现代码不会执行（编译期 CMake 只选单平台源文件）
#include <cstdint>
using DWORD = uint32_t;
using HANDLE = void*;
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#endif
#endif

namespace portpilot::core::platform {

// ---------------------------------------------------------------------------
// SerialBackendWindows：Windows 串口后端
//   - 使用 CreateFile / SetCommState(DCB) / ReadFile / WriteFile / CloseHandle
//   - 编译期隔离：仅 _WIN32 下编译
//   - 对齐契约 §9：与 POSIX 后端保持行为一致
// ---------------------------------------------------------------------------
class SerialBackendWindows : public ISerialBackend {
public:
    SerialBackendWindows() = default;
    ~SerialBackendWindows() override;

    SerialBackendWindows(const SerialBackendWindows&) = delete;
    SerialBackendWindows& operator=(const SerialBackendWindows&) = delete;

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
    static domain::Result OpenErrorFromWin32(DWORD err, const std::string& path);
    domain::Result ApplyDCB(const domain::ConnectTarget& config);

#ifdef _WIN32
    HANDLE h_{INVALID_HANDLE_VALUE};
#else
    int64_t h_{-1};
#endif
    mutable std::mutex mu_;
    std::string opened_path_;
};

} // namespace portpilot::core::platform

#endif // PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_WINDOWS_H
