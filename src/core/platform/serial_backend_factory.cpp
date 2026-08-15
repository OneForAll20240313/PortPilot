#include "serial_backend.h"
#include "serial_backend_posix.h"
#include "serial_backend_windows.h"

namespace portpilot::core::platform {

ISerialBackendPtr SerialBackendFactory::Create() {
#if defined(_WIN32)
    return std::make_unique<SerialBackendWindows>();
#else
    return std::make_unique<SerialBackendPosix>();
#endif
}

} // namespace portpilot::core::platform
