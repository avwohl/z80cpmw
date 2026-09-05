/*
 * CatalogV0.cpp - The two documents described in CatalogV0.h.
 *
 * Nothing here opens a socket, a file or a thread. The one caller that does all
 * three - DiskCatalog::fetchCatalog - hands the response text in as a value.
 */

#include "CatalogV0.h"
#include "include/nlohmann/json.hpp"

#include <cstddef>

using json = nlohmann::json;

namespace catalogv0 {

const char* const INTERFACE = "v0";

// The one URL in the binary. The `catalog-v0` tag carries this single small file
// and nothing else, so re-cutting it costs one upload of a few kilobytes - which
// is what makes a floating entry point safe here where a floating release tag
// full of 200 MB of images would not be.
const char* const INDEX_URL =
    "https://github.com/avwohl/romwbw_disks/releases/download/catalog-v0/index-v0.json";

namespace {

//=============================================================================
// The non-throwing accessors
//
// nlohmann throws on a type mismatch - `.get<std::string>()` on a number is a
// json::type_error - and this code runs on a detached thread. So nothing below
// is ever asked for a value without its type being checked first, and a value
// of the wrong type reads exactly as an absent one: the document said nothing
// this build understands, which is the same answer either way.
//=============================================================================

const json* member(const json& j, const char* key) {
    if (!j.is_object()) return nullptr;
    auto it = j.find(key);
    return it == j.end() ? nullptr : &(*it);
}

std::string str(const json& j, const char* key) {
    const json* v = member(j, key);
    if (!v || !v->is_string()) return std::string();
    return v->get_ref<const json::string_t&>();
}

// Unsigned, and a negative number reads as absent rather than wrapping. A size
// is what isDiskDownloaded compares a file against, so a negative one that
// became 18446744073709551615 would report every image as truncated.
unsigned long long u64(const json& j, const char* key) {
    const json* v = member(j, key);
    if (!v) return 0;
    if (v->is_number_unsigned()) return v->get<json::number_unsigned_t>();
    if (v->is_number_integer()) {
        json::number_integer_t n = v->get<json::number_integer_t>();
        return n < 0 ? 0 : static_cast<unsigned long long>(n);
    }
    return 0;
}

long long i64(const json& j, const char* key) {
    const json* v = member(j, key);
    if (!v) return 0;
    if (v->is_number_unsigned()) {
        json::number_unsigned_t n = v->get<json::number_unsigned_t>();
        // A generation past the range of a long long is not a number anyone
        // published; clamp rather than wrap, because this value is only ever
        // compared for equality.
        return n > static_cast<json::number_unsigned_t>(0x7FFFFFFFFFFFFFFFLL)
                   ? 0x7FFFFFFFFFFFFFFFLL
                   : static_cast<long long>(n);
    }
    if (v->is_number_integer()) return v->get<json::number_integer_t>();
    return 0;
}

bool flag(const json& j, const char* key, bool fallback) {
    const json* v = member(j, key);
    if (!v || !v->is_boolean()) return fallback;
    return v->get<bool>();
}

// A count for display. Clamped rather than wrapped for the same reason as i64,
// and 0 for anything unreadable, which reads as "the index did not say".
int countOf(const json& j, const char* key) {
    unsigned long long n = u64(j, key);
    return n > 1000000ULL ? 1000000 : static_cast<int>(n);
}

// The two version bytes out of an `hbios` (or `hcb`) object, which spell them
// differently: the index says ver_byte/upd_byte and a ROM's hcb says
// version/update. Both halves must read, because half a release is not one.
bool versionBytes(const json& owner, const char* objectKey,
                  const char* verKey, const char* updKey,
                  unsigned char& ver, unsigned char& upd) {
    const json* obj = member(owner, objectKey);
    if (!obj) return false;
    unsigned char v = 0, u = 0;
    if (!parseHexByte(str(*obj, verKey), v)) return false;
    if (!parseHexByte(str(*obj, updKey), u)) return false;
    ver = v;
    upd = u;
    return true;
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

bool parseHexByte(const std::string& text, unsigned char& out) {
    // Exactly "0x" and two hex digits, which is what tools/gen_catalog.py emits.
    // Nothing looser: "0x" alone, "0x350" and "53" are all documents this build
    // does not understand, and reading them as 0, 0x35 and 53 would each be a
    // confident wrong answer about which ROM boots.
    if (text.size() != 4) return false;
    if (text[0] != '0' || (text[1] != 'x' && text[1] != 'X')) return false;
    int hi = hexDigit(text[2]);
    int lo = hexDigit(text[3]);
    if (hi < 0 || lo < 0) return false;
    out = static_cast<unsigned char>((hi << 4) | lo);
    return true;
}

bool parseIndex(const std::string& text, std::vector<IndexEntry>& out, std::string& error) {
    out.clear();

    // The non-throwing parse. `is_discarded()` is how it reports a syntax error,
    // and it is the only way this can be told about one without an exception
    // reaching a thread with no handler.
    const json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded()) {
        error = "The disk catalog index is not valid JSON";
        return false;
    }
    if (!doc.is_object()) {
        error = "The disk catalog index is not a JSON object";
        return false;
    }

    // `schema` is checked and `schema_version` is not, and the asymmetry is the
    // point. The schema NAME distinguishes this document from a per-version
    // catalog, so reading one as the other is a real mistake worth refusing.
    // The schema VERSION is bumped only if the shape changes incompatibly, and
    // refusing an unrecognised one here would mean a shipped build stops working
    // the day a field is added - which is exactly what "adding a field is not an
    // interface break" forbids.
    const std::string schema = str(doc, "schema");
    if (!schema.empty() && schema != "romwbw-disks-index") {
        error = "Not a disk catalog index (schema is \"" + schema + "\")";
        return false;
    }

    const json* versions = member(doc, "romwbw_versions");
    if (!versions || !versions->is_array()) {
        error = "The disk catalog index lists no RomWBW versions";
        return false;
    }

    for (const auto& v : *versions) {
        if (!v.is_object()) continue;

        IndexEntry e;
        e.romwbwVersion = str(v, "romwbw_version");
        e.catalogUrl = str(v, "catalog_url");
        e.haveHbios = versionBytes(v, "hbios", "ver_byte", "upd_byte", e.verByte, e.updByte);

        // Skipped, not fatal. These three are what every later step needs - a
        // name to remember the choice by, a URL to fetch, and the pair that says
        // whether the core can boot it - and an entry short of any of them is one
        // this build cannot use. A LATER index that adds an entry shaped
        // differently must not take the entries this build does understand down
        // with it, which is what refusing the whole document would do.
        if (e.romwbwVersion.empty() || e.catalogUrl.empty() || !e.haveHbios) continue;

        e.label = str(v, "label");
        if (e.label.empty()) e.label = "RomWBW " + e.romwbwVersion;
        e.status = str(v, "status");
        e.isDefault = flag(v, "default", false);
        e.catalogSha256 = str(v, "catalog_sha256");
        e.catalogSize = u64(v, "catalog_size");
        e.generation = i64(v, "generation");
        e.romCount = countOf(v, "rom_count");
        e.diskCount = countOf(v, "disk_count");

        out.push_back(e);
    }

    if (out.empty()) {
        error = "The disk catalog index carries no usable RomWBW version";
        return false;
    }
    return true;
}

bool parseCatalog(const std::string& text, Catalog& out, std::string& error) {
    out = Catalog();

    const json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded()) {
        error = "The disk catalog is not valid JSON";
        return false;
    }
    if (!doc.is_object()) {
        error = "The disk catalog is not a JSON object";
        return false;
    }

    const std::string schema = str(doc, "schema");
    if (!schema.empty() && schema != "romwbw-disks-catalog") {
        error = "Not a disk catalog (schema is \"" + schema + "\")";
        return false;
    }

    out.baseUrl = str(doc, "base_url");
    if (out.baseUrl.empty()) {
        // The one field whose absence makes the document useless: without it
        // there is no URL for a single asset, and this is the release that
        // stopped being able to invent one from a tag.
        error = "The disk catalog carries no download base URL";
        return false;
    }

    out.romwbwVersion = str(doc, "romwbw_version");
    out.status = str(doc, "status");
    out.releaseTag = str(doc, "release_tag");
    out.generation = i64(doc, "generation");

    // roms[] absent or empty is not an error, and neither is a catalog with no
    // emu_avw in it. Nothing below looks for an id by name.
    const json* roms = member(doc, "roms");
    if (roms && roms->is_array()) {
        for (const auto& r : *roms) {
            if (!r.is_object()) continue;
            RomItem item;
            item.id = str(r, "id");
            item.filename = str(r, "filename");
            if (item.id.empty() || item.filename.empty()) continue;
            item.name = str(r, "name");
            item.description = str(r, "description");
            item.size = u64(r, "size");
            item.sha256 = str(r, "sha256");
            item.isDefault = flag(r, "default", false);
            item.haveHcb = versionBytes(r, "hcb", "version", "update",
                                        item.hcbVersion, item.hcbUpdate);
            out.roms.push_back(item);
        }
    }

    const json* disks = member(doc, "disks");
    if (disks && disks->is_array()) {
        for (const auto& d : *disks) {
            if (!d.is_object()) continue;
            DiskItem item;
            item.id = str(d, "id");
            item.filename = str(d, "filename");
            // An entry with no filename names no asset, and one with no id has
            // no stable key - the one thing 6.1 says to hold on to. Skipped
            // rather than fatal, for the reason parseIndex skips.
            if (item.id.empty() || item.filename.empty()) continue;
            item.name = str(d, "name");
            item.description = str(d, "description");
            item.size = u64(d, "size");
            // Read but not validated: an empty <sha256> and a missing one are the
            // same string, and DiskLedger::normalizedHash is the single place
            // that decides whether one is usable. Same rule the XML parser this
            // replaces followed, kept deliberately.
            item.sha256 = str(d, "sha256");
            item.license = str(d, "license");
            item.format = str(d, "format");
            item.bootable = flag(d, "bootable", false);
            item.hostTransfer = flag(d, "host_transfer", false);
            // Present only on hd1k_combo. Absent means "no opinion", which is
            // not the same as slot 0 - hence the separate flag rather than a
            // default of 0.
            const json* slot = member(d, "defaultSlot");
            if (slot && (slot->is_number_unsigned() || slot->is_number_integer())) {
                item.haveDefaultSlot = true;
                item.defaultSlot = static_cast<int>(i64(d, "defaultSlot"));
            }
            out.disks.push_back(item);
        }
    }

    return true;
}

std::string assetUrl(const std::string& baseUrl, const std::string& filename) {
    return baseUrl + filename;
}

std::vector<size_t> runnableVersions(const std::vector<IndexEntry>& entries,
                                     const ReleaseSupported& supported) {
    std::vector<size_t> runnable;
    if (!supported) return runnable;
    for (size_t i = 0; i < entries.size(); i++) {
        if (!entries[i].haveHbios) continue;
        if (supported(entries[i].verByte, entries[i].updByte)) runnable.push_back(i);
    }
    return runnable;
}

size_t chooseVersion(const std::vector<IndexEntry>& entries,
                     const std::vector<size_t>& runnable,
                     const std::string& preferredVersion) {
    if (runnable.empty()) return static_cast<size_t>(-1);

    // The user's own choice wins while it is still runnable. It is compared
    // against `romwbw_version` and not against the label, because the label is
    // display text the index may reword at any time and the version string is
    // the key the choice was stored under.
    if (!preferredVersion.empty()) {
        for (size_t i : runnable) {
            if (entries[i].romwbwVersion == preferredVersion) return i;
        }
        // Falling through rather than failing is deliberate: a preference for a
        // release this build can no longer run - the user downgraded the app, or
        // the repo retired the version - has to degrade to something bootable
        // rather than leaving them with no catalog at all. What must NOT happen
        // is any file being deleted over it, and nothing here deletes.
    }

    for (size_t i : runnable) {
        if (entries[i].isDefault) return i;
    }
    return runnable.front();
}

size_t chooseRom(const std::vector<RomItem>& roms) {
    // Empty covers both "the document had no roms[]" and "it had an empty one".
    // parseCatalog produces the same vector for either, and there is nothing a
    // caller would do differently: the release publishes no ROM this build can
    // fetch.
    if (roms.empty()) return static_cast<size_t>(-1);

    for (size_t i = 0; i < roms.size(); i++) {
        if (roms[i].isDefault) return i;
    }

    // No entry flagged. Both published catalogs flag exactly one, and nothing
    // in the schema promises it - `default` is documented as "whether this is
    // the ROM to offer first", not as a field that must appear - so the first
    // entry is taken and that is treated as normal rather than as an error. The
    // alternative, refusing, would strand a whole release over a missing
    // boolean in a document whose ROM list is otherwise complete.
    return 0;
}

std::string displayLabel(const IndexEntry& entry) {
    std::string label = entry.label.empty() ? ("RomWBW " + entry.romwbwVersion) : entry.label;
    if (!entry.status.empty() && entry.status != "stable") {
        label += " (" + entry.status + ")";
    }
    return label;
}

}  // namespace catalogv0
