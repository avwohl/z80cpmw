/*
 * CrashHandler.cpp - Crash reporting implementation
 *
 * This is a GUI-subsystem process: without a handler, an unhandled exception
 * kills it with no visible trace at all (issue #1 was exactly such a silent
 * crash-to-desktop).
 *
 * The report is written by a dedicated thread created at install time
 * (Breakpad pattern). The failing thread only stores its context, signals the
 * report thread and blocks - a few thin syscalls that work even on an
 * exhausted stack. Just as important: the failing thread is usually the UI
 * thread, and showing a modal dialog there would pump its message queue and
 * re-dispatch WM_TIMER into the corrupted emulator. The report thread has its
 * own clean stack and no message queue to poison.
 */

#include "pch.h"
#include "CrashHandler.h"
#include <dbghelp.h>
#include <csignal>
#include <exception>

// Precomputed at install time so the crash path does no heap allocation.
static wchar_t g_dumpDir[MAX_PATH] = {};

static volatile LONG g_crashing = 0;

// Crash context handed from the failing thread to the report thread.
static EXCEPTION_POINTERS* g_crashInfo = nullptr;
static DWORD g_crashThreadId = 0;
static HANDLE g_crashEvent = nullptr;

typedef BOOL(WINAPI* MiniDumpWriteDumpFn)(
    HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION,
    PMINIDUMP_USER_STREAM_INFORMATION,
    PMINIDUMP_CALLBACK_INFORMATION);

bool IsCrashing() {
    return g_crashing != 0;
}

// Runs on the pre-created report thread once a crash is signaled: write the
// dump, tell the user, kill the process. The failing thread stays blocked the
// whole time, so its stack is intact in the dump.
static DWORD WINAPI crashReportThread(LPVOID) {
    WaitForSingleObject(g_crashEvent, INFINITE);

    wchar_t dumpPath[MAX_PATH + 64];
    wchar_t displayPath[MAX_PATH + 64];
    SYSTEMTIME st;
    GetLocalTime(&st);
    wsprintfW(dumpPath, L"%s\\z80cpmw-crash-%04u%02u%02u-%02u%02u%02u.dmp",
              g_dumpDir, st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond);
    lstrcpynW(displayPath, dumpPath, ARRAYSIZE(displayPath));

    bool wrote = false;
    if (g_dumpDir[0]) {
        HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
        if (dbghelp) {
            auto writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(
                GetProcAddress(dbghelp, "MiniDumpWriteDump"));
            if (writeDump) {
                HANDLE file = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr,
                                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                          nullptr);
                if (file != INVALID_HANDLE_VALUE) {
                    MINIDUMP_EXCEPTION_INFORMATION mei = {};
                    mei.ThreadId = g_crashThreadId;
                    mei.ExceptionPointers = g_crashInfo;
                    mei.ClientPointers = FALSE;
                    wrote = writeDump(GetCurrentProcess(), GetCurrentProcessId(),
                                      file, MiniDumpNormal,
                                      g_crashInfo ? &mei : nullptr,
                                      nullptr, nullptr) != FALSE;

                    // Show the real location: under MSIX, %LOCALAPPDATA%\z80cpmw
                    // is virtualized to ...\Packages\<pkg>\LocalCache\Local\...
                    // and the literal dumpPath does not exist in Explorer.
                    wchar_t finalPath[MAX_PATH + 64];
                    DWORD n = GetFinalPathNameByHandleW(
                        file, finalPath, ARRAYSIZE(finalPath),
                        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                    if (n > 0 && n < ARRAYSIZE(finalPath)) {
                        const wchar_t* p = finalPath;
                        if (wcsncmp(p, L"\\\\?\\", 4) == 0) p += 4;
                        lstrcpynW(displayPath, p, ARRAYSIZE(displayPath));
                    }
                    CloseHandle(file);
                }
            }
        }
    }

    wchar_t message[MAX_PATH + 320];
    if (wrote) {
        wsprintfW(message,
                  L"z80cpmw hit an internal error and has to close.\n\n"
                  L"A crash report was saved to:\n%s\n\n"
                  L"Please attach that file to an issue at\n"
                  L"https://github.com/avwohl/z80cpmw/issues",
                  displayPath);
    } else {
        wsprintfW(message,
                  L"z80cpmw hit an internal error and has to close.\n\n"
                  L"(A crash report could not be written.)\n\n"
                  L"Please report this at\n"
                  L"https://github.com/avwohl/z80cpmw/issues");
    }
    MessageBoxW(nullptr, message, L"z80cpmw crashed",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TASKMODAL);

    TerminateProcess(GetCurrentProcess(), 3);
    return 0;
}

// Called on the failing thread. Hands off to the report thread and blocks;
// does almost nothing itself so it works even on an exhausted stack.
static void reportCrashAndDie(EXCEPTION_POINTERS* info) {
    if (InterlockedExchange(&g_crashing, 1)) {
        // Second failure (possibly inside reporting): just die.
        TerminateProcess(GetCurrentProcess(), 3);
    }
    g_crashInfo = info;
    g_crashThreadId = GetCurrentThreadId();
    if (g_crashEvent && SetEvent(g_crashEvent)) {
        Sleep(INFINITE);  // the report thread terminates the process
    }
    TerminateProcess(GetCurrentProcess(), 3);
}

static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info) {
    reportCrashAndDie(info);
    return EXCEPTION_EXECUTE_HANDLER;  // not reached
}

static void onTerminate() {
    // No exception context, but the dump still captures every thread's stack,
    // including the one that called terminate().
    reportCrashAndDie(nullptr);
}

// UCRT abort() runs a user-installed SIGABRT handler before its own
// kill-the-process step. That step is __fastfail on Windows 8+, which the
// unhandled-exception filter never sees - so this handler is the only way
// abort()/CRT asserts produce a report.
static void onSigAbrt(int) {
    reportCrashAndDie(nullptr);
}

void InstallCrashHandler(const std::string& dumpDirUtf8) {
    if (MultiByteToWideChar(CP_UTF8, 0, dumpDirUtf8.c_str(), -1,
                            g_dumpDir, MAX_PATH) == 0) {
        // Fall back to %TEMP% rather than losing the dump entirely.
        if (GetTempPathW(MAX_PATH, g_dumpDir) == 0) {
            g_dumpDir[0] = L'\0';
        }
    }
    g_dumpDir[MAX_PATH - 1] = L'\0';

    g_crashEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_crashEvent) {
        HANDLE thread = CreateThread(nullptr, 64 * 1024, crashReportThread,
                                     nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        } else {
            CloseHandle(g_crashEvent);
            g_crashEvent = nullptr;
        }
    }

    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    std::set_terminate(onTerminate);
    signal(SIGABRT, onSigAbrt);
}
