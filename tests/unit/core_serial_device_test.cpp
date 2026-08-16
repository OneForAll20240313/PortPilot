// glibc 特性宏由 tests/CMakeLists.txt 的 target_compile_definitions 统一注入
// （_DEFAULT_SOURCE / _XOPEN_SOURCE=600 / _GNU_SOURCE），此处不再重复定义，避免重定义 warning
#include <gtest/gtest.h>
#include "core/serial_device.h"
#include "domain/types.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace ppc = portpilot::core;
namespace ppd = portpilot::domain;

TEST(SerialDeviceTest, EmptyPathGivesParamInvalid) {
    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = "";
    auto r = dev.open(cfg);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.code, "SESS_PARAM_INVALID");
}

TEST(SerialDeviceTest, NoSuchDevice) {
    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = "/this/path/does/not/exist/portpilot_test";
    auto r = dev.open(cfg);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.code, "DEVICE_NO_SUCH");
    EXPECT_NE(r.message.find("设备不存在"), std::string::npos);
}

TEST(SerialDeviceTest, NoSuchDeviceSingleComponent) {
    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = "/no_such_device_portpilot_xyz_12345";
    auto r = dev.open(cfg);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.code, "DEVICE_NO_SUCH");
}

TEST(SerialDeviceTest, NotOpenState) {
    ppc::SerialDevice dev;
    EXPECT_FALSE(dev.isOpen());
    auto w = dev.write({0x01, 0x02});
    EXPECT_FALSE(w.ok);
    EXPECT_EQ(w.code, "DEVICE_NOT_OPEN");
    auto rd = dev.read();
    EXPECT_TRUE(rd.empty());
    auto c = dev.close();
    EXPECT_FALSE(c.ok);
    EXPECT_EQ(c.code, "DEVICE_NOT_OPEN");
}

namespace {

struct PtyPair {
    int master{-1};
    std::string slave;
    ~PtyPair() {
        if (master >= 0) ::close(master);
    }
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

TEST(SerialDeviceTest, PtyCustomPathOpenClose) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);
    ASSERT_FALSE(pty->slave.empty());

    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = pty->slave;
    cfg.baud = 9600;
    cfg.dataBits = ppd::DataBits::Eight;
    cfg.stopBits = ppd::StopBits::One;
    cfg.parity = ppd::Parity::None;

    auto r = dev.open(cfg);
    EXPECT_TRUE(r.ok) << "open failed: " << r.code << " " << r.message;
    EXPECT_TRUE(dev.isOpen());

    auto c = dev.close();
    EXPECT_TRUE(c.ok) << "close failed: " << c.code << " " << c.message;
    EXPECT_FALSE(dev.isOpen());
}

TEST(SerialDeviceTest, PtyMultiLevelPathWorks) {
    // /dev/pts/X is a multi-level custom path (not standard enumeration)
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);
    ASSERT_EQ(pty->slave.compare(0, 9, "/dev/pts/"), 0)
        << "slave should start with /dev/pts/: " << pty->slave;

    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = pty->slave;
    cfg.baud = 115200;
    auto r = dev.open(cfg);
    EXPECT_TRUE(r.ok) << "open " << pty->slave << ": " << r.code << " " << r.message;
    ASSERT_TRUE(dev.isOpen());
    EXPECT_TRUE(dev.close().ok);
}

TEST(SerialDeviceTest, PtyWriteReadLoopback) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);

    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = pty->slave;
    cfg.baud = 9600;
    ASSERT_TRUE(dev.open(cfg).ok);

    ppd::Bytes out = {'H', 'e', 'l', 'l', 'o'};
    auto wr = dev.write(out);
    ASSERT_TRUE(wr.ok) << wr.code << " " << wr.message;

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    uint8_t buf[32] = {0};
    ssize_t n = ::read(pty->master, buf, sizeof(buf));
    ASSERT_GT(n, 0);
    ppd::Bytes got(buf, buf + n);
    EXPECT_EQ(got, out);
}

TEST(SerialDeviceTest, PtyMasterToSlaveRead) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);

    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = pty->slave;
    cfg.baud = 9600;
    ASSERT_TRUE(dev.open(cfg).ok);

    ppd::Bytes payload = {0xAA, 0xBB, 0xCC, 0xDD};
    ::write(pty->master, payload.data(), payload.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ppd::Bytes got = dev.read();
    ASSERT_FALSE(got.empty());
    EXPECT_EQ(got.size(), payload.size());
    EXPECT_EQ(got, payload);
}

TEST(SerialDeviceTest, OnDataCallback) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);

    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = pty->slave;
    cfg.baud = 9600;
    ASSERT_TRUE(dev.open(cfg).ok);

    bool fired = false;
    ppd::Bytes seen;
    dev.onData([&](const ppd::Bytes& b) {
        fired = true;
        seen = b;
    });

    ppd::Bytes payload = {0x01, 0x02, 0x03};
    ::write(pty->master, payload.data(), payload.size());
    for (int i = 0; i < 20 && !fired; ++i) {
        (void)dev.read();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(fired);
    if (!seen.empty()) {
        EXPECT_EQ(seen, payload);
    }
}

TEST(SerialDeviceTest, DoubleOpenRejected) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);

    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = pty->slave;
    ASSERT_TRUE(dev.open(cfg).ok);

    ppd::ConnectTarget cfg2;
    cfg2.port = pty->slave;
    auto r = dev.open(cfg2);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.code, "DEVICE_BUSY");
    EXPECT_TRUE(dev.close().ok);
}

TEST(SerialDeviceTest, ProbeNoSuchPath) {
    ppc::SerialDevice dev;
    ppd::ConnectTarget params;
    params.port = "/nope/no/such/device";
    auto pr = dev.probe(params, nullptr);
    EXPECT_FALSE(pr.success);
    EXPECT_NE(pr.message.find("设备不存在"), std::string::npos);
}

TEST(SerialDeviceTest, ProbeEmptyPath) {
    ppc::SerialDevice dev;
    ppd::ConnectTarget params;
    auto pr = dev.probe(params, nullptr);
    EXPECT_FALSE(pr.success);
}

TEST(SerialDeviceTest, ProbePtySuccess) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);

    ppc::SerialDevice dev;
    ppd::ConnectTarget params;
    params.port = pty->slave;
    bool cbCalled = false;
    auto cb = [&](const ppd::ConnectTarget& t) {
        cbCalled = true;
        EXPECT_EQ(t.port, pty->slave);
        return true;
    };
    auto pr = dev.probe(params, cb);
    EXPECT_TRUE(pr.success) << pr.message;
    EXPECT_TRUE(cbCalled);
    EXPECT_EQ(pr.confirmedTarget.port, pty->slave);
}

TEST(SerialDeviceTest, ProbeCriteriaFails) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);

    ppc::SerialDevice dev;
    ppd::ConnectTarget params;
    params.port = pty->slave;
    auto pr = dev.probe(params, [](const ppd::ConnectTarget&) { return false; });
    EXPECT_FALSE(pr.success);
    EXPECT_NE(pr.message.find("判定条件未满足"), std::string::npos);
}

TEST(SerialDeviceTest, UnsupportedBaudRejected) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);

    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = pty->slave;
    cfg.baud = 999999999u; // unsupported baud
    auto r = dev.open(cfg);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.code, "SESS_PARAM_INVALID");
    EXPECT_NE(r.message.find("不支持的波特率"), std::string::npos);
}

TEST(SerialDeviceTest, PermissionDeniedMessageFormat) {
    char tmpl[256] = {0};
    std::snprintf(tmpl, sizeof(tmpl), "/tmp/portpilot_no_perm_XXXXXX");
    int fd = ::mkstemp(tmpl);
    if (fd < 0) GTEST_SKIP() << "cannot create temp file";
    ::close(fd);
    ::chmod(tmpl, 0000);

    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = tmpl;
    auto r = dev.open(cfg);

    ::chmod(tmpl, 0644);
    ::unlink(tmpl);

    if (::geteuid() == 0) {
        GTEST_SKIP() << "running as root; permission test skipped";
    }
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.code, "DEVICE_PERMISSION");
    EXPECT_NE(r.message.find("dialout"), std::string::npos);
}

TEST(SerialDeviceTest, DestructorClosesFd) {
    auto pty = OpenPtyPair();
    ASSERT_NE(pty, nullptr);
    int fd = -1;
    {
        ppc::SerialDevice dev;
        ppd::ConnectTarget cfg;
        cfg.port = pty->slave;
        ASSERT_TRUE(dev.open(cfg).ok);
        fd = static_cast<int>(dev.nativeHandle());
        ASSERT_GE(fd, 0);
    }
    struct stat st{};
    EXPECT_EQ(::fstat(fd, &st), -1);
    EXPECT_EQ(errno, EBADF);
}
