// Core 层汇总：引入全部组件的编译单元（头文件大多为 header-only）
// 为避免链接空库警告，保留一个带导出符号的编译单元入口
// Issue #49：DevicePort 接口定义已移至 Domain 层（domain/device_port.h）
//            Core 层仅保留实现：SerialWorker / NetworkTransport / MockDevicePort / SerialDevice
#include "common_types.h"
#include "logger.h"
#include "event_bus.h"
#include "mock_device_port.h"
#include "serial_worker.h"
#include "network_transport.h"
#include "file_repository.h"
#include "protocol_engine.h"

namespace portpilot::core {

// Core 层版本标识（供装配根查询 / CI 验证）
const char* core_version() noexcept {
    return "0.1.0-m1";
}

}  // namespace portpilot::core
