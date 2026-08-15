#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include "logger.h"
#include "event_bus.h"
#include "common_types.h"
#include "mock_device_port.h"
#include "serial_worker.h"
#include "platform/serial_backend.h"
#include "network_transport.h"
#include "file_repository.h"
#include "protocol_engine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace ppc = portpilot::core;
namespace ppd = portpilot::domain;

// =========================================================================
// 测试辅助：StubSerialBackend（纯内存实现，不做真实系统调用）
// 用于 SerialWorker 的参数校验 / 钩子逻辑 / 状态机等纯行为测试
// =========================================================================
class StubSerialBackend : public ppc::platform::ISerialBackend {
public:
    ppd::Result open(const ppd::ConnectTarget& config) override {
        opened_ = true;
        cfg_ = config;
        return ppd::Result::Ok();
    }
    ppd::Result close() override {
        opened_ = false;
        return ppd::Result::Ok();
    }
    ppd::Result write(const ppd::Bytes& data) override {
        last_written_ = data;
        total_written_ += data.size();
        return ppd::Result::Ok();
    }
    ppd::Bytes read() override {
        if (!pending_.empty()) {
            ppd::Bytes out = std::move(pending_.front());
            pending_.erase(pending_.begin());
            return out;
        }
        return {};
    }
    bool isOpen() const override { return opened_; }
    ppd::ProbeResult probe(const ppd::ConnectTarget& params,
                            ppd::ProbeCriteriaCallback onCriteria) override {
        ppd::ProbeResult r;
        r.confirmedTarget = params;
        r.success = !onCriteria || onCriteria(params);
        return r;
    }
    int64_t nativeHandle() const override { return -1; }
    bool isBusy(const std::string&) const override { return false; }

    // ---- 测试辅助 ----
    void inject(const ppd::Bytes& d) { pending_.push_back(d); }
    ppd::Bytes last_written() const { return last_written_; }
    std::size_t total_written() const { return total_written_; }
    ppd::ConnectTarget last_config() const { return cfg_; }

private:
    bool opened_{false};
    ppd::ConnectTarget cfg_;
    ppd::Bytes last_written_;
    std::size_t total_written_{0};
    std::vector<ppd::Bytes> pending_;
};

// PTY pair helper（同 core_serial_device_test.cpp 的定义，用于需要真实 open 的测试）
namespace {
struct PtyPair {
    int master{-1};
    std::string slave;
    ~PtyPair() { if (master >= 0) ::close(master); }
};
std::unique_ptr<PtyPair> OpenPtyPair() {
    auto p = std::make_unique<PtyPair>();
    int m = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (m < 0) return nullptr;
    if (::grantpt(m) != 0) { ::close(m); return nullptr; }
    if (::unlockpt(m) != 0) { ::close(m); return nullptr; }
    char buf[512] = {0};
    if (::ptsname_r(m, buf, sizeof(buf)) != 0) { ::close(m); return nullptr; }
    p->master = m;
    p->slave = buf;
    return p;
}
} // namespace

// =========================================================================
// 0. 基础 / Result / UUID（继续使用 core:: 模板版 Result<T>，非 DevicePort 契约）
// =========================================================================
TEST(CoreCommon, ResultOkAndError) {
    using namespace portpilot::core;
    auto ok = make_ok<int>(42);
    EXPECT_TRUE(ok.ok());
    EXPECT_EQ(*ok, 42);

    auto err = make_err<int>(ErrorCode::SESS_NOT_FOUND, "no session");
    EXPECT_FALSE(err.ok());
    EXPECT_EQ(err.error.code, ErrorCode::SESS_NOT_FOUND);
    EXPECT_EQ(err.error.message, "no session");

    Result<void> vok = make_ok();
    EXPECT_TRUE(vok.ok());
}

TEST(CoreCommon, UUIDFormatV4) {
    using namespace portpilot::core;
    const std::string a = gen_uuid_v4();
    const std::string b = gen_uuid_v4();
    EXPECT_EQ(a.size(), 36u);
    EXPECT_NE(a, b);
    EXPECT_EQ(a[14], '4');
    const char c = a[19];
    EXPECT_TRUE(c == '8' || c == '9' || c == 'a' || c == 'b' || c == 'A' || c == 'B');
}

// =========================================================================
// 1. Logger
// =========================================================================
TEST(LoggerTest, LevelsAndFiltering) {
    using namespace portpilot::core;
    auto& logger = Logger::instance();
    logger.clear_callbacks();
    logger.set_level(LogLevel::Info);
    logger.set_default_stderr(false);

    int infoCount = 0;
    int debugCount = 0;
    logger.add_callback([&](LogLevel lv, const std::string&, const std::string&, std::int64_t) {
        if (lv == LogLevel::Info) ++infoCount;
        if (lv == LogLevel::Debug) ++debugCount;
    });

    PP_INFO("tst") << "hello";
    PP_DEBUG("tst") << "no show";
    PP_WARN("tst") << "warn";

    EXPECT_EQ(infoCount, 1);
    EXPECT_EQ(debugCount, 0);
}

TEST(LoggerTest, StreamHelperFormat) {
    using namespace portpilot::core;
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::Trace);
    std::string got;
    logger.clear_callbacks();
    logger.add_callback([&](LogLevel, const std::string&, const std::string& msg, std::int64_t) {
        got = msg;
    });
    PP_INFO("mod") << "x=" << 42 << " y=" << 3.14;
    EXPECT_EQ(got, "x=42 y=3.14");
}

// =========================================================================
// 2. EventBus
// =========================================================================
TEST(EventBusTest, PubSubBasic) {
    using namespace portpilot::core;
    auto& bus = EventBus::instance();
    bus.clear();
    int count = 0;
    auto id = bus.on("buffer.dataReceived", [&](const std::string&, const EventPayload& pl) {
        ++count;
        if (!pl.empty()) {
            auto it = pl.find("connectionId");
            ASSERT_NE(it, pl.end());
            EXPECT_EQ(std::any_cast<std::string>(it->second), "conn-1");
        }
    });
    EXPECT_NE(id, 0u);
    bus.emit("buffer.dataReceived", EventPayload{{"connectionId", std::string("conn-1")}});
    bus.emit("buffer.dataReceived", EventPayload{});
    EXPECT_EQ(count, 2);
}

TEST(EventBusTest, OnceFiresSingleTime) {
    using namespace portpilot::core;
    auto& bus = EventBus::instance();
    bus.clear();
    int count = 0;
    bus.once("session.created", [&](const std::string&, const EventPayload&) { ++count; });
    bus.emit("session.created");
    bus.emit("session.created");
    EXPECT_EQ(count, 1);
}

TEST(EventBusTest, OffRemovesSubscription) {
    using namespace portpilot::core;
    auto& bus = EventBus::instance();
    bus.clear();
    int count = 0;
    auto id = bus.on("x.y", [&](const std::string&, const EventPayload&) { ++count; });
    EXPECT_TRUE(bus.off(id));
    EXPECT_FALSE(bus.off(id));
    bus.emit("x.y");
    EXPECT_EQ(count, 0);
}

TEST(EventBusTest, WildcardCatchesAll) {
    using namespace portpilot::core;
    auto& bus = EventBus::instance();
    bus.clear();
    int total = 0;
    bus.on("*", [&](const std::string&, const EventPayload&) { ++total; });
    bus.emit("a.b");
    bus.emit("c.d");
    EXPECT_EQ(total, 2);
}

TEST(EventBusTest, HandlerExceptionDoesNotBreakOthers) {
    using namespace portpilot::core;
    auto& bus = EventBus::instance();
    bus.clear();
    int a = 0, b = 0;
    bus.on("evt", [&](const std::string&, const EventPayload&) { ++a; throw std::runtime_error("boom"); });
    bus.on("evt", [&](const std::string&, const EventPayload&) { ++b; });
    bus.emit("evt");
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}

// =========================================================================
// 3. MockDevicePort (L2-002) - 统一使用 domain::DevicePort 契约
// =========================================================================
TEST(MockDevicePortTest, OpenCloseAndLoopback) {
    ppc::MockDevicePort port;
    EXPECT_FALSE(port.isOpen());

    ppd::ConnectTarget t;
    t.type = ppd::PortType::Serial;
    t.port = "/dev/ttyUSB0";
    t.baud = 115200;
    auto or_ = port.open(t);
    ASSERT_TRUE(or_.ok) << or_.code << " " << or_.message;
    EXPECT_TRUE(port.isOpen());

    port.set_loopback(true);
    ppd::Bytes tx = {0x01, 0x02, 0x03, 0x04};
    auto wr = port.write(tx);
    ASSERT_TRUE(wr.ok) << wr.code << " " << wr.message;
    EXPECT_EQ(port.total_tx_bytes(), 4u);

    auto rd = port.read();
    EXPECT_EQ(rd, tx);
}

TEST(MockDevicePortTest, FailInjection) {
    ppc::MockDevicePort port;
    port.set_fail_next_open(true);
    auto r = port.open(ppd::ConnectTarget{});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.code, "DEV_OPEN_FAILED");
    EXPECT_FALSE(port.isOpen());

    ASSERT_TRUE(port.open(ppd::ConnectTarget{}).ok);
    port.set_fail_next_write(true);
    EXPECT_FALSE(port.write(ppd::Bytes{0x00}).ok);
}

TEST(MockDevicePortTest, OnDataCallback) {
    ppc::MockDevicePort port;
    ASSERT_TRUE(port.open(ppd::ConnectTarget{}).ok);
    port.set_loopback(true);
    int cbCount = 0;
    ppd::Bytes last;
    port.onData([&](const ppd::Bytes& d) { ++cbCount; last = d; });
    const ppd::Bytes d = {0xAA, 0xBB};
    port.write(d);
    EXPECT_EQ(cbCount, 1);
    EXPECT_EQ(last, d);

    port.inject_rx({0xCC, 0xDD, 0xEE});
    EXPECT_EQ(cbCount, 2);
}

// =========================================================================
// 4. SerialWorker - Issue #50 Qt-free 重构后：
//    参数校验部分使用 StubBackend（避免真实系统调用）
//    真实 I/O 场景由 core_serial_device_test.cpp 中的 PTY 测试覆盖
// =========================================================================
TEST(SerialWorkerTest, RequiresSerialConfig) {
    // 注入 StubBackend：不做真实 open，专注验证 SerialWorker 的参数校验逻辑
    auto stub = std::make_unique<StubSerialBackend>();
    StubSerialBackend* rawStub = stub.get();
    ppc::SerialWorker sw(std::move(stub));

    ppd::ConnectTarget t;
    t.type = ppd::PortType::Network;  // 错的
    EXPECT_FALSE(sw.open(t).ok);

    t.type = ppd::PortType::Serial;
    t.port = "";  // 空端口
    t.baud = 9600;
    EXPECT_FALSE(sw.open(t).ok);

    t.port = "/dev/ttyS0";
    t.baud = 0;
    EXPECT_FALSE(sw.open(t).ok);

    t.baud = 115200;
    auto or_ = sw.open(t);
    ASSERT_TRUE(or_.ok) << or_.code << " " << or_.message;
    EXPECT_TRUE(sw.isOpen());
    EXPECT_EQ(rawStub->last_config().port, "/dev/ttyS0");
    EXPECT_EQ(rawStub->last_config().baud, 115200u);
    auto cfg = sw.serial_config();
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->port, "/dev/ttyS0");
    EXPECT_EQ(cfg->baud, 115200u);
}

TEST(SerialWorkerTest, WriteAndRxInject) {
    // 注入 StubBackend：专注测试 inject_rx / onData / write_observer 钩子
    auto stub = std::make_unique<StubSerialBackend>();
    ppc::SerialWorker sw(std::move(stub));

    ppd::ConnectTarget t;
    t.type = ppd::PortType::Serial;
    t.port = "COM3";   // 假名字，stub 不关心
    t.baud = 9600;
    ASSERT_TRUE(sw.open(t).ok);

    std::atomic<int> cbHits{0};
    sw.onData([&](const ppd::Bytes&) { ++cbHits; });

    sw.inject_rx({0x01, 0x02});
    sw.inject_rx({0x03});
    EXPECT_EQ(cbHits.load(), 2);

    auto r1 = sw.read();
    EXPECT_EQ(r1.size(), 2u);

    ppd::Bytes sent;
    sw.set_write_observer([&](const ppd::Bytes& d) { sent = d; });
    auto wr = sw.write({0x10, 0x20, 0x30});
    ASSERT_TRUE(wr.ok) << wr.code << " " << wr.message;
    EXPECT_EQ(sent, (ppd::Bytes{0x10, 0x20, 0x30}));
}

// =========================================================================
// 5. NetworkTransport - 统一使用 domain::DevicePort 契约
// =========================================================================
TEST(NetworkTransportTest, ValidateConfig) {
    ppc::NetworkTransport nt;
    ppd::ConnectTarget t;
    t.type = ppd::PortType::Serial;
    EXPECT_FALSE(nt.open(t).ok);
    t.type = ppd::PortType::Network;
    t.addr = "";
    t.tcpPort = 0;
    EXPECT_FALSE(nt.open(t).ok);
    t.addr = "127.0.0.1";
    t.tcpPort = 8080;
    auto or_ = nt.open(t);
    ASSERT_TRUE(or_.ok) << or_.code << " " << or_.message;
    EXPECT_TRUE(nt.isOpen());
    auto cfg = nt.last_config();
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->addr, "127.0.0.1");
    EXPECT_EQ(cfg->tcpPort, 8080);
}

TEST(NetworkTransportTest, WriteCounting) {
    ppc::NetworkTransport nt;
    ppd::ConnectTarget t;
    t.type = ppd::PortType::Network;
    t.addr = "x";
    t.tcpPort = 1000;
    ASSERT_TRUE(nt.open(t).ok);
    ASSERT_TRUE(nt.write({1, 2, 3}).ok);
    ASSERT_TRUE(nt.write({4, 5}).ok);
    EXPECT_EQ(nt.total_tx(), 5u);
}

// =========================================================================
// 6. FileRepository (InMemory) - 继续使用 core:: 模板版 Result<T>
// =========================================================================
TEST(InMemoryFileRepositoryTest, KVBasic) {
    using namespace portpilot::core;
    InMemoryFileRepository repo;
    EXPECT_FALSE(repo.get("a").ok());
    ASSERT_TRUE(repo.set("a", "1").ok());
    ASSERT_TRUE(repo.set("b", "2").ok());
    ASSERT_TRUE(repo.set("aa", "3").ok());

    auto r = repo.get("a");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(*r, "1");

    auto keys = repo.keys("a");
    ASSERT_TRUE(keys.ok());
    EXPECT_EQ(keys->size(), 2u);

    ASSERT_TRUE(repo.remove("a").ok());
    EXPECT_FALSE(repo.get("a").ok());
    EXPECT_FALSE(repo.remove("a").ok());
}

TEST(InMemoryFileRepositoryTest, DocumentCRUD) {
    using namespace portpilot::core;
    InMemoryFileRepository repo;
    EXPECT_FALSE(repo.load_document("id1", "protocols").ok());

    ASSERT_TRUE(repo.save_document("id1", "protocols", "{\"name\":\"p1\"}").ok());
    ASSERT_TRUE(repo.save_document("id2", "protocols", "{\"name\":\"p2\"}").ok());
    ASSERT_TRUE(repo.save_document("id1", "sessions", "{\"name\":\"s1\"}").ok());

    auto r = repo.load_document("id1", "protocols");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(*r, "{\"name\":\"p1\"}");

    auto list = repo.list_documents("protocols");
    ASSERT_TRUE(list.ok());
    EXPECT_EQ(list->size(), 2u);

    ASSERT_TRUE(repo.delete_document("id1", "protocols").ok());
    EXPECT_FALSE(repo.load_document("id1", "protocols").ok());
}

// =========================================================================
// 7. ProtocolEngine (Basic) - 继续使用 core:: 模板版 Result<T>
// =========================================================================
TEST(ProtocolEngineUtil, EncodingDecodingRoundtrip) {
    using namespace portpilot::core;
    Bytes buf;
    BasicProtocolEngine::encode_uint(buf, 0, 2, ByteOrder::Big, 0x1234);
    EXPECT_EQ(buf, (Bytes{0x12, 0x34}));
    EXPECT_EQ(BasicProtocolEngine::decode_uint(buf, 0, 2, ByteOrder::Big), 0x1234u);

    BasicProtocolEngine::encode_uint(buf, 0, 4, ByteOrder::Little, 0xAABBCCDD);
    EXPECT_EQ(buf, (Bytes{0xDD, 0xCC, 0xBB, 0xAA}));
}

TEST(ProtocolEngineUtil, HexConversions) {
    using namespace portpilot::core;
    EXPECT_EQ(BasicProtocolEngine::hex_to_bytes("7E0D0A"), (Bytes{0x7E, 0x0D, 0x0A}));
    EXPECT_EQ(BasicProtocolEngine::bytes_to_hex(Bytes{0x01, 0xAB}), "01ab");
}

TEST(ProtocolEngineTest, FixedLengthWithStartPattern) {
    using namespace portpilot::core;
    ProtocolSchema s;
    s.id = "p1";
    s.name = "FixedProto";
    s.lengthType = LengthType::Fixed;
    s.frameDef.fixedLength = 4;
    s.frameDef.startPattern = {0x7E};

    BasicProtocolEngine eng;
    Bytes stream = {0x7E, 0x01, 0x02, 0x03, 0x04,
                    0x7E, 0x0A, 0x0B, 0x0C, 0x0D,
                    0x7E, 0x00};

    auto pr = eng.parseByteStream(s, stream);
    EXPECT_EQ(pr.frames.size(), 2u);
    EXPECT_EQ(pr.frames[0].raw, (Bytes{0x7E, 0x01, 0x02, 0x03}));
    EXPECT_EQ(pr.frames[1].raw, (Bytes{0x7E, 0x0A, 0x0B, 0x0C}));
    EXPECT_EQ(pr.leftover.size(), 2u);
}

TEST(ProtocolEngineTest, VariableLengthWithCrc) {
    using namespace portpilot::core;
    ProtocolSchema s;
    s.lengthType = LengthType::Variable;
    LengthField lf;
    lf.offset = 1;
    lf.width = 2;
    lf.byteOrder = ByteOrder::Big;
    s.frameDef.lengthField = lf;
    s.frameDef.startPattern = {0x7E};
    CrcDef crc;
    crc.type = CrcType::Crc8;
    crc.position = CrcPosition::Tail;
    s.frameDef.crc = crc;

    BasicProtocolEngine eng;
    Bytes frame;
    frame.push_back(0x7E);
    frame.push_back(0x00); frame.push_back(0x02);
    frame.push_back(0xAA); frame.push_back(0xBB);
    auto c = BasicProtocolEngine::crc8(frame);
    frame.push_back(c);

    Bytes stream = frame;
    stream.push_back(0x7E);

    auto pr = eng.parseByteStream(s, stream);
    ASSERT_EQ(pr.frames.size(), 1u);
    EXPECT_EQ(pr.frames[0].raw, frame);
    EXPECT_EQ(pr.leftover, (Bytes{0x7E}));
}

TEST(ProtocolEngineTest, EncodeFixedAndExtractFields) {
    using namespace portpilot::core;
    ProtocolSchema s;
    s.lengthType = LengthType::Fixed;
    s.frameDef.fixedLength = 6;
    s.frameDef.startPattern = {0xAA};

    FieldDef f1; f1.name = "cmd"; f1.type = FieldType::Uint8;  f1.offset = 1;
    FieldDef f2; f2.name = "value"; f2.type = FieldType::Uint16; f2.offset = 2;
        f2.byteOrder = ByteOrder::Big;
    FieldDef f3; f3.name = "name"; f3.type = FieldType::Ascii; f3.offset = 4; f3.length = 2;
    s.fields = {f1, f2, f3};

    BasicProtocolEngine eng;
    std::vector<FieldValue> values = {
        std::uint64_t{0x05},
        std::uint64_t{0x1234},
        std::string{"OK"}
    };
    auto enc = eng.encodeFrame(s, values);
    ASSERT_TRUE(enc.ok());
    EXPECT_EQ(*enc, (Bytes{0xAA, 0x05, 0x12, 0x34, 'O', 'K'}));

    Frame f; f.raw = *enc;
    auto pf = eng.extractFields(s, f);
    ASSERT_TRUE(pf.ok());
    EXPECT_EQ(std::get<std::uint64_t>(pf->fields.at("cmd")), 0x05u);
    EXPECT_EQ(std::get<std::uint64_t>(pf->fields.at("value")), 0x1234u);
    EXPECT_EQ(std::get<std::string>(pf->fields.at("name")), "OK");
}

TEST(ProtocolEngineTest, EncodeAndParseRoundtripVariable) {
    using namespace portpilot::core;
    ProtocolSchema s;
    s.lengthType = LengthType::Variable;
    LengthField lf; lf.offset = 1; lf.width = 2; lf.byteOrder = ByteOrder::Big;
    s.frameDef.lengthField = lf;
    s.frameDef.startPattern = {0x7E};

    FieldDef f1; f1.name = "a"; f1.type = FieldType::Uint8; f1.offset = 3;
    FieldDef f2; f2.name = "b"; f2.type = FieldType::Uint8; f2.offset = 4;
    FieldDef f3; f3.name = "c"; f2.type = FieldType::Uint8; f3.offset = 5; f3.type = FieldType::Uint16;
        f3.length = 2; f3.byteOrder = ByteOrder::Big;
    s.fields = {f1, f2, f3};

    BasicProtocolEngine eng;
    auto enc = eng.encodeFrame(s, {
        std::uint64_t{0x01}, std::uint64_t{0x02}, std::uint64_t{0x0100}
    });
    ASSERT_TRUE(enc.ok());

    auto pr = eng.parseByteStream(s, *enc);
    ASSERT_EQ(pr.frames.size(), 1u);
    auto pf = eng.extractFields(s, pr.frames[0]);
    ASSERT_TRUE(pf.ok());
    EXPECT_EQ(std::get<std::uint64_t>(pf->fields.at("a")), 0x01u);
    EXPECT_EQ(std::get<std::uint64_t>(pf->fields.at("b")), 0x02u);
    EXPECT_EQ(std::get<std::uint64_t>(pf->fields.at("c")), 0x0100u);
}
