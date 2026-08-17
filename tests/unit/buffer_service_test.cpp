#include <gtest/gtest.h>

#include <any>
#include <cstdint>
#include <string>
#include <vector>

#include "buffer_service.h"
#include "core/event_bus.h"

namespace pps = portpilot::service;
namespace ppc = portpilot::core;
namespace ppd = portpilot::domain;

// =========================================================================
// BufferService（字节流 Qt-free 内核，#36）
//   验证：onData → buffer.dataReceived 事件载荷（connectionId/bytes/timestamp）
//        订阅/取消订阅 行为正确
// =========================================================================

TEST(BufferServiceTest, PushDataEmitDataReceivedWithPayload) {
    ppc::EventBus::instance().clear();
    pps::BufferService buffer;

    std::vector<std::string> conns;
    std::vector<ppd::Bytes> datas;
    int hits = 0;

    buffer.onDataReceived([&](const std::string&, const ppc::EventPayload& payload) {
        ++hits;
        auto cIt = payload.find("connectionId");
        ASSERT_NE(cIt, payload.end());
        conns.push_back(std::any_cast<std::string>(cIt->second));

        auto bIt = payload.find("bytes");
        ASSERT_NE(bIt, payload.end());
        datas.push_back(std::any_cast<ppd::Bytes>(bIt->second));

        auto tIt = payload.find("timestamp");
        ASSERT_NE(tIt, payload.end());
        ASSERT_TRUE(std::any_cast<std::int64_t>(tIt->second) > 0);
    });

    ppd::Bytes d = {0x01, 0x02, 0x03};
    buffer.pushData("conn-1", d);

    EXPECT_EQ(hits, 1);
    ASSERT_EQ(conns.size(), 1u);
    EXPECT_EQ(conns[0], "conn-1");
    ASSERT_EQ(datas.size(), 1u);
    EXPECT_EQ(datas[0], d);
}

TEST(BufferServiceTest, OffRemovesSubscription) {
    ppc::EventBus::instance().clear();
    pps::BufferService buffer;

    int hits = 0;
    auto id = buffer.onDataReceived([&](const std::string&, const ppc::EventPayload&) { ++hits; });
    EXPECT_NE(id, 0u);

    buffer.pushData("a", {0x01});
    EXPECT_EQ(hits, 1);

    buffer.off(id);
    buffer.pushData("a", {0x02});
    EXPECT_EQ(hits, 1);
}

TEST(BufferServiceTest, ListenerCountReflectsSubscriptions) {
    ppc::EventBus::instance().clear();
    pps::BufferService buffer;

    EXPECT_EQ(buffer.dataReceivedListeners(), 0u);
    auto id = buffer.onDataReceived([&](const std::string&, const ppc::EventPayload&) {});
    EXPECT_EQ(buffer.dataReceivedListeners(), 1u);
    buffer.off(id);
    EXPECT_EQ(buffer.dataReceivedListeners(), 0u);
}

TEST(BufferServiceTest, NullHandlerRejected) {
    ppc::EventBus::instance().clear();
    pps::BufferService buffer;
    EXPECT_EQ(buffer.onDataReceived(nullptr), 0u);
}