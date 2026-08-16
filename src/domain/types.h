#ifndef PORTPILOT_DOMAIN_TYPES_H
#define PORTPILOT_DOMAIN_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace portpilot::domain {

using Bytes = std::vector<uint8_t>;
using ConnectionId = std::string;
using SessionId = std::string;
using SessionMode = std::string;
using ErrorCode = std::string;

enum class PortType {
    Serial,
    Network
};

enum class Parity {
    None,
    Even,
    Odd,
    Mark,
    Space
};

enum class StopBits {
    One,
    OnePointFive,
    Two
};

enum class DataBits : uint8_t {
    Five = 5,
    Six = 6,
    Seven = 7,
    Eight = 8
};

struct Result {
    bool ok{false};
    ErrorCode code{};
    std::string message{};

    static Result Ok() { return {true, "", ""}; }
    static Result Err(ErrorCode code, const std::string& msg = "") {
        return {false, std::move(code), msg};
    }
};

// ---------------------------------------------------------------------------
// 设备层错误码公共常量（SSOT）
//
// 对齐 service-api.md §9 DevicePort 接口错误约定与需求文档 2.2.1/9.3：
//   - 需求 DoD「设备不存在时返回 NoSuchDeviceError 提示」→ kNoSuchDeviceError
//   - 打开失败需区分权限（PermissionError）与设备不存在（NoSuchDeviceError）
// 值保持与既有后端实现一致（#37/PR#48 已验收，勿变更字符串值）。
// ---------------------------------------------------------------------------
inline constexpr const char* kNoSuchDeviceError    = "DEVICE_NO_SUCH";       // 设备不存在（NoSuchDeviceError）
inline constexpr const char* kDevicePermissionError = "DEVICE_PERMISSION";   // 无权限访问设备（PermissionError）
inline constexpr const char* kDeviceBusyError       = "DEVICE_BUSY";         // 设备被占用
inline constexpr const char* kDeviceNotOpenError    = "DEVICE_NOT_OPEN";     // 设备未打开
inline constexpr const char* kDeviceIoFailedError   = "DEVICE_IO_FAILED";    // 收发 I/O 失败
inline constexpr const char* kDeviceConfigError     = "DEVICE_CONFIG_FAILED";// 串口参数配置失败
inline constexpr const char* kDeviceOpenFailedError = "DEVICE_OPEN_FAILED";  // 打开设备失败（其它原因）

struct ConnectTarget {
    PortType type{PortType::Serial};

    // Serial fields
    std::string port;
    uint32_t baud{9600};
    DataBits dataBits{DataBits::Eight};
    StopBits stopBits{StopBits::One};
    Parity parity{Parity::None};

    // Network fields (reserved)
    std::string addr;
    uint16_t tcpPort{0};
};

using DataCallback = std::function<void(const Bytes&)>;
using ProbeCriteriaCallback = std::function<bool(const ConnectTarget&)>;

struct ProbeResult {
    bool success{false};
    std::string message{};
    ConnectTarget confirmedTarget{};
};

std::string to_string(Parity p);
std::string to_string(StopBits s);
std::string to_string(DataBits d);
std::string to_string(PortType t);

Parity parity_from_string(const std::string& s);
StopBits stop_bits_from_string(const std::string& s);
DataBits data_bits_from_int(int v);
PortType port_type_from_string(const std::string& s);

} // namespace portpilot::domain

#endif // PORTPILOT_DOMAIN_TYPES_H
