#include <gtest/gtest.h>
#include "domain/types.h"
#include "domain/device_port.h"
#include <string>

namespace ppd = portpilot::domain;

TEST(TypesTest, ParityToStringRoundTrip) {
    EXPECT_EQ(ppd::to_string(ppd::Parity::None), "none");
    EXPECT_EQ(ppd::to_string(ppd::Parity::Even), "even");
    EXPECT_EQ(ppd::to_string(ppd::Parity::Odd), "odd");
    EXPECT_EQ(ppd::to_string(ppd::Parity::Mark), "mark");
    EXPECT_EQ(ppd::to_string(ppd::Parity::Space), "space");

    EXPECT_EQ(ppd::parity_from_string("none"), ppd::Parity::None);
    EXPECT_EQ(ppd::parity_from_string("even"), ppd::Parity::Even);
    EXPECT_EQ(ppd::parity_from_string("odd"), ppd::Parity::Odd);
    EXPECT_EQ(ppd::parity_from_string("mark"), ppd::Parity::Mark);
    EXPECT_EQ(ppd::parity_from_string("space"), ppd::Parity::Space);
}

TEST(TypesTest, StopBitsToStringRoundTrip) {
    EXPECT_EQ(ppd::to_string(ppd::StopBits::One), "1");
    EXPECT_EQ(ppd::to_string(ppd::StopBits::OnePointFive), "1.5");
    EXPECT_EQ(ppd::to_string(ppd::StopBits::Two), "2");

    EXPECT_EQ(ppd::stop_bits_from_string("1"), ppd::StopBits::One);
    EXPECT_EQ(ppd::stop_bits_from_string("1.5"), ppd::StopBits::OnePointFive);
    EXPECT_EQ(ppd::stop_bits_from_string("2"), ppd::StopBits::Two);
}

TEST(TypesTest, DataBitsToStringRoundTrip) {
    EXPECT_EQ(ppd::to_string(ppd::DataBits::Five), "5");
    EXPECT_EQ(ppd::to_string(ppd::DataBits::Six), "6");
    EXPECT_EQ(ppd::to_string(ppd::DataBits::Seven), "7");
    EXPECT_EQ(ppd::to_string(ppd::DataBits::Eight), "8");

    EXPECT_EQ(ppd::data_bits_from_int(5), ppd::DataBits::Five);
    EXPECT_EQ(ppd::data_bits_from_int(6), ppd::DataBits::Six);
    EXPECT_EQ(ppd::data_bits_from_int(7), ppd::DataBits::Seven);
    EXPECT_EQ(ppd::data_bits_from_int(8), ppd::DataBits::Eight);
}

TEST(TypesTest, PortTypeToStringRoundTrip) {
    EXPECT_EQ(ppd::to_string(ppd::PortType::Serial), "serial");
    EXPECT_EQ(ppd::to_string(ppd::PortType::Network), "network");

    EXPECT_EQ(ppd::port_type_from_string("serial"), ppd::PortType::Serial);
    EXPECT_EQ(ppd::port_type_from_string("network"), ppd::PortType::Network);
}

TEST(TypesTest, DataBitsThrowsOnInvalid) {
    EXPECT_THROW(ppd::data_bits_from_int(4), std::invalid_argument);
    EXPECT_THROW(ppd::data_bits_from_int(9), std::invalid_argument);
}

TEST(TypesTest, ParityThrowsOnInvalid) {
    EXPECT_THROW(ppd::parity_from_string("bad"), std::invalid_argument);
}

TEST(TypesTest, StopBitsThrowsOnInvalid) {
    EXPECT_THROW(ppd::stop_bits_from_string("3"), std::invalid_argument);
}

TEST(TypesTest, PortTypeThrowsOnInvalid) {
    EXPECT_THROW(ppd::port_type_from_string("usb"), std::invalid_argument);
}

TEST(TypesTest, ResultHelpers) {
    auto ok = ppd::Result::Ok();
    EXPECT_TRUE(ok.ok);
    EXPECT_TRUE(ok.code.empty());

    auto err = ppd::Result::Err("SESS_PARAM_INVALID", "bad param");
    EXPECT_FALSE(err.ok);
    EXPECT_EQ(err.code, "SESS_PARAM_INVALID");
    EXPECT_EQ(err.message, "bad param");
}

TEST(TypesTest, ConnectTargetDefault) {
    ppd::ConnectTarget t{};
    EXPECT_EQ(t.type, ppd::PortType::Serial);
    EXPECT_EQ(t.baud, 9600u);
    EXPECT_EQ(t.dataBits, ppd::DataBits::Eight);
    EXPECT_EQ(t.stopBits, ppd::StopBits::One);
    EXPECT_EQ(t.parity, ppd::Parity::None);
}

TEST(TypesTest, BytesVectorWorks) {
    ppd::Bytes b = {0x01, 0x02, 0x03};
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x01);
    EXPECT_EQ(b[2], 0x03);
}

TEST(TypesTest, DataCallbackInvocable) {
    bool called = false;
    ppd::Bytes received;
    ppd::DataCallback cb = [&](const ppd::Bytes& data) {
        called = true;
        received = data;
    };
    ppd::Bytes payload = {0xAA, 0xBB};
    cb(payload);
    EXPECT_TRUE(called);
    EXPECT_EQ(received, payload);
}

class MockDevicePort : public ppd::DevicePort {
public:
    bool open_called{false};
    bool close_called{false};
    bool is_open{false};
    ppd::ConnectTarget last_config{};
    ppd::Bytes last_written{};
    ppd::Bytes to_read{};
    ppd::DataCallback data_cb{};
    ppd::ProbeResult probe_ret{};
    ppd::ProbeCriteriaCallback last_criteria{};

    ppd::Result open(const ppd::ConnectTarget& config) override {
        open_called = true;
        last_config = config;
        is_open = true;
        return ppd::Result::Ok();
    }
    ppd::Result close() override {
        close_called = true;
        is_open = false;
        return ppd::Result::Ok();
    }
    ppd::Result write(const ppd::Bytes& data) override {
        last_written = data;
        return ppd::Result::Ok();
    }
    ppd::Bytes read() override {
        return to_read;
    }
    void onData(ppd::DataCallback callback) override {
        data_cb = std::move(callback);
    }
    bool isOpen() const override {
        return is_open;
    }
    ppd::ProbeResult probe(const ppd::ConnectTarget& params, ppd::ProbeCriteriaCallback onCriteria) override {
        last_criteria = std::move(onCriteria);
        probe_ret.confirmedTarget = params;
        return probe_ret;
    }
};

TEST(DevicePortTest, MockDevicePortCanOpenClose) {
    MockDevicePort port;
    EXPECT_FALSE(port.isOpen());

    ppd::ConnectTarget cfg;
    cfg.port = "/dev/ttyUSB0";
    cfg.baud = 115200;
    auto r = port.open(cfg);
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(port.isOpen());
    EXPECT_TRUE(port.open_called);
    EXPECT_EQ(port.last_config.port, "/dev/ttyUSB0");
    EXPECT_EQ(port.last_config.baud, 115200u);

    auto rc = port.close();
    EXPECT_TRUE(rc.ok);
    EXPECT_FALSE(port.isOpen());
    EXPECT_TRUE(port.close_called);
}

TEST(DevicePortTest, MockDevicePortWriteRead) {
    MockDevicePort port;
    ppd::Bytes out = {0x10, 0x20, 0x30};
    auto r = port.write(out);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(port.last_written, out);

    port.to_read = {0xA, 0xB};
    ppd::Bytes got = port.read();
    EXPECT_EQ(got, port.to_read);
}

TEST(DevicePortTest, MockDevicePortDataCallback) {
    MockDevicePort port;
    bool fired = false;
    ppd::Bytes seen;
    port.onData([&](const ppd::Bytes& d) {
        fired = true;
        seen = d;
    });
    ppd::Bytes payload = {0xFF};
    ASSERT_NE(port.data_cb, nullptr);
    port.data_cb(payload);
    EXPECT_TRUE(fired);
    EXPECT_EQ(seen, payload);
}

TEST(DevicePortTest, MockDevicePortProbe) {
    MockDevicePort port;
    ppd::ConnectTarget params;
    params.port = "COM3";
    port.probe_ret.success = true;
    bool criteria_called = false;
    ppd::ConnectTarget seen_params;
    auto criteria = [&](const ppd::ConnectTarget& t) {
        criteria_called = true;
        seen_params = t;
        return true;
    };
    auto pr = port.probe(params, criteria);
    EXPECT_TRUE(pr.success);
    EXPECT_EQ(pr.confirmedTarget.port, "COM3");
    EXPECT_NE(port.last_criteria, nullptr);
    port.last_criteria(params);
    EXPECT_TRUE(criteria_called);
    EXPECT_EQ(seen_params.port, "COM3");
}

TEST(DevicePortTest, ProbeCriteriaCallbackType) {
    ppd::ProbeCriteriaCallback accept = [](const ppd::ConnectTarget&) { return true; };
    ppd::ProbeCriteriaCallback reject = [](const ppd::ConnectTarget&) { return false; };
    ppd::ConnectTarget t;
    EXPECT_TRUE(accept(t));
    EXPECT_FALSE(reject(t));
}

TEST(DevicePortTest, DevicePortPtrIsUniquePtr) {
    ppd::DevicePortPtr ptr = std::make_unique<MockDevicePort>();
    EXPECT_NE(ptr, nullptr);
    EXPECT_FALSE(ptr->isOpen());
}
