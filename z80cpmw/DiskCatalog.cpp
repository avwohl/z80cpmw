/*
 * DiskCatalog.cpp - Disk Image Catalog and Download Manager Implementation
 */

#include "pch.h"
#include "DiskCatalog.h"
#include <thread>
#include <sstream>

const std::wstring DiskCatalog::CATALOG_URL =
    L"https://github.com/avwohl/ioscpm/releases/latest/download/disks.xml";
const std::wstring DiskCatalog::RELEASE_BASE_URL =
    L"https://github.com/avwohl/ioscpm/releases/latest/download/";

DiskCatalog::DiskCatalog() {
    // Default to app directory\disks
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) *lastSlash = '\0';
    m_downloadDir = std::string(path) + "\\disks";
}

DiskCatalog::~DiskCatalog() {
    cancelDownload();
}

void DiskCatalog::setDownloadDirectory(const std::string& dir) {
    m_downloadDir = dir;
    // Create directory if it doesn't exist
    CreateDirectoryA(dir.c_str(), nullptr);
    updateDownloadedStatus();
}

void DiskCatalog::fetchCatalog(CatalogLoadedCallback callback) {
    // Run in background thread
    std::thread([this, callback]() {
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

        m_catalogEntries = entries;
        updateDownloadedStatus();

        if (callback) {
            callback(true, m_catalogEntries, "");
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

    // Run in background thread
    std::thread([this, filename, progressCb, completeCb]() {
        // Create download directory if needed
        CreateDirectoryA(m_downloadDir.c_str(), nullptr);

        // Build URL
        std::wstring url = RELEASE_BASE_URL;
        for (char c : filename) {
            url += static_cast<wchar_t>(c);
        }

        std::string localPath = m_downloadDir + "\\" + filename;
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
    m_cancelRequested = true;
}

bool DiskCatalog::isDiskDownloaded(const std::string& filename) const {
    std::string path = getDiskPath(filename);
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool DiskCatalog::deleteDownloadedDisk(const std::string& filename) {
    std::string path = getDiskPath(filename);
    if (DeleteFileA(path.c_str())) {
        // Update status
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
    return m_downloadDir + "\\" + filename;
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
    hSession = WinHttpOpen(L"z80cpmw/1.0",
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
    hSession = WinHttpOpen(L"z80cpmw/1.0",
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

    // Read and write data
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
                break;
            }

            if (bytesAvailable == 0) {
                break;
            }

            DWORD toRead = (bytesAvailable < sizeof(buffer)) ? bytesAvailable : (DWORD)sizeof(buffer);
            if (WinHttpReadData(hRequest, buffer, toRead, &bytesRead)) {
                fwrite(buffer, 1, bytesRead, file);
                bytesDownloaded += bytesRead;

                if (progressCb) {
                    progressCb(bytesDownloaded, totalSize);
                }
            }
        } while (bytesAvailable > 0 && !m_cancelRequested);

        success = !m_cancelRequested;
    }

cleanup:
    if (file) fclose(file);
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
    for (auto& entry : m_catalogEntries) {
        entry.isDownloaded = isDiskDownloaded(entry.filename);
    }
}
