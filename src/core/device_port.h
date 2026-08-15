#pragma once

#include "common_types.h"
#include <functional>
#include <string>
#include <cstdint>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// DevicePort 接口（Core 层，L2-001；对齐 service-api.md §9）
// 跨平台设备抽象接口；串口 SerialWorker 与网络 NetworkTransport 均实现此接口
// UI/Service 不直接调用，由会话 connect/disconnect 间接使用
// ---------------------------------------------------------------------------
class DevicePort {
public:
    virtual ~DevicePort() = default;

    // 打开连接
    virtual Result<void> open(const ConnectTarget& config) = 0;
    // 关闭连接
    virtual Result<void> close() = 0;
    // 同步发送
    virtual Result<std::size_t> write(const Bytes& data) = 0;
    // 同步读取（可选读取上限；空返回表示 EOF / 关闭）
    virtual Result<Bytes> read(std::size_t maxBytes = 4096) = 0;

    // 数据到达回调（供 Service 层转 buffer.dataReceived）
    using DataCallback = std::function<void(const Bytes& data)>;
    virtual void onData(DataCallback cb) = 0;

    // 当前是否打开
    virtual bool isOpen() const = 0;

    // 探测（架构预留，D-53 低优先级；默认实现：失败）
    struct ProbeResult {
        bool reachable{false};
        std::uint32_t rttMs{0};
        std::string detail;
    };
    virtual Result<ProbeResult> probe(const ConnectTarget& /*params*/,
                                      std::function<bool(const ProbeResult&)> /*onCriteria*/ = nullptr) {
        return make_err<ProbeResult>(ErrorCode::ERR_PRECONDITION, "DevicePort::probe 未在当前实现启用");
    }
};

}  // namespace portpilot::core
