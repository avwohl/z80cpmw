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
 *                                        and roms[] may be absent or empty -
 *                                        chooseRom() answers "none" for both
 *   do not assume `emu_avw` is present   nothing looks for it by name,
 *                                        chooseRom() included
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

// ---------------------------------------------------------------------------
// Pointing the application at a different catalog
//
// INDEX_URL is the index this build SHIPS with. These three resolve which one
// is actually in use, so that a romwbw_disks release can be tested before it is
// published and so that somebody can run their own.
//
// Precedence matches romwbw_emu/tools/romwbw-get, deliberately, so the two
// behave the same way and one set of instructions covers both: the environment
// first, so a single test run needs nothing stored; then the setting; then the
// built-in.

// The index actually in use. `configured` is Config's catalogIndexUrl - empty
// meaning "no preference", which is NOT the same as "the built-in URL": storing
// a copy would freeze this install onto whatever the default was on the day it
// was typed, where empty picks up a default that moves in a later build.
std::string indexUrl(const std::string& configured);

// Is the application reading a catalog other than the one it ships with?
bool isCustomIndex(const std::string& configured);

// A short, stable, filesystem- and key-safe tag for the index in use, and
// EMPTY for the built-in one.
//
// Empty is the point. Every path this scopes has to come out byte-identical to
// what a machine already has, or one visit to a test catalog would strand the
// user's library behind a name nothing reads afterwards. Only a custom index
// gets a suffix.
//
// The hash is FNV-1a folded to 32 bits, the same function and the same folding
// as ioscpm and cpmdroid use, so one index URL produces one tag on every client
// - which makes a bug report comparable across ports.
std::string indexScope(const std::string& configured);

// The tag on its own, for testing and for anything that needs it without a
// configured value in hand.
std::string fnv1a32(const std::string& s);

// Is $ROMWBW_INDEX_URL set to something that will actually be used, and if so
// what? THE ONLY READER OF THAT VARIABLE. indexUrl() calls this, and so does the
// Settings dialog when it decides whether to disable the field - which is the
// point of it existing: the dialog used to test `env != nullptr && *env != 0`
// and so treated a variable holding one space as set, disabling the field and
// announcing an override that indexUrl() had already discarded as whitespace.
// One reader, one answer.
bool indexUrlFromEnvironment(std::string& out);

// What a URL typed into the Settings field should be STORED as: trimmed, and
// EMPTY when it is the built-in index, whether that was left blank or pasted in
// full. See the note on Config::catalogIndexUrl for why storing the default is
// not the same as leaving it empty.
std::string normalizedIndexSetting(const std::string& typed);

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
// Parsed AND fetched, which it was not before: a release's ROM is the last
// version-coupled thing in this application, and while the ROM came only from
// the package, a new RomWBW release could not reach a user without a store
// submission. `size` and `sha256` are what a fetched ROM is checked against -
// every time it is loaded, not only when it is downloaded - and `default` is
// what chooseRom() reads. See DiskCatalog::getRomRequirement().
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

// Where the in-app help lives, out of the index's optional `help` block.
//
// THE POINT OF THIS BEING IN A DOCUMENT is that no client compiles in a help
// URL. This application fetched help from
// avwohl/ioscpm/releases/latest/download/ until 1.0.32, through two constants in
// HelpWindow.cpp, and that kept ioscpm's Latest release load-bearing for every
// port long after the disk images had moved to romwbw_disks - the one subsystem
// still coupled to a name a client had to be rebuilt to change. Reading it from
// the index means romwbw_disks can rename the tag, re-cut it or move it to
// another host with no release of this client, which is the same promise
// `catalogUrl` makes for a RomWBW release. A fork inherits it: a custom index
// names its own help, so $ROMWBW_INDEX_URL redirects help too.
//
// BOTH FIELDS OR NEITHER. `ok()` is false for an index that predates the block
// and for one whose block is half-written, and a caller with no location falls
// back to the topics compiled into the binary rather than treating it as an
// error - an older published index legitimately has no `help` key at all.
struct HelpLocation {
    std::string indexUrl;   // absolute URL of help_index.json
    std::string baseUrl;    // prefix a topic's filename is appended to; ends in "/"

    bool ok() const { return !indexUrl.empty() && !baseUrl.empty(); }
};

// index-v0.json -> its `help` block. False when the document will not parse, or
// carries no `help` object, or that object is missing either URL; `out` is left
// empty and this is not an error condition for the caller. Separate from
// parseIndex() rather than a member of it because the two are wanted at
// different moments: the version list on the way to a ROM, this on the way to
// the Help window, and neither should have to parse for the other's sake.
bool parseHelpLocation(const std::string& text, HelpLocation& out);

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

// Which of a catalog's `roms[]` a machine on that release boots: the entry
// flagged `default: true`, else the first.
//
// Returns npos when `roms` is empty, which is the answer for a catalog that
// carried no `roms[]` at all as well as for one that carried an empty array -
// parseCatalog cannot tell those two apart and neither can this, because there
// is nothing a client would do differently. It is a REPORTABLE condition and
// not a reason to boot something else: the release has no ROM this build could
// fetch, so a machine can only be started on it if the bundled ROM already is
// that release.
//
// THE THREE THINGS 6.1 FORBIDS, and they are all forbidden here rather than
// left to a caller. Never by array position: `roms[0]` is the default today and
// the schema promises nothing about order. Never by hardcoding `emu_avw`:
// "do not assume emu_avw is present" is written into the compatibility rules,
// and a future catalog may publish a different set entirely - so nothing here
// looks for a particular id, only for the one the CALLER was told to prefer.
// And never assume the array exists or is non-empty, which is what the npos
// return is for. Taking the FIRST entry flagged is deliberate: two flagged
// entries is a broken document a client should route around rather than refuse,
// exactly as chooseVersion does with two `default: true` index entries.
//
// `preferredId` is the user's stored ROM choice, matched against `id` and never
// against `filename` - a filename carries the release, an id does not, so only
// an id survives a version switch. Empty means "no preference", which is the
// ordinary case and is what the one-argument form passes.
size_t chooseRom(const std::vector<RomItem>& roms, const std::string& preferredId);

inline size_t chooseRom(const std::vector<RomItem>& roms) {
    return chooseRom(roms, std::string());
}

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
