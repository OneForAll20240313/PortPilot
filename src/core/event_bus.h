#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <any>
#include <optional>
#include <cstdint>
#include "common_types.h"

namespace portpilot::core {

// ---------------------------------------------------------------------------
// EventBus / EventLoop
// 对齐 events.md 事件契约：发布/订阅模型，统一跨模块事件派发（D-47）
// - 事件名：`域名.动作`，如 `session.stateChanged`
// - 事件负载：类型化字段（使用 std::unordered_map<string, any> 作通用承载）
// - 订阅：on(event, handler) / once(event, handler)
// - 取消订阅：返回 SubscriptionId，off(id)
// ---------------------------------------------------------------------------
using EventPayload = std::unordered_map<std::string, std::any>;
using EventHandler = std::function<void(const std::string& event, const EventPayload& payload)>;
using SubscriptionId = std::uint64_t;

class EventBus {
public:
    static EventBus& instance() {
        static EventBus inst;
        return inst;
    }

    // 订阅：返回可用于取消的 id
    SubscriptionId on(const std::string& event, EventHandler handler) {
        if (!handler) return 0;
        std::lock_guard<std::mutex> lk(mu_);
        const auto id = ++next_id_;
        subs_[event].push_back(Entry{id, false, std::move(handler)});
        return id;
    }

    // 单次订阅：触发一次后自动取消
    SubscriptionId once(const std::string& event, EventHandler handler) {
        if (!handler) return 0;
        std::lock_guard<std::mutex> lk(mu_);
        const auto id = ++next_id_;
        subs_[event].push_back(Entry{id, true, std::move(handler)});
        return id;
    }

    // 取消订阅
    bool off(SubscriptionId id) {
        if (id == 0) return false;
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [ev, entries] : subs_) {
            auto it = std::find_if(entries.begin(), entries.end(),
                                   [id](const Entry& e) { return e.id == id; });
            if (it != entries.end()) {
                entries.erase(it);
                return true;
            }
        }
        return false;
    }

    // 派发事件：顺序同步调用所有 handler；异常在 handler 内部处理，不中断其余
    void emit(const std::string& event, const EventPayload& payload = {}) {
        std::vector<Entry> to_call;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = subs_.find(event);
            if (it != subs_.end()) {
                to_call = it->second;  // copy（期间释放锁，避免重入死锁）
                // 移除 once 标记的订阅
                auto& entries = it->second;
                entries.erase(std::remove_if(entries.begin(), entries.end(),
                                             [](const Entry& e) { return e.once; }),
                              entries.end());
            }
            // wildcard 订阅：`*` 接收所有事件
            auto wi = subs_.find("*");
            if (wi != subs_.end()) {
                for (const auto& e : wi->second) to_call.push_back(e);
            }
        }
        for (const auto& entry : to_call) {
            try { entry.handler(event, payload); }
            catch (...) { /* swallow: cross-module event never throws upward */ }
        }
    }

    // 清空所有订阅（测试辅助 / 重置）
    void clear() {
        std::lock_guard<std::mutex> lk(mu_);
        subs_.clear();
        next_id_ = 0;
    }

    // 查询某事件订阅数量（测试辅助）
    std::size_t listener_count(const std::string& event) const {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = subs_.find(event);
        return it == subs_.end() ? 0 : it->second.size();
    }

private:
    EventBus() = default;
    struct Entry {
        SubscriptionId id{0};
        bool once{false};
        EventHandler handler;
    };
    mutable std::mutex mu_;
    std::unordered_map<std::string, std::vector<Entry>> subs_;
    SubscriptionId next_id_{0};
};

}  // namespace portpilot::core
