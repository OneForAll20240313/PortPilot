#include "qt_signal_bridge.h"

#include <any>
#include <string>
#include <vector>

#include "core/event_bus.h"
#include "service/buffer_service.h"

namespace portpilot::ui {

namespace {
constexpr const char* kFieldConnectionId = "connectionId";
constexpr const char* kFieldBytes = "bytes";
}  // namespace

QtSignalBridge::QtSignalBridge(service::BufferService* buffer, QObject* parent)
    : QObject(parent), buffer_(buffer) {
    if (buffer_ == nullptr) return;

    // 订阅 buffer.dataReceived：事件回调里发射 Qt 信号，Qt 信号/槽机制保证
    // 槽在所属线程触发（QueuedConnection/AutoConnection），无跨线程问题。
    subId_ = buffer_->onDataReceived([this](const std::string&, const core::EventPayload& payload) {
        QString connId;
        QByteArray bytes;

        auto cIt = payload.find(kFieldConnectionId);
        if (cIt != payload.end()) {
            try {
                connId = QString::fromStdString(std::any_cast<std::string>(cIt->second));
            } catch (const std::bad_any_cast&) {
                connId = QStringLiteral("<non-string>");
            }
        }

        auto bIt = payload.find(kFieldBytes);
        if (bIt != payload.end()) {
            try {
                const auto& raw = std::any_cast<std::vector<std::uint8_t>>(bIt->second);
                bytes = QByteArray(reinterpret_cast<const char*>(raw.data()),
                                   static_cast<int>(raw.size()));
            } catch (const std::bad_any_cast&) {
                bytes = QByteArray();
            }
        }

        Q_EMIT dataReceived(connId, bytes);
    });
}

QtSignalBridge::~QtSignalBridge() {
    if (buffer_ != nullptr) {
        buffer_->off(subId_);
    }
}

}  // namespace portpilot::ui