#include "port_enumerator.h"
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace portpilot::core {

namespace {

const domain::ErrorCode ERR_ENUM_FAILED = "SESS_PORT_ENUM_FAILED";

// 标准串口设备名前缀（Linux /dev 常见命名）
const char* const kStandardPrefixes[] = {
    "ttyS", "ttyUSB", "ttyACM", "ttyAMA", "ttyTHS", "ttymxc", "ttyS0",
};

bool IsStandardSerialName(const std::string& name) {
    for (const char* prefix : kStandardPrefixes) {
        if (name.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

// 伪终端节点为纯数字命名（如 0, 1, 2）
bool IsPtsNodeName(const std::string& name) {
    return !name.empty() &&
           name.find_first_not_of("0123456789") == std::string::npos;
}

// 探测单个路径可否配置为串口：open + 应用 termios 均成功才算候选。
// 成功返回 0；失败返回捕获的 errno，供错误码映射（需求 #33 手动路径校验）。
int ProbeSerialPath(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return errno;
    }
    termios t{};
    if (::tcgetattr(fd, &t) != 0) {
        int e = errno;
        ::close(fd);
        return e;
    }
    ::cfmakeraw(&t);
    if (::tcsetattr(fd, TCSANOW, &t) != 0) {
        int e = errno;
        ::close(fd);
        return e;
    }
    ::close(fd);
    return 0;
}

// 甄别设备能否配置串口属性：open + 应用 termios 均成功。
// 仅返回 true 的设备列入候选，保证候选可成功打开（对齐 DoD）
bool IsSerialConfigurable(const std::string& path) {
    return ProbeSerialPath(path) == 0;
}

void AddCandidate(const std::string& path, domain::PortEnumResult& out) {
    if (!IsSerialConfigurable(path)) {
        return;
    }
    domain::PortInfo pi;
    pi.id = path;
    pi.name = path.substr(path.find_last_of('/') + 1);
    pi.friendlyName = path;
    pi.systemLocation = path;
    pi.isBusy = false;
    pi.isLoopback = false;
    out.ports.push_back(std::move(pi));
}

} // namespace

domain::PortEnumResult PortEnumerator::listAvailablePorts() {
    domain::PortEnumResult result;
    result.ok = true;

    // 1) 扫描 /dev 下标准串口设备
    DIR* devDir = ::opendir("/dev");
    if (devDir == nullptr) {
        result.ok = false;
        result.code = ERR_ENUM_FAILED;
        result.message = std::string("无法枚举 /dev 目录: ") + std::strerror(errno);
        return result;
    }
    while (dirent* ent = ::readdir(devDir)) {
        const std::string name = ent->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        if (IsStandardSerialName(name)) {
            AddCandidate("/dev/" + name, result);
        }
    }
    ::closedir(devDir);

    // 2) 扫描 /dev/pts/* 非标准伪终端（需求 #34）
    DIR* ptsDir = ::opendir("/dev/pts");
    if (ptsDir != nullptr) {
        while (dirent* ent = ::readdir(ptsDir)) {
            const std::string name = ent->d_name;
            if (name == "." || name == "..") {
                continue;
            }
            if (IsPtsNodeName(name)) {
                AddCandidate("/dev/pts/" + name, result);
            }
        }
        ::closedir(ptsDir);
    }

    return result;
}

domain::PortEnumResult PortEnumerator::probePort(const std::string& path) {
    domain::PortEnumResult result;
    result.ok = false;

    // 需求 #33 DoD：设备不存在返回 NoSuchDeviceError 提示
    if (path.empty()) {
        result.code = "SESS_PARAM_INVALID";
        result.message = "设备路径不能为空";
        return result;
    }

    const int err = ProbeSerialPath(path);
    if (err != 0) {
        switch (err) {
            case ENOENT:
            case ENOTDIR:
            case ENXIO:
                result.code = domain::kNoSuchDeviceError;
                result.message = "设备不存在: " + path + " (" + std::strerror(err) + ")";
                break;
            case EACCES:
            case EPERM:
                result.code = domain::kDevicePermissionError;
                result.message = "无权限访问设备: " + path +
                                 " (请确认用户是否在 dialout 组，详情: " + std::strerror(err) + ")";
                break;
            case EBUSY:
                result.code = domain::kDeviceBusyError;
                result.message = "设备已被占用: " + path + " (" + std::strerror(err) + ")";
                break;
            default:
                result.code = domain::kDeviceConfigError;
                result.message = "设备不可配置为串口: " + path + " (" + std::strerror(err) + ")";
                break;
        }
        return result;
    }

    // 探测成功：路径可打开且可配置串口属性，回填 PortInfo 供连接前确认
    domain::PortInfo pi;
    pi.id = path;
    pi.name = path.substr(path.find_last_of('/') + 1);
    pi.friendlyName = path;
    pi.systemLocation = path;
    pi.isBusy = false;
    pi.isLoopback = false;
    result.ports.push_back(std::move(pi));
    result.ok = true;
    result.message = "设备可达，探测成功";
    return result;
}

} // namespace portpilot::core