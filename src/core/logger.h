#pragma once

#include <functional>
#include <string>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// LogLevel：分级日志（对齐一般惯例：TRACE/DEBUG/INFO/WARN/ERROR/FATAL）
// ---------------------------------------------------------------------------
enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Fatal = 5 };

inline const char* to_string(LogLevel lv) noexcept {
    switch (lv) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "?????";
}

// ---------------------------------------------------------------------------
// 日志回调类型（接收：级别、模块、消息、时间戳毫秒）
// ---------------------------------------------------------------------------
using LogCallback = std::function<void(LogLevel, const std::string&, const std::string&, std::int64_t)>;

// ---------------------------------------------------------------------------
// Logger（线程安全）
// - 支持默认 stderr 输出 + 可挂载回调（供 UI/文件/测试收集）
// - 支持按级别过滤
// ---------------------------------------------------------------------------
class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(LogLevel lv) noexcept {
        std::lock_guard<std::mutex> lk(mu_);
        level_ = lv;
    }

    LogLevel level() const noexcept {
        std::lock_guard<std::mutex> lk(mu_);
        return level_;
    }

    void add_callback(LogCallback cb) {
        if (!cb) return;
        std::lock_guard<std::mutex> lk(mu_);
        callbacks_.push_back(std::move(cb));
    }

    void clear_callbacks() {
        std::lock_guard<std::mutex> lk(mu_);
        callbacks_.clear();
    }

    bool enabled(LogLevel lv) const noexcept {
        std::lock_guard<std::mutex> lk(mu_);
        return lv >= level_;
    }

    void log(LogLevel lv, const std::string& module, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu_);
        if (lv < level_) return;
        const auto ms = timestamp_ms();
        // 默认输出
        if (default_stderr_) {
            std::cerr << format_line(lv, module, msg, ms) << std::endl;
        }
        for (const auto& cb : callbacks_) {
            try { cb(lv, module, msg, ms); } catch (...) { /* swallow */ }
        }
    }

    void set_default_stderr(bool on) noexcept {
        std::lock_guard<std::mutex> lk(mu_);
        default_stderr_ = on;
    }

    // 便捷流式：PP_LOG(INFO, "core") << "x=" << 42;
    class StreamHelper {
    public:
        StreamHelper(LogLevel lv, std::string module, Logger& owner)
            : lv_(lv), module_(std::move(module)), owner_(owner), enabled_(owner.enabled(lv)) {}
        ~StreamHelper() {
            if (enabled_) owner_.log(lv_, module_, ss_.str());
        }
        template <typename T>
        StreamHelper& operator<<(T&& v) {
            if (enabled_) ss_ << std::forward<T>(v);
            return *this;
        }
    private:
        LogLevel lv_;
        std::string module_;
        Logger& owner_;
        bool enabled_;
        std::ostringstream ss_;
    };

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static std::int64_t timestamp_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    static std::string format_line(LogLevel lv, const std::string& module,
                                   const std::string& msg, std::int64_t ms) {
        using namespace std::chrono;
        auto tp = system_clock::time_point{milliseconds{ms}};
        std::time_t t = system_clock::to_time_t(tp);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        auto ms_part = static_cast<int>(ms % 1000);
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setw(3) << std::setfill('0') << ms_part
           << " [" << to_string(lv) << "] "
           << "[" << (module.empty() ? std::string("-") : module) << "] "
           << msg;
        return ss.str();
    }

    mutable std::mutex mu_;
    LogLevel level_{LogLevel::Info};
    std::vector<LogCallback> callbacks_;
    bool default_stderr_{false};  // 默认关闭 stderr，避免污染测试输出
};

// 便捷宏（构造 StreamHelper 临时对象，其析构时落日志）
#define PP_LOG_LV(lv, mod)  ::portpilot::core::Logger::StreamHelper(lv, mod, ::portpilot::core::Logger::instance())
#define PP_TRACE(mod)  PP_LOG_LV(::portpilot::core::LogLevel::Trace, mod)
#define PP_DEBUG(mod)  PP_LOG_LV(::portpilot::core::LogLevel::Debug, mod)
#define PP_INFO(mod)   PP_LOG_LV(::portpilot::core::LogLevel::Info, mod)
#define PP_WARN(mod)   PP_LOG_LV(::portpilot::core::LogLevel::Warn, mod)
#define PP_ERROR(mod)  PP_LOG_LV(::portpilot::core::LogLevel::Error, mod)
#define PP_FATAL(mod)  PP_LOG_LV(::portpilot::core::LogLevel::Fatal, mod)

}  // namespace portpilot::core
