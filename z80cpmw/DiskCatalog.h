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
#include <mutex>

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

    // Fetch catalog from GitHub (async).
    //
    // CALLER'S CONTRACT, and it is the one that used to be unwritten and cost a
    // shipping crash: the callback runs LATER, on a DETACHED thread, and this
    // class will never tell you when. Nothing here can be cancelled or waited
    // for - see cancelDownload() for why that is on purpose - so a callback
    // that captures anything shorter-lived than this DiskCatalog has to carry
    // its own proof that the thing is still alive at the instant it is touched.
    // SettingsDialogPostGate is what the Settings dialog carries; a bare raw
    // pointer, or a weak_ptr locked before the call rather than around it, is
    // not enough.
    void fetchCatalog(CatalogLoadedCallback callback);

    // Download a disk image (async). Same contract as fetchCatalog, and it
    // binds harder: progressCb is called once per read block - at most one
    // 64KB buffer each - for the whole transfer, so a stale capture here is
    // touched over and over for as long as the download runs, where
    // fetchCatalog's is touched once at the end.
    void downloadDisk(const std::string& filename,
                      DownloadProgressCallback progressCb,
                      DownloadCompleteCallback completeCb);

    // Ask the in-flight disk download to stop early.
    //
    // NOT a lifetime tool, and deliberately not grown into one - three reasons,
    // in the order that decides it.
    //
    // It does not stop the callback, which is the only thing a dying caller
    // needs stopped. m_cancelRequested makes downloadToFile's read loop bail
    // out, and the worker then goes on to call completeCb with "Download
    // cancelled" - straight into the freed object, exactly the crash this was
    // supposed to prevent. Nor does it reach the path that actually crashed:
    // downloadToString, which is the whole of a catalog fetch, never reads
    // m_cancelRequested at all.
    //
    // Making it stop the callback means joining the worker, and the worker sits
    // in WinHTTP. Nothing in this file calls WinHttpSetTimeouts, so a join is
    // bounded only by WinHTTP's own defaults - tens of seconds per stage - with
    // the UI thread stopped inside it. Closing Settings on a dead network would
    // look like a hang, where the gate blocks it for one wxPostEvent.
    //
    // And the download is worth finishing. This object is MainWindow's
    // m_diskCatalog and outlives every dialog, so a disk the user asked for
    // still lands in the data folder after the dialog that asked for it has
    // gone; cancelling on close would throw away the user's own request to buy
    // nothing. So callers guard their own lifetime instead - see
    // SettingsDialogPostGate - and this stays what it is: a way for the user to
    // abandon a download, not a way for an object to survive one.
    void cancelDownload();

    // Check if a disk is already downloaded
    bool isDiskDownloaded(const std::string& filename) const;

    // Delete a downloaded disk
    bool deleteDownloadedDisk(const std::string& filename);

    // Get path to downloaded disk
    std::string getDiskPath(const std::string& filename) const;

    // Get current download state
    DownloadState getDownloadState() const { return m_downloadState; }

    // Get a snapshot of the cached catalog entries (copied under the catalog
    // lock; the fetchCatalog worker may reassign the vector at any time)
    std::vector<DiskEntry> getCatalogEntries() const {
        std::lock_guard<std::mutex> lock(m_catalogMutex);
        return m_catalogEntries;
    }

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
    // Guards m_catalogEntries: it is written by the fetchCatalog worker thread
    // and read from the UI thread and the download worker.
    mutable std::mutex m_catalogMutex;
    std::vector<DiskEntry> m_catalogEntries;
    std::atomic<DownloadState> m_downloadState{DownloadState::Idle};
    std::atomic<bool> m_cancelRequested{false};

    static const std::wstring CATALOG_URL;
    static const std::wstring RELEASE_BASE_URL;
};
