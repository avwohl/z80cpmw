/*
 * Utf8Path.h - Every path this application holds in a std::string is UTF-8.
 *
 * The data folder is built with WideCharToMultiByte(CP_UTF8), the file dialogs
 * convert what they return the same way, and z80cpmw.json is UTF-8 because
 * nlohmann refuses to write anything else. What used to break the rule was the
 * other end: fopen, std::ifstream and the -A Win32 calls read a narrow path in
 * the ANSI code page, not UTF-8. A path with only ASCII in it reads the same
 * either way, so nothing showed until a Windows user name with an accented
 * letter put a non-ASCII character into the data folder - and then every disk
 * and ROM in it failed to open.
 *
 * So no narrow path goes to the OS. A file is opened through the wide API, with
 * the path converted here, and a path the OS hands back comes through fromWide.
 *
 * Why not <activeCodePage>UTF-8</activeCodePage> in the manifest, which would
 * make CP_ACP mean UTF-8 and fix every -A call at once: wxMBConv_win32::WC2MB
 * (3.2.6 to 3.3.3 and master) converts with CP_ACP, WC_NO_BEST_FIT_CHARS and a
 * lpUsedDefaultChar pointer, and WideCharToMultiByte documents both as invalid
 * for UTF-8. Every wxString-to-narrow conversion in the app would be at risk,
 * and that cannot be checked from here. It also needs Windows 10 1903, and the
 * package admits 1809.
 *
 * Header-only, so the test suites that link DiskHash.cpp or emu_io_windows.cpp
 * need no new file on their cl line.
 */

#pragma once

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace utf8path {

// UTF-8 to UTF-16. Malformed input becomes U+FFFD, which names no file, so a
// bad path fails to open rather than opening something else.
inline std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    if (needed <= 0) return std::wstring();
    std::wstring out((size_t)needed, L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), &out[0], needed);
    if (written <= 0) return std::wstring();
    out.resize((size_t)written);
    return out;
}

// UTF-16 to UTF-8, for a path the OS returned.
inline std::string fromWide(const wchar_t* w, size_t len) {
    if (!w || len == 0) return std::string();
    int needed = WideCharToMultiByte(CP_UTF8, 0, w, (int)len, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return std::string();
    std::string out((size_t)needed, '\0');
    int written = WideCharToMultiByte(CP_UTF8, 0, w, (int)len, &out[0], needed, nullptr, nullptr);
    if (written <= 0) return std::string();
    out.resize((size_t)written);
    return out;
}

inline std::string fromWide(const std::wstring& w) {
    return fromWide(w.data(), w.size());
}

inline std::string fromWide(const wchar_t* w) {
    return w ? fromWide(w, wcslen(w)) : std::string();
}

// ANSI code page to UTF-8, for the one kind of narrow text the OS still hands
// back in it: MSVC's std::system_category().message(), which is FormatMessageA.
inline std::string fromAnsi(const std::string& ansi) {
    if (ansi.empty()) return std::string();
    int needed = MultiByteToWideChar(CP_ACP, 0, ansi.data(), (int)ansi.size(), nullptr, 0);
    if (needed <= 0) return std::string();
    std::wstring w((size_t)needed, L'\0');
    int written = MultiByteToWideChar(CP_ACP, 0, ansi.data(), (int)ansi.size(), &w[0], needed);
    if (written <= 0) return std::string();
    return fromWide(w.data(), (size_t)written);
}

// fopen with a UTF-8 path. The mode is ASCII ("rb", "r+b", "wb", "ab"...).
inline FILE* fopen(const std::string& path, const char* mode) {
    std::wstring wmode;
    for (const char* m = mode; m && *m; ++m) wmode += (wchar_t)(unsigned char)*m;
    return ::_wfopen(wide(path).c_str(), wmode.c_str());
}

// For std::ifstream / std::ofstream, which take a filesystem::path: a path
// built from a wide string, so the stream opens it through the wide API.
inline std::filesystem::path fsPath(const std::string& path) {
    return std::filesystem::path(wide(path));
}

}  // namespace utf8path
