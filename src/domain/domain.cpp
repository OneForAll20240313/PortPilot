#include "types.h"
#include "device_port.h"
#include <string>
#include <stdexcept>

namespace portpilot::domain {

std::string to_string(Parity p) {
    switch (p) {
        case Parity::None:  return "none";
        case Parity::Even:  return "even";
        case Parity::Odd:   return "odd";
        case Parity::Mark:  return "mark";
        case Parity::Space: return "space";
    }
    return "none";
}

std::string to_string(StopBits s) {
    switch (s) {
        case StopBits::One:          return "1";
        case StopBits::OnePointFive: return "1.5";
        case StopBits::Two:          return "2";
    }
    return "1";
}

std::string to_string(DataBits d) {
    return std::to_string(static_cast<uint8_t>(d));
}

std::string to_string(PortType t) {
    switch (t) {
        case PortType::Serial:  return "serial";
        case PortType::Network: return "network";
    }
    return "serial";
}

Parity parity_from_string(const std::string& s) {
    if (s == "none")       return Parity::None;
    if (s == "even")       return Parity::Even;
    if (s == "odd")        return Parity::Odd;
    if (s == "mark")       return Parity::Mark;
    if (s == "space")      return Parity::Space;
    throw std::invalid_argument("Unknown parity: " + s);
}

StopBits stop_bits_from_string(const std::string& s) {
    if (s == "1")          return StopBits::One;
    if (s == "1.5")        return StopBits::OnePointFive;
    if (s == "2")          return StopBits::Two;
    throw std::invalid_argument("Unknown stop bits: " + s);
}

DataBits data_bits_from_int(int v) {
    switch (v) {
        case 5: return DataBits::Five;
        case 6: return DataBits::Six;
        case 7: return DataBits::Seven;
        case 8: return DataBits::Eight;
        default: throw std::invalid_argument("Unknown data bits: " + std::to_string(v));
    }
}

PortType port_type_from_string(const std::string& s) {
    if (s == "serial")   return PortType::Serial;
    if (s == "network")  return PortType::Network;
    throw std::invalid_argument("Unknown port type: " + s);
}

} // namespace portpilot::domain
