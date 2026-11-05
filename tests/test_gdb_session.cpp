/*
 * test_gdb_session.cpp - GdbSession over real TCP sockets.
 *
 * What is under test is z80cpmw's half of the GDB stub: GdbSession, its debug
 * thread, and the way it starts, hands over and ends. The protocol itself is
 * cpmemu's gdbstub.cc and the banked-memory target is romwbw_emu's
 * romwbw_gdb.cc; both are compiled in as they are. The client is a few lines of
 * the GDB remote serial protocol, so no GDB is needed.
 *
 * The guest is four instructions in RAM bank 8EH at 0100H:
 *     0100  LD A,0
 *     0102  INC A
 *     0103  INC A
 *     0104  JR 0102
 *
 * Needs no wxWidgets and no window. On Windows, tests\run_tests.bat links
 * z80cpmw\gdb_platform_windows.cpp; elsewhere, tests/gdb_platform_posix.cpp.
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sock_t;
typedef int socklen_t;
static const sock_t kBad = INVALID_SOCKET;
static void closesock(sock_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
static const sock_t kBad = -1;
static void closesock(sock_t s) { close(s); }
#endif

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "GdbSession.h"
#include "gdbstub.h"
#include "qkz80.h"
#include "romwbw_gdb.h"
#include "romwbw_mem.h"

static int g_failures = 0;
static int g_checks = 0;

static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) g_failures++;
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

// --- a client of the remote serial protocol --------------------------------

static sock_t connectTo(int port) {
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kBad) return kBad;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(0x7F000001u);
    a.sin_port = htons((unsigned short)port);
    for (int i = 0; i < 50; i++) {   // the listener is up, but be patient
        if (connect(s, (struct sockaddr*)&a, sizeof a) == 0) return s;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    closesock(s);
    return kBad;
}

struct Client {
    sock_t s = kBad;
    std::string buf;
    explicit Client(sock_t sock) : s(sock) {
#ifdef _WIN32
        DWORD ms = 5000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof ms);
#else
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
    }
    ~Client() { if (s != kBad) closesock(s); }
    void sendPacket(const std::string& payload) {
        unsigned sum = 0;
        for (char c : payload) sum += (unsigned char)c;
        char tail[8];
        snprintf(tail, sizeof tail, "#%02x", sum & 0xFF);
        std::string p = "$" + payload + tail;
        send(s, p.data(), (int)p.size(), 0);
    }
    // The next packet's payload, or "<none>" when none comes in 5 s.
    std::string recvPacket() {
        for (;;) {
            size_t i = buf.find('$');
            size_t j = i == std::string::npos ? i : buf.find('#', i + 1);
            if (i != std::string::npos && j != std::string::npos && buf.size() >= j + 3) {
                std::string body = buf.substr(i + 1, j - i - 1);
                buf.erase(0, j + 3);
                send(s, "+", 1, 0);
                return body;
            }
            char tmp[1024];
            int n = (int)recv(s, tmp, sizeof tmp, 0);
            if (n <= 0) return "<none>";
            buf.append(tmp, (size_t)n);
        }
    }
    std::string ask(const std::string& payload) {
        sendPacket(payload);
        return recvPacket();
    }
};

static unsigned regWord(const std::string& g, int index) {
    // each register is 4 hex digits, low byte first
    unsigned lo = (unsigned)std::stoul(g.substr((size_t)index * 4, 2), nullptr, 16);
    unsigned hi = (unsigned)std::stoul(g.substr((size_t)index * 4 + 2, 2), nullptr, 16);
    return hi << 8 | lo;
}

// --- the machine ------------------------------------------------------------

struct Machine {
    banked_mem mem;
    qkz80 cpu;
    std::atomic<bool> stopReq{false};
    std::atomic<bool> finished{false};
    std::atomic<bool> finishedKilled{false};
    std::atomic<long> steps{0};
    GdbSession session;

    static GdbSession::Hooks hooks(Machine* m) {
        GdbSession::Hooks h;
        // As EmulatorEngine::gdbStep: stop when asked, run one instruction.
        h.step = [m](gdb::StepEvents& ev) {
            if (m->stopReq) throw GdbRunEnd{false};
            ev.exec_pc = m->cpu.regs.PC.get_pair16();
            ev.exec_sp = m->cpu.regs.SP.get_pair16();
            m->cpu.execute();
            m->steps++;
        };
        h.finished = [m](bool killed) {
            m->finishedKilled = killed;
            m->finished = true;
        };
        return h;
    }

    Machine() : mem(), cpu(&mem), session(cpu, mem, hooks(this)) {
        mem.enable_banking();
        mem.select_bank(0x8E);
        const uint8_t prog[] = {0x3E, 0x00, 0x3C, 0x3C, 0x18, 0xFC};
        for (size_t i = 0; i < sizeof prog; i++) mem.store_mem((uint16_t)(0x0100 + i), prog[i]);
        cpu.set_cpu_mode(qkz80::MODE_Z80);
        cpu.regs.PC.set_pair16(0x0100);
        cpu.regs.SP.set_pair16(0xF000);
    }
    ~Machine() {
        stopReq = true;
        session.stop();
    }
};

static int freePort() {
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(0x7F000001u);
    a.sin_port = 0;
    bind(s, (struct sockaddr*)&a, sizeof a);
    socklen_t len = sizeof a;
    getsockname(s, (struct sockaddr*)&a, &len);
    int port = ntohs(a.sin_port);
    closesock(s);
    return port;
}

static bool waitFor(std::atomic<bool>& flag, int ms) {
    for (int i = 0; i < ms / 10 && !flag; i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return flag;
}

static double seconds(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// --- cases -------------------------------------------------------------------

static void test_session_with_a_debugger() {
    printf("\n== a debugger connects, debugs and detaches\n");
    Machine m;
    int port = freePort();
    check(m.session.state() == GdbSession::State::Off, "a new session is off");
    check(m.session.start(gdb::kLoopback, port), "start() listens on 127.0.0.1");
    check(m.session.state() == GdbSession::State::Waiting, "the state is Waiting");
    check(m.session.where() == "127.0.0.1:" + std::to_string(port), "where() is 127.0.0.1:PORT, was " + m.session.where());
    check(m.session.statusText().find("127.0.0.1:" + std::to_string(port)) != std::string::npos,
          "the status line names the address to connect to");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    check(m.steps == 0, "no instruction runs while the stub waits for a debugger");

    Client c(connectTo(port));
    check(c.s != kBad, "a client connects");
    std::string r = c.ask("?");
    check(r.rfind("T05", 0) == 0 || r.rfind("S05", 0) == 0, "the stop reply to ? is SIGTRAP: " + r);
    check(m.session.state() == GdbSession::State::Connected, "the state is Connected");

    std::string g = c.ask("g");
    check(g.size() >= 13 * 4, "the register reply holds 13 registers");
    check(regWord(g, 5) == 0x0100, "PC is 0100H: no instruction has run");
    check(m.steps == 0, "still no instruction after the register read");

    std::string mem = c.ask("m100,6");
    check(mem == "3e003c3c18fc", "m reads the guest program from bank 8EH: " + mem);

    check(c.ask("Z0,103,1") == "OK", "Z0 sets a breakpoint at 0103H");
    r = c.ask("c");
    check(r.rfind("T05", 0) == 0, "continue stops at the breakpoint: " + r);
    g = c.ask("g");
    check(regWord(g, 5) == 0x0103, "PC is 0103H");
    check((regWord(g, 0) >> 8) == 1, "A is 1: the instructions ran");
    r = c.ask("c");
    g = c.ask("g");
    check(regWord(g, 5) == 0x0103 && (regWord(g, 0) >> 8) == 3,
          "continue again: back at 0103H with A = 3");
    r = c.ask("s");
    g = c.ask("g");
    check(r.rfind("T05", 0) == 0 && regWord(g, 5) == 0x0104, "a step moves PC to 0104H");

    // A register write reaches the CPU of the engine.
    check(c.ask("P4=00e0") == "OK", "P writes SP");
    check(m.cpu.regs.SP.get_pair16() == 0xE000, "the CPU has SP = E000H");

    check(c.ask("D") == "OK", "D detaches");
    check(waitFor(m.finished, 3000), "the debug thread says it is finished");
    check(!m.finishedKilled, "and not by a kill");
    check(m.session.state() == GdbSession::State::Detached, "the state is Detached");
    long before = m.steps;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(m.steps == before, "the debug thread runs no more instructions after it has finished");
    m.session.stop();
    check(m.session.state() == GdbSession::State::Off, "stop() after a detach turns the session off");
}

static void test_stop_while_running() {
    printf("\n== the machine stops while the debugger lets the guest run\n");
    Machine m;
    int port = freePort();
    m.session.start(gdb::kLoopback, port);
    Client c(connectTo(port));
    c.ask("?");
    c.sendPacket("c");   // no reply until the guest stops
    for (int i = 0; i < 200 && m.steps < 1000; i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    check(m.steps >= 1000, "the guest runs on the debug thread");
    auto t0 = std::chrono::steady_clock::now();
    m.stopReq = true;
    m.session.stop();
    check(seconds(t0) < 3.0, "stop() joins the debug thread within 3 s");
    std::string r = c.recvPacket();
    check(r == "W00", "the debugger is told the program ended (W00): " + r);
    check(m.finished, "the finished hook ran");
}

static void test_stop_while_waiting() {
    printf("\n== the machine stops while no debugger has connected\n");
    Machine m;
    m.session.start(gdb::kLoopback, freePort());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto t0 = std::chrono::steady_clock::now();
    m.stopReq = true;
    m.session.stop();
    check(seconds(t0) < 3.0, "stop() does not wait for a debugger that never comes");
    check(m.steps == 0, "the guest never ran");
    check(m.session.state() == GdbSession::State::Off, "the session is off");
}

static void test_kill() {
    printf("\n== the debugger kills the program\n");
    Machine m;
    int port = freePort();
    m.session.start(gdb::kLoopback, port);
    Client c(connectTo(port));
    c.ask("?");
    c.sendPacket("k");
    check(waitFor(m.finished, 3000), "the finished hook runs after k");
    check(m.finishedKilled, "and says the program was killed");
    check(m.session.state() == GdbSession::State::Killed, "the state is Killed");
}

static void test_port_in_use() {
    printf("\n== the port is taken\n");
    Machine m;
    int port = freePort();
    sock_t blocker = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(0x7F000001u);
    a.sin_port = htons((unsigned short)port);
    bind(blocker, (struct sockaddr*)&a, sizeof a);
    listen(blocker, 1);
    check(!m.session.start(gdb::kLoopback, port), "start() fails");
    check(m.session.state() == GdbSession::State::Failed, "the state is Failed");
    check(m.session.error().find(std::to_string(port)) != std::string::npos,
          "the error names the port: " + m.session.error());
    closesock(blocker);
    check(!m.session.start(gdb::kLoopback, 0), "port 0 is refused");
    check(!m.session.start(gdb::kLoopback, 70000), "port 70000 is refused");
}

static void test_every_interface() {
    printf("\n== listening on every interface\n");
    Machine m;
    int port = freePort();
    check(m.session.start(0, port), "start() on 0.0.0.0");
    check(m.session.where().find("every network address") != std::string::npos,
          "where() says every network address: " + m.session.where());
    Client c(connectTo(port));
    check(c.s != kBad && c.ask("?").rfind("T05", 0) == 0, "a client reaches the stub through 127.0.0.1");
}

static void test_restart() {
    printf("\n== a session can be started again\n");
    Machine m;
    int port = freePort();
    m.session.start(gdb::kLoopback, port);
    {
        Client c(connectTo(port));
        c.ask("?");
        c.ask("D");
    }
    waitFor(m.finished, 3000);
    m.session.stop();
    m.finished = false;
    check(m.session.start(gdb::kLoopback, port), "start() again on the same port");
    Client c2(connectTo(port));
    check(c2.s != kBad && c2.ask("?").rfind("T05", 0) == 0, "a second debugger connects to the fresh stub");
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    test_session_with_a_debugger();
    test_stop_while_running();
    test_stop_while_waiting();
    test_kill();
    test_port_in_use();
    test_every_interface();
    test_restart();
    printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
