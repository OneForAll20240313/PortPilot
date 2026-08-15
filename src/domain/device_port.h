#ifndef PORTPILOT_DOMAIN_DEVICE_PORT_H
#define PORTPILOT_DOMAIN_DEVICE_PORT_H

#include "types.h"
#include <memory>

namespace portpilot::domain {

class DevicePort {
public:
    virtual ~DevicePort() = default;

    virtual Result open(const ConnectTarget& config) = 0;
    virtual Result close() = 0;
    virtual Result write(const Bytes& data) = 0;
    virtual Bytes read() = 0;
    virtual void onData(DataCallback callback) = 0;
    virtual bool isOpen() const = 0;
    virtual ProbeResult probe(const ConnectTarget& params, ProbeCriteriaCallback onCriteria) = 0;
};

using DevicePortPtr = std::unique_ptr<DevicePort>;

} // namespace portpilot::domain

#endif // PORTPILOT_DOMAIN_DEVICE_PORT_H
