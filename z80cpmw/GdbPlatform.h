/*
 * GdbPlatform.h - what the GDB stub needs from the host, beyond the socket
 * functions of cpmemu's os/platform.h.
 *
 * gdb_platform_windows.cpp implements both the platform:: socket calls that
 * cpmemu's gdbstub.cc makes and the two functions here.  tests/ has a POSIX
 * version of the same, so GdbSession can be exercised on a machine that has no
 * Windows SDK.
 */

#pragma once

#include <string>

namespace gdbplat {

// While true, a blocked accept or receive of the stub gives up within 100 ms:
// accept returns "no socket", and receive returns an error, which the stub
// treats as the debugger going away.  GdbSession::stop() sets it, so the debug
// thread can be joined while it waits for a debugger that never connects.
void set_abort(bool abort);

// The non-loopback IPv4 addresses of this PC, as "a.b.c.d, e.f.g.h", for the
// status line when the stub listens on every interface.  Empty if none is
// known.
std::string local_ipv4_addresses();

}  // namespace gdbplat
