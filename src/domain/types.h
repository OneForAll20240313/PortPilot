#ifndef PORTPILOT_DOMAIN_TYPES_H
#define PORTPILOT_DOMAIN_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace portpilot::domain {

using Bytes = std::vector<uint8_t>;
using ConnectionId = std::string;
using SessionId = std::string;
using SessionMode = std::string;
using ErrorCode = std::string;

enum class PortType {
    Serial,
    Network
};

enum class Parity {
    None,
    Even,
    Odd,
    Mark,
    Space
};

enum class StopBits {
    One,
    OnePointFive,
    Two
};

enum class DataBits : uint8_t {
    Five = 5,
    Six = 6,
    Seven = 7,
    Eight = 8
};

struct Result {
    bool ok{false};
    ErrorCode code{};
    std::string message{};

    static Result Ok() { return {true, "", ""}; }
    static Result Err(ErrorCode code, const std::string& msg = "") {
        return {false, std::move(code), msg};
    }
};

struct ConnectTarget {
    PortType type{PortType::Serial};

    // Serial fields
    std::string port;
    uint32_t baud{9600};
    DataBits dataBits{DataBits::Eight};
    StopBits stopBits{StopBits::One};
    Parity parity{Parity::None};

    // Network fields (reserved)
    std::string addr;
    uint16_t tcpPort{0};
};

using DataCallback = std::function<void(const Bytes&)>;
using ProbeCriteriaCallback = std::function<bool(const ConnectTarget&)>;

struct ProbeResult {
    bool success{false};
    std::string message{};
    ConnectTarget confirmedTarget{};
};

std::string to_string(Parity p);
std::string to_string(StopBits s);
std::string to_string(DataBits d);
std::string to_string(PortType t);

Parity parity_from_string(const std::string& s);
StopBits stop_bits_from_string(const std::string& s);
DataBits data_bits_from_int(int v);
PortType port_type_from_string(const std::string& s);

} // namespace portpilot::domain

#endif // PORTPILOT_DOMAIN_TYPES_H
