/*
 * DiskMigrationV0.cpp - The mapping described in DiskMigrationV0.h.
 *
 * Nothing here opens a file, renames one or asks Windows anything. The two
 * callers that do - DiskCatalog::migrateFilesToInterfaceV0 for the images and
 * the ledger, ConfigManager::migrateToInterfaceV0 for the slots and the
 * profiles - hand the results back as values.
 */

#include "DiskMigrationV0.h"

namespace diskv0 {

const char* const INTERFACE = "v0";
const char* const BUNDLED_ROMWBW = "3.5.1";

const std::vector<std::string>& legacyCatalogFilenames() {
    static const std::vector<std::string> names = {
        "hd1k_combo.img",
        "hd1k_cpm22.img",
        "hd1k_zsdos.img",
        "hd1k_zpm3.img",
        "hd1k_cpm3.img",
        "hd1k_nzcom.img",
        "hd1k_qpm.img",
        "hd1k_games.img",
        "hd1k_aztecc.img",
        "hd1k_bascomp.img",
        "hd1k_cowgol.img",
        "hd1k_fortran.img",
        "hd1k_hitechc.img",
        "hd1k_tpascal.img",
        "hd1k_z80asm.img",
        "hd1k_ws4.img",
        "hd1k_z3plus.img",
        "hd1k_bp.img",
        "hd1k_msxroms1.img",
        "hd1k_msxroms2.img",
    };
    return names;
}

// The tag a v0 name carries in its stem, "-v0-", built from INTERFACE so the
// two cannot drift.
static std::string interfaceTag() {
    return std::string("-") + INTERFACE + "-";
}

// Where the extension starts, or npos. The LAST dot, because the stem of
// hd1k_combo-v0-3.5.1.img contains three of them and only the last one is the
// extension.
static size_t extensionDot(const std::string& filename) {
    size_t dot = filename.find_last_of('.');
    // A leading dot is the whole name, not an extension: ".hidden" has no stem
    // to suffix and must not become "-v0-3.5.1.hidden".
    if (dot == 0) return std::string::npos;
    return dot;
}

bool looksLikeV0Name(const std::string& filename) {
    const std::string folded = DiskLedger::fold(filename);
    size_t dot = extensionDot(folded);
    const std::string stem = dot == std::string::npos ? folded : folded.substr(0, dot);

    const std::string tag = interfaceTag();
    size_t at = stem.rfind(tag);
    // Something has to follow the tag: "hd1k_combo-v0-.img" names no release and
    // is not a name this scheme ever produced.
    return at != std::string::npos && at + tag.size() < stem.size();
}

bool v0NameFor(const std::string& filename, std::string& out) {
    if (filename.empty()) return false;
    if (looksLikeV0Name(filename)) return false;

    const std::string folded = DiskLedger::fold(filename);
    const std::string* matched = nullptr;
    for (const auto& known : legacyCatalogFilenames()) {
        if (DiskLedger::fold(known) == folded) {
            matched = &known;
            break;
        }
    }
    if (!matched) return false;

    // Built from the catalog's own spelling, never from the caller's - see the
    // note on legacyCatalogFilenames().
    size_t dot = extensionDot(*matched);
    if (dot == std::string::npos) return false;
    out = matched->substr(0, dot) + interfaceTag() + BUNDLED_ROMWBW + matched->substr(dot);
    return true;
}

std::string basenameOf(const std::string& path) {
    size_t sep = path.find_last_of("\\/");
    return sep == std::string::npos ? path : path.substr(sep + 1);
}

std::string parentOf(const std::string& path) {
    size_t sep = path.find_last_of("\\/");
    return sep == std::string::npos ? std::string() : path.substr(0, sep);
}

bool isDirectlyIn(const std::string& path, const std::string& dir) {
    if (dir.empty()) return false;

    // A trailing separator on the directory is not part of its name. It reaches
    // this function from whatever built the string, and comparing "…\data\"
    // against a parent of "…\data" would answer no for every file in it.
    std::string wanted = dir;
    while (!wanted.empty() && (wanted.back() == '\\' || wanted.back() == '/')) {
        wanted.pop_back();
    }
    if (wanted.empty()) return false;

    const std::string parent = parentOf(path);
    if (parent.empty()) return false;
    return DiskLedger::fold(parent) == DiskLedger::fold(wanted);
}

bool migratedDiskPath(const std::string& storedPath,
                      const std::string& dataDir,
                      const LandedNames& landed,
                      std::string& out) {
    if (storedPath.empty()) return false;
    if (!isDirectlyIn(storedPath, dataDir)) return false;

    auto it = landed.find(DiskLedger::fold(basenameOf(storedPath)));
    if (it == landed.end()) return false;

    out = parentOf(storedPath) + "\\" + it->second;
    return true;
}

int migrateLedgerKeys(DiskLedger& ledger, const LandedNames& landed) {
    int moved = 0;
    for (const auto& [legacyName, v0Name] : landed) {
        const DiskRecord* existing = ledger.record(legacyName);
        if (!existing) continue;
        if (ledger.record(v0Name)) continue;

        // Copied out before either write: setRecord may insert, and the record
        // being read is the one about to be erased.
        const DiskRecord carried = *existing;
        ledger.setRecord(v0Name, carried);
        ledger.removeRecord(legacyName);
        moved++;
    }
    return moved;
}

}  // namespace diskv0
