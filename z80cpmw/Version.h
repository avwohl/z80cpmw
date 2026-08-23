/*
 * Version.h - Application version definition
 *
 * THE SINGLE SOURCE OF THE VERSION. Change the four numbers below and nothing
 * else: the exe and its VERSIONINFO resource derive from the macros here, and
 * packaging/scripts/build-msix.ps1 and build-nsis.ps1 parse these #defines and
 * inject the result into the MSIX manifest and the NSIS installer at package
 * time. No packaging file stores a version of its own - the committed
 * AppxManifest.xml carries a 0.0.0.0 placeholder, and z80cpmw.nsi refuses to
 * build unless the scripts supply the numbers.
 *
 * The scripts match "#define VERSION_<FIELD> <digits>" anchored to the start of
 * a line, so keep these four on their own lines as plain integers.
 *
 * VERSION_BUILD is the fourth field and is normally 0: the Microsoft Store
 * reserves the revision field and rejects a package whose fourth number is not
 * zero (it also rejects a zero first field, which is what makes 0.0.0.0 a safe
 * placeholder - it packs, but it can never ship).
 */

#pragma once

// ============================================
// Version numbers - CHANGE THESE TO UPDATE
// ============================================
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 21
#define VERSION_BUILD 0

// ============================================
// Derived values - do not edit below
// ============================================

// Helper macros for stringification
#define _VER_STR(x) #x
#define _VER_XSTR(x) _VER_STR(x)

// String versions (auto-generated from numbers above)
#define VERSION_STRING \
    _VER_XSTR(VERSION_MAJOR) "." _VER_XSTR(VERSION_MINOR) "." _VER_XSTR(VERSION_PATCH)

#define VERSION_STRING_FULL \
    _VER_XSTR(VERSION_MAJOR) "." _VER_XSTR(VERSION_MINOR) "." _VER_XSTR(VERSION_PATCH) "." _VER_XSTR(VERSION_BUILD)

// For RC file VERSIONINFO (comma-separated)
#define VERSION_RC VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,VERSION_BUILD

// Wide form of VERSION_STRING, for Win32 APIs that take LPCWSTR - the WinHTTP
// user agent is the only current caller. Two-step widen: the inner macro has to
// expand VERSION_MAJOR to "1" before L can be pasted onto it.
#define _VER_WIDEN2(x) L##x
#define _VER_WIDEN(x) _VER_WIDEN2(x)
#define VERSION_STRING_W _VER_WIDEN(_VER_XSTR(VERSION_MAJOR)) L"." _VER_WIDEN(_VER_XSTR(VERSION_MINOR)) L"." _VER_WIDEN(_VER_XSTR(VERSION_PATCH))
