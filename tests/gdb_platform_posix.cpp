/*
 * gdb_platform_posix.cpp - the platform:: socket calls of cpmemu's gdbstub.cc,
 * and gdbplat:: from GdbPlatform.h, for a POSIX machine.
 *
 * NOT part of the application. z80cpmw is a Windows program and its own
 * version of this file is z80cpmw/gdb_platform_windows.cpp. This one exists so
 * that tests/test_gdb_session.cpp - GdbSession, the real gdbstub.cc and the
 * real romwbw_gdb.cc over real TCP sockets - can be built and run on a machine
 * without the Windows SDK. It follows the Windows file call for call: the same
 * 100 ms slices for a wait without limit, the same abort flag, the same return
 * values. What it cannot show is that the Windows file is right.
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include "GdbPlatform.h"
#include "os/platform.h"

namespace {
std::atomic<bool> g_abort{false};

int readable(int fd, int timeout_ms) {
    struct pollfd p;
    p.fd = fd;
    p.events = POLLIN;
    p.revents = 0;
    int n = poll(&p, 1, timeout_ms);
    if (n < 0) return -1;
    return n > 0 ? 1 : 0;
}
}  // namespace

namespace gdbplat {
void set_abort(bool abort) { g_abort = abort; }
std::string local_ipv4_addresses() { return ""; }
}  // namespace gdbplat

namespace platform {

bool is_terminal() { return false; }
int console_getchar() { return -1; }
int termination_requested() { return 0; }

socket_t net_listen(uint32_t addr, int port, int* bound_port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return kNoSocket;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(addr);
    a.sin_port = htons((unsigned short)port);
    if (bind(s, (struct sockaddr*)&a, sizeof a) != 0 || listen(s, 1) != 0) {
        close(s);
        return kNoSocket;
    }
    socklen_t len = sizeof a;
    if (getsockname(s, (struct sockaddr*)&a, &len) != 0) {
        close(s);
        return kNoSocket;
    }
    if (bound_port) *bound_port = ntohs(a.sin_port);
    return (socket_t)s;
}

socket_t net_listen_local(int port, int* bound_port) {
    return net_listen(INADDR_LOOPBACK, port, bound_port);
}

socket_t net_accept(socket_t listener) {
    for (;;) {
        if (g_abort) return kNoSocket;
        int r = readable((int)listener, 100);
        if (r < 0) return kNoSocket;
        if (r == 0) continue;
        int c = accept((int)listener, NULL, NULL);
        if (c < 0) return kNoSocket;
        int one = 1;
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
#ifdef SO_NOSIGPIPE
        setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
        return (socket_t)c;
    }
}

int net_recv(socket_t s, void* buf, int len, int timeout_ms) {
    if (timeout_ms >= 0) {
        int r = readable((int)s, timeout_ms);
        if (r < 0) return -1;
        if (r == 0) return -2;
    } else {
        for (;;) {
            if (g_abort) return -1;
            int r = readable((int)s, 100);
            if (r < 0) return -1;
            if (r > 0) break;
        }
    }
    ssize_t n = recv((int)s, buf, (size_t)len, 0);
    if (n > 0) return (int)n;
    if (n == 0) return 0;
    return -1;
}

bool net_send_all(socket_t s, const void* buf, int len) {
    const char* p = (const char*)buf;
    while (len > 0) {
        ssize_t n = send((int)s, p, (size_t)len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += n;
        len -= (int)n;
    }
    return true;
}

void net_close(socket_t s) {
    if (s != kNoSocket) close((int)s);
}

int wait_console_or_socket(socket_t s, int timeout_ms) {
    (void)timeout_ms;
    for (;;) {
        if (g_abort) return -1;
        int r = readable((int)s, 10);
        if (r < 0) return -1;
        if (r > 0) return 2;
    }
}

}  // namespace platform
