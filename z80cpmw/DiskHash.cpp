/*
 * DiskHash.cpp - See DiskHash.h for why these two live apart from DiskCatalog.
 */

#include "DiskHash.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstdio>
#include <memory>
#include <vector>

// SHA-256 comes from the OS rather than from a vendored implementation: it is
// one call away in every Windows this app supports, and a hash is exactly the
// kind of code that must not be written twice. Named here rather than in the
// .vcxproj so the project keeps holding no library list of its own for this
// file, the way DiskCatalog.h names winhttp.
#pragma comment(lib, "bcrypt.lib")

namespace {

// BCrypt handles, released however the function leaves. A type rather than a
// ladder of gotos, because a hash is the wrong place to be clever.
struct Sha256Hasher {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<UCHAR> object;

    ~Sha256Hasher() {
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    }

    bool begin() {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
                &alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
            return false;
        }
        DWORD objectSize = 0;
        DWORD written = 0;
        if (!BCRYPT_SUCCESS(BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                                              reinterpret_cast<PUCHAR>(&objectSize),
                                              sizeof(objectSize), &written, 0))) {
            return false;
        }
        object.resize(objectSize);
        return BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, object.data(), objectSize,
                                               nullptr, 0, 0));
    }

    bool update(const unsigned char* data, ULONG length) {
        return BCRYPT_SUCCESS(BCryptHashData(hash, const_cast<PUCHAR>(data), length, 0));
    }

    bool finish(std::string& hexOut) {
        UCHAR digest[32] = {};
        if (!BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0))) return false;
        static const char* kHex = "0123456789abcdef";
        std::string hex;
        hex.reserve(64);
        for (UCHAR b : digest) {
            hex += kHex[b >> 4];
            hex += kHex[b & 0x0F];
        }
        hexOut = hex;
        return true;
    }
};

}  // namespace

namespace diskhash {

bool sha256File(const std::string& path, std::string& hexOut) {
    Sha256Hasher hasher;
    if (!hasher.begin()) return false;

    std::unique_ptr<FILE, int (*)(FILE*)> file(fopen(path.c_str(), "rb"), &fclose);
    if (!file) return false;

    // The same 64KB block the download loop reads in.
    std::vector<unsigned char> buffer(64 * 1024);
    for (;;) {
        size_t read = fread(buffer.data(), 1, buffer.size(), file.get());
        if (read > 0 && !hasher.update(buffer.data(), static_cast<ULONG>(read))) return false;
        if (read < buffer.size()) {
            // Short read. EOF, or an error - and the difference is the whole
            // reason this is not a while(fread) loop. See DiskHash.h.
            if (ferror(file.get())) return false;
            break;
        }
    }

    return hasher.finish(hexOut);
}

bool sha256Bytes(const void* data, size_t size, std::string& hexOut) {
    Sha256Hasher hasher;
    if (!hasher.begin()) return false;

    // BCryptHashData takes a ULONG, and a std::string can be longer than one on
    // a 64-bit build, so it is fed in blocks rather than in one call. No caller
    // is anywhere near it - the largest published catalog is 14,694 bytes - but
    // a truncating cast here would hash a PREFIX and report it as the whole,
    // which is the one failure this file exists to make impossible.
    const unsigned char* p = static_cast<const unsigned char*>(data);
    size_t left = size;
    while (left > 0) {
        const ULONG chunk = left > 0x10000000u ? 0x10000000u : static_cast<ULONG>(left);
        if (!hasher.update(p, chunk)) return false;
        p += chunk;
        left -= chunk;
    }

    return hasher.finish(hexOut);
}

bool statFile(const std::string& path, DiskFileFacts& out) {
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) return false;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return false;

    out.size = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;

    ULARGE_INTEGER written;
    written.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    written.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    out.modified = static_cast<int64_t>(written.QuadPart);
    return true;
}

}  // namespace diskhash
