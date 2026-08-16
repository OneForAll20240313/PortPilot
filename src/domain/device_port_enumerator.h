#ifndef PORTPILOT_DOMAIN_DEVICE_PORT_ENUMERATOR_H
#define PORTPILOT_DOMAIN_DEVICE_PORT_ENUMERATOR_H

#include "types.h"
#include <memory>
#include <string>
#include <vector>

namespace portpilot::domain {

// ---------------------------------------------------------------------------
// PortInfo：可用串口信息（对齐契约 service-api.md §1 A-214 / §9）
//   枚举系统可用串口，含 /dev/tty*、/dev/pts/* 等非标准设备（需求 #34）
// ---------------------------------------------------------------------------
struct PortInfo {
    std::string id;              // 唯一标识（此处为设备路径）
    std::string name;            // 设备名（如 ttyUSB0 / pts/0）
    std::string friendlyName;    // 友好名（此处复用设备路径）
    bool isBusy{false};          // 是否被占用
    std::string vendorId;        // USB vendor id（非 USB 设备为空）
    std::string productId;       // USB product id（非 USB 设备为空）
    std::string systemLocation;  // 系统位置（此处为设备路径）
    bool isLoopback{false};      // 是否回环设备
};

// ---------------------------------------------------------------------------
// PortEnumResult：枚举结果（对齐 ProbeResult 风格，携带错误码）
//   错误码约定：SESS_PORT_ENUM_FAILED（A-214 枚举失败）
// ---------------------------------------------------------------------------
struct PortEnumResult {
    bool ok{false};
    ErrorCode code{};
    std::string message{};
    std::vector<PortInfo> ports{};
};

// ---------------------------------------------------------------------------
// DevicePortEnumerator：系统级串口发现接口（契约 §9）
//   语义独立于单个连接实例 DevicePort；由 Core 层 PortEnumerator 实现
// ---------------------------------------------------------------------------
class DevicePortEnumerator {
public:
    virtual ~DevicePortEnumerator() = default;

    // 枚举系统可用串口（含 pty 等非标准设备）
    virtual PortEnumResult listAvailablePorts() = 0;
};

using DevicePortEnumeratorPtr = std::unique_ptr<DevicePortEnumerator>;

} // namespace portpilot::domain

#endif // PORTPILOT_DOMAIN_DEVICE_PORT_ENUMERATOR_H