/*
 * DiskCatalog.cpp - Disk Image Catalog and Download Manager Implementation
 */

#include "pch.h"
#include "DiskCatalog.h"
#include "Version.h"
#include <thread>
#include <sstream>
#include <vector>
#include <memory>
#include <cstdio>

// The two measurements, kept in their own file so a suite can link them without
// linking WinHTTP and this class. DiskHash.h says why that mattered enough to
// split.
#include "DiskHash.h"

// Disk images are pinned to one explicit ioscpm release tag (not "latest"), so a
// new ioscpm release can't silently swap the disk images out from under an
// installed client and re-introduce an HBIOS/CBIOS version mismatch with the
// embedded ROM. When you rebuild the ROMs + disks against a new RomWBW version
// and cut a new ioscpm release, bump RELEASE_TAG here to that new tag (and bump
// the app version). This is the single source of truth for both URLs below.
// v1.4.12 (2026-09-01) replaces v1.4.5. The only difference between the two
// catalogs is hd1k_combo.img: 7042 bytes each, differing on one line, the
// <sha256> going be19984e... -> 89b8ae1a.... Every other filename, size and
// hash is identical, and so is the catalog's version attribute - which matters
// because a moved version attribute is what triggers the disk-wipe on ports
// that have one (this one does not; nothing here reads that attribute).
//
// What the new combo image fixes is R8: the old one hands an unfiltered host
// basename to F_DELETE, so importing a host file whose name contains ? or *
// erased every matching CP/M file first. It also brings the v1.36 W8 - no
// truncation of a binary export at the first 1Ah, host paths containing a
// space, 32-bit byte counts, and no bogus org 0100h padding w8.com with 256
// leading zero bytes.
static const std::wstring RELEASE_TAG = L"v1.4.12";

const std::wstring DiskCatalog::CATALOG_URL =
    L"https://github.com/avwohl/ioscpm/releases/download/" + RELEASE_TAG + L"/disks.xml";
const std::wstring DiskCatalog::RELEASE_BASE_URL =
    L"https://github.com/avwohl/ioscpm/releases/download/" + RELEASE_TAG + L"/";

DiskCatalog::DiskCatalog() {
    // m_downloadDir is assigned here without taking m_downloadDirMutex, and
    // that is the one place it is safe: a constructor runs before any other
    // thread can hold a reference to the object, since the workers capture
    // shared_from_this() and there is no shared_ptr to share until this returns.
    // Every other write goes through setDownloadDirectory().
    //
    // Default to user data directory\data (for Store app compatibility)
    // Will be overridden by MainWindow::loadSettings() with proper path
    wchar_t* localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        int len = WideCharToMultiByte(CP_UTF8, 0, localAppData, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string path(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, localAppData, -1, &path[0], len, nullptr, nullptr);
            m_downloadDir = path + "\\z80cpmw\\data";
        }
        CoTaskMemFree(localAppData);
    }

    // Fallback to app directory if LocalAppData fails
    if (m_downloadDir.empty()) {
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        char* lastSlash = strrchr(path, '\\');
        if (lastSlash) *lastSlash = '\0';
        m_downloadDir = std::string(path) + "\\data";
    }
}

DiskCatalog::~DiskCatalog() {
    // Nothing to cancel here, and that is a fact about ownership rather than an
    // oversight: every worker holds a shared_from_this() for as long as it is
    // running, so by the time the last reference goes and this runs, no worker
    // is left to tell. The cancelDownload() that used to be here could only ever
    // have an effect in the one case where the object was being destroyed with a
    // worker still going - exactly the case shared ownership removed. The
    // shutdown cancel that call was accidentally providing is now explicit, in
    // MainWindow::onDestroy.
}

void DiskCatalog::setDownloadDirectory(const std::string& dir) {
    {
        std::lock_guard<std::mutex> lock(m_downloadDirMutex);
        m_downloadDir = dir;
    }
    // Both calls below use the caller's own 'dir' rather than re-reading the
    // member, and the lock is released before them on purpose:
    // updateDownloadedStatus() takes m_downloadDirMutex itself, and holding it
    // across a CreateDirectory would put a filesystem call inside the lock that
    // every worker read contends on.
    CreateDirectoryA(dir.c_str(), nullptr);
    updateDownloadedStatus();

    // A different data folder is a different ledger, so the one in hand stops
    // describing anything. Dropped rather than re-read here: this runs on the UI
    // thread (loadSettings at startup, and the Settings dialog), and the next
    // fetchCatalog worker is where reading a file belongs.
    {
        std::lock_guard<std::mutex> lock(m_ledgerMutex);
        m_ledger = DiskLedger();
        m_ledgerLoaded = false;
    }
}

void DiskCatalog::fetchCatalog(CatalogLoadedCallback callback) {
    // Detached, so this returns immediately and nobody can join it. The 'self'
    // capture is what makes 'this' safe to use below: it is a reference to this
    // object that outlives the whole lambda body, so the members the worker
    // reads cannot be freed underneath it however early the app is quit. 'this'
    // is captured alongside it only so the member names still resolve.
    // 'callback' is captured BY VALUE, so the std::function itself is alive
    // whenever it is invoked. What is NOT this function's to guarantee is what
    // the callback closes over - see the contract on fetchCatalog in
    // DiskCatalog.h.
    std::thread([this, self = shared_from_this(), callback]() {
        std::string xml;
        std::string error;

        if (!downloadToString(CATALOG_URL, xml, error)) {
            if (callback) {
                callback(false, {}, error);
            }
            return;
        }

        std::vector<DiskEntry> entries;
        if (!parseCatalogXML(xml, entries, error)) {
            if (callback) {
                callback(false, {}, error);
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_catalogMutex);
            m_catalogEntries = entries;
        }
        updateDownloadedStatus();
        // Worker-thread only, and this is the worker. It reads up to 211MB when
        // a measurement has gone stale, which is why it is here and not in
        // updateDownloadedStatus() - that one is also called from
        // setDownloadDirectory() on the UI thread, and hashing the library
        // inside the Settings dialog's OK handler would freeze it.
        updateFreshness();

        if (callback) {
            callback(true, getCatalogEntries(), "");
        }
    }).detach();
}

void DiskCatalog::downloadDisk(const std::string& filename,
                                DownloadProgressCallback progressCb,
                                DownloadCompleteCallback completeCb) {
    if (m_downloadState == DownloadState::Downloading) {
        if (completeCb) {
            completeCb(false, "Download already in progress");
        }
        return;
    }

    m_downloadState = DownloadState::Downloading;
    m_cancelRequested = false;

    // Run in background thread. 'self' for the same reason as in fetchCatalog,
    // and it matters more here: the read loop in downloadToFile touches
    // m_cancelRequested once per 64KB block for the whole of a 49MB transfer,
    // so this worker is reading members continuously for tens of seconds rather
    // than for the few milliseconds a fetch spends outside WinHTTP.
    std::thread([this, self = shared_from_this(), filename, progressCb, completeCb]() {
        // One read of the download directory for the whole of this transfer.
        // Taken once rather than twice so the directory that is created and the
        // path the bytes are written to cannot be two different strings if the
        // Settings dialog moves the folder mid-download.
        const std::string downloadDir = getDownloadDirectory();

        // Create download directory if needed
        CreateDirectoryA(downloadDir.c_str(), nullptr);

        // Build URL
        std::wstring url = RELEASE_BASE_URL;
        for (char c : filename) {
            url += static_cast<wchar_t>(c);
        }

        std::string localPath = downloadDir + "\\" + filename;
        std::string error;

        bool success = downloadToFile(url, localPath, progressCb, error);

        if (m_cancelRequested) {
            m_downloadState = DownloadState::Cancelled;
            // Delete partial file
            DeleteFileA(localPath.c_str());
            if (completeCb) {
                completeCb(false, "Download cancelled");
            }
        } else if (success) {
            // What the catalog says these bytes should be. Empty when the entry
            // carries no <sha256>, which is a case that has to keep working:
            // an older catalog than this build expects must still install.
            std::string catalogHash;
            for (const auto& entry : getCatalogEntries()) {
                if (entry.filename == filename) {
                    catalogHash = entry.sha256;
                    break;
                }
            }

            std::string wanted;
            if (DiskLedger::normalizedHash(catalogHash, wanted)) {
                std::string actual;
                if (!diskhash::sha256File(localPath, actual)) {
                    m_downloadState = DownloadState::Failed;
                    DeleteFileA(localPath.c_str());
                    if (completeCb) {
                        completeCb(false, "Downloaded file could not be read back");
                    }
                    return;
                }
                if (actual != wanted) {
                    // The size check in downloadToFile passes on a transfer that
                    // is the right length and the wrong bytes; this does not.
                    // Keeping it would also poison the ledger, which records
                    // provenance only for a download it VERIFIED - a recorded
                    // hash these bytes never had would read as Current for ever.
                    m_downloadState = DownloadState::Failed;
                    DeleteFileA(localPath.c_str());
                    if (completeCb) {
                        completeCb(false, "Downloaded image does not match the catalog checksum");
                    }
                    return;
                }

                // Verified. This is the one moment provenance can honestly be
                // written: we know both which published image was asked for and
                // that the bytes on disk are it.
                loadLedgerIfNeeded();
                DiskFileFacts facts;
                bool haveFacts = diskhash::statFile(localPath, facts);
                DiskLedger updated;
                {
                    std::lock_guard<std::mutex> lock(m_ledgerMutex);
                    m_ledger.recordInstall(filename, wanted, haveFacts ? &facts : nullptr);
                    updated = m_ledger;
                }
                saveLedger(updated);
            }

            m_downloadState = DownloadState::Completed;
            updateDownloadedStatus();
            updateFreshness();
            if (completeCb) {
                completeCb(true, "");
            }
        } else {
            m_downloadState = DownloadState::Failed;
            // Delete partial file
            DeleteFileA(localPath.c_str());
            if (completeCb) {
                completeCb(false, error);
            }
        }
    }).detach();
}

void DiskCatalog::cancelDownload() {
    // A request, not a barrier: the worker still reaches its completeCb after
    // seeing this, and may not read it at all before the process dies.
    // DiskCatalog.h says what that rules the function out of, and names its one
    // caller.
    m_cancelRequested = true;
}

bool DiskCatalog::isDiskDownloaded(const std::string& filename) const {
    size_t expectedSize = 0;
    {
        std::lock_guard<std::mutex> lock(m_catalogMutex);
        for (const auto& entry : m_catalogEntries) {
            if (entry.filename == filename) {
                expectedSize = entry.size;
                break;
            }
        }
    }

    std::string path = getDiskPath(filename);
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) {
        return false;
    }
    ULONGLONG size = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;

    // A truncated cached download must not count as downloaded. When the
    // catalog knows the size, require at least that much (a cache from an
    // older release may legitimately be larger); otherwise settle for
    // non-empty.
    if (expectedSize > 0) {
        return size >= (ULONGLONG)expectedSize;
    }
    return size > 0;
}

bool DiskCatalog::deleteDownloadedDisk(const std::string& filename) {
    std::string path = getDiskPath(filename);
    if (DeleteFileA(path.c_str())) {
        {
            // Update status
            std::lock_guard<std::mutex> lock(m_catalogMutex);
            for (auto& entry : m_catalogEntries) {
                if (entry.filename == filename) {
                    entry.isDownloaded = false;
                    entry.freshness = DiskFreshness::NotInstalled;
                    break;
                }
            }
        }
        // The record described a file that is gone. Left behind it would be
        // claimed by the next file to take that name - a hand-copied image, or a
        // download this build did not verify - and describe bytes it has never
        // seen. Taken outside the catalog lock: saveLedger writes a file.
        DiskLedger updated;
        {
            std::lock_guard<std::mutex> lock(m_ledgerMutex);
            m_ledger.removeRecord(filename);
            updated = m_ledger;
        }
        saveLedger(updated);
        return true;
    }
    return false;
}

std::string DiskCatalog::getDiskPath(const std::string& filename) const {
    return getDownloadDirectory() + "\\" + filename;
}

bool DiskCatalog::downloadToString(const std::wstring& url, std::string& result, std::string& error) {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    bool success = false;

    // Parse URL
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp)) {
        error = "Invalid URL";
        return false;
    }

    // Open session
    hSession = WinHttpOpen(L"z80cpmw/" VERSION_STRING_W,
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        error = "Failed to open HTTP session";
        goto cleanup;
    }

    // Connect
    hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        error = "Failed to connect to server";
        goto cleanup;
    }

    // Open request
    hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                                   nullptr, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        error = "Failed to create request";
        goto cleanup;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = "Failed to send request";
        goto cleanup;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        error = "Failed to receive response";
        goto cleanup;
    }

    // Check for redirect (GitHub releases redirect)
    {
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           nullptr, &statusCode, &statusCodeSize, nullptr);

        if (statusCode >= 300 && statusCode < 400) {
            // Get redirect URL
            wchar_t redirectUrl[2048] = {};
            DWORD redirectSize = sizeof(redirectUrl);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, nullptr,
                                   redirectUrl, &redirectSize, nullptr)) {
                // Close current handles and follow redirect
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return downloadToString(redirectUrl, result, error);
            }
        }

        if (statusCode != 200) {
            error = "HTTP error: " + std::to_string(statusCode);
            goto cleanup;
        }
    }

    // Read data
    {
        std::stringstream ss;
        DWORD bytesAvailable = 0;
        DWORD bytesRead = 0;
        char buffer[8192];

        do {
            bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
                break;
            }

            if (bytesAvailable == 0) {
                break;
            }

            DWORD toRead = (bytesAvailable < sizeof(buffer)) ? bytesAvailable : (DWORD)sizeof(buffer);
            if (WinHttpReadData(hRequest, buffer, toRead, &bytesRead)) {
                ss.write(buffer, bytesRead);
            }
        } while (bytesAvailable > 0);

        result = ss.str();
        success = true;
    }

cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    return success;
}

bool DiskCatalog::downloadToFile(const std::wstring& url, const std::string& localPath,
                                  DownloadProgressCallback progressCb, std::string& error) {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    FILE* file = nullptr;
    bool success = false;
    size_t totalSize = 0;

    // Parse URL
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp)) {
        error = "Invalid URL";
        return false;
    }

    // Open session
    hSession = WinHttpOpen(L"z80cpmw/" VERSION_STRING_W,
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        error = "Failed to open HTTP session";
        goto cleanup;
    }

    // Connect
    hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        error = "Failed to connect to server";
        goto cleanup;
    }

    // Open request
    hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                                   nullptr, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        error = "Failed to create request";
        goto cleanup;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = "Failed to send request";
        goto cleanup;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        error = "Failed to receive response";
        goto cleanup;
    }

    // Check for redirect
    {
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           nullptr, &statusCode, &statusCodeSize, nullptr);

        if (statusCode >= 300 && statusCode < 400) {
            wchar_t redirectUrl[2048] = {};
            DWORD redirectSize = sizeof(redirectUrl);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, nullptr,
                                   redirectUrl, &redirectSize, nullptr)) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return downloadToFile(redirectUrl, localPath, progressCb, error);
            }
        }

        if (statusCode != 200) {
            error = "HTTP error: " + std::to_string(statusCode);
            goto cleanup;
        }
    }

    // Get content length
    {
        DWORD contentLength = 0;
        DWORD clSize = sizeof(contentLength);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                               nullptr, &contentLength, &clSize, nullptr)) {
            totalSize = contentLength;
        }
    }

    // Open output file
    file = fopen(localPath.c_str(), "wb");
    if (!file) {
        error = "Failed to create local file";
        goto cleanup;
    }

    // Read and write data. Every failure here must end as an error, never as
    // success: a short file reported as success gets cached and boots the
    // guest from a truncated image on every later run.
    {
        size_t bytesDownloaded = 0;
        DWORD bytesAvailable = 0;
        DWORD bytesRead = 0;
        char buffer[65536];

        do {
            if (m_cancelRequested) {
                error = "Cancelled";
                goto cleanup;
            }

            bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
                error = "Connection lost during download";
                goto cleanup;
            }

            if (bytesAvailable == 0) {
                break;
            }

            DWORD toRead = (bytesAvailable < sizeof(buffer)) ? bytesAvailable : (DWORD)sizeof(buffer);
            if (!WinHttpReadData(hRequest, buffer, toRead, &bytesRead)) {
                error = "Read failed during download";
                goto cleanup;
            }
            if (fwrite(buffer, 1, bytesRead, file) != bytesRead) {
                error = "Write failed (disk full?)";
                goto cleanup;
            }
            bytesDownloaded += bytesRead;

            if (progressCb) {
                progressCb(bytesDownloaded, totalSize);
            }
        } while (bytesAvailable > 0);

        if (bytesDownloaded == 0) {
            error = "Empty download";
            goto cleanup;
        }
        if (totalSize > 0 && bytesDownloaded != totalSize) {
            error = "Incomplete download (" + std::to_string(bytesDownloaded) +
                    " of " + std::to_string(totalSize) + " bytes)";
            goto cleanup;
        }

        success = true;
    }

cleanup:
    if (file) {
        fclose(file);
        // Never leave a partial file behind: any later existence check would
        // treat it as a completed download.
        if (!success) {
            remove(localPath.c_str());
        }
    }
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    return success;
}

bool DiskCatalog::parseCatalogXML(const std::string& xml, std::vector<DiskEntry>& entries, std::string& error) {
    entries.clear();

    // Simple XML parsing (no external dependencies)
    size_t pos = 0;
    while (true) {
        // Find next <disk> element
        size_t diskStart = xml.find("<disk>", pos);
        if (diskStart == std::string::npos) break;

        size_t diskEnd = xml.find("</disk>", diskStart);
        if (diskEnd == std::string::npos) break;

        std::string diskXml = xml.substr(diskStart, diskEnd - diskStart + 7);
        DiskEntry entry;

        // Extract fields
        auto extractField = [&diskXml](const std::string& tag) -> std::string {
            std::string openTag = "<" + tag + ">";
            std::string closeTag = "</" + tag + ">";
            size_t start = diskXml.find(openTag);
            if (start == std::string::npos) return "";
            start += openTag.length();
            size_t end = diskXml.find(closeTag, start);
            if (end == std::string::npos) return "";
            return diskXml.substr(start, end - start);
        };

        entry.filename = extractField("filename");
        entry.name = extractField("name");
        entry.description = extractField("description");
        entry.license = extractField("license");
        // Read but not validated here: an <sha256></sha256> and a missing
        // element both come back as the empty string, and telling a usable hash
        // from either is DiskLedger::normalizedHash's job, in one place.
        entry.sha256 = extractField("sha256");

        std::string sizeStr = extractField("size");
        if (!sizeStr.empty()) {
            entry.size = std::stoull(sizeStr);
        }

        if (!entry.filename.empty()) {
            entries.push_back(entry);
        }

        pos = diskEnd + 7;
    }

    if (entries.empty()) {
        error = "No disk entries found in catalog";
        return false;
    }

    return true;
}

void DiskCatalog::updateDownloadedStatus() {
    // Snapshot the names first: isDiskDownloaded takes the catalog lock
    // itself, so it must not be called while we hold it.
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(m_catalogMutex);
        names.reserve(m_catalogEntries.size());
        for (const auto& entry : m_catalogEntries) {
            names.push_back(entry.filename);
        }
    }

    for (const auto& name : names) {
        bool downloaded = isDiskDownloaded(name);
        std::lock_guard<std::mutex> lock(m_catalogMutex);
        for (auto& entry : m_catalogEntries) {
            if (entry.filename == name) {
                entry.isDownloaded = downloaded;
                break;
            }
        }
    }
}

//=============================================================================
// Provenance
//
// DiskLedger.h carries the reasoning; what is here is the I/O it deliberately
// does not have. The short version: a downloaded disk is a writable CP/M
// volume, so "its bytes differ from the catalog" is not evidence that it is
// stale - it is the normal state of a disk somebody has saved a file on. What
// decides staleness is which published image the bytes CAME FROM, and the only
// moment that can honestly be recorded is a download this code verified.
//=============================================================================

std::string DiskCatalog::ledgerPath() const {
    return getDownloadDirectory() + "\\disk_ledger.json";
}

void DiskCatalog::loadLedgerIfNeeded() {
    {
        std::lock_guard<std::mutex> lock(m_ledgerMutex);
        if (m_ledgerLoaded) return;
    }

    // Read outside the lock: this is a file, and the lock is taken by callers
    // that must not wait on one.
    std::string text;
    const std::string path = ledgerPath();
    std::unique_ptr<FILE, int (*)(FILE*)> file(fopen(path.c_str(), "rb"), &fclose);
    if (file) {
        char buffer[4096];
        size_t read = 0;
        while ((read = fread(buffer, 1, sizeof(buffer), file.get())) > 0) {
            text.append(buffer, read);
        }
    }

    DiskLedger loaded = DiskLedger::deserialize(text);

    std::lock_guard<std::mutex> lock(m_ledgerMutex);
    // Another worker may have got here first. Its copy is at least as new as
    // this one, and overwriting it would throw away a provenance record written
    // by a download that finished while this read was in flight.
    if (m_ledgerLoaded) return;
    m_ledger = loaded;
    m_ledgerLoaded = true;
}

void DiskCatalog::saveLedger(const DiskLedger& ledger) const {
    const std::string path = ledgerPath();
    const std::string text = ledger.serialize();

    // Written through a temporary and moved into place. A ledger truncated by a
    // crash mid-write deserialises to an empty one, which is safe by design -
    // but an empty one costs a re-hash of the whole library, and there is no
    // reason to accept that when a rename is atomic.
    const std::string temp = path + ".new";
    {
        std::unique_ptr<FILE, int (*)(FILE*)> file(fopen(temp.c_str(), "wb"), &fclose);
        if (!file) return;
        if (fwrite(text.data(), 1, text.size(), file.get()) != text.size()) return;
    }
    MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

void DiskCatalog::updateFreshness() {
    loadLedgerIfNeeded();

    // Everything below works on copies, and each lock is taken and released on
    // its own. Nothing here holds two at once - which is the rule that keeps
    // this out of the self-deadlock updateDownloadedStatus() had to be written
    // around.
    const std::vector<DiskEntry> entries = getCatalogEntries();
    DiskLedger ledger;
    {
        std::lock_guard<std::mutex> lock(m_ledgerMutex);
        ledger = m_ledger;
    }

    std::vector<std::pair<std::string, DiskFreshness>> verdicts;
    verdicts.reserve(entries.size());
    bool ledgerChanged = false;

    for (const auto& entry : entries) {
        const std::string path = getDiskPath(entry.filename);

        DiskFileFacts facts;
        const bool present = diskhash::statFile(path, facts);

        DiskFreshness verdict = ledger.freshness(entry.filename, entry.sha256,
                                                 present ? &facts : nullptr);

        if (verdict == DiskFreshness::NeedsMeasurement) {
            // The expensive branch, and the reason this function is
            // worker-thread-only. It is reached for a file installed before this
            // bookkeeping existed, and for one whose size or write time has
            // moved since we last looked - i.e. once per file that changed,
            // rather than once per fetch.
            std::string measured;
            if (diskhash::sha256File(path, measured)) {
                ledger.recordMeasurement(entry.filename, measured, facts);
                // An image that already hashes to the catalog is current
                // whoever downloaded it, so it can stop being provenance-less
                // and never has to be hashed again. Nineteen of the twenty.
                ledger.adoptProvenanceIfCurrent(entry.filename, entry.sha256);
                ledgerChanged = true;
                verdict = ledger.freshness(entry.filename, entry.sha256, &facts);
            } else {
                // Unreadable. Say nothing rather than guess; the next fetch
                // tries again.
                verdict = DiskFreshness::NeedsMeasurement;
            }
        }

        verdicts.emplace_back(entry.filename, verdict);
    }

    if (ledgerChanged) {
        DiskLedger toSave;
        {
            std::lock_guard<std::mutex> lock(m_ledgerMutex);
            // Merge rather than assign: a download that completed while this was
            // hashing has written a provenance record straight into m_ledger,
            // and the copy taken at the top of this function does not have it.
            for (const auto& [name, record] : ledger.records()) {
                if (!m_ledger.record(name)) {
                    m_ledger.setRecord(name, record);
                } else if (m_ledger.record(name)->installedCatalogSha256.empty() &&
                           !record.installedCatalogSha256.empty()) {
                    m_ledger.setRecord(name, record);
                } else if (!m_ledger.record(name)->hasMeasurement && record.hasMeasurement) {
                    DiskRecord merged = *m_ledger.record(name);
                    merged.hasMeasurement = true;
                    merged.measuredSha256 = record.measuredSha256;
                    merged.measuredSize = record.measuredSize;
                    merged.measuredModified = record.measuredModified;
                    m_ledger.setRecord(name, merged);
                }
            }
            toSave = m_ledger;
        }
        saveLedger(toSave);
    }

    std::lock_guard<std::mutex> lock(m_catalogMutex);
    for (const auto& [name, verdict] : verdicts) {
        for (auto& entry : m_catalogEntries) {
            if (entry.filename == name) {
                entry.freshness = verdict;
                break;
            }
        }
    }
}

DiskFreshness DiskCatalog::getFreshness(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(m_catalogMutex);
    for (const auto& entry : m_catalogEntries) {
        if (entry.filename == filename) return entry.freshness;
    }
    return DiskFreshness::NotInstalled;
}
