#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <system_error>
#include <type_traits>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// 字节序列（跨模块统一）
// ---------------------------------------------------------------------------
using Bytes = std::vector<std::uint8_t>;

// ---------------------------------------------------------------------------
// 连接目标（对齐 session.schema.json connectTarget）
// ---------------------------------------------------------------------------
enum class ConnectionType { Serial, Network };

enum class Parity { None, Even, Odd, Mark, Space };
enum class StopBits { One, OnePointFive, Two };
enum class DataBits : int { Five = 5, Six = 6, Seven = 7, Eight = 8 };

struct SerialConfig {
    std::string port;
    std::uint32_t baud{9600};
    DataBits dataBits{DataBits::Eight};
    StopBits stopBits{StopBits::One};
    Parity parity{Parity::None};
};

struct NetworkConfig {
    std::string addr;
    std::uint16_t tcpPort{0};
};

struct ConnectTarget {
    ConnectionType type{ConnectionType::Serial};
    std::optional<SerialConfig> serial;
    std::optional<NetworkConfig> network;
};

// ---------------------------------------------------------------------------
// 会话状态机（D-38 四态，对齐 session.schema.json state）
// ---------------------------------------------------------------------------
enum class SessionState { Offline, Connecting, Online, Disconnecting };

// ---------------------------------------------------------------------------
// 错误码（对齐 service-api.md 错误码约定；含通用 ERR_PRECONDITION）
// 各模块前缀：SESS / BUF / PROTO / VIZ / TERM / CMD / CFG / ROUTE
// ---------------------------------------------------------------------------
enum class ErrorCode {
    // 通用
    OK = 0,
    ERR_PRECONDITION,

    // 会话 SESS_
    SESS_PARAM_INVALID,
    SESS_NOT_FOUND,
    SESS_NAME_INVALID,
    SESS_BUSY,
    SESS_STATE_INVALID,
    SESS_MODE_CONFLICT,
    SESS_IO_FAILED,

    // 字节流 BUF_
    BUF_CONN_INVALID,
    BUF_SEND_FAILED,
    BUF_KEYWORD_INVALID,
    BUF_PARAM_INVALID,
    BUF_TAG_INVALID,
    BUF_CTRL_INVALID,

    // 协议 PROTO_
    PROTO_FRAME_INVALID,
    PROTO_FIELD_INVALID,
    PROTO_PARAM_INVALID,
    PROTO_NOT_FOUND,
    PROTO_IN_USE,
    PROTO_ENCODE_INVALID,
    PROTO_CONN_INVALID,
    PROTO_LOOPBACK_TIMEOUT,
    PROTO_LOOPBACK_MISMATCH,
    PROTO_TEMPLATE_INVALID,

    // 可视化 VIZ_
    VIZ_TYPE_INVALID,
    VIZ_PROTO_INVALID,
    VIZ_BIND_INVALID,
    VIZ_CONNECT_INVALID,
    VIZ_SCENE_NOT_FOUND,
    VIZ_PARAM_INVALID,
    VIZ_ELEMENT_NOT_FOUND,
    VIZ_SOURCE_NOT_FOUND,
    VIZ_VIEW_INVALID,
    VIZ_CUSTOM_INVALID,

    // 终端 TERM_
    TERM_LINE_INVALID,
    TERM_CONN_INVALID,
    TERM_CMD_NOT_FOUND,
    TERM_CMD_INVALID,
    TERM_PLACEHOLDER_INVALID,
    TERM_DANGER_CONFIRM,

    // 命令 CMD_
    CMD_PARAM_INVALID,
    CMD_STATE_INVALID,
    CMD_CONN_INVALID,
    CMD_NOT_FOUND,
    CMD_MACRO_INVALID,
    CMD_MACRO_NOT_FOUND,
    CMD_SCRIPT_INVALID,
    CMD_SDK_CONN,
    CMD_SDK_SEND,
    CMD_SDK_READ,
    CMD_SDK_TIMEOUT,
    CMD_FIELD_NOT_FOUND,

    // 设置 CFG_
    CFG_KEY_NOT_FOUND,
    CFG_PARAM_INVALID,
    CFG_ONLINE_LOCKED,
    CFG_NOT_FOUND,
    CFG_IO_FAILED,
    CFG_THEME_INVALID,

    // 路由 ROUTE_
    ROUTE_VIEW_INVALID,
    ROUTE_MODULE_INVALID,
    ROUTE_SESSION_INVALID,
    ROUTE_NO_BACK,

    // DevicePort 级
    DEV_OPEN_FAILED,
    DEV_NOT_OPEN,
    DEV_WRITE_FAILED,
    DEV_READ_FAILED,
    DEV_PERMISSION_DENIED,
    DEV_NOT_FOUND,
};

// Result<T>：基于 error_code 的结果类型
struct Error {
    ErrorCode code{ErrorCode::OK};
    std::string message;
};

template <typename T>
struct Result {
    std::optional<T> value;
    Error error;

    bool ok() const noexcept { return error.code == ErrorCode::OK && value.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
    const T& operator*() const { return *value; }
    T& operator*() { return *value; }
    const T* operator->() const { return &*value; }
    T* operator->() { return &*value; }
};

template <>
struct Result<void> {
    Error error;
    bool ok() const noexcept { return error.code == ErrorCode::OK; }
    explicit operator bool() const noexcept { return ok(); }
};

// 便捷构造（自动推导 decay 后的类型，既支持右值也支持左值）
template <typename T>
auto make_ok(T&& v) -> Result<std::decay_t<T>> {
    using U = std::decay_t<T>;
    return Result<U>{U{std::forward<T>(v)}, Error{ErrorCode::OK, {}}};
}

inline Result<void> make_ok() {
    return Result<void>{Error{ErrorCode::OK, {}}};
}

template <typename T>
Result<T> make_err(ErrorCode c, std::string msg = {}) {
    return Result<T>{std::nullopt, Error{c, std::move(msg)}};
}

inline Result<void> make_err_void(ErrorCode c, std::string msg = {}) {
    return Result<void>{Error{c, std::move(msg)}};
}

// ---------------------------------------------------------------------------
// UUID 生成（简易 v4 占位，满足 Core 层 id 分配即可）
// ---------------------------------------------------------------------------
std::string gen_uuid_v4();

}  // namespace portpilot::core
