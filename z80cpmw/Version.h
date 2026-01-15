/*
 * Version.h - Application version definition
 *
 * Change version here - all locations will be updated automatically.
 */

#pragma once

// ============================================
// Version numbers - CHANGE THESE TO UPDATE
// ============================================
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 9
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
