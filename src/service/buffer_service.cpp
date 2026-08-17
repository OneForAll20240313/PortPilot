#include "buffer_service.h"

#include <chrono>
#include <utility>

namespace portpilot::service {

namespace {

// 事件名（对齐 events.md 字节流域，勿改字符串值）
constexpr const char* kEventDataReceived = "buffer.dataReceived";
constexpr const char* kEventSent = "buffer.sent";

// 载荷字段键
constexpr const char* kFieldConnectionId = "connectionId";
constexpr const char* kFieldBytes = "bytes";
constexpr const char* kFieldTimestamp = "timestamp";

}  // namespace

std::uint64_t BufferService::onDataReceived(core::EventHandler handler) {
    if (!handler) return 0;
    return bus_.on(kEventDataReceived, std::move(handler));
}

void BufferService::pushData(const std::string& connectionId, const domain::Bytes& data) {
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    core::EventPayload payload{
        {kFieldConnectionId, connectionId},
        {kFieldBytes, data},
        {kFieldTimestamp, nowMs},
    };
    bus_.emit(kEventDataReceived, std::move(payload));
}

void BufferService::off(std::uint64_t id) {
    bus_.off(id);
}

std::size_t BufferService::dataReceivedListeners() const {
    return bus_.listener_count(kEventDataReceived);
}

}  // namespace portpilot::service