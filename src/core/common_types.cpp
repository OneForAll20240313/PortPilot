#include "common_types.h"
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace portpilot::core {

std::string gen_uuid_v4() {
    // 简易 UUID v4（满足 Core 层唯一 id 需求，不依赖系统库）
    using clock = std::chrono::steady_clock;
    const auto seed_val = static_cast<std::uint64_t>(clock::now().time_since_epoch().count());
    static thread_local std::mt19937_64 rng(seed_val);
    std::uniform_int_distribution<std::uint64_t> dist;
    const auto a = dist(rng);
    const auto b = dist(rng);
    // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    const std::uint32_t p1 = static_cast<std::uint32_t>(a >> 32);
    const std::uint16_t p2 = static_cast<std::uint16_t>(a >> 16);
    const std::uint16_t p3 = static_cast<std::uint16_t>((a & 0x0FFF) | 0x4000);  // version 4
    const std::uint16_t p4 = static_cast<std::uint16_t>((b & 0x3FFF) | 0x8000);  // variant
    const std::uint64_t p5 = b >> 16;
    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << p1 << '-'
       << std::setw(4) << p2 << '-'
       << std::setw(4) << p3 << '-'
       << std::setw(4) << p4 << '-'
       << std::setw(12) << (p5 & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

}  // namespace portpilot::core
