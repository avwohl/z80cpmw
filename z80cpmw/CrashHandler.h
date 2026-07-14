/*
 * CrashHandler.h - Crash reporting (minidump on unhandled failure)
 */

#pragma once

#include <string>

// Install process-wide crash reporting: unhandled SEH exceptions, escaped C++
// exceptions (std::terminate) and abort()/CRT asserts (via a SIGABRT handler)
// all write a minidump to dumpDir and tell the user where it is before the
// process exits.
void InstallCrashHandler(const std::string& dumpDirUtf8);

// True once a crash report is in progress. Message handlers that could run
// from a modal pump (e.g. the emulator timer) must bail out when this is set,
// so a corrupted process never keeps executing while the report is shown.
bool IsCrashing();
