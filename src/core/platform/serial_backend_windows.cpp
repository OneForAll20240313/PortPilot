#include "serial_backend_windows.h"
#include "domain/types.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace portpilot::core::platform {

using domain::Result;
using domain::ConnectTarget;
using domain::Bytes;
using domain::ErrorCode;

namespace {

const ErrorCode ERR_DEVICE_NO_SUCH     = "DEVICE_NO_SUCH";
const ErrorCode ERR_DEVICE_PERMISSION  = "DEVICE_PERMISSION";
const ErrorCode ERR_DEVICE_BUSY        = "DEVICE_BUSY";
const ErrorCode ERR_DEVICE_NOT_OPEN    = "DEVICE_NOT_OPEN";
const ErrorCode ERR_DEVICE_IO_FAILED   = "DEVICE_IO_FAILED";
const ErrorCode ERR_DEVICE_CONFIG      = "DEVICE_CONFIG_FAILED";
const ErrorCode ERR_SESS_PARAM_INVALID = "SESS_PARAM_INVALID";

// DCB 波特率映射：Windows 用 DWORD 直接表示数值（如 9600, 115200...）
// 同时也支持预定义常量 CBR_xxx；为兼容老系统，优先使用常量映射，
// 未知值直接按数值传入（Windows 10+ 接受任意整数波特率）
DWORD BaudToDword(uint32_t baud) {
    switch (baud) {
#ifdef _WIN32
        case 110:    return CBR_110;
        case 300:    return CBR_300;
        case 600:    return CBR_600;
        case 1200:   return CBR_1200;
        case 2400:   return CBR_2400;
        case 4800:   return CBR_4800;
        case 9600:   return CBR_9600;
        case 14400:  return CBR_14400;
        case 19200:  return CBR_19200;
        case 38400:  return CBR_38400;
        case 56000:  return CBR_56000;
        case 57600:  return CBR_57600;
        case 115200: return CBR_115200;
        case 128000: return CBR_128000;
        case 256000: return CBR_256000;
#endif
        default:     return static_cast<DWORD>(baud);
    }
}

#ifdef _WIN32
DWORD LastError() { return ::GetLastError(); }
#else
DWORD LastError() { return 0; }
#endif

} // namespace

SerialBackendWindows::~SerialBackendWindows() {
    std::lock_guard<std::mutex> lk(mu_);
#ifdef _WIN32
    if (h_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(h_);
        h_ = INVALID_HANDLE_VALUE;
    }
#endif
}

Result SerialBackendWindows::OpenErrorFromWin32(DWORD err, const std::string& path) {
#ifdef _WIN32
    switch (err) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return Result::Err(ERR_DEVICE_NO_SUCH,
                               "设备不存在: " + path + " (Win32 error " + std::to_string(err) + ")");
        case ERROR_ACCESS_DENIED:
            return Result::Err(ERR_DEVICE_PERMISSION,
                               "无权限访问设备: " + path +
                               " (请确认用户是否在 Administrators 组或拥有串口访问权限)");
        case ERROR_SHARING_VIOLATION:
            return Result::Err(ERR_DEVICE_BUSY,
                               "设备已被占用: " + path);
        case ERROR_INVALID_PARAMETER:
            return Result::Err(ERR_SESS_PARAM_INVALID,
                               "参数非法: " + path);
        default: {
            LPSTR buf = nullptr;
            size_t len = ::FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, nullptr);
            std::string msg;
            if (buf && len) {
                msg.assign(buf, buf + len);
                while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n')) msg.pop_back();
                ::LocalFree(buf);
            } else {
                msg = "Win32 error " + std::to_string(err);
            }
            return Result::Err("DEVICE_OPEN_FAILED", "打开设备失败: " + path + " - " + msg);
        }
    }
#else
    (void)err;
    return Result::Err("DEVICE_OPEN_FAILED", "打开设备失败: " + path + " (非 Windows 平台占位)");
#endif
}

Result SerialBackendWindows::ApplyDCB(const ConnectTarget& config) {
#ifdef _WIN32
    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!::GetCommState(h_, &dcb)) {
        return Result::Err(ERR_DEVICE_CONFIG,
                           "读取串口配置失败 (Win32 " + std::to_string(LastError()) + ")");
    }

    dcb.BaudRate = BaudToDword(config.baud);
    // 如果用户传的波特率不是 CBR_xxx 常量，直接按数值设置即可（现代 Windows 接受）

    // 字节大小
    switch (config.dataBits) {
        case domain::DataBits::Five:  dcb.ByteSize = 5; break;
        case domain::DataBits::Six:   dcb.ByteSize = 6; break;
        case domain::DataBits::Seven: dcb.ByteSize = 7; break;
        case domain::DataBits::Eight: dcb.ByteSize = 8; break;
        default:                      dcb.ByteSize = 8; break;
    }

    // 停止位
    switch (config.stopBits) {
        case domain::StopBits::One:
            dcb.StopBits = ONESTOPBIT;
            break;
        case domain::StopBits::OnePointFive:
            dcb.StopBits = ONE5STOPBITS;
            break;
        case domain::StopBits::Two:
            dcb.StopBits = TWOSTOPBITS;
            break;
    }

    // 校验位
    switch (config.parity) {
        case domain::Parity::None:
            dcb.Parity = NOPARITY;
            break;
        case domain::Parity::Even:
            dcb.Parity = EVENPARITY;
            break;
        case domain::Parity::Odd:
            dcb.Parity = ODDPARITY;
            break;
        case domain::Parity::Mark:
            dcb.Parity = MARKPARITY;
            break;
        case domain::Parity::Space:
            dcb.Parity = SPACEPARITY;
            break;
    }
    dcb.fParity = (config.parity != domain::Parity::None) ? TRUE : FALSE;

    // 二进制模式 + 忽略 DSR（不要求调制解调器控制信号）+ 接收启用
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;      // 禁用硬件流控
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutX = FALSE;             // 禁用软件流控 XON/XOFF
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;
    dcb.XonLim = 100;
    dcb.XoffLim = 100;
    dcb.XonChar = 0x11;
    dcb.XoffChar = 0x13;
    dcb.ErrorChar = 0;
    dcb.EofChar = 0;
    dcb.EvtChar = 0;

    if (!::SetCommState(h_, &dcb)) {
        return Result::Err(ERR_DEVICE_CONFIG,
                           "设置串口参数失败 (Win32 " + std::to_string(LastError()) + ")");
    }

    // 超时：对齐 POSIX 的 VMIN=1/VTIME=0 语义 —— ReadIntervalTimeout=MAXDWORD，
    // 其余为 0：ReadFile 立即返回已有数据；若无数据则阻塞等待（重叠 I/O 下不适用）。
    // 我们使用非阻塞语义（ReadFile 立即返回），配合上层轮询/选择。
    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout = MAXDWORD;
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = 0;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 5000;  // 写入总超时 5s，防止卡死
    if (!::SetCommTimeouts(h_, &to)) {
        return Result::Err(ERR_DEVICE_CONFIG,
                           "设置串口超时失败 (Win32 " + std::to_string(LastError()) + ")");
    }

    ::PurgeComm(h_, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
    return Result::Ok();
#else
    (void)config;
    return Result::Err(ERR_DEVICE_CONFIG, "ApplyDCB 仅在 Windows 可用");
#endif
}

Result SerialBackendWindows::open(const ConnectTarget& config) {
    std::lock_guard<std::mutex> lk(mu_);
#ifdef _WIN32
    if (h_ != INVALID_HANDLE_VALUE) {
        return Result::Err(ERR_DEVICE_BUSY, "设备已打开，请先关闭");
    }
    if (config.port.empty()) {
        return Result::Err(ERR_SESS_PARAM_INVALID, "设备路径不能为空");
    }

    // Windows 规范：COM 口名需加前缀 \\.\ 以防设备名>COM9 被解析为相对路径
    std::string winName = config.port;
    if (winName.find("\\\\.\\") != 0) {
        winName = "\\\\.\\" + winName;
    }

    HANDLE h = ::CreateFileA(
        winName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,                          // 独占方式：禁止共享
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,  // 重叠 I/O，不阻塞线程
        nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return OpenErrorFromWin32(LastError(), config.port);
    }
    h_ = h;

    // 暂时关闭重叠 I/O 进行配置（简化：我们的读写都使用同步循环）
    // 取消重叠模式：后续 ReadFile/WriteFile 为同步调用，配合超时设置实现非阻塞读
    if (!::CancelIo(h_)) {
        DWORD e = LastError();
        if (e != ERROR_NOT_FOUND) {
            // 忽略非关键错误
        }
    }
    // 重新创建同步句柄（简化：不用 OVERLAPPED，由上层线程驱动）
    ::CloseHandle(h_);
    h_ = ::CreateFileA(winName.c_str(),
                       GENERIC_READ | GENERIC_WRITE,
                       0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h_ == INVALID_HANDLE_VALUE) {
        DWORD e = LastError();
        return OpenErrorFromWin32(e, config.port);
    }

    Result r = ApplyDCB(config);
    if (!r.ok) {
        ::CloseHandle(h_);
        h_ = INVALID_HANDLE_VALUE;
        return r;
    }

    opened_path_ = config.port;
    return Result::Ok();
#else
    (void)config;
    return Result::Err("DEVICE_OPEN_FAILED", "SerialBackendWindows 仅在 Windows 可用");
#endif
}

Result SerialBackendWindows::close() {
    std::lock_guard<std::mutex> lk(mu_);
#ifdef _WIN32
    if (h_ == INVALID_HANDLE_VALUE) {
        return Result::Err(ERR_DEVICE_NOT_OPEN, "设备未打开");
    }
    BOOL ok = ::CloseHandle(h_);
    h_ = INVALID_HANDLE_VALUE;
    opened_path_.clear();
    if (!ok) {
        return Result::Err(ERR_DEVICE_IO_FAILED,
                           "关闭设备失败 (Win32 " + std::to_string(LastError()) + ")");
    }
    return Result::Ok();
#else
    return Result::Err(ERR_DEVICE_NOT_OPEN, "设备未打开 (非 Windows 平台)");
#endif
}

Result SerialBackendWindows::write(const Bytes& data) {
    std::lock_guard<std::mutex> lk(mu_);
#ifdef _WIN32
    if (h_ == INVALID_HANDLE_VALUE) {
        return Result::Err(ERR_DEVICE_NOT_OPEN, "设备未打开");
    }
    if (data.empty()) {
        return Result::Ok();
    }
    DWORD total = 0;
    const DWORD N = static_cast<DWORD>(data.size());
    while (total < N) {
        DWORD written = 0;
        BOOL ok = ::WriteFile(h_, data.data() + total, N - total, &written, nullptr);
        if (!ok) {
            DWORD e = LastError();
            // ERROR_IO_PENDING 在同步句柄上不会出现；保险起见
            if (e == ERROR_IO_INCOMPLETE || e == ERROR_IO_PENDING) {
                continue;
            }
            return Result::Err(ERR_DEVICE_IO_FAILED,
                               "写入设备失败 (Win32 " + std::to_string(e) + ")");
        }
        if (written == 0) break;
        total += written;
    }
    if (total != N) {
        return Result::Err(ERR_DEVICE_IO_FAILED,
                           "写入设备不完整 (sent=" + std::to_string(total) + "/" + std::to_string(N) + ")");
    }
    return Result::Ok();
#else
    (void)data;
    return Result::Err(ERR_DEVICE_NOT_OPEN, "设备未打开 (非 Windows 平台)");
#endif
}

Bytes SerialBackendWindows::read() {
    std::lock_guard<std::mutex> lk(mu_);
    Bytes buf;
#ifdef _WIN32
    if (h_ == INVALID_HANDLE_VALUE) return buf;

    // 先查询可用字节数
    DWORD errors = 0;
    COMSTAT stat{};
    if (!::ClearCommError(h_, &errors, &stat)) {
        return buf;
    }
    DWORD avail = stat.cbInQue;
    if (avail == 0) return buf;

    DWORD chunk = (avail > 2048) ? 2048 : avail;
    buf.resize(chunk);
    DWORD got = 0;
    BOOL ok = ::ReadFile(h_, buf.data(), chunk, &got, nullptr);
    if (!ok || got == 0) {
        buf.clear();
        return buf;
    }
    if (got < chunk) buf.resize(got);
#endif
    return buf;
}

bool SerialBackendWindows::isOpen() const {
    std::lock_guard<std::mutex> lk(mu_);
#ifdef _WIN32
    return h_ != INVALID_HANDLE_VALUE;
#else
    return false;
#endif
}

domain::ProbeResult SerialBackendWindows::probe(const ConnectTarget& params,
                                                 domain::ProbeCriteriaCallback onCriteria) {
    domain::ProbeResult r;
    r.success = false;
    r.confirmedTarget = params;

    if (params.port.empty()) {
        r.message = "设备路径为空";
        return r;
    }

#ifdef _WIN32
    std::string winName = params.port;
    if (winName.find("\\\\.\\") != 0) {
        winName = "\\\\.\\" + winName;
    }
    HANDLE h = ::CreateFileA(winName.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = LastError();
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
            r.message = "设备不存在 (Win32 " + std::to_string(e) + ")";
        } else if (e == ERROR_ACCESS_DENIED) {
            r.message = "无权限访问设备: 请确认拥有串口访问权限";
        } else if (e == ERROR_SHARING_VIOLATION) {
            r.message = "设备已被占用";
        } else {
            r.message = "打开设备失败 (Win32 " + std::to_string(e) + ")";
        }
        return r;
    }
    ::CloseHandle(h);

    bool criteriaOk = !onCriteria || onCriteria(params);
    if (!criteriaOk) {
        r.message = "探测判定条件未满足";
        return r;
    }
    r.success = true;
    r.message = "设备可达，探测成功";
#else
    r.message = "SerialBackendWindows probe 仅在 Windows 可用";
#endif
    return r;
}

int64_t SerialBackendWindows::nativeHandle() const {
    std::lock_guard<std::mutex> lk(mu_);
#ifdef _WIN32
    return reinterpret_cast<int64_t>(h_);
#else
    return h_;
#endif
}

bool SerialBackendWindows::isBusy(const std::string& port) const {
    if (port.empty()) return false;
#ifdef _WIN32
    std::string winName = port;
    if (winName.find("\\\\.\\") != 0) {
        winName = "\\\\.\\" + winName;
    }
    HANDLE h = ::CreateFileA(winName.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return LastError() == ERROR_SHARING_VIOLATION;
    }
    ::CloseHandle(h);
#endif
    return false;
}

} // namespace portpilot::core::platform
