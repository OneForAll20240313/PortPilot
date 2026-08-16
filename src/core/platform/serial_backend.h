#ifndef PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_H
#define PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_H

#include "domain/types.h"
#include <cstdint>
#include <memory>
#include <string>

namespace portpilot::core::platform {

// ---------------------------------------------------------------------------
// ISerialBackend：跨平台串口后端（平台抽象层，纯虚接口）
//
// 设计原则：
//   - Core 层禁止直接依赖 Qt / QSerialPort；串口是操作系统能力
//   - 签名对齐 domain::DevicePort 契约（service-api §9）
//   - 不承担线程安全/回调分发，由上层 SerialDevice 统一包装
//   - 编译期 #ifdef _WIN32 隔离后端，CMake 按平台选源文件
// ---------------------------------------------------------------------------
class ISerialBackend {
public:
    virtual ~ISerialBackend() = default;

    // 打开串口；失败返回错误码对齐契约
    virtual domain::Result open(const domain::ConnectTarget& config) = 0;

    // 关闭串口
    virtual domain::Result close() = 0;

    // 写入数据；阻塞直到全部写入或出错
    virtual domain::Result write(const domain::Bytes& data) = 0;

    // 读取数据；非阻塞，返回当前可用的字节，空表示无数据
    virtual domain::Bytes read() = 0;

    // 是否已打开
    virtual bool isOpen() const = 0;

    // 探测设备是否存在/可达/可打开；可选回调附加判定
    virtual domain::ProbeResult probe(const domain::ConnectTarget& params,
                                      domain::ProbeCriteriaCallback onCriteria) = 0;

    // 平台特有的 native handle（调试/扩展用，默认 -1 / INVALID_HANDLE_VALUE）
    virtual int64_t nativeHandle() const = 0;

    // 是否被占用（辅助方法：打开失败返回 DEVICE_BUSY 时可判定）
    virtual bool isBusy(const std::string& port) const = 0;
};

using ISerialBackendPtr = std::unique_ptr<ISerialBackend>;

// ---------------------------------------------------------------------------
// SerialBackendFactory：按平台创建对应后端
// - POSIX/Linux/macOS -> SerialBackendPosix
// - Windows            -> SerialBackendWindows
// ---------------------------------------------------------------------------
class SerialBackendFactory {
public:
    static ISerialBackendPtr Create();
};

} // namespace portpilot::core::platform

#endif // PORTPILOT_CORE_PLATFORM_SERIAL_BACKEND_H
