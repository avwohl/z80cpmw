/*
 * DiskHash.h - The two measurements DiskLedger reasons about, and the only part
 * of the provenance machinery that touches a file.
 *
 * Split out of DiskCatalog for one reason: to be testable. DiskLedger.cpp is
 * pure because it has to be checked, and these two functions are the other half
 * of the same argument - a wrong hash marks every image in the library as
 * differing from the catalog, and a write time read the wrong way re-hashes
 * 211MB on every launch for ever. Neither failure is visible by reading the
 * code, and neither could be reached while they were private statics on a class
 * that also owns WinHTTP.
 *
 * Win32 and the CRT, and nothing else - no WinHTTP, no wx, no window. That is
 * what lets tests/test_diskledger.cpp link this beside DiskLedger.cpp.
 */

#pragma once

#include "DiskLedger.h"

#include <cstddef>
#include <string>

namespace diskhash {

// SHA-256 of a file's contents, as 64 lowercase hex characters.
//
// Reads the whole file in 64KB blocks - 49MB and ~784 blocks for
// hd1k_combo.img - so it belongs on a worker thread and nowhere near the UI.
// Returns false, and leaves hexOut untouched, if the file cannot be opened or a
// read fails partway. A short read is EOF or an error and the two are told
// apart: treating an error as EOF would hash a PREFIX of the file and report it
// as the file's hash, which is the one failure mode that writes a confident
// wrong answer instead of no answer.
bool sha256File(const std::string& path, std::string& hexOut);

// SHA-256 of bytes already in memory, as 64 lowercase hex characters.
//
// For the catalog documents, which are fetched into a std::string and have to be
// checked against the catalog_sha256 the index publishes BEFORE they are parsed.
// Writing them to a file first so that sha256File could be used would mean the
// document that decides what to download is checked from a copy on disk rather
// than from the bytes that arrived. Returns false only when the OS refuses to
// give us a hash provider.
bool sha256Bytes(const void* data, size_t size, std::string& hexOut);

// Size and last-write time, which are the two facts that decide whether a
// stored measurement still describes the file. False when there is no file, or
// when the name is a directory.
//
// The time is raw FILETIME ticks kept as an integer all the way to the JSON. A
// time that round-trips a hair off invalidates every measurement on every
// launch.
bool statFile(const std::string& path, DiskFileFacts& out);

}  // namespace diskhash
