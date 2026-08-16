#ifndef PORTPILOT_CORE_PORT_ENUMERATOR_H
#define PORTPILOT_CORE_PORT_ENUMERATOR_H

#include "domain/device_port_enumerator.h"

namespace portpilot::core {

// ---------------------------------------------------------------------------
// PortEnumerator：实现 domain::DevicePortEnumerator（契约 A-214）
//
// 职责：
//   - 扫描 /dev 下标准串口设备（ttyS* / ttyUSB* / ttyACM* 等）
//   - 扫描 /dev/pts/* 非标准伪终端（需求 #34）
//   - 甄别可配置串口属性（open + termios 应用）的设备后列入候选列表
//
// 架构：Core 层 Qt-free，POSIX 实现依赖 dirent/termios；Windows 由平台隔离
// ---------------------------------------------------------------------------
class PortEnumerator : public domain::DevicePortEnumerator {
public:
    domain::PortEnumResult listAvailablePorts() override;

    // 校验/探测手动输入的自定义设备路径（需求 #33）
    domain::PortEnumResult probePort(const std::string& path) override;
};

} // namespace portpilot::core

#endif // PORTPILOT_CORE_PORT_ENUMERATOR_H