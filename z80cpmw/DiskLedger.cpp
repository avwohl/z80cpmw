/*
 * DiskLedger.cpp - The decisions described in DiskLedger.h.
 *
 * Nothing here opens a file, hashes one or reaches the network. Every fact
 * arrives as a value, which is what lets tests/test_diskledger.cpp check the
 * whole of it on a machine with no data folder and no catalog.
 */

#include "DiskLedger.h"
#include "include/nlohmann/json.hpp"

#include <algorithm>
#include <cctype>

using json = nlohmann::json;

std::string DiskLedger::fold(const std::string& filename) {
    std::string out;
    out.reserve(filename.size());
    for (unsigned char c : filename) {
        out += static_cast<char>(std::tolower(c));
    }
    return out;
}

bool DiskLedger::normalizedHash(const std::string& raw, std::string& out) {
    size_t begin = raw.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return false;
    size_t end = raw.find_last_not_of(" \t\r\n");

    std::string trimmed = raw.substr(begin, end - begin + 1);
    if (trimmed.size() != 64) return false;

    std::string lowered;
    lowered.reserve(64);
    for (unsigned char c : trimmed) {
        if (!std::isxdigit(c)) return false;
        lowered += static_cast<char>(std::tolower(c));
    }

    out = lowered;
    return true;
}

const DiskRecord* DiskLedger::record(const std::string& filename) const {
    auto it = m_records.find(fold(filename));
    return it == m_records.end() ? nullptr : &it->second;
}

void DiskLedger::setRecord(const std::string& filename, const DiskRecord& r) {
    m_records[fold(filename)] = r;
}

void DiskLedger::removeRecord(const std::string& filename) {
    m_records.erase(fold(filename));
}

void DiskLedger::recordInstall(const std::string& filename,
                               const std::string& catalogSha256,
                               const DiskFileFacts* facts) {
    // An entry the catalog gives no usable hash for cannot have provenance
    // recorded for it, and inventing one would make it read as Current for ever.
    std::string hash;
    if (!normalizedHash(catalogSha256, hash)) return;

    DiskRecord r;
    r.installedCatalogSha256 = hash;
    if (facts) {
        // The download was verified against this hash, so the measurement is
        // known without reading the file a second time.
        r.hasMeasurement = true;
        r.measuredSha256 = hash;
        r.measuredSize = facts->size;
        r.measuredModified = facts->modified;
    }
    setRecord(filename, r);
}

void DiskLedger::recordMeasurement(const std::string& filename,
                                   const std::string& sha256,
                                   const DiskFileFacts& facts) {
    std::string hash;
    if (!normalizedHash(sha256, hash)) return;

    // Preserve any provenance already recorded; a measurement is evidence about
    // the bytes and says nothing about where they came from.
    DiskRecord r;
    if (const DiskRecord* existing = record(filename)) {
        r = *existing;
    }
    r.hasMeasurement = true;
    r.measuredSha256 = hash;
    r.measuredSize = facts.size;
    r.measuredModified = facts.modified;
    setRecord(filename, r);
}

void DiskLedger::adoptProvenanceIfCurrent(const std::string& filename,
                                          const std::string& catalogSha256) {
    std::string catalog;
    if (!normalizedHash(catalogSha256, catalog)) return;

    const DiskRecord* existing = record(filename);
    if (!existing) return;
    if (!existing->installedCatalogSha256.empty()) return;  // already known
    if (!existing->hasMeasurement) return;
    if (existing->measuredSha256 != catalog) return;

    DiskRecord r = *existing;
    r.installedCatalogSha256 = catalog;
    setRecord(filename, r);
}

bool DiskLedger::measurementApplies(const DiskRecord& r, const DiskFileFacts& f) {
    if (!r.hasMeasurement) return false;
    return r.measuredSize == f.size && r.measuredModified == f.modified;
}

DiskFreshness DiskLedger::freshness(const std::string& filename,
                                    const std::string& catalogSha256,
                                    const DiskFileFacts* facts) const {
    std::string catalog;
    if (!normalizedHash(catalogSha256, catalog)) return DiskFreshness::Unverifiable;
    if (!facts) return DiskFreshness::NotInstalled;

    const DiskRecord* stored = record(filename);

    std::string provenance;
    bool haveProvenance = stored &&
                          normalizedHash(stored->installedCatalogSha256, provenance);

    if (haveProvenance) {
        if (provenance == catalog) return DiskFreshness::Current;

        // Superseded. Whether replacing it is lossy depends on a measurement
        // that still applies; with no usable measurement, nothing may be
        // decided yet. Falling to NeedsMeasurement rather than assuming is what
        // keeps the automatic path off a file we have not looked at.
        if (!measurementApplies(*stored, *facts)) return DiskFreshness::NeedsMeasurement;
        return stored->measuredSha256 == provenance
                   ? DiskFreshness::SupersededPristine
                   : DiskFreshness::SupersededModified;
    }

    // No provenance. A measurement can still settle whether the file IS the
    // catalog's image, which is the common migration case.
    if (!stored || !measurementApplies(*stored, *facts)) return DiskFreshness::NeedsMeasurement;
    return stored->measuredSha256 == catalog
               ? DiskFreshness::UnknownProvenanceMatches
               : DiskFreshness::UnknownProvenanceDiffers;
}

DiskRefreshAction DiskLedger::action(DiskFreshness f) {
    switch (f) {
    case DiskFreshness::Unverifiable:
        // There is no hash to compare against and none to verify a replacement
        // with, so an Update control here could only ever fail. Do not light it.
    case DiskFreshness::NotInstalled:
    case DiskFreshness::Current:
    case DiskFreshness::UnknownProvenanceMatches:
        // Matching the catalog is as good as current, whoever fetched it.
        return DiskRefreshAction::None;

    case DiskFreshness::NeedsMeasurement:
        return DiskRefreshAction::Measure;

    case DiskFreshness::SupersededPristine:
        // Proven to be the image we downloaded, and the publisher has moved on.
        // This is the only verdict that earns the automatic path.
        return DiskRefreshAction::RefreshAutomatically;

    case DiskFreshness::SupersededModified:
    case DiskFreshness::UnknownProvenanceDiffers:
        // Ambiguity never earns the automatic path, and the control has to say
        // what it costs.
        return DiskRefreshAction::OfferUpdateLossy;
    }
    return DiskRefreshAction::None;
}

DiskRefreshPlan DiskLedger::plan(DiskFreshness f, bool isMounted) {
    switch (action(f)) {
    case DiskRefreshAction::None:
    case DiskRefreshAction::Measure:
        return DiskRefreshPlan::DoNothing;
    case DiskRefreshAction::OfferUpdate:
        return DiskRefreshPlan::OfferUpdate;
    case DiskRefreshAction::OfferUpdateLossy:
        return DiskRefreshPlan::OfferUpdateLossy;
    case DiskRefreshAction::RefreshAutomatically:
        return isMounted ? DiskRefreshPlan::DeferredMounted : DiskRefreshPlan::RefreshNow;
    }
    return DiskRefreshPlan::DoNothing;
}

bool DiskLedger::allowsUserRequestedUpdate(DiskFreshness f) {
    switch (f) {
    case DiskFreshness::SupersededPristine:
    case DiskFreshness::SupersededModified:
    case DiskFreshness::UnknownProvenanceDiffers:
        return true;
    case DiskFreshness::Unverifiable:
    case DiskFreshness::NotInstalled:
    case DiskFreshness::Current:
    case DiskFreshness::UnknownProvenanceMatches:
    case DiskFreshness::NeedsMeasurement:
        return false;
    }
    return false;
}

const char* DiskLedger::describe(DiskFreshness f) {
    switch (f) {
    case DiskFreshness::NotInstalled:
        return "Available";
    case DiskFreshness::SupersededPristine:
        return "Update available";
    case DiskFreshness::SupersededModified:
        return "Update available (overwrites your changes)";
    case DiskFreshness::UnknownProvenanceDiffers:
        // Says what is known and not what is guessed: this file is either the
        // superseded image or the user's own work, and nothing here can tell.
        return "Differs from catalog";
    case DiskFreshness::Unverifiable:
    case DiskFreshness::Current:
    case DiskFreshness::UnknownProvenanceMatches:
    case DiskFreshness::NeedsMeasurement:
        return "Downloaded";
    }
    return "Downloaded";
}

std::string DiskLedger::serialize() const {
    json doc = json::object();
    for (const auto& [name, r] : m_records) {
        json entry = json::object();
        entry["installedCatalogSha256"] = r.installedCatalogSha256;
        if (r.hasMeasurement) {
            entry["measuredSha256"] = r.measuredSha256;
            entry["measuredSize"] = r.measuredSize;
            entry["measuredModified"] = r.measuredModified;
        }
        doc[name] = entry;
    }
    return doc.dump(2);
}

DiskLedger DiskLedger::deserialize(const std::string& text) {
    DiskLedger ledger;
    if (text.empty()) return ledger;

    // A ledger that will not parse comes back EMPTY rather than half-read. A
    // half-read one is the dangerous shape: a missing record reads as "no
    // provenance", which is recoverable, but a record carrying a stale
    // measurement against the wrong file reads as a verdict.
    json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return ledger;

    for (auto it = doc.begin(); it != doc.end(); ++it) {
        const json& entry = it.value();
        if (!entry.is_object()) continue;

        DiskRecord r;
        if (entry.contains("installedCatalogSha256") &&
            entry["installedCatalogSha256"].is_string()) {
            r.installedCatalogSha256 = entry["installedCatalogSha256"].get<std::string>();
        }
        // All three measurement fields or none: a hash without the facts it was
        // taken against can never be shown to still apply, and would only ever
        // force a re-measure.
        if (entry.contains("measuredSha256") && entry["measuredSha256"].is_string() &&
            entry.contains("measuredSize") && entry["measuredSize"].is_number_unsigned() &&
            entry.contains("measuredModified") && entry["measuredModified"].is_number_integer()) {
            r.hasMeasurement = true;
            r.measuredSha256 = entry["measuredSha256"].get<std::string>();
            r.measuredSize = entry["measuredSize"].get<uint64_t>();
            r.measuredModified = entry["measuredModified"].get<int64_t>();
        }
        ledger.setRecord(it.key(), r);
    }
    return ledger;
}
