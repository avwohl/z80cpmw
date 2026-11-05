/*
 * GdbSession.cpp - see GdbSession.h.
 */

#include "GdbSession.h"

#include <thread>

#include "GdbPlatform.h"
#include "gdbstub.h"
#include "romwbw_gdb.h"

struct GdbSession::Impl {
    Impl(qkz80& cpu, banked_mem& mem)
        : target(cpu, mem, &stop), stub(target, tracker) {
        target.stub = &stub;
        target.tracker = &tracker;
    }
    // RomwbwTarget wants a volatile bool* (romwbw_emu sets one from a signal
    // handler), and the target reads it on the debug thread. A plain volatile
    // is not synchronisation, so this class reads and writes its own atomic
    // copy and sets the volatile beside it only for the target's benefit.
    volatile bool stop = false;
    std::atomic<bool> stopping{false};
    RomwbwTarget target;
    gdb::Tracker tracker;
    gdb::Stub stub;
    std::thread thread;
};

GdbSession::GdbSession(qkz80& cpu, banked_mem& mem, Hooks hooks)
    : m_cpu(cpu), m_mem(mem), m_hooks(std::move(hooks)) {}

GdbSession::~GdbSession() { stop(); }

bool GdbSession::start(uint32_t addr, int port) {
    stop();
    m_state = State::Off;
    m_error.clear();
    m_where.clear();
    m_impl.reset(new Impl(m_cpu, m_mem));
    m_impl->target.step_fn = m_hooks.step;
    gdbplat::set_abort(false);

    if (port < 1 || port > 65535) {
        m_error = "the port must be from 1 to 65535";
        m_state = State::Failed;
        return false;
    }
    // Stub::listen writes its reasons to stderr, which this program has not
    // got; the reason shown to the user is the one built here.
    if (!m_impl->stub.listen(addr, port, nullptr)) {
        m_error = "cannot listen on port " + std::to_string(port) +
                  " (is another program using it?)";
        m_state = State::Failed;
        return false;
    }
    if (addr == gdb::kLoopback) {
        m_where = "127.0.0.1:" + std::to_string(port);
    } else if (addr == 0) {
        std::string ips = gdbplat::local_ipv4_addresses();
        m_where = "port " + std::to_string(port) + " on every network address";
        if (!ips.empty()) m_where += " of this PC (" + ips + ")";
    } else {
        m_where = std::to_string(addr >> 24) + "." + std::to_string((addr >> 16) & 255u) + "." +
                  std::to_string((addr >> 8) & 255u) + "." + std::to_string(addr & 255u) + ":" +
                  std::to_string(port);
    }
    m_state = State::Waiting;
    m_impl->thread = std::thread([this] { threadMain(); });
    return true;
}

void GdbSession::stop() {
    if (!m_impl) return;
    if (m_impl->thread.joinable()) {
        m_impl->stopping = true;
        m_impl->stop = true;
        gdbplat::set_abort(true);
        m_impl->thread.join();
        gdbplat::set_abort(false);
    }
    m_impl->stop = false;
    m_impl->stopping = false;
    if (m_state != State::Failed) m_state = State::Off;
}

std::string GdbSession::statusText() const {
    switch (m_state) {
    case State::Off:
        return "";
    case State::Waiting:
        return "waiting for GDB, connect to " + m_where;
    case State::Connected:
        return "GDB connected";
    case State::Detached:
        return "GDB detached";
    case State::Killed:
        return "GDB ended the program";
    case State::Ended:
        return "stopped";
    case State::Failed:
        return "GDB stub is off: " + m_error;
    }
    return "";
}

// The debug thread. It is the only thread that touches the CPU between start()
// and the call to hooks.finished.
void GdbSession::threadMain() {
    Impl& s = *m_impl;
    bool killed = false;
    bool ended = false;   // the machine stopped, or the debugger vanished into stop()
    if (!s.stub.accept()) {
        if (s.stopping) {
            ended = true;
        } else {
            m_error = "the connection from the debugger failed";
            m_state = State::Failed;
            if (m_hooks.finished) m_hooks.finished(false);
            return;
        }
    } else {
        m_state = State::Connected;
        try {
            s.stub.serve();
        } catch (const GdbRunEnd&) {
            ended = true;
        }
        killed = s.stub.killed();
    }
    if (s.stopping) ended = true;
    if (ended && !killed) {
        // Tell a debugger that is still there that the program has ended (the
        // W packet). Does nothing when none is attached.
        s.stub.report_exit(0);
    }
    m_state = killed ? State::Killed : ended ? State::Ended : State::Detached;
    if (m_hooks.finished) m_hooks.finished(killed);
}
