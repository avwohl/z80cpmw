/*
 * DiskCatalog.h - Disk Image Catalog and Download Manager
 *
 * Fetches disk catalog from GitHub releases and manages downloads.
 */

#pragma once

#include "DiskLedger.h"

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
    // The catalog's <sha256> for this image, exactly as published - or empty,
    // which is a real case the parser cannot distinguish from a missing element
    // and which DiskLedger::normalizedHash is the test for. Nothing compared
    // this before 1.0.25, which is why a repinned catalog could not be noticed:
    // hd1k_combo.img is 51,380,224 bytes at v1.4.5 and at v1.4.12 alike and
    // differs in 5,121 of them, so no size check can ever see the difference.
    std::string sha256;
    bool isDownloaded = false;
    // Whether the copy in the data folder is still the image the catalog names,
    // and what may be done about it if not. Filled in by the fetchCatalog
    // worker; DiskLedger.h has the whole of the reasoning.
    //
    // NeedsMeasurement is the right default and NotInstalled is not: this value
    // is read before any fetch has computed it, and "we have not looked yet" is
    // what is true then. It also makes the two callers correct without either of
    // them special-casing an uncomputed entry - describe() renders it as plain
    // "Downloaded", and action() asks for a measurement rather than offering
    // anything.
    DiskFreshness freshness = DiskFreshness::NeedsMeasurement;
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

// The permission a DiskCatalog worker thread needs before it may touch
// something shorter-lived than the DiskCatalog, and the thing that shorter-lived
// object revokes, as it dies, to shut those workers out for good.
//
// WHY THIS EXISTS. fetchCatalog and downloadDisk run their callbacks on a
// DETACHED std::thread. Shared ownership (see the note on DiskCatalog below)
// keeps the DiskCatalog itself alive under a worker, but it does nothing for
// whatever the CALLBACK closes over, and that is where both crashes were. The
// first was the Settings dialog, a stack object - ShowWxSettingsDialogInternal's
// "SettingsDialogWx dlg(nullptr, catalog)" - destroyed the moment ShowModal()
// returns, whose constructor starts a catalog fetch (the "Constructor: starting
// catalog refresh" trace), so closing Settings before the download landed left
// the worker calling wxPostEvent on a freed dialog. Measured, not deduced: every
// dump it produced was 0xC0000005 at the same address, z80cpmw.exe+0x5ABF3,
// reading garbage (0xFFFFFFFFFFFFFFFF, 0x40, 0x14DBC427666D) with the faulting
// stack running thread trampoline -> fetchCatalog's worker -> the std::function
// call. Note that a crash here need not exit the process: CrashHandler's report
// thread puts up a modal message box before it terminates, so "still running" is
// not the test - a new .dmp is.
//
// WHY A weak_ptr TO THE POSTED-TO OBJECT IS NOT ENOUGH, which is the whole
// reason this is a mutex and not an atomic flag. The obvious fix - hand the
// worker a weak_ptr, lock it, and post if the lock succeeds - leaves the crash
// in place with a smaller window: between "lock succeeded" and "the post ran",
// the UI thread can run the destructor. The shared_ptr the worker is holding
// keeps the CONTROL BLOCK alive, not the object, and it is the object that the
// post dereferences. Nothing in that scheme ever makes the destructor and the
// post exclude each other.
//
// WHICH WINDOW THIS CLOSES, AND HOW. postIfOpen() holds m_mutex ACROSS the post
// and close() takes the same mutex, so the two can never overlap. Once close()
// has returned, every later postIfOpen() finds m_open false and does nothing;
// and a post that had already begun completed before close() could acquire the
// lock, i.e. while the posted-to object was still whole. There is no third
// state, so "the destructor has returned" and "a worker may still post" are now
// mutually exclusive facts rather than a race with better odds. With the gate
// in, the driver that found the dialog crash ran 0/50/150/250/400/600/800ms with
// repeats - 144 open-close cycles - and wrote no dump at all, while the Settings
// dialog left open still reaches "Catalog loaded" with the catalog's twenty
// entries in the list.
//
// TWO HOLDERS, TWO POSTS, ONE REASON.
//  - SettingsDialogWx holds one for its wxPostEvent calls; ~SettingsDialogWx
//    closes it. That is the crash above.
//  - MainWindow holds one for the PostMessage in postToUiThread(), which is how
//    downloadAndStartWithDefaults' completion callbacks get back to the UI
//    thread; onDestroy() closes it, that being where the HWND dies. A bare HWND
//    captured by value would remove the use-after-free but not the whole
//    hazard, because window handles are recycled - IsWindow's own documentation
//    warns of exactly that - so a stale handle can name a window that is not
//    ours, and a WM_APP_RUN_ON_UI carrying a heap pointer would be delivered to
//    it. The gate is what makes that impossible rather than unlikely.
//
// LIFETIME. The gate outlives its holder because the callbacks hold shared_ptr
// copies of it and the holder holds one more; the last callback to be destroyed
// frees it.
//
// HOW LONG close() CAN BLOCK THE UI THREAD. Only for one post. For the dialog
// that is wxPostEvent, which wx/event.h defines as dest->AddPendingEvent(event)
// -> QueueEvent(Clone()): one heap clone appended to the handler's pending list,
// no I/O, no dialog code, no callback of ours. For MainWindow it is one
// PostMessage, which is asynchronous by definition. Neither waits for the
// download: a fetch with minutes left to run delays closing Settings, or
// quitting the app, by nothing. That is deliberate, and it is why the fix is not
// a cancel - see the comment on DiskCatalog::cancelDownload.
//
// The dialog's calls stayed wxPostEvent rather than becoming wxQueueEvent, and
// that was checked rather than assumed. wx/event.h says wxPostEvent is "not
// thread-safe, use wxQueueEvent()", and names the reason: Clone()
// shallow-copies wxString members, so a refcounted string buffer would end up
// shared between the worker that posts and the main thread that handles - and
// both of those events carry a wxString. It does not bite this build:
// wx/string.h typedefs wxStringImpl to wxStdString to std::wstring
// unconditionally ("All the symbols here only exist for compatibility"), and the
// MSVC std::wstring copy constructor deep-copies. wxQueueEvent would mean a heap
// event whose ownership the gate has to hand back and delete on the refused path
// - a second way to get lifetime wrong in the code whose whole job is to get
// lifetime right. If wxString ever goes back to copy-on-write under this
// toolchain, that trade flips.
//
// The callable handed to postIfOpen must therefore stay that cheap: it runs
// under the gate lock while the UI thread may be waiting in close(), so it must
// not call back into the posted-to object and must not block on the UI thread.
// Lock ordering is one-way for the same reason - a worker takes this mutex and
// then wx's pending-event locks or USER32's queue lock, while the UI thread
// holds only this one inside close() and has released it long before it goes
// near either.
class WorkerPostGate {
public:
    // UI thread, from the holder's destructor (or, for MainWindow, from
    // onDestroy) and nowhere else. Idempotent, so a second close is harmless.
    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = false;
    }

    // Worker thread. 'post' runs only while the posted-to object is provably
    // alive, and is not called at all once close() has returned.
    template <typename PostFn>
    void postIfOpen(PostFn&& post) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) return;
        post();
    }

private:
    std::mutex m_mutex;
    bool m_open = true;
};

// SHARED OWNERSHIP IS PART OF THIS CLASS'S CONTRACT, not a detail of how
// MainWindow happens to store it. A DiskCatalog must be owned by a shared_ptr -
// MainWindow's m_diskCatalog is a std::make_shared in its member-init list -
// because fetchCatalog and downloadDisk each hand their worker a
// shared_from_this(), and a DiskCatalog built any other way makes that call
// throw std::bad_weak_ptr on the first fetch.
//
// WHAT THAT BUYS, AND WHAT IT COST TO NOT HAVE IT. The workers run detached and
// nothing joins them, so before this the object could be destroyed underneath
// one: ~MainWindow freed m_diskCatalog and the worker read on regardless -
// m_cancelRequested and m_downloadState down the download path, m_catalogMutex
// and m_catalogEntries down the fetch path, m_downloadDir down both - all out of
// the freed block. Measured on the shipping code with a 4s sleep inserted after
// the release to widen the window: with a 49MB download running, the read loop
// in downloadToFile read m_cancelRequested back as raw byte 0xDD - the debug
// CRT's freed-memory fill - within 0-78ms of the release, took that for "true",
// and cancelled a download the user had asked for. The same window with a
// catalog fetch in flight put the worker into std::mutex::lock on the freed
// m_catalogMutex, where it never came out. And an isolated replay of that same
// tail against the same CRT faults outright on "m_catalogEntries = entries".
// Worst of the three is what the RELEASE CRT does, which is what ships. Rebuild
// that same replay /MD and free() no longer fills: the read of m_cancelRequested
// came back FALSE in 3/3 runs, so nothing bailed out, and the whole tail ran to
// completion - locking, assigning a vector and reading a download directory
// (back as a 15-character string, not the 45-character path that went in) out of
// a block the allocator had already handed to someone else. A hang and a fault
// at least stop. That one does not.
//
// WHY NOT JOIN IN THE DESTRUCTOR, the obvious alternative: the worker sits in
// WinHTTP and nothing in this file calls WinHttpSetTimeouts, so a join is
// bounded only by WinHTTP's own defaults - tens of seconds per stage - with the
// UI thread stopped inside it. Quitting on a dead network would hang. Shared
// ownership costs one atomic refcount and blocks nothing.
class DiskCatalog : public std::enable_shared_from_this<DiskCatalog> {
public:
    DiskCatalog();

    // MAY RUN ON A WORKER THREAD. Whichever reference is released last destroys
    // the object, and when the app quits with a transfer in flight that is the
    // worker's, not MainWindow's. That is safe because of what this destructor
    // and the implicit member destruction actually do: a string, a vector, a
    // mutex and two atomics destroyed, and nothing else - no window handle, no
    // wx, no COM, no UI. It also cannot race the worker that runs it: a
    // worker's shared_ptr lives in the lambda's captures, which the thread
    // trampoline destroys only after the lambda body has returned, so the last
    // reference is dropped by a worker that has already finished with the
    // object.
    ~DiskCatalog();

    // Set the local directory for downloaded disks
    void setDownloadDirectory(const std::string& dir);
    // Returns a copy, not a reference: the caller must not be handed a string
    // a worker can reassign under it.
    std::string getDownloadDirectory() const {
        std::lock_guard<std::mutex> lock(m_downloadDirMutex);
        return m_downloadDir;
    }

    // Fetch catalog from GitHub (async).
    //
    // CALLER'S CONTRACT, and it is the one that used to be unwritten and cost a
    // shipping crash: the callback runs LATER, on a DETACHED thread, and this
    // class will never tell you when. Nothing here can be cancelled or waited
    // for - see cancelDownload() for why that is on purpose. The worker holds a
    // shared_from_this(), so THIS OBJECT is guaranteed to still be there when
    // the callback runs; what is not guaranteed, and never can be from here, is
    // whatever the callback closes over. Anything shorter-lived than this
    // DiskCatalog has to carry its own proof that it is still alive at the
    // instant it is touched. WorkerPostGate is that proof; a bare raw pointer,
    // or a weak_ptr locked before the post rather than around it, is not.
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
    // STILL NOT A LIFETIME TOOL, and deliberately not grown into one. It does
    // not stop the callback: m_cancelRequested makes downloadToFile's read loop
    // bail out, and the worker then goes on to call completeCb with "Download
    // cancelled". Nor does it reach a catalog fetch at all - downloadToString,
    // which is the whole of one, never reads the flag. Keeping this object alive
    // under its workers is shared ownership's job, and keeping a CALLER alive is
    // WorkerPostGate's; neither is this function's.
    //
    // What it is for, and it now has exactly one caller: MainWindow::onDestroy
    // calls it when the window goes away. At that point the transfer cannot
    // finish - the process is a few tens of milliseconds from ExitProcess - and
    // the read loop seeing the flag is what gets downloadToFile to its cleanup
    // label, which closes the file and removes the partial image. Without it the
    // worker is killed mid-fwrite and a half-written .img is left in the data
    // folder, where diskFileLooksComplete's 1MB floor takes it for a finished
    // download on the next run and boots the guest from a truncated disk. Best
    // effort by nature: the worker may not get another loop iteration before the
    // process dies. It used to get this signal by accident, from ~DiskCatalog,
    // and read it back out of freed memory - which is why the debug build
    // cancelled (0xDD reads as true) and the release build did not (3/3 runs
    // read false and carried on into the freed block).
    //
    // The one thing it must NOT become is a way to WAIT. Making it stop the
    // callback means joining the worker, and the worker sits in WinHTTP.
    // Nothing in this file calls WinHttpSetTimeouts, so a join is bounded only
    // by WinHTTP's own defaults - tens of seconds per stage - with the UI thread
    // stopped inside it. Quitting, or closing Settings, on a dead network would
    // look like a hang, where the gate blocks it for one post.
    //
    // And short of shutdown the download is worth finishing. This object
    // outlives every dialog, so a disk the user asked for still lands in the
    // data folder after the dialog that asked for it has gone; cancelling on
    // dialog close would throw away the user's own request to buy nothing.
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

    // The ledger's verdict for one entry, as of the last fetch. Returns
    // NotInstalled for a name the catalog does not carry, which is also what a
    // caller with no catalog yet gets.
    DiskFreshness getFreshness(const std::string& filename) const;

private:
    // MUST RUN ON A WORKER THREAD, for the same reason diskhash::sha256File
    // does: this is what calls it, once per downloaded image whose measurement
    // has gone stale. Takes no lock across any other lock - it works on copies and
    // assigns the results back one lock at a time - because updateDownloadedStatus
    // reaching getDiskPath is exactly the shape that made m_downloadDirMutex a
    // separate mutex in the first place.
    void updateFreshness();

    std::string ledgerPath() const;
    void loadLedgerIfNeeded();
    void saveLedger(const DiskLedger& ledger) const;

    // Download a URL to a string (blocking)
    bool downloadToString(const std::wstring& url, std::string& result, std::string& error);

    // Download a URL to a file (blocking, with progress)
    bool downloadToFile(const std::wstring& url, const std::string& localPath,
                        DownloadProgressCallback progressCb, std::string& error);

    // Parse catalog XML
    bool parseCatalogXML(const std::string& xml, std::vector<DiskEntry>& entries, std::string& error);

    // Update downloaded status for all entries
    void updateDownloadedStatus();

    // Guards m_downloadDir. Written by setDownloadDirectory() from
    // loadSettings() at startup and by the Settings dialog, and read by both
    // workers (downloadDisk builds its local path from it, updateDownloadedStatus
    // stats every entry against it) and by getDownloadDirectory() on the UI
    // thread. It was unsynchronised, and safe only because of when it happened
    // to be written rather than because of anything the class guarantees - a
    // property of the call order, not of the type. Separate from m_catalogMutex
    // on purpose: updateDownloadedStatus() takes that one while holding this,
    // so sharing would self-deadlock.
    mutable std::mutex m_downloadDirMutex;
    std::string m_downloadDir;
    // Guards m_catalogEntries: it is written by the fetchCatalog worker thread
    // and read from the UI thread and the download worker.
    mutable std::mutex m_catalogMutex;
    std::vector<DiskEntry> m_catalogEntries;
    std::atomic<DownloadState> m_downloadState{DownloadState::Idle};
    std::atomic<bool> m_cancelRequested{false};

    // Guards m_ledger. Its own mutex, and never held while either of the two
    // above is: every user of it copies the ledger out, works on the copy, and
    // assigns it back, so there is no path on which two of these three locks are
    // held at once. m_ledgerLoaded exists so the file is read once rather than
    // on every fetch; setDownloadDirectory clears it, because a different data
    // folder is a different ledger.
    mutable std::mutex m_ledgerMutex;
    DiskLedger m_ledger;
    bool m_ledgerLoaded = false;

    static const std::wstring CATALOG_URL;
    static const std::wstring RELEASE_BASE_URL;
};
