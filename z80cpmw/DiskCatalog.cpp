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

// emu_romwbw_release_supported(), and nothing else from the core.
//
// Which RomWBW releases to OFFER is the core's answer and not this class's: a
// client can be built against a newer or an older core than it expects, so a
// hardcoded list here would be wrong in one direction the day romwbw_disks
// publishes a release the core has not been checked against, and wrong in the
// other the day the core gains one. Asking costs one call over an index entry
// this worker has already fetched, and costs no download at all.
#include "emu_init.h"

namespace {

// A URL for WinHTTP, which wants wide characters.
//
// This used to be a bare per-byte static_cast<wchar_t>, i.e. a Latin-1 widen,
// and it was safe only because both halves of the URL were ASCII literals in
// this file. Now the base comes out of a fetched document, so a byte above 0x7F
// is refused rather than reinterpreted: a URL that needs one is a URL that has
// not been percent-encoded, and silently producing a mangled host is a worse
// answer than saying no.
bool widenUrl(const std::string& url, std::wstring& out) {
    std::wstring wide;
    wide.reserve(url.size());
    for (unsigned char c : url) {
        if (c > 0x7F) return false;
        wide += static_cast<wchar_t>(c);
    }
    out = wide;
    return true;
}

}  // namespace

// THERE IS NO RELEASE TAG HERE ANY MORE, and that is the whole of this release.
//
// What used to be at the top of this file was `RELEASE_TAG = L"v1.4.12"` and two
// URLs interpolated out of it - one for `disks.xml` and one for the download
// base - with a comment describing which ioscpm release the disks were pinned to
// and what had changed in it. Repointing a client at a new set of images meant
// editing that constant, cutting a release, and hoping every copy of the story
// in every client's comments stayed true. The tag was also the only thing that
// said which RomWBW release the images were built for, so nothing could offer
// two, and a client could not tell a repin from a rebuild.
//
// The catalog now lives in `avwohl/romwbw_disks` as two documents, and this
// application compiles in exactly one URL - catalogv0::INDEX_URL, the index.
// Everything else is read out of a document that arrived over the network:
//
//   index-v0.json         which RomWBW releases exist, and for each, the version
//                         bytes a core needs to boot it, the absolute URL of its
//                         catalog, and that catalog's size and sha256
//   catalog-v0-<ver>.json base_url, roms[] and disks[] for one release
//
//   asset URL = base_url + filename          (base_url ends in "/"; nothing is
//                                             inserted between them)
//
// So a URL cannot be assembled here from a version string, which means it cannot
// be assembled WRONG here from a stale one. CatalogV0.h holds the parse and the
// choice of release; this file holds the transport, the cache and the ledger.
//
// One consequence worth naming because it is invisible from this file:
// tools/check-shipped-disks.sh matched the quoted vX.Y.Z above with a regex, so with
// the constant gone it prints NO PIN FOUND and exits 1 for a client that is
// working correctly. It is byte-identical across five repositories and is not
// this repository's to rewrite alone.

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

void DiskCatalog::setPreferredRomwbwVersion(const std::string& romwbwVersion) {
    std::lock_guard<std::mutex> lock(m_indexMutex);
    m_preferredVersion = romwbwVersion;
}

std::string DiskCatalog::getPreferredRomwbwVersion() const {
    std::lock_guard<std::mutex> lock(m_indexMutex);
    return m_preferredVersion;
}

std::string DiskCatalog::getSelectedRomwbwVersion() const {
    std::lock_guard<std::mutex> lock(m_indexMutex);
    return m_selectedVersion;
}

std::vector<catalogv0::IndexEntry> DiskCatalog::getRunnableVersions() const {
    std::lock_guard<std::mutex> lock(m_indexMutex);
    return m_runnableVersions;
}

std::vector<catalogv0::RomItem> DiskCatalog::getCatalogRoms() const {
    std::lock_guard<std::mutex> lock(m_catalogMutex);
    return m_catalogRoms;
}

DiskCatalog::RomRequirement DiskCatalog::getRomRequirement() const {
    RomRequirement req;
    std::string filename;
    {
        std::lock_guard<std::mutex> lock(m_catalogMutex);
        // The base URL is what says a catalog has been fetched: parseCatalog
        // refuses a document without one, so a non-empty base is proof that a
        // whole catalog landed and not that some field happened to be set.
        req.haveCatalog = !m_catalogBaseUrl.empty();
        req.romwbwVersion = m_catalogRomwbwVersion;
        const size_t pick = catalogv0::chooseRom(m_catalogRoms);
        if (req.haveCatalog && pick < m_catalogRoms.size()) {
            req.haveRom = true;
            req.rom = m_catalogRoms[pick];
            req.url = catalogv0::assetUrl(m_catalogBaseUrl, req.rom.filename);
            filename = req.rom.filename;
        }
    }
    // Outside the lock: getDownloadDirectory takes m_downloadDirMutex, and no
    // path in this class holds two of its three locks at once.
    if (!filename.empty()) req.localPath = getDownloadDirectory() + "\\" + filename;
    return req;
}

bool DiskCatalog::verifyRom(const std::string& path, const catalogv0::RomItem& rom,
                            std::string& reason) {
    DiskFileFacts facts;
    if (!diskhash::statFile(path, facts)) {
        reason = rom.filename + " is not in the data folder";
        return false;
    }

    // EXACT, not "at least", which is where this differs from a disk. A cached
    // image from an older release may legitimately be longer than the catalog
    // says and isDiskDownloaded settles for >=; a ROM is a fixed-size image
    // copied into fifteen banks and a byte either way is the wrong file. It is
    // also the check that catches the truncation the 1 MB completeness floor
    // guarding a cached disk cannot see - every published ROM is 512 KB, well
    // under it.
    if (rom.size > 0 && facts.size != rom.size) {
        reason = rom.filename + " is " + std::to_string(facts.size) +
                 " bytes, not the " + std::to_string(rom.size) +
                 " the catalog publishes";
        return false;
    }

    // normalizedHash rather than a string compare, so "the catalog carries no
    // usable hash" and "the hash does not match" stay the two different answers
    // they are - the single place that decides it for a disk image and for a
    // catalog document alike.
    std::string wanted;
    if (!DiskLedger::normalizedHash(rom.sha256, wanted)) {
        // REFUSED, where a disk in the same position is accepted. The two are
        // not the same risk: an unverifiable disk is a volume that may be
        // stale, and the ledger goes on to say so per file, while an
        // unverifiable ROM is fifteen banks of unknown bytes under a CPU. It
        // also cannot happen with any catalog this repository publishes -
        // tools/gen_catalog.py computes every sha256 from the built file - so
        // what this refuses is a document that has been tampered with or
        // truncated between the index's checksum check and here.
        reason = "the catalog publishes no checksum for " + rom.filename +
                 ", so it cannot be checked";
        return false;
    }
    std::string actual;
    if (!diskhash::sha256File(path, actual)) {
        reason = rom.filename + " could not be read back to check it";
        return false;
    }
    if (actual != wanted) {
        reason = rom.filename + " does not match the checksum the catalog publishes";
        return false;
    }
    return true;
}

void DiskCatalog::downloadRom(DownloadProgressCallback progressCb,
                              DownloadCompleteCallback completeCb) {
    // The same one-transfer-at-a-time guard downloadDisk uses, and the same
    // reason: two WinHTTP transfers writing the data folder at once is a
    // combination nothing here has ever been built for. The ROM fetch and the
    // disk fetches are chained through the UI thread by their callbacks, so in
    // ordinary use this never fires.
    if (m_downloadState == DownloadState::Downloading) {
        if (completeCb) completeCb(false, "Download already in progress");
        return;
    }

    m_downloadState = DownloadState::Downloading;
    m_cancelRequested = false;

    // THE SPAWN IS A PATH TOO, and this is the one transfer in the class where
    // that has to be said out loud. std::thread's constructor throws
    // std::system_error when the OS will not give it a thread, and here the
    // exception would leave a UI thread that is waiting to start the machine
    // holding a callback that can never arrive - and m_downloadState stuck at
    // Downloading, so every later transfer would be refused with "Download
    // already in progress". fetchCatalog and downloadDisk can let it propagate
    // because nothing is gated on them; the header promises completeCb exactly
    // once on every path, and this is the path that promise would otherwise
    // miss.
    try {
        std::thread([this, self = shared_from_this(), progressCb, completeCb]() {
            // The catch-all is here for the reason it is in fetchCatalog: this
            // is a DETACHED thread, so an exception leaving this lambda is
            // std::terminate with no dump, no message and no callback - and a
            // caller that is GATING THE START OF THE MACHINE on completeCb
            // would be left waiting for a callback that can never come. Every
            // failure below is an ordinary early return that calls completeCb
            // once.
            std::string error;
            bool ok = false;
            try {
                ok = downloadRomInto(progressCb, error);
            } catch (const std::exception& e) {
                error = std::string("The ROM could not be downloaded: ") + e.what();
            } catch (...) {
                error = "The ROM could not be downloaded";
            }

            // Cancelled is told apart from Failed for the reason downloadDisk
            // tells them apart: the flag is set by MainWindow::onDestroy and
            // nothing else, so "the app is closing" is not a transfer that went
            // wrong.
            m_downloadState = ok ? DownloadState::Completed
                                 : (m_cancelRequested ? DownloadState::Cancelled
                                                      : DownloadState::Failed);
            if (completeCb) completeCb(ok, ok ? std::string() : error);
        }).detach();
    } catch (const std::exception& e) {
        // No worker, so nothing else will ever answer for this call. Put the
        // state back first - a Downloading that no thread is going to clear
        // would lock out every later transfer for the life of the process.
        m_downloadState = DownloadState::Failed;
        if (completeCb) {
            completeCb(false, std::string("The ROM download could not be started: ") +
                              e.what());
        }
    }
}

bool DiskCatalog::downloadRomInto(DownloadProgressCallback progressCb, std::string& error) {
    const RomRequirement req = getRomRequirement();
    if (!req.haveCatalog) {
        error = "The disk catalog has not been loaded, so there is nowhere to "
                "download a ROM from";
        return false;
    }
    if (!req.haveRom) {
        error = "The catalog for RomWBW " + req.romwbwVersion + " publishes no ROM";
        return false;
    }

    std::wstring url;
    if (!widenUrl(req.url, url)) {
        error = "The catalog gives " + req.rom.filename +
                " a download address that cannot be used";
        return false;
    }

    const std::string downloadDir = getDownloadDirectory();
    CreateDirectoryA(downloadDir.c_str(), nullptr);

    // Downloaded beside the real name and moved onto it only after both checks
    // pass. Written this way round because the file it may be replacing is not
    // always ours: a ROM that fails verification is re-fetched once, and if the
    // fetch or the checks fail the copy already in the folder has to be exactly
    // where it was. It is also the shape saveLedger already uses for the same
    // reason - write beside, then one atomic rename.
    const std::string finalPath = req.localPath;
    const std::string tempPath = finalPath + ".new";

    if (!downloadToFile(url, tempPath, progressCb, error)) {
        DeleteFileA(tempPath.c_str());
        if (m_cancelRequested) error = "Download cancelled";
        return false;
    }
    if (m_cancelRequested) {
        DeleteFileA(tempPath.c_str());
        error = "Download cancelled";
        return false;
    }

    std::string reason;
    if (!verifyRom(tempPath, req.rom, reason)) {
        DeleteFileA(tempPath.c_str());
        // Named as what it is - the file that arrived, not the file that was
        // asked for - because the two are different objects here and the user's
        // copy is still on disk.
        error = "The downloaded " + req.rom.filename + " is not the published "
                "ROM: " + reason;
        return false;
    }

    if (!MoveFileExA(tempPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        const DWORD err = GetLastError();
        DeleteFileA(tempPath.c_str());
        error = "Could not put " + req.rom.filename + " into the data folder (error " +
                std::to_string(err) + ")";
        return false;
    }
    return true;
}

bool DiskCatalog::findDiskById(const std::string& id, DiskEntry& out) const {
    std::lock_guard<std::mutex> lock(m_catalogMutex);
    for (const auto& entry : m_catalogEntries) {
        if (entry.id == id) {
            out = entry;
            return true;
        }
    }
    return false;
}

bool DiskCatalog::fetchIndex(std::vector<catalogv0::IndexEntry>& entries, std::string& error) {
    std::wstring url;
    if (!widenUrl(catalogv0::INDEX_URL, url)) {
        error = "The disk catalog index URL is not a usable URL";
        return false;
    }

    std::string text;
    if (!downloadToString(url, text, error)) return false;
    return catalogv0::parseIndex(text, entries, error);
}

bool DiskCatalog::fetchVersionCatalog(const catalogv0::IndexEntry& entry,
                                      catalogv0::Catalog& catalog, std::string& error) {
    std::wstring url;
    if (!widenUrl(entry.catalogUrl, url)) {
        error = "RomWBW " + entry.romwbwVersion + " lists a catalog URL that cannot be used";
        return false;
    }

    std::string text;
    if (!downloadToString(url, text, error)) return false;

    // Size first, because it is free and because a mismatch here is the ordinary
    // way a truncated or intercepted response shows up. A zero in the index means
    // the document did not say, which is not the same as a document of length 0.
    if (entry.catalogSize > 0 && text.size() != entry.catalogSize) {
        error = "The RomWBW " + entry.romwbwVersion + " catalog is " +
                std::to_string(text.size()) + " bytes, not the " +
                std::to_string(entry.catalogSize) + " the index published";
        return false;
    }

    // normalizedHash rather than a string compare, so that "the index carried no
    // usable hash" and "the hash does not match" are the two different answers
    // they are - the same single place that decides it for a disk image.
    std::string wanted;
    if (DiskLedger::normalizedHash(entry.catalogSha256, wanted)) {
        std::string actual;
        if (!diskhash::sha256Bytes(text.data(), text.size(), actual)) {
            error = "The RomWBW " + entry.romwbwVersion + " catalog could not be checked";
            return false;
        }
        if (actual != wanted) {
            // Refused, not parsed. This document is what every later download's
            // URL and checksum come out of, so accepting one the index does not
            // vouch for would make every one of those checks worth nothing.
            error = "The RomWBW " + entry.romwbwVersion +
                    " catalog does not match the checksum the index published";
            return false;
        }
    }

    return catalogv0::parseCatalog(text, catalog, error);
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
        // THE CATCH-ALL IS LOAD-BEARING, and it is new with the JSON.
        //
        // This worker is detached, so an exception that leaves this lambda is
        // std::terminate: no dump, no message, no callback, the app simply
        // gone. That was already true of the XML parser this replaces - it
        // called std::stoull on a <size> element straight out of the response,
        // so one malformed catalog ended the process - and a JSON reader has
        // more ways to throw, not fewer. CatalogV0.cpp is written so that it
        // cannot throw on any document at all; this is here because "cannot" is
        // a claim about code that will be edited, and because std::bad_alloc
        // does not care how carefully the parse was written.
        std::string error;
        bool ok = false;
        try {
            ok = fetchCatalogInto(error);
        } catch (const std::exception& e) {
            error = std::string("The disk catalog could not be read: ") + e.what();
        } catch (...) {
            error = "The disk catalog could not be read";
        }

        // Exactly one callback on every path, which is the rule that keeps the
        // Settings dialog's Refresh button from being disabled for ever.
        if (!callback) return;
        if (!ok) {
            callback(false, {}, error);
            return;
        }
        callback(true, getCatalogEntries(), "");
    }).detach();
}

bool DiskCatalog::fetchCatalogInto(std::string& error) {
    std::vector<catalogv0::IndexEntry> index;
    if (!fetchIndex(index, error)) return false;

    // Ask the core, do not assume. emu_romwbw_release_supported answers from
    // ROMWBW_SUPPORTED_RELEASES in the linked romwbw_emu, so this build offers
    // what it can actually boot rather than what it was written expecting.
    const std::vector<size_t> runnable = catalogv0::runnableVersions(
        index, [](unsigned char ver, unsigned char upd) {
            emu_romwbw_release r;
            r.ver = ver;
            r.upd = upd;
            return emu_romwbw_release_supported(r);
        });

    if (runnable.empty()) {
        // A REAL condition and not a network failure, so it is reported as
        // itself. It means this build's emulator core can boot none of the
        // releases romwbw_disks publishes - the client is older or newer than
        // the repository - and the one thing that must not happen is a silent
        // fallback to some other release's images, which would download disks
        // this machine cannot boot and print an HBIOS/CBIOS version mismatch at
        // the user instead of an explanation.
        std::string offered;
        for (const auto& e : index) {
            if (!offered.empty()) offered += ", ";
            offered += e.romwbwVersion;
        }
        error = "This build cannot run any of the RomWBW releases the disk catalog "
                "offers (" + offered + "). It emulates " +
                std::string(emu_romwbw_supported_list()) + ".";
        return false;
    }

    const size_t chosen = catalogv0::chooseVersion(index, runnable, getPreferredRomwbwVersion());
    if (chosen >= index.size()) {
        error = "No usable RomWBW release in the disk catalog index";
        return false;
    }

    catalogv0::Catalog catalog;
    if (!fetchVersionCatalog(index[chosen], catalog, error)) return false;

    std::vector<DiskEntry> entries;
    entries.reserve(catalog.disks.size());
    for (const auto& disk : catalog.disks) {
        DiskEntry entry;
        entry.id = disk.id;
        entry.filename = disk.filename;
        entry.name = disk.name;
        entry.description = disk.description;
        entry.license = disk.license;
        entry.sha256 = disk.sha256;
        entry.size = static_cast<size_t>(disk.size);
        // Resolved here, once, from the base_url of the document these entries
        // came out of - so an entry can never be downloaded from the base of a
        // catalog it was not in.
        entry.url = catalogv0::assetUrl(catalog.baseUrl, disk.filename);
        entries.push_back(entry);
    }

    if (entries.empty()) {
        error = "No disk entries found in catalog";
        return false;
    }

    // NOTHING IS DELETED OR INVALIDATED HERE, and that is deliberate rather than
    // unfinished. `generation` is what the interface offers a client for
    // deciding that a version's artifacts have changed, and on iOS it is wired
    // to deleting the images the catalog names. This client has never had such a
    // wipe and must not gain one: DiskLedger answers the same question per file
    // and with evidence - it knows whether the copy on disk is still the one we
    // downloaded, and whether the user has since written to it - where a
    // generation compare knows only that something in the catalog moved. Nor is
    // there anything to invalidate on a version SWITCH: every v0 filename
    // carries its release, so 3.5.1 and 3.6.0 images coexist in one folder and
    // switching back and forth costs nothing. The value is parsed and reaches a
    // caller on the entries getRunnableVersions() returns; nothing in this
    // application acts on it, and nothing may be made to delete on it.
    {
        std::lock_guard<std::mutex> lock(m_catalogMutex);
        m_catalogEntries = entries;
        m_catalogRoms = catalog.roms;
        // In the same critical section as the two above, because the ROM's URL
        // is built from this base and checked against a hash out of these
        // roms[]: three facts about one document that must never be readable in
        // a mixture from two.
        m_catalogBaseUrl = catalog.baseUrl;
        // The INDEX entry's version, not the catalog document's own, though
        // tools/verify_catalog.py fails a release where the two disagree. Two
        // reasons to prefer it: parseIndex refuses an entry without one, so it
        // cannot be empty, and it is the same string assigned to
        // m_selectedVersion below - which makes "the release the catalog in
        // hand is for" one answer rather than two that could be compared
        // against each other and found different.
        m_catalogRomwbwVersion = index[chosen].romwbwVersion;
    }
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        m_selectedVersion = index[chosen].romwbwVersion;
        m_runnableVersions.clear();
        m_runnableVersions.reserve(runnable.size());
        for (size_t i : runnable) m_runnableVersions.push_back(index[i]);
    }

    updateDownloadedStatus();
    // Worker-thread only, and this is the worker. It reads up to 211MB when
    // a measurement has gone stale, which is why it is here and not in
    // updateDownloadedStatus() - that one is also called from
    // setDownloadDirectory() on the UI thread, and hashing the library
    // inside the Settings dialog's OK handler would freeze it.
    updateFreshness();
    return true;
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
        // ONE LOOKUP OF ONE SNAPSHOT, before anything is fetched, and both facts
        // come out of it: where these bytes live and what they must hash to.
        //
        // Splitting those two is what left the first-run path unverified. The
        // URL used to be built from a compile-time base, so a download could
        // start with no catalog at all, and the sha256 was looked up separately
        // AFTERWARDS - in an m_catalogEntries that was still empty, because
        // MainWindow::downloadAndStartWithDefaults never fetched a catalog. The
        // lookup found nothing, normalizedHash read the empty string as "this
        // catalog carries no hash", and two images totalling 57 MB were written
        // with no check and no ledger record on the most ordinary path in the
        // application. Now there is no URL without an entry, so there is no
        // download without the hash that goes with it.
        DiskEntry wantedEntry;
        bool haveEntry = false;
        {
            std::lock_guard<std::mutex> lock(m_catalogMutex);
            for (const auto& entry : m_catalogEntries) {
                if (entry.filename == filename) {
                    wantedEntry = entry;
                    haveEntry = true;
                    break;
                }
            }
        }
        if (!haveEntry || wantedEntry.url.empty()) {
            m_downloadState = DownloadState::Failed;
            if (completeCb) {
                completeCb(false, "The disk catalog has not been loaded, so there is "
                                  "nowhere to download " + filename + " from");
            }
            return;
        }

        std::wstring url;
        if (!widenUrl(wantedEntry.url, url)) {
            m_downloadState = DownloadState::Failed;
            if (completeCb) {
                completeCb(false, "The catalog gives " + filename + " a download "
                                  "address that cannot be used");
            }
            return;
        }

        // One read of the download directory for the whole of this transfer.
        // Taken once rather than twice so the directory that is created and the
        // path the bytes are written to cannot be two different strings if the
        // Settings dialog moves the folder mid-download.
        const std::string downloadDir = getDownloadDirectory();

        // Create download directory if needed
        CreateDirectoryA(downloadDir.c_str(), nullptr);

        // getLocalName rather than the catalog's own filename, which is the same
        // string now that the catalog serves v0 names and was not while the
        // storage migration had renamed the images and the catalog had not. Kept
        // as one call so that the path written to and the ledger key recorded
        // below cannot be derived differently from each other; see the note on
        // getLocalName for why it now maps nothing.
        const std::string localName = getLocalName(filename);
        std::string localPath = downloadDir + "\\" + localName;
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
            // What the catalog says these bytes should be, from the SAME entry
            // the URL came from. Empty when the entry carries no sha256, which
            // is a case that has to keep working: an older catalog than this
            // build expects must still install.
            std::string wanted;
            if (DiskLedger::normalizedHash(wantedEntry.sha256, wanted)) {
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
                    // Keyed on the LOCAL name, like every other ledger key:
                    // the record describes the file that is there.
                    m_ledger.recordInstall(localName, wanted, haveFacts ? &facts : nullptr);
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
            m_ledger.removeRecord(getLocalName(filename));
            updated = m_ledger;
        }
        saveLedger(updated);
        return true;
    }
    return false;
}

std::string DiskCatalog::getLocalName(const std::string& catalogFilename) const {
    std::string v0;
    return diskv0::v0NameFor(catalogFilename, v0) ? v0 : catalogFilename;
}

std::string DiskCatalog::getDiskPath(const std::string& filename) const {
    return getDownloadDirectory() + "\\" + getLocalName(filename);
}

DiskCatalog::V0FileMigration DiskCatalog::migrateFilesToInterfaceV0() {
    V0FileMigration result;

    // One read of the folder for the whole pass, and from getDownloadDirectory()
    // rather than from a fourth construction of %LOCALAPPDATA%\z80cpmw\data:
    // the folder renamed in has to be the folder the rest of the app reads.
    //
    // NOT getDiskPath(), which is the only place in this class that does not
    // want it. getDiskPath answers "where does this catalog entry live", and
    // that answer is already the v0 one - getDiskPath(legacy) and
    // getDiskPath(v0) name the same file, so a rename built from them would be
    // a no-op. This is the one piece of code that needs the two literal names.
    const std::string dir = getDownloadDirectory();

    for (const auto& legacy : diskv0::legacyCatalogFilenames()) {
        std::string v0;
        if (!diskv0::v0NameFor(legacy, v0)) continue;

        const std::string oldPath = dir + "\\" + legacy;
        const std::string newPath = dir + "\\" + v0;

        if (GetFileAttributesA(newPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            // Already there - the pass has run before, or the user fetched it
            // by hand. Keep it, and leave whatever is under the old name
            // exactly where it is: this migration deletes nothing.
            result.landed[DiskLedger::fold(legacy)] = v0;
            continue;
        }
        if (GetFileAttributesA(oldPath.c_str()) == INVALID_FILE_ATTRIBUTES) continue;

        // MoveFileExA with NO flags, and both halves of that are deliberate.
        // MOVEFILE_REPLACE_EXISTING would delete a file this code has just
        // established is not there and, if it appeared in between, is not ours
        // to remove. MOVEFILE_COPY_ALLOWED would let a failed rename become a
        // copy, which resets the write time and invalidates the cached
        // measurement - 211 MB of re-hashing to save one call - and cannot
        // happen anyway, since both names are in one directory and therefore on
        // one volume.
        if (MoveFileExA(oldPath.c_str(), newPath.c_str(), 0)) {
            result.landed[DiskLedger::fold(legacy)] = v0;
            result.renamed++;
        } else {
            result.failures.push_back(legacy + " (Win32 error " +
                                      std::to_string(GetLastError()) + ")");
        }
    }

    if (result.landed.empty()) return result;

    // The ledger key has to move with the file or the record is left describing
    // a name nothing will ask about again, and - as deleteDownloadedDisk's
    // comment says of the same shape - would be claimed by the next file to
    // take that name. Moving it carries the (size, mtime) the measurement was
    // taken against, which the rename did not change, so nineteen of the twenty
    // images come out Current against the v0 catalog instead of being re-hashed.
    loadLedgerIfNeeded();
    DiskLedger updated;
    {
        std::lock_guard<std::mutex> lock(m_ledgerMutex);
        result.ledgerKeysMoved = diskv0::migrateLedgerKeys(m_ledger, result.landed);
        updated = m_ledger;
    }
    if (result.ledgerKeysMoved > 0) saveLedger(updated);

    return result;
}

bool DiskCatalog::downloadToString(const std::wstring& url, std::string& result,
                                   std::string& error, int redirectsLeft) {
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
                // Bounded, because this recursion used to have no bound at all:
                // a server answering its own URL with a 302 recursed until the
                // stack ran out, on a worker thread with nothing to catch it.
                // It never bit while there was one URL and it pointed at GitHub;
                // this release makes it three, two of them read out of documents.
                if (redirectsLeft <= 0) {
                    error = "Too many redirects fetching the disk catalog";
                    goto cleanup;
                }
                // Close current handles and follow redirect
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return downloadToString(redirectUrl, result, error, redirectsLeft - 1);
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
        size_t total = 0;
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
                total += bytesRead;
                // Capped, because this reads into memory with no idea what it
                // asked for. The documents it fetches are 3 KB and 15 KB; a URL
                // that answers with a disk image, or with a stream that does not
                // end, must cost an error rather than the machine's memory. It
                // is a refusal and not a truncation - half a catalog parsed as a
                // whole one is the worst of the three outcomes.
                if (total > MAX_DOCUMENT_BYTES) {
                    error = "The disk catalog is far larger than a catalog can be";
                    goto cleanup;
                }
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
                                  DownloadProgressCallback progressCb, std::string& error,
                                  int redirectsLeft) {
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
                // Bounded for the same reason downloadToString's is; a release
                // asset is one hop to objects.githubusercontent.com.
                if (redirectsLeft <= 0) {
                    error = "Too many redirects downloading the disk image";
                    goto cleanup;
                }
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return downloadToFile(redirectUrl, localPath, progressCb, error,
                                      redirectsLeft - 1);
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

// The hand-rolled XML parser that used to be here is gone with the URL it was
// fed by. It read six elements out of <disk> blocks with substring searches, and
// its last line was `entry.size = std::stoull(sizeStr)` - an unguarded throw on
// a detached thread, so a served catalog with a non-numeric <size> ended the
// process. The interface still publishes disks-v0-<ver>.xml in exactly that
// shape, precisely so a client could move to the new URLs before learning the
// JSON; this build does both at once and reads the JSON, which carries the `id`
// that CATALOG_SCHEMA.md 6.1 asks a client to key on and the roms[] the XML has
// no room for. The replacement is catalogv0::parseCatalog, in a file with no
// WinHTTP and no threads so that a test can drive it against the real published
// documents.

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
        // The ledger and the path are about the FILE; the verdict is written
        // back against the CATALOG entry. They are the same string now that the
        // catalog serves v0 names, and they were not for one release - mixing
        // them up then was what would have made a migrated library read as
        // twenty unmeasured files and re-hash 211 MB.
        const std::string localName = getLocalName(entry.filename);
        const std::string path = getDiskPath(entry.filename);

        DiskFileFacts facts;
        const bool present = diskhash::statFile(path, facts);

        DiskFreshness verdict = ledger.freshness(localName, entry.sha256,
                                                 present ? &facts : nullptr);

        if (verdict == DiskFreshness::NeedsMeasurement) {
            // The expensive branch, and the reason this function is
            // worker-thread-only. It is reached for a file installed before this
            // bookkeeping existed, and for one whose size or write time has
            // moved since we last looked - i.e. once per file that changed,
            // rather than once per fetch.
            std::string measured;
            if (diskhash::sha256File(path, measured)) {
                ledger.recordMeasurement(localName, measured, facts);
                // An image that already hashes to the catalog is current
                // whoever downloaded it, so it can stop being provenance-less
                // and never has to be hashed again. Nineteen of the twenty.
                ledger.adoptProvenanceIfCurrent(localName, entry.sha256);
                ledgerChanged = true;
                verdict = ledger.freshness(localName, entry.sha256, &facts);
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
