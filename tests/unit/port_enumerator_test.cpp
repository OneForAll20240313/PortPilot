// glibc 特性宏由 tests/CMakeLists.txt 的 target_compile_definitions 统一注入
// （_DEFAULT_SOURCE / _XOPEN_SOURCE=600 / _GNU_SOURCE），此处不再重复定义
#include <gtest/gtest.h>
#include "core/port_enumerator.h"
#include "core/serial_device.h"
#include "domain/types.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace ppc = portpilot::core;
namespace ppd = portpilot::domain;

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

TEST(PortEnumeratorTest, EnumerateReturnsOk) {
    ppc::PortEnumerator en;
    auto res = en.listAvailablePorts();
    EXPECT_TRUE(res.ok) << res.code << " " << res.message;
}

TEST(PortEnumeratorTest, EnumerateResultWellFormed) {
    ppc::PortEnumerator en;
    auto res = en.listAvailablePorts();
    ASSERT_TRUE(res.ok) << res.code << " " << res.message;
    for (const auto& pi : res.ports) {
        EXPECT_FALSE(pi.id.empty());
        EXPECT_FALSE(pi.name.empty());
        EXPECT_EQ(pi.id, pi.systemLocation);
    }
}

TEST(PortEnumeratorTest, EnumeratesNonStandardPts) {
    auto pty = OpenPtyPair();
    if (pty == nullptr) {
        GTEST_SKIP() << "系统不支持 pty，跳过";
        return;
    }
    // 仅当 slave 位于 /dev/pts/ 下才属于本需求（#34）场景
    if (pty->slave.compare(0, 9, "/dev/pts/") != 0) {
        GTEST_SKIP() << "slave 不在 /dev/pts/ 下: " << pty->slave;
        return;
    }

    ppc::PortEnumerator en;
    auto res = en.listAvailablePorts();
    ASSERT_TRUE(res.ok) << res.code << " " << res.message;

    bool found = false;
    for (const auto& pi : res.ports) {
        if (pi.id == pty->slave) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "候选列表应包含伪终端 " << pty->slave;

    // DoD：候选应可成功打开
    ppc::SerialDevice dev;
    ppd::ConnectTarget cfg;
    cfg.port = pty->slave;
    cfg.baud = 9600;
    auto r = dev.open(cfg);
    EXPECT_TRUE(r.ok) << r.code << " " << r.message;
    if (r.ok) {
        EXPECT_TRUE(dev.close().ok);
    }
}