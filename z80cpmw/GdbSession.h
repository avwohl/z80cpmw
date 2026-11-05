/*
 * GdbSession.h - the GDB stub of z80cpmw, and the thread that serves it.
 *
 * The stub itself (gdbstub.cc) is cpmemu's and the banked-memory target
 * (romwbw_gdb.cc) is romwbw_emu's; both are compiled out of the sibling
 * checkouts. This class is the part that is z80cpmw's: it owns a stub, a
 * target and a tracker, listens on a TCP port, and runs a DEBUG THREAD.
 *
 * Why a thread. Stub::serve() does not return while the debugger holds the
 * program, and the program is the Z80 CPU. Without the stub, the CPU runs on the
 * UI thread in batches (MainWindow::onTimer -> EmulatorEngine::runBatch). A
 * serve() on that thread would freeze the window. So while a session is
 * active the debug thread OWNS THE CPU: it waits for the debugger, serves it,
 * and the UI thread's runBatch() does nothing. When the debugger detaches, the
 * debug thread runs the rest of the session out and hands the CPU back (the
 * engine does that, through the owner callback below).
 *
 * The engine decides how one instruction runs (step) and what the UI thread may
 * touch meanwhile. This class knows nothing of Windows.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class qkz80;
class banked_mem;

namespace gdb {
struct StepEvents;
}

class GdbSession {
public:
    // What the engine gives the session.
    struct Hooks {
        // Run ONE instruction of the guest (or sleep if the guest is idle) and
        // fill in the event. Called on the debug thread only. Throws
        // GdbRunEnd (romwbw_gdb.h) when the machine must stop.
        std::function<void(gdb::StepEvents&)> step;
        // Called on the debug thread, last thing before it exits. The engine
        // takes the CPU back (or not, if the debugger killed the program).
        std::function<void(bool killed)> finished;
    };

    enum class State {
        Off,        // no session
        Waiting,    // listening, no debugger yet; no instruction has run
        Connected,  // a debugger holds the program or runs it
        Detached,   // the debugger left; the guest runs on the UI thread again
        Killed,     // the debugger sent "k"; the engine will stop the machine
        Ended,      // the machine stopped under the debugger
        Failed      // could not listen
    };

    GdbSession(qkz80& cpu, banked_mem& mem, Hooks hooks);
    ~GdbSession();

    // Every start() builds a fresh stub: a Stub is good for one debugger.
    // Listen on addr (host byte order; gdb::kLoopback or 0 for all interfaces)
    // and port, and start the debug thread. False if the port cannot be
    // used; then state() is Failed and error() says why.
    bool start(uint32_t addr, int port);

    // End the session and join the debug thread. Safe to call at any time and
    // more than once. Not to be called from the debug thread.
    void stop();

    State state() const { return m_state; }
    const std::string& error() const { return m_error; }
    // "127.0.0.1:1234" - what to type into GDB. With every interface, the
    // addresses of this PC are listed too.
    const std::string& where() const { return m_where; }

    // The text for the status line, or empty when there is no session.
    std::string statusText() const;

private:
    struct Impl;
    void threadMain();

    qkz80& m_cpu;
    banked_mem& m_mem;
    std::unique_ptr<Impl> m_impl;
    Hooks m_hooks;
    std::atomic<State> m_state{State::Off};
    std::string m_error;
    std::string m_where;
};
