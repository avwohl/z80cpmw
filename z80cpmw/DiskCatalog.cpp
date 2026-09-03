/*
 * DiskCatalog.cpp - Disk Image Catalog and Download Manager Implementation
 */

#include "pch.h"
#include "DiskCatalog.h"
#include "Version.h"
#include <thread>
#include <sstream>

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
            m_downloadState = DownloadState::Completed;
            updateDownloadedStatus();
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
        // Update status
        std::lock_guard<std::mutex> lock(m_catalogMutex);
        for (auto& entry : m_catalogEntries) {
            if (entry.filename == filename) {
                entry.isDownloaded = false;
                break;
            }
        }
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
