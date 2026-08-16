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

// 甄别设备能否配置串口属性：open + 应用 termios 均成功。
// 仅返回 true 的设备列入候选，保证候选可成功打开（对齐 DoD）
bool IsSerialConfigurable(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    termios t{};
    bool ok = true;
    if (::tcgetattr(fd, &t) != 0) {
        ok = false;
    } else {
        ::cfmakeraw(&t);
        if (::tcsetattr(fd, TCSANOW, &t) != 0) {
            ok = false;
        }
    }
    ::close(fd);
    return ok;
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

} // namespace portpilot::core