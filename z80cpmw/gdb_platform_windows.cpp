/*
 * gdb_platform_windows.cpp - the platform:: functions that cpmemu's gdbstub.cc
 * calls, for a Win32 GUI program.
 *
 * The socket functions are cpmemu's own, from src/os/windows/platform.cc, with
 * the same return values (cpmemu's os/platform.h says which), and with one
 * addition: a blocking accept or receive wakes every 100 ms and looks at
 * gdbplat::set_abort(), because this program must be able to stop the thread
 * that waits for the debugger.  cpmemu's platform.cc is NOT compiled here: it
 * owns the console, and this program has none.
 *
 * The console functions answer for a program with no console: stdin is not a
 * terminal, and nothing can be read from it.
 *
 * Not in the precompiled header's world: winsock2.h has to come before
 * windows.h, and pch.h includes windows.h.  This file is compiled without pch.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "GdbPlatform.h"
#include "os/platform.h"

#pragma comment(lib, "ws2_32.lib")

namespace {
std::atomic<bool> g_abort{false};

bool winsock_start() {
    static bool started = false;
    if (started) return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "gdb stub: WSAStartup failed\n");
        return false;
    }
    started = true;
    return true;
}

// 1 when s can be read within timeout_ms (a closed socket counts), 0 when the
// time ran out, -1 on an error.  select() on Windows ignores its first
// argument.
int socket_readable(SOCKET s, int timeout_ms) {
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(s, &rd);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int n = select(0, &rd, NULL, NULL, &tv);
    if (n == SOCKET_ERROR) return -1;
    return n > 0 ? 1 : 0;
}
}  // namespace

namespace gdbplat {

void set_abort(bool abort) { g_abort = abort; }

std::string local_ipv4_addresses() {
    if (!winsock_start()) return "";
    char host[256];
    if (gethostname(host, sizeof host) != 0) return "";
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* list = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &list) != 0 || !list) return "";
    std::string out;
    for (struct addrinfo* p = list; p; p = p->ai_next) {
        if (p->ai_family != AF_INET) continue;
        const struct sockaddr_in* a = (const struct sockaddr_in*)p->ai_addr;
        unsigned long ip = ntohl(a->sin_addr.s_addr);
        if ((ip >> 24) == 127) continue;                  // loopback
        if ((ip >> 16) == 0xA9FE) continue;               // 169.254, no DHCP answer
        char text[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, (void*)&a->sin_addr, text, sizeof text)) continue;
        if (out.find(text) != std::string::npos) continue;
        if (!out.empty()) out += ", ";
        out += text;
    }
    freeaddrinfo(list);
    return out;
}

}  // namespace gdbplat

namespace platform {

// No console.
bool is_terminal() { return false; }
int console_getchar() { return -1; }
int termination_requested() { return 0; }

socket_t net_listen(uint32_t addr, int port, int* bound_port) {
    if (!winsock_start()) return kNoSocket;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        fprintf(stderr, "gdb stub: socket: error %d\n", (int)WSAGetLastError());
        return kNoSocket;
    }
    // No SO_REUSEADDR: on Windows it lets a second process take the same
    // port, which is not what a debugger port should allow.
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(addr);
    a.sin_port = htons((unsigned short)port);
    if (bind(s, (struct sockaddr*)&a, (int)sizeof a) != 0 || listen(s, 1) != 0) {
        fprintf(stderr, "gdb stub: cannot listen on %u.%u.%u.%u:%d: error %d\n",
                (unsigned)(addr >> 24), (unsigned)(addr >> 16) & 255u,
                (unsigned)(addr >> 8) & 255u, (unsigned)addr & 255u,
                port, (int)WSAGetLastError());
        closesocket(s);
        return kNoSocket;
    }
    int len = (int)sizeof a;
    if (getsockname(s, (struct sockaddr*)&a, &len) != 0) {
        fprintf(stderr, "gdb stub: getsockname: error %d\n", (int)WSAGetLastError());
        closesocket(s);
        return kNoSocket;
    }
    if (bound_port) *bound_port = (int)ntohs(a.sin_port);
    return (socket_t)s;
}

socket_t net_listen_local(int port, int* bound_port) {
    return net_listen(INADDR_LOOPBACK, port, bound_port);
}

socket_t net_accept(socket_t listener) {
    for (;;) {
        if (g_abort) return kNoSocket;
        int r = socket_readable((SOCKET)listener, 100);
        if (r < 0) {
            fprintf(stderr, "gdb stub: accept: error %d\n", (int)WSAGetLastError());
            return kNoSocket;
        }
        if (r == 0) continue;
        SOCKET c = accept((SOCKET)listener, NULL, NULL);
        if (c == INVALID_SOCKET) {
            fprintf(stderr, "gdb stub: accept: error %d\n", (int)WSAGetLastError());
            return kNoSocket;
        }
        BOOL one = TRUE;
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, (int)sizeof one);
        return (socket_t)c;
    }
}

int net_recv(socket_t s, void* buf, int len, int timeout_ms) {
    if (timeout_ms >= 0) {
        int r = socket_readable((SOCKET)s, timeout_ms);
        if (r < 0) return -1;
        if (r == 0) return -2;
    } else {
        // Wait without limit, in slices, so that set_abort() is heard.
        for (;;) {
            if (g_abort) return -1;
            int r = socket_readable((SOCKET)s, 100);
            if (r < 0) return -1;
            if (r > 0) break;
        }
    }
    int n = recv((SOCKET)s, (char*)buf, len, 0);
    if (n > 0) return n;
    if (n == 0) return 0;
    return -1;
}

bool net_send_all(socket_t s, const void* buf, int len) {
    const char* p = (const char*)buf;
    while (len > 0) {
        int n = send((SOCKET)s, p, len, 0);
        if (n == SOCKET_ERROR) return false;
        p += n;
        len -= n;
    }
    return true;
}

void net_close(socket_t s) {
    if (s != kNoSocket) closesocket((SOCKET)s);
}

// The stub calls this only from wait_console(), which GdbSession does not
// install, and from accept() when is_terminal() is true, which it never is.
// Answered anyway: the socket alone counts.
int wait_console_or_socket(socket_t s, int timeout_ms) {
    if (s == kNoSocket) {
        if (timeout_ms > 0) Sleep((DWORD)timeout_ms);
        return 0;
    }
    DWORD start = GetTickCount();
    for (;;) {
        if (g_abort) return -1;
        int r = socket_readable((SOCKET)s, 10);
        if (r < 0) return -1;
        if (r > 0) return 2;
        if (timeout_ms >= 0 && (int)(GetTickCount() - start) >= timeout_ms) return 0;
    }
}

}  // namespace platform
