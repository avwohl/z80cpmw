/*
 * DiskCatalog.h - Disk Image Catalog and Download Manager
 *
 * Fetches disk catalog from GitHub releases and manages downloads.
 */

#pragma once

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

#pragma comment(lib, "winhttp.lib")

// Disk entry from catalog
struct DiskEntry {
    std::string filename;
    std::string name;
    std::string description;
    size_t size = 0;
    std::string license;
    bool isDownloaded = false;
};

// Download state
enum class DownloadState {
    Idle,
    Downloading,
    Completed,
    Failed,
    Cancelled
};

// Progress callback: (bytesDownloaded, totalBytes)
using DownloadProgressCallback = std::function<void(size_t, size_t)>;

// Completion callback: (success, errorMessage)
using DownloadCompleteCallback = std::function<void(bool, const std::string&)>;

// Catalog loaded callback: (success, entries, errorMessage)
using CatalogLoadedCallback = std::function<void(bool, const std::vector<DiskEntry>&, const std::string&)>;

class DiskCatalog {
public:
    DiskCatalog();
    ~DiskCatalog();

    // Set the local directory for downloaded disks
    void setDownloadDirectory(const std::string& dir);
    std::string getDownloadDirectory() const { return m_downloadDir; }

    // Fetch catalog from GitHub (async)
    void fetchCatalog(CatalogLoadedCallback callback);

    // Download a disk image (async)
    void downloadDisk(const std::string& filename,
                      DownloadProgressCallback progressCb,
                      DownloadCompleteCallback completeCb);

    // Cancel current download
    void cancelDownload();

    // Check if a disk is already downloaded
    bool isDiskDownloaded(const std::string& filename) const;

    // Delete a downloaded disk
    bool deleteDownloadedDisk(const std::string& filename);

    // Get path to downloaded disk
    std::string getDiskPath(const std::string& filename) const;

    // Get current download state
    DownloadState getDownloadState() const { return m_downloadState; }

    // Get cached catalog entries
    const std::vector<DiskEntry>& getCatalogEntries() const { return m_catalogEntries; }

private:
    // Download a URL to a string (blocking)
    bool downloadToString(const std::wstring& url, std::string& result, std::string& error);

    // Download a URL to a file (blocking, with progress)
    bool downloadToFile(const std::wstring& url, const std::string& localPath,
                        DownloadProgressCallback progressCb, std::string& error);

    // Parse catalog XML
    bool parseCatalogXML(const std::string& xml, std::vector<DiskEntry>& entries, std::string& error);

    // Update downloaded status for all entries
    void updateDownloadedStatus();

    std::string m_downloadDir;
    std::vector<DiskEntry> m_catalogEntries;
    std::atomic<DownloadState> m_downloadState{DownloadState::Idle};
    std::atomic<bool> m_cancelRequested{false};

    static const std::wstring CATALOG_URL;
    static const std::wstring RELEASE_BASE_URL;
};
