#pragma once

#include "common_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// FileRepository（配置/协议持久化抽象）
// 对齐 13.3 Core 层组件：SQLite 配置/协议持久化
// - 本层只定义接口与内存实现（作为单测/装配根的 InMemory 版本）
// - 真实的 SQLite/JSON 文件版本由后续上层实现提供，保持接口不变
// ---------------------------------------------------------------------------
class FileRepository {
public:
    virtual ~FileRepository() = default;

    // KV 读写（配置三级作用域 g/s/t 由 key 前缀区分）
    virtual Result<std::string> get(const std::string& key) = 0;
    virtual Result<void> set(const std::string& key, const std::string& value) = 0;
    virtual Result<void> remove(const std::string& key) = 0;
    virtual Result<std::vector<std::string>> keys(const std::string& prefix = "") = 0;

    // 文档读写（协议定义 JSON / 会话 JSON）
    virtual Result<std::string> load_document(const std::string& id, const std::string& collection) = 0;
    virtual Result<void> save_document(const std::string& id, const std::string& collection,
                                       const std::string& json) = 0;
    virtual Result<std::vector<std::string>> list_documents(const std::string& collection) = 0;
    virtual Result<void> delete_document(const std::string& id, const std::string& collection) = 0;

    // 全量 flush 到磁盘（可选）
    virtual Result<void> flush() { return make_ok(); }
};

// ---------------------------------------------------------------------------
// InMemoryFileRepository：内存实现（测试 / 开发期默认）
// ---------------------------------------------------------------------------
class InMemoryFileRepository : public FileRepository {
public:
    Result<std::string> get(const std::string& key) override {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = kv_.find(key);
        if (it == kv_.end()) return make_err<std::string>(ErrorCode::CFG_KEY_NOT_FOUND, "key 不存在: " + key);
        return make_ok(it->second);
    }

    Result<void> set(const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lk(mu_);
        kv_[key] = value;
        return make_ok();
    }

    Result<void> remove(const std::string& key) override {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = kv_.find(key);
        if (it == kv_.end()) return make_err_void(ErrorCode::CFG_KEY_NOT_FOUND, "key 不存在: " + key);
        kv_.erase(it);
        return make_ok();
    }

    Result<std::vector<std::string>> keys(const std::string& prefix = "") override {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<std::string> out;
        for (const auto& [k, v] : kv_) {
            if (prefix.empty() || k.compare(0, prefix.size(), prefix) == 0) {
                out.push_back(k);
            }
        }
        return make_ok(std::move(out));
    }

    Result<std::string> load_document(const std::string& id, const std::string& collection) override {
        std::lock_guard<std::mutex> lk(mu_);
        auto ci = docs_.find(collection);
        if (ci == docs_.end()) return make_err<std::string>(ErrorCode::CFG_NOT_FOUND, "集合不存在: " + collection);
        auto di = ci->second.find(id);
        if (di == ci->second.end()) return make_err<std::string>(ErrorCode::CFG_NOT_FOUND, "文档不存在: " + id);
        return make_ok(di->second);
    }

    Result<void> save_document(const std::string& id, const std::string& collection,
                               const std::string& json) override {
        std::lock_guard<std::mutex> lk(mu_);
        docs_[collection][id] = json;
        return make_ok();
    }

    Result<std::vector<std::string>> list_documents(const std::string& collection) override {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<std::string> out;
        auto ci = docs_.find(collection);
        if (ci == docs_.end()) return make_ok(std::move(out));
        for (const auto& [id, _] : ci->second) out.push_back(id);
        return make_ok(std::move(out));
    }

    Result<void> delete_document(const std::string& id, const std::string& collection) override {
        std::lock_guard<std::mutex> lk(mu_);
        auto ci = docs_.find(collection);
        if (ci == docs_.end()) return make_err_void(ErrorCode::CFG_NOT_FOUND, "集合不存在");
        auto di = ci->second.find(id);
        if (di == ci->second.end()) return make_err_void(ErrorCode::CFG_NOT_FOUND, "文档不存在");
        ci->second.erase(di);
        return make_ok();
    }

private:
    mutable std::mutex mu_;
    std::unordered_map<std::string, std::string> kv_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> docs_;
};

}  // namespace portpilot::core
