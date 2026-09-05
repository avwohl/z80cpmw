/*
 * DiskCatalog.h - Disk Image Catalog and Download Manager
 *
 * Fetches disk catalog from GitHub releases and manages downloads.
 */

#pragma once

#include "CatalogV0.h"
#include "DiskLedger.h"
#include "DiskMigrationV0.h"

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
    // The catalog's stable key, e.g. "hd1k_combo". CATALOG_SCHEMA.md 6.1 asks
    // that a client key on this and not on the filename or the array position,
    // and the reason is exactly this migration: the FILENAME changed under every
    // entry when the catalog moved to romwbw_disks, and it changes again for
    // every RomWBW release, while the id did not move at all. Empty only for a
    // catalog document that carried no id, which the parser drops.
    std::string id;
    std::string filename;
    // Where these bytes come from: base_url + filename, resolved when the
    // catalog was parsed.
    //
    // A resolved URL stored per entry, rather than a base URL member the
    // download worker re-reads. That is not tidiness. RELEASE_BASE_URL used to
    // be a compile-time constant and is now text out of a fetched document, so
    // the obvious translation - one std::string member written by the fetch
    // worker and read by the download worker - recreates precisely the
    // unsynchronised cross-thread string that m_downloadDir had to be fixed for
    // (see the note on m_downloadDirMutex). Here the URL travels inside
    // m_catalogEntries, which already has a mutex and is already copied out
    // whole before use, so the URL and the sha256 a download is checked against
    // come from ONE snapshot of ONE document and cannot be from two.
    std::string url;
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
    // WHAT IT FETCHES, WHICH IS NOW TWO DOCUMENTS AND NOT ONE. There is no
    // release tag in this class any more. The worker gets index-v0.json from
    // the single compiled-in catalogv0::INDEX_URL, keeps the RomWBW releases
    // this build's core says it can boot, picks one (see
    // setPreferredRomwbwVersion), verifies that entry's catalog against the
    // catalog_sha256 and catalog_size the index published for it, and reads
    // base_url, roms[] and disks[] out of it. Two round trips where there was
    // one, and three new ways to fail - each of which still ends in exactly one
    // callback(false, ...).
    void fetchCatalog(CatalogLoadedCallback callback);

    // Which RomWBW release to fetch the catalog for, e.g. "3.5.1".
    //
    // UI THREAD, and a PREFERENCE rather than a command: the next fetch honours
    // it only if the index still carries that version AND this build's core can
    // boot it, and otherwise falls back to the index's own default. So a user
    // who chose a release that has since been retired, or who downgraded the
    // app, gets a working catalog rather than none - and nothing is deleted
    // over it either way. Empty means "no preference", which is what a fresh
    // install and every configuration written before this release say.
    //
    // Changing it does NOT invalidate, delete or unmount anything. Switching
    // 3.5.1 -> 3.6.0 -> 3.5.1 must cost nothing, which is why the per-version
    // scoping the interface asks for lives in the FILENAMES here rather than in
    // a cache key: hd1k_combo-v0-3.5.1.img and hd1k_combo-v0-3.6.0.img are two
    // files that coexist in one folder, so there is never anything to throw
    // away when the choice moves.
    void setPreferredRomwbwVersion(const std::string& romwbwVersion);
    std::string getPreferredRomwbwVersion() const;

    // The RomWBW release the entries currently in hand were fetched for, or
    // empty before any successful fetch. This is the answer to "what am I
    // looking at", which the preference above is not: the two differ whenever
    // the preference could not be honoured.
    std::string getSelectedRomwbwVersion() const;

    // The index entries this build can actually boot, in index order, as of the
    // last successful fetch. Empty before one, and empty is also the real answer
    // when this core can run no release the repo publishes - fetchCatalog reports
    // that case as an error rather than quietly fetching something.
    std::vector<catalogv0::IndexEntry> getRunnableVersions() const;

    // The ROMs the selected catalog publishes, in document order.
    //
    // For a caller that wants to SAY something about them - the Settings dialog
    // names the count. A caller that wants the one to boot asks
    // getRomRequirement(), which applies catalogv0::chooseRom's rule rather than
    // taking an element of this.
    std::vector<catalogv0::RomItem> getCatalogRoms() const;

    // The ROM a machine on the selected release must boot, and everything
    // needed to get it and to check it.
    //
    // WHY THIS IS ONE STRUCT FROM ONE LOCK. The URL, the size and the sha256
    // are three facts about one file and they must come from one snapshot of
    // one document, for the reason DiskEntry::url is resolved when the catalog
    // is parsed rather than re-derived at download time: base_url stopped being
    // a compile-time constant and became text out of a fetched document, so
    // reading it separately from the entry it belongs to would let a ROM be
    // fetched from the base of a catalog it was not in - and then checked
    // against the hash of a third. Every field below is filled in a single
    // critical section.
    struct RomRequirement {
        // Whether a catalog has been fetched at all. False on a fresh launch,
        // and false is not an error: it means nothing has yet said which
        // release this machine is for, and the bundled ROM is what there is.
        bool haveCatalog = false;
        // Whether that catalog names a ROM. False when roms[] was absent or
        // empty - CATALOG_SCHEMA 6.1 allows both - which is a REPORTABLE
        // condition and not a reason to boot another release's ROM.
        bool haveRom = false;
        // The release the catalog in hand is for. This is the release the ROM
        // belongs to, taken from the same document, and not the user's
        // preference: the two differ whenever the preference could not be
        // honoured.
        std::string romwbwVersion;
        catalogv0::RomItem rom;
        // base_url + filename, resolved here so it cannot be built from
        // another catalog's base.
        std::string url;
        // Where it goes in the data folder, under the catalog's own filename -
        // which carries the interface and the release, so 3.5.1's ROM and
        // 3.6.0's coexist there exactly as their disks do.
        std::string localPath;
    };
    RomRequirement getRomRequirement() const;

    // Is the file at 'path' the ROM this entry describes? Size first, then
    // sha256; 'reason' names the failure in a sentence a user can act on.
    //
    // CALLED BEFORE EVERY LOAD, not only after a download, which is the whole
    // point of it being a separate function. A ROM is 512 KB and hashing it
    // costs a few milliseconds, and it is the one file in the data folder whose
    // corruption produces a machine that boots to nothing at all: no error, no
    // banner, a black screen and a status bar saying "Running". The disks get
    // the same treatment through DiskLedger, which can afford to remember a
    // measurement because a 49 MB image is expensive to re-read; this is cheap
    // enough that there is nothing to remember.
    //
    // It does NOT replace emu_validate_rom_hcb, which the load still runs over
    // the bytes it is given. A hash says these are the published bytes; the HCB
    // check says the core can run them. Neither answers the other's question.
    //
    // A catalog entry carrying no usable sha256 is REFUSED here, where a disk
    // in the same position is accepted: an unverifiable disk is a volume that
    // may be stale and the ledger goes on to say so per file, an unverifiable
    // ROM is fifteen banks of unknown bytes under a CPU.
    static bool verifyRom(const std::string& path, const catalogv0::RomItem& rom,
                          std::string& reason);

    // Fetch the selected release's ROM into the data folder (async).
    //
    // THE ONE TRANSFER IN THIS CLASS THAT SOMETHING WAITS ON, and the contract
    // is therefore stricter than fetchCatalog's and downloadDisk's. Those two
    // are documented above as fire-and-forget: nobody is blocked on them, a
    // disk that does not arrive is an empty drive. A ROM that does not arrive
    // is a machine that must not start, so completeCb is the thing a caller
    // gates the start on - it is called EXACTLY ONCE on every path, including
    // the paths that never open a socket, and it carries a sentence naming the
    // release, the file and the reason whenever it fails.
    //
    // Everything else about the worker is the same as downloadDisk's, and
    // deliberately so: a detached thread holding shared_from_this(), one
    // transfer at a time through m_downloadState, and a caller whose captures
    // are its own to keep alive (WorkerPostGate).
    //
    // IT NEVER OVERWRITES A FILE IT HAS NOT VERIFIED. The bytes land in
    // "<filename>.new" and are moved over the real name only once the size and
    // the sha256 both match, so a failed or corrupt transfer leaves whatever
    // was already there untouched - and this class gains no way to delete a
    // ROM a user put in that folder themselves.
    void downloadRom(DownloadProgressCallback progressCb,
                     DownloadCompleteCallback completeCb);

    // The catalog entry with this id, e.g. "hd1k_combo". False when no catalog
    // has been fetched or no entry carries the id - a version can publish an id
    // another does not (hd1k_ws4 is in 3.5.1 and not in 3.6.0), so a caller has
    // to be able to be told no.
    bool findDiskById(const std::string& id, DiskEntry& out) const;

    // Download a disk image (async). Same contract as fetchCatalog, and it
    // binds harder: progressCb is called once per read block - at most one
    // 64KB buffer each - for the whole transfer, so a stale capture here is
    // touched over and over for as long as the download runs, where
    // fetchCatalog's is touched once at the end.
    //
    // A CATALOG MUST HAVE BEEN FETCHED FIRST, and that is now enforced rather
    // than assumed. The URL and the expected sha256 both come out of the entry
    // this filename names, in one lookup of one snapshot, so a filename the
    // catalog does not carry fails immediately with a message saying so. It used
    // to build a URL from a compile-time base and look the hash up separately,
    // which meant the ordinary first-run path - MainWindow's
    // downloadAndStartWithDefaults, which never fetched a catalog - wrote two
    // 8-to-49 MB images with no hash check and no ledger record at all.
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

    // The name a catalog entry's file has IN THE DATA FOLDER, which was not
    // always the name the catalog gave it.
    //
    // IT IS NOW THE IDENTITY FUNCTION FOR EVERYTHING THE CATALOG SERVES, and
    // that is by design rather than by accident: it exists because the storage
    // migration renamed the images to their interface-v0 names in a release
    // where the catalog still called them hd1k_combo.img, so for one release
    // "hd1k_combo.img" and "hd1k_combo-v0-3.5.1.img" were two names for one
    // file. Everything local went through here - the path, the ledger key, the
    // stat behind isDiskDownloaded - so that the two could not drift apart and
    // report a library that was all missing. The catalog now serves v0 names and
    // v0NameFor() refuses a name that already carries the suffix, so it maps
    // nothing.
    //
    // What still reaches it with a pre-v0 name is MainWindow's Settings
    // write-back, which resolves a BARE NAME out of the dialog and can be handed
    // one from a configuration whose rename did not complete. It answers with
    // the v0 name there, finds no file, and leaves the slot alone - which is the
    // safe outcome and the same one it had before. It can be deleted; todo.txt
    // carries that.
    std::string getLocalName(const std::string& catalogFilename) const;

    // Get path to downloaded disk
    std::string getDiskPath(const std::string& filename) const;

    // What migrateFilesToInterfaceV0() did to the data folder.
    struct V0FileMigration {
        // What the config and the profiles may now be rewritten against - see
        // diskv0::LandedNames, which is where the rule lives.
        diskv0::LandedNames landed;
        int renamed = 0;
        int ledgerKeysMoved = 0;
        // One entry per image that should have been renamed and was not, with
        // the Win32 error, for a caller that wants to say so. A non-empty list
        // means the pass must run again on the next launch: it is idempotent, so
        // re-running costs nothing, and marking it done would strand the file
        // under its old name for ever.
        std::vector<std::string> failures;
    };

    // Rename every pre-v0 catalog image in the data folder onto its v0 name and
    // move its ledger record with it.
    //
    // UI THREAD, at startup, before the first fetchCatalog. That is not a
    // preference: the first fetch is started by the Settings dialog's
    // constructor, and by then isDiskDownloaded() and updateFreshness() are both
    // asking about v0 names. The cost is twenty GetFileAttributes calls and, if
    // anything moved, one read and one write of disk_ledger.json - which is what
    // loadLedgerIfNeeded() already does on the fetch worker, on a file of a few
    // hundred bytes.
    //
    // It renames only the twenty names diskv0::legacyCatalogFilenames() carries,
    // never the contents of the folder: R8 and W8 put the user's own host files
    // in there while the app runs.
    V0FileMigration migrateFilesToInterfaceV0();

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
    // How many 3xx hops to follow. A GitHub release asset is one redirect to
    // objects.githubusercontent.com, so five is four more than anything here
    // has ever needed; what it rules out is the loop.
    static const int MAX_REDIRECTS = 5;

    // The ceiling on a document read into memory by downloadToString. The
    // largest published catalog is 14,694 bytes, so this is three orders of
    // magnitude of headroom - it is not a size check, it is the difference
    // between a wrong URL costing an error and costing the process.
    static const size_t MAX_DOCUMENT_BYTES = 4 * 1024 * 1024;

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

    // Download a URL to a string (blocking).
    //
    // 'redirectsLeft' bounds the Location chase, which used to be an unbounded
    // recursion: a server answering 302 with its own URL was a stack overflow,
    // and this is now the transport for three URLs rather than one. The response
    // is capped too - see MAX_DOCUMENT_BYTES.
    bool downloadToString(const std::wstring& url, std::string& result, std::string& error,
                          int redirectsLeft = MAX_REDIRECTS);

    // Download a URL to a file (blocking, with progress). Same redirect bound,
    // for the same reason; no size cap, because the caller is asking for a file
    // whose size the catalog states and downloadToFile checks against
    // Content-Length.
    bool downloadToFile(const std::wstring& url, const std::string& localPath,
                        DownloadProgressCallback progressCb, std::string& error,
                        int redirectsLeft = MAX_REDIRECTS);

    // The whole of a catalog refresh - index, choice, catalog, cache, ledger -
    // as a function that can FAIL rather than a lambda body that can throw.
    // WORKER THREAD, called once by fetchCatalog inside its catch-all, so that
    // every early return there is an ordinary `return false` with an error
    // string and there is exactly one place the callback is invoked.
    bool fetchCatalogInto(std::string& error);

    // The whole of a ROM fetch, as a function that can FAIL rather than a
    // lambda body that can throw - the same split fetchCatalogInto exists for,
    // and it matters more here because a caller is gating the start of the
    // machine on the callback this wraps. WORKER THREAD, called once by
    // downloadRom inside its catch-all.
    bool downloadRomInto(DownloadProgressCallback progressCb, std::string& error);

    // Fetch and parse index-v0.json. WORKER THREAD.
    bool fetchIndex(std::vector<catalogv0::IndexEntry>& entries, std::string& error);

    // Fetch, VERIFY and parse one version's catalog. WORKER THREAD.
    //
    // Verified before it is parsed, against the catalog_size and catalog_sha256
    // the index carries for it, which is the whole reason those two fields are
    // in the index. A catalog is small - 11,826 bytes for 3.5.1 - and it is the
    // document that decides which 211 MB of images this app will fetch and what
    // it will check them against, so it is the one worth checking hardest.
    bool fetchVersionCatalog(const catalogv0::IndexEntry& entry,
                             catalogv0::Catalog& catalog, std::string& error);

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
    // Under m_catalogMutex with m_catalogEntries, and assigned in the same
    // critical section, because the two are one document: a caller that saw the
    // disks of 3.6.0 beside the ROMs of 3.5.1 would be reading a catalog that
    // never existed.
    std::vector<catalogv0::RomItem> m_catalogRoms;
    // The base_url and the romwbw_version of that same document, in that same
    // critical section, and for that same reason. A ROM entry carries no URL of
    // its own the way DiskEntry does - it is catalogv0::RomItem, the parser's
    // own type, shared with the Settings dialog - so the base it is joined to
    // has to travel beside it rather than being re-read from a member a later
    // fetch may already have replaced. getRomRequirement() does the join under
    // the lock and hands out the result.
    std::string m_catalogBaseUrl;
    std::string m_catalogRomwbwVersion;
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

    // Guards the three things the index decides. Its own mutex, and never held
    // across either of the others, for the reason m_downloadDirMutex is its own:
    // m_preferredVersion is written on the UI thread when the user chooses a
    // release, and all three are read and written by the fetch worker.
    mutable std::mutex m_indexMutex;
    std::string m_preferredVersion;
    std::string m_selectedVersion;
    std::vector<catalogv0::IndexEntry> m_runnableVersions;
};
