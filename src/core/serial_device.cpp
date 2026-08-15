#include "serial_device.h"
#include "domain/types.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mutex>

namespace portpilot::core {

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

speed_t BaudToSpeedT(uint32_t baud) {
    switch (baud) {
        case 0:       return B0;
        case 50:      return B50;
        case 75:      return B75;
        case 110:     return B110;
        case 134:     return B134;
        case 150:     return B150;
        case 200:     return B200;
        case 300:     return B300;
        case 600:     return B600;
        case 1200:    return B1200;
        case 1800:    return B1800;
        case 2400:    return B2400;
        case 4800:    return B4800;
        case 9600:    return B9600;
        case 19200:   return B19200;
        case 38400:   return B38400;
        case 57600:   return B57600;
        case 115200:  return B115200;
        case 230400:  return B230400;
        case 460800:  return B460800;
#ifdef B500000
        case 500000:  return B500000;
#endif
#ifdef B576000
        case 576000:  return B576000;
#endif
#ifdef B921600
        case 921600:  return B921600;
#endif
        default:      return static_cast<speed_t>(-1);
    }
}

int DataBitsToFlag(domain::DataBits db) {
    switch (db) {
        case domain::DataBits::Five:  return CS5;
        case domain::DataBits::Six:   return CS6;
        case domain::DataBits::Seven: return CS7;
        case domain::DataBits::Eight: return CS8;
    }
    return CS8;
}

} // namespace

SerialDevice::~SerialDevice() {
    std::lock_guard<std::mutex> lk(mu_);
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Result SerialDevice::OpenErrorFromErrno(int err, const std::string& path) {
    switch (err) {
        case ENOENT:
        case ENOTDIR:
        case ENXIO:
            return Result::Err(ERR_DEVICE_NO_SUCH,
                               "设备不存在: " + path + " (" + std::strerror(err) + ")");
        case EACCES:
        case EPERM: {
            std::string msg = "无权限打开设备 " + path +
                              ": 当前用户可能不在 dialout 组 (Linux)。"
                              " 请执行: sudo usermod -aG dialout $USER，然后重新登录。"
                              " 详情: " + std::strerror(err);
            return Result::Err(ERR_DEVICE_PERMISSION, msg);
        }
        case EBUSY:
            return Result::Err(ERR_DEVICE_BUSY,
                               "设备已被占用: " + path + " (" + std::strerror(err) + ")");
        case EINVAL:
            return Result::Err(ERR_SESS_PARAM_INVALID,
                               "参数非法: " + path + " (" + std::strerror(err) + ")");
        default:
            return Result::Err("DEVICE_OPEN_FAILED",
                               "打开设备失败: " + path + " (" + std::strerror(err) + ")");
    }
}

Result SerialDevice::ApplyTermios(const ConnectTarget& config) {
    termios t{};
    if (::tcgetattr(fd_, &t) != 0) {
        return Result::Err(ERR_DEVICE_CONFIG,
                           "读取串口配置失败: " + std::string(std::strerror(errno)));
    }

    // 原始模式：不做行处理、不回显、不做信号转换
    ::cfmakeraw(&t);

    speed_t spd = BaudToSpeedT(config.baud);
    if (spd == static_cast<speed_t>(-1)) {
        return Result::Err(ERR_SESS_PARAM_INVALID,
                           "不支持的波特率: " + std::to_string(config.baud));
    }
    if (::cfsetspeed(&t, spd) != 0) {
        return Result::Err(ERR_DEVICE_CONFIG,
                           "设置波特率失败: " + std::string(std::strerror(errno)));
    }

    t.c_cflag &= ~CSIZE;
    t.c_cflag |= static_cast<tcflag_t>(DataBitsToFlag(config.dataBits));

    switch (config.stopBits) {
        case domain::StopBits::One:
            t.c_cflag &= ~CSTOPB;
            break;
        case domain::StopBits::OnePointFive:
        case domain::StopBits::Two:
            t.c_cflag |= CSTOPB;
            break;
    }

    switch (config.parity) {
        case domain::Parity::None:
            t.c_cflag &= ~(PARENB | PARODD);
            break;
        case domain::Parity::Even:
            t.c_cflag |= PARENB;
            t.c_cflag &= ~PARODD;
            break;
        case domain::Parity::Odd:
            t.c_cflag |= (PARENB | PARODD);
            break;
        case domain::Parity::Mark:
            t.c_cflag |= (PARENB | PARODD | CMSPAR);
            break;
        case domain::Parity::Space:
            t.c_cflag |= PARENB;
            t.c_cflag |= CMSPAR;
            t.c_cflag &= ~PARODD;
            break;
    }

    // 禁用硬件流控（默认）
    t.c_cflag &= ~CRTSCTS;
    // 启用接收端
    t.c_cflag |= (CLOCAL | CREAD);

    // 阻塞等待至少1字节，无超时（契约 VMIN=1 / VTIME=0）
    t.c_cc[VMIN]  = 1;
    t.c_cc[VTIME] = 0;

    if (::tcsetattr(fd_, TCSANOW, &t) != 0) {
        return Result::Err(ERR_DEVICE_CONFIG,
                           "设置串口参数失败: " + std::string(std::strerror(errno)));
    }
    ::tcflush(fd_, TCIOFLUSH);
    return Result::Ok();
}

Result SerialDevice::open(const ConnectTarget& config) {
    std::lock_guard<std::mutex> lk(mu_);
    if (fd_ >= 0) {
        return Result::Err(ERR_DEVICE_BUSY, "设备已打开，请先关闭");
    }
    if (config.port.empty()) {
        return Result::Err(ERR_SESS_PARAM_INVALID, "设备路径不能为空");
    }

    int flags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC;
    int newFd = ::open(config.port.c_str(), flags);
    if (newFd < 0) {
        return OpenErrorFromErrno(errno, config.port);
    }

    fd_ = newFd;
    Result r = ApplyTermios(config);
    if (!r.ok) {
        ::close(fd_);
        fd_ = -1;
        return r;
    }

    opened_path_ = config.port;
    return Result::Ok();
}

Result SerialDevice::close() {
    std::lock_guard<std::mutex> lk(mu_);
    if (fd_ < 0) {
        return Result::Err(ERR_DEVICE_NOT_OPEN, "设备未打开");
    }
    int ret = ::close(fd_);
    fd_ = -1;
    opened_path_.clear();
    if (ret != 0) {
        return Result::Err(ERR_DEVICE_IO_FAILED,
                           "关闭设备失败: " + std::string(std::strerror(errno)));
    }
    return Result::Ok();
}

Result SerialDevice::write(const Bytes& data) {
    std::lock_guard<std::mutex> lk(mu_);
    if (fd_ < 0) {
        return Result::Err(ERR_DEVICE_NOT_OPEN, "设备未打开");
    }
    if (data.empty()) {
        return Result::Ok();
    }
    ssize_t total = 0;
    const size_t N = data.size();
    while (static_cast<size_t>(total) < N) {
        ssize_t n = ::write(fd_, data.data() + total, N - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return Result::Err(ERR_DEVICE_IO_FAILED,
                               "写入设备失败: " + std::string(std::strerror(errno)));
        }
        if (n == 0) break;
        total += n;
    }
    if (static_cast<size_t>(total) != N) {
        return Result::Err(ERR_DEVICE_IO_FAILED,
                           "写入设备不完整 (sent=" + std::to_string(total) + "/" + std::to_string(N) + ")");
    }
    return Result::Ok();
}

Bytes SerialDevice::read() {
    Bytes buf;
    domain::DataCallback cb_copy;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (fd_ < 0) return buf;

        uint8_t chunk[2048];
        ssize_t n = ::read(fd_, chunk, sizeof(chunk));
        if (n > 0) {
            buf.assign(chunk, chunk + n);
            cb_copy = data_cb_;
        }
    }
    if (cb_copy && !buf.empty()) {
        try {
            cb_copy(buf);
        } catch (...) {
        }
    }
    return buf;
}

void SerialDevice::onData(domain::DataCallback callback) {
    std::lock_guard<std::mutex> lk(mu_);
    data_cb_ = std::move(callback);
}

bool SerialDevice::isOpen() const {
    std::lock_guard<std::mutex> lk(mu_);
    return fd_ >= 0;
}

domain::ProbeResult SerialDevice::probe(const ConnectTarget& params,
                                         domain::ProbeCriteriaCallback onCriteria) {
    domain::ProbeResult r;
    r.success = false;
    r.confirmedTarget = params;

    if (params.port.empty()) {
        r.message = "设备路径为空";
        return r;
    }

    struct stat st{};
    if (::stat(params.port.c_str(), &st) != 0) {
        if (errno == ENOENT || errno == ENOTDIR || errno == ENXIO) {
            r.message = "设备不存在: " + std::string(std::strerror(errno));
            return r;
        }
        if (errno == EACCES || errno == EPERM) {
            r.message = "无权限访问设备: 请确认是否在 dialout 组。"
                        " 详情: " + std::string(std::strerror(errno));
            return r;
        }
        r.message = std::strerror(errno);
        return r;
    }

    // 可以打开 + criteria 判定通过
    int fd = ::open(params.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        int e = errno;
        if (e == EACCES || e == EPERM) {
            r.message = "无权限打开设备: Linux 请执行 `sudo usermod -aG dialout $USER` 然后重新登录。"
                        " 错误: " + std::string(std::strerror(e));
        } else if (e == EBUSY) {
            r.message = "设备已被占用";
        } else {
            r.message = "打开设备失败: " + std::string(std::strerror(e));
        }
        return r;
    }
    ::close(fd);

    bool criteriaOk = !onCriteria || onCriteria(params);
    if (!criteriaOk) {
        r.message = "探测判定条件未满足";
        return r;
    }
    r.success = true;
    r.message = "设备可达，探测成功";
    return r;
}

} // namespace portpilot::core
