/*
 * CatalogV0.h - The two interface-v0 catalog documents, and which RomWBW
 * release this build fetches one for.
 *
 * The catalog stopped being one `disks.xml` on an ioscpm release tag and became
 * two documents in `avwohl/romwbw_disks`:
 *
 *   index-v0.json          one compiled-in URL, lists the RomWBW releases
 *   catalog-v0-<ver>.json  one per release, holds base_url, roms[] and disks[]
 *
 * So there is no tag to interpolate any more and nothing builds an asset URL
 * out of a version string. The index is the only URL in the binary; every other
 * URL is read out of a document - the chosen entry's `catalog_url`, and then
 * `base_url + filename` for each asset. That is the whole of the change, and it
 * is why `RELEASE_TAG` is deleted rather than repointed: a constant that names a
 * tag can go stale silently, where a document that names its own base cannot.
 *
 * ## Why this is a file of its own, with no Win32, no WinHTTP and no threads
 *
 * The same reason DiskLedger.cpp and DiskMigrationV0.cpp have none. Two of them:
 *
 * TESTABILITY. DiskCatalog.cpp cannot be linked by a test - it owns WinHTTP and
 * a detached thread - so anything decided inside it is checked by running the
 * app against the live network. Everything here is a pure function of a string,
 * so tests/test_catalogv0.cpp drives it against the real published documents.
 *
 * AND BECAUSE THE PARSE RUNS SOMEWHERE NOTHING CAN CATCH. DiskCatalog's fetch
 * happens on a DETACHED thread, and DiskCatalog.cpp holds no try/catch at all;
 * the XML parser this replaces called `std::stoull` on a `<size>` element
 * straight out of the response, so a malformed catalog was std::terminate with
 * no dump path and no callback. Every accessor below is non-throwing by
 * construction - a value of the wrong type reads as absent, exactly as an
 * unknown field is ignored - so a hostile or truncated document produces a
 * parse failure rather than a dead process. DiskCatalog wraps the worker in a
 * catch-all as well; two independent guards, because only one of them can be
 * checked here.
 *
 * ## What CATALOG_SCHEMA.md 6.1 requires of a reader, and where each is met
 *
 *   ignore unknown fields, everywhere    nothing is round-tripped and no key is
 *                                        rejected; parse* reads what it knows
 *   key on `id`, never on position       DiskItem::id / RomItem::id; nothing
 *                                        here indexes roms[] or disks[]
 *   entries appearing and disappearing   hd1k_ws4 is in 3.5.1 and not in 3.6.0,
 *                                        and roms[] may be empty
 *   do not assume `emu_avw` is present   nothing looks for it by name
 *   optional/nullable fields             `slices`, `defaultSlot`, `cbios`,
 *                                        `released`, `package_sha256`
 *   new `status` / `license` values      free text, displayed, never switched on
 *   `generation` jumping by more than 1  compared, never computed on
 *   `base_url` ends with "/"             assetUrl() concatenates and inserts
 *                                        nothing - see the note on it
 *   fall back sanely on zero or two
 *   `default: true` entries              chooseVersion(), which never requires
 *                                        one to exist
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace catalogv0 {

// The interface these documents describe, and the one URL this application
// compiles in. Everything else is read out of a document.
extern const char* const INTERFACE;
extern const char* const INDEX_URL;

// One entry of index-v0.json's `romwbw_versions[]`.
//
// Only what a client acts on is kept. `released`, `disks_xml_url`, `notes` and
// `repo` are read by nobody here and are therefore not stored - which is not the
// same as failing on them, and is the ordinary case the "ignore unknown fields"
// rule describes from the other side.
struct IndexEntry {
    std::string romwbwVersion;   // "3.5.1" - the <ver> in every asset name
    std::string label;           // "RomWBW 3.5.1", for a menu. Never parsed.
    std::string status;          // "stable" / "preview" today, and NOT a closed set
    bool isDefault = false;      // the index promises exactly one, but see chooseVersion
    std::string catalogUrl;      // absolute already; never built from the tag
    std::string catalogSha256;   // of the document at catalogUrl
    unsigned long long catalogSize = 0;
    long long generation = 0;    // compared, never computed on. See the note in DiskCatalog.h
    int romCount = 0;
    int diskCount = 0;

    // hbios.ver_byte / upd_byte, parsed from the hex STRINGS "0x35" / "0x10".
    // haveHbios is false when either is missing or unreadable, and an entry
    // without them can never be offered: the pair is the whole of what decides
    // whether this build's core can boot the release.
    bool haveHbios = false;
    unsigned char verByte = 0;
    unsigned char updByte = 0;
};

// One entry of a catalog's `roms[]`.
//
// Parsed, and deliberately not downloaded - see DiskCatalog::getCatalogRoms()
// for why this build still boots its bundled ROM. Reading them costs nothing and
// is what lets the Settings dialog say so out loud rather than leaving the user
// to wonder why the catalog's ROMs never appear.
struct RomItem {
    std::string id;              // "emu_avw". The key. Never assumed present.
    std::string filename;        // append to base_url
    std::string name;
    std::string description;
    unsigned long long size = 0; // 524288 for both published ROMs; nothing promises it
    std::string sha256;
    bool isDefault = false;
    // hcb.version / hcb.update, the two bytes emu_validate_rom_hcb reads back
    // out of the image at 0x105/0x106. Checking them here rejects a ROM this
    // core could not run before spending 512 KB on it.
    bool haveHcb = false;
    unsigned char hcbVersion = 0;
    unsigned char hcbUpdate = 0;
};

// One entry of a catalog's `disks[]`. `format`, `bootable`, `hostTransfer` and
// `defaultSlot` are carried because the catalog states them and a client that
// guesses them gets them wrong - hd1k_combo is the only image with R8/W8 on it.
struct DiskItem {
    std::string id;              // "hd1k_combo". The key.
    std::string filename;
    std::string name;
    std::string description;
    unsigned long long size = 0;
    std::string sha256;
    std::string license;         // free text: Mixed, Abandonware, Freeware, Open Source
    std::string format;          // "hd1k" or "hd1k_combo"
    bool bootable = false;
    bool hostTransfer = false;
    bool haveDefaultSlot = false;
    int defaultSlot = 0;
};

// A whole catalog-v0-<ver>.json.
struct Catalog {
    std::string romwbwVersion;
    std::string status;
    std::string releaseTag;
    std::string baseUrl;         // ends with "/" - see assetUrl()
    long long generation = 0;
    std::vector<RomItem> roms;
    std::vector<DiskItem> disks;
};

// "0x35" -> 0x35. False, leaving `out` untouched, for anything else.
//
// The index writes these two as hex STRINGS while `major`/`minor`/`update`/
// `patch` beside them are integers, and `hcb.platform` is an integer while
// `hcb.version` next to it is a string. CATALOG_SCHEMA.md says of that "the
// asymmetry is real; do not assume a uniform encoding", so it is read as what it
// is rather than through a number accessor that would silently yield 0 - and 0
// is a plausible-looking upd_byte, which is what makes a silent 0 dangerous
// here rather than merely wrong.
bool parseHexByte(const std::string& text, unsigned char& out);

// index-v0.json -> the entries, in document order.
//
// False, with `error` set, when the text will not parse, is not an object, or
// carries no usable `romwbw_versions[]` entry. An entry missing the fields a
// client must have - the version, the catalog URL, the two version bytes - is
// SKIPPED rather than failing the document: a future index that adds an entry
// shaped differently must not take the ones this build understands with it.
bool parseIndex(const std::string& text, std::vector<IndexEntry>& out, std::string& error);

// catalog-v0-<ver>.json -> the document. False, with `error` set, when it will
// not parse or carries no `base_url` - without which no asset URL exists and
// there is nothing this catalog could be used for. An empty `disks[]` is a
// real answer and not an error.
bool parseCatalog(const std::string& text, Catalog& out, std::string& error);

// base_url + filename, with NOTHING between them.
//
// A function rather than a `+` so the rule has one home and one test. `base_url`
// ends with "/" in the document (tools/gen_catalog.py:125), which is the point
// of the field: the three clients disagreed about the separator - iOS's base had
// no trailing slash and its parser appended "/" - and v0 settles it in the
// document instead of in each client. So this inserts no separator and does not
// "fix" a base that lacks one; a base_url without its slash is a broken document
// and had better produce a URL that visibly fails rather than one that quietly
// works here and nowhere else.
std::string assetUrl(const std::string& baseUrl, const std::string& filename);

// Whether this build's emulator core can boot a release, as {ver_byte, upd_byte}.
// A std::function so that emu_init.h stays out of this file: the answer belongs
// to the core, and the core is what DiskCatalog links.
using ReleaseSupported = std::function<bool(unsigned char ver, unsigned char upd)>;

// The index entries this build can actually run, as positions into `entries`, in
// index order.
//
// ASK, do not assume, and do not hardcode. A client can be built against a newer
// or an older core than it expects - this tree already links romwbw_emu's
// runtime release API while the shipped build lags it - so "offer everything" is
// wrong the moment the repo publishes a release the core has not been checked
// against, and "offer the one I was compiled for" is wrong the moment the core
// gains one. An entry with no readable hbios pair can never be run and never
// survives.
std::vector<size_t> runnableVersions(const std::vector<IndexEntry>& entries,
                                     const ReleaseSupported& supported);

// Which of the survivors to fetch: the user's own choice if it is still one of
// them, else the entry marked `default: true`, else the first.
//
// Returns npos when `runnable` is empty, which is a REPORTABLE condition and not
// a reason to fall back to anything: it means this build's core can run no
// release this repo publishes, and a client that quietly fetched something
// anyway would download disks it cannot boot.
//
// The index promises exactly one `default: true` and tools/verify_catalog.py
// fails a release without it, but this takes the FIRST one it finds and settles
// for the first survivor when there is none - a client should not crash or
// refuse over a broken promise it can route around.
size_t chooseVersion(const std::vector<IndexEntry>& entries,
                     const std::vector<size_t>& runnable,
                     const std::string& preferredVersion);

// The label a picker shows: "RomWBW 3.6.0 (preview)".
//
// The status is appended for everything except "stable" - not only for the one
// value "preview" - because `status` is free text copied from the version
// metadata and a value this build has never heard of must still reach the user's
// eyes. A release published as not-yet-recommended has to LOOK like one; that
// is the whole reason the field is in the index rather than inferred from a
// GitHub prerelease flag.
std::string displayLabel(const IndexEntry& entry);

}  // namespace catalogv0
