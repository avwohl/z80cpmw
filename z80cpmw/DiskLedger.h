/*
 * DiskLedger.h - Whether the disk image in the data folder is still the one the
 * catalog names, and what the app is allowed to do about it when it is not.
 *
 * Ported from ioscpm's iOSCPM/Views/DiskLedger.swift, deliberately and almost
 * line for line, because `todo.txt` said the first port to solve this was worth
 * copying rather than re-deriving and that port solved it. What is dropped is
 * the half that is iOS-only: NetworkCondition and the expensive/constrained
 * deferrals, which are about somebody's cellular data plan and have no meaning
 * on a desktop. What is kept is every decision that can lose a user's work.
 *
 * This file holds NO Win32, no WinHTTP and no file system. That is the only
 * reason tests/test_diskledger.cpp can check it, and it is why the hashing and
 * the stat live in DiskCatalog.cpp instead: the facts come in as values.
 *
 * ## The hole this closes
 *
 * `disks.xml` carries a `version` attribute, and on the ports that read it that
 * attribute is the only thing that has ever caused an installed image to be
 * replaced. It is 13 at v1.4.5 and 13 at v1.4.12 alike, left there deliberately,
 * because moving it wipes every downloaded disk. So when `hd1k_combo.img` was
 * republished with the fixed `R8.COM` - the one that no longer hands an
 * unfiltered host basename to F_DELETE - nothing reached a machine that already
 * had the old one. Repointing the catalog changes what a NEW download gets and
 * nothing else - that was true of the `RELEASE_TAG` constant this application
 * used to carry, and it is equally true of the interface-v0 index that replaced
 * it. That is the gap.
 *
 * ## Why the obvious fix destroys user data
 *
 * "Hash the file, re-download when it differs from the catalog" is wrong, and
 * wrong in the direction that loses work. A downloaded disk is a WRITABLE CP/M
 * VOLUME: the emulator writes the guest's changes straight into
 * `data\hd1k_combo.img`, so the first time a user saves a file inside a catalog
 * disk its bytes stop matching the catalog for ever - and it is not stale, it is
 * theirs. A refresh keyed on that comparison would silently overwrite 49 MB of
 * somebody's work with a fresh download.
 *
 * ## So staleness is decided from provenance, not from bytes
 *
 * The ledger records, per filename, THE CATALOG <sha256> THAT A VERIFIED
 * DOWNLOAD ACTUALLY MATCHED. That is a fact about which published image these
 * bytes came from, and local writes cannot change it.
 *
 *     superseded  <=>  recorded provenance != the catalog's current <sha256>
 *
 * "Have the bytes moved since we wrote them" is a second, independent question,
 * and it decides only whether replacing them is LOSSY:
 *
 *     pristine    <=>  the bytes still hash to the provenance we recorded
 *
 * A superseded-and-pristine image can be refreshed automatically; nothing is
 * lost. A superseded image the user has written to is offered as a control with
 * the cost spelled out, and is never replaced on the app's own initiative.
 *
 * ## Migration, and the honest limit
 *
 * Every install in service has no ledger at all, because nothing has ever
 * written one. For those, provenance is unknowable: an image that does not hash
 * to the catalog is either the superseded one or one the user has written to,
 * and there is no evidence here that separates them. Such an image gets the
 * control and never the automatic path. The automatic half therefore only starts
 * working for images downloaded by a build that carries this file - which is the
 * honest answer, not a shortcoming to be optimised away.
 *
 * One case does resolve on its own: an image that already hashes to the catalog
 * is current whoever downloaded it, so its provenance is adopted on sight and it
 * never has to be hashed again. That covers nineteen of the twenty entries.
 *
 * ## Measurement caching
 *
 * Hashing the library costs ~211 MB of reads - the twenty catalog entries total
 * 210,763,776 bytes - and that cannot happen on every catalog fetch. A
 * measurement is therefore stored beside the provenance with the (size, mtime)
 * it was taken against, and re-used while both still agree with the file.
 */

#pragma once

#include <cstdint>
#include <map>
#include <string>

// The app's record of one file in the data folder.
//
// installedCatalogSha256 is the load-bearing field and the only one that cannot
// be recomputed: it says which published image these bytes came from. Empty
// means "no provenance", which is not the same as "no record" - a record can
// exist carrying only a measurement. The rest is a cache of an expensive
// measurement.
struct DiskRecord {
    // The catalog <sha256> a verified download matched when this file was
    // installed. Lowercase hex, or empty. Never inferred from the bytes on disk;
    // that is exactly the inference that cannot be made.
    std::string installedCatalogSha256;

    // The last hash actually computed over the file, if one has been, and the
    // two facts it was computed against.
    bool        hasMeasurement = false;
    std::string measuredSha256;
    uint64_t    measuredSize = 0;
    // The write time as raw FILETIME 100ns ticks. An integer on purpose: a
    // value that round-trips through JSON a hair off invalidates every
    // measurement on every launch, and re-hashes the whole library for ever.
    int64_t     measuredModified = 0;
};

// What the file system says about a file, reduced to the two facts that decide
// whether a stored measurement still applies. Kept as a value so the decision
// logic never touches the file system and stays testable here.
struct DiskFileFacts {
    uint64_t size = 0;
    int64_t  modified = 0;
};

// What is known about one catalog entry's installed copy.
enum class DiskFreshness {
    // The catalog carries no usable <sha256>, so nothing can be decided - and
    // nothing should be attempted.
    Unverifiable,
    // No file on disk. The ordinary download path owns this case.
    NotInstalled,
    // Provenance recorded and equal to the catalog's current hash.
    Current,
    // Provenance recorded and different from the catalog's current hash: the
    // publisher moved the bytes, and ours still hash to what we downloaded, so
    // replacing them takes nothing with it.
    SupersededPristine,
    // Superseded, and the bytes have moved since we wrote them. Replacing them
    // would discard the user's own work.
    SupersededModified,
    // No provenance recorded - installed before this bookkeeping existed, or
    // dropped into the data folder by hand - but it hashes to the catalog, so it
    // is current whoever fetched it.
    UnknownProvenanceMatches,
    // No provenance, and it does not hash to the catalog. Genuinely ambiguous:
    // the superseded image, or the user's own writes. Never distinguishable.
    UnknownProvenanceDiffers,
    // A file is present and has not been hashed yet. Nothing may be decided
    // until it has been.
    NeedsMeasurement,
};

// What the app may do about a verdict, before anything else is consulted.
enum class DiskRefreshAction {
    None,                  // leave it alone
    Measure,               // hash the file, then ask again
    OfferUpdate,           // show an Update control; replacing loses nothing
    OfferUpdateLossy,      // show an Update control that SAYS what it discards
    RefreshAutomatically,  // may also refresh unasked; only ever for a pristine file
};

// The final word on one disk, once what the emulator is doing is folded in.
enum class DiskRefreshPlan {
    DoNothing,
    OfferUpdate,
    OfferUpdateLossy,
    RefreshNow,
    // The file is in a slot on a running machine. Not a reason to hide the
    // control, and the one deferral an explicit click must NOT override.
    DeferredMounted,
};

// The per-filename records, and every decision that can be made from them.
//
// Filenames are matched case-insensitively throughout: Windows file names are
// case-insensitive, so a user's HD1K_COMBO.IMG and the catalog's
// hd1k_combo.img are one file, and the ledger must not answer differently for
// them.
class DiskLedger {
public:
    DiskLedger() = default;

    // Lowercased, which is the key everything is stored under.
    static std::string fold(const std::string& filename);

    // A catalog hash reduced to the one form everything else compares against.
    // Returns false if it is not a SHA256 at all.
    //
    // "non-empty" is NOT the test for "the catalog carries a hash":
    // <sha256></sha256> parses to the empty string, because the parser stores
    // whatever sits between the tags. Length and alphabet are checked here so no
    // other caller has to remember.
    static bool normalizedHash(const std::string& raw, std::string& out);

    // Null when nothing is recorded for that name.
    const DiskRecord* record(const std::string& filename) const;
    void setRecord(const std::string& filename, const DiskRecord& r);
    void removeRecord(const std::string& filename);

    // Record a verified download: these bytes came from the image the catalog
    // currently names, and we know their hash exactly because we just checked
    // it. 'facts' may be null when the stat failed; the provenance is still
    // recorded, only the measurement cache is skipped.
    void recordInstall(const std::string& filename,
                       const std::string& catalogSha256,
                       const DiskFileFacts* facts);

    // Store a hash computed over the file, against the facts it was computed
    // for. Creates a provenance-less record if there was none - that is the
    // migration case, and it must not invent a provenance.
    void recordMeasurement(const std::string& filename,
                           const std::string& sha256,
                           const DiskFileFacts& facts);

    // An image whose bytes already hash to the catalog is current whoever
    // downloaded it, so its provenance can be adopted rather than left unknown.
    // This is what stops a migrating install re-hashing nineteen files every
    // time the catalog is fetched.
    void adoptProvenanceIfCurrent(const std::string& filename,
                                  const std::string& catalogSha256);

    // Whether a stored measurement still describes the file on disk.
    static bool measurementApplies(const DiskRecord& r, const DiskFileFacts& f);

    // The verdict for one catalog entry. 'facts' is null when there is no file.
    // 'catalogSha256' is the entry's <sha256> exactly as the catalog gave it,
    // empty string included.
    DiskFreshness freshness(const std::string& filename,
                            const std::string& catalogSha256,
                            const DiskFileFacts* facts) const;

    static DiskRefreshAction action(DiskFreshness f);

    // Fold in what the emulator is doing. 'isMounted' is true when the file is
    // selected in a slot AND the machine is running: replacing it then undoes
    // itself, because the guest's in-memory image is written back over the fresh
    // download on the next flush, and the ledger would meanwhile record the new
    // hash as this file's provenance - leaving a superseded image permanently
    // labelled current.
    static DiskRefreshPlan plan(DiskFreshness f, bool isMounted);

    // An explicit click. Allowed whatever the plan says about the automatic
    // path - that is what makes restricting the automatic half defensible - but
    // still refused for an entry there is nothing to be done about. It does NOT
    // override a mounted disk; the caller checks that separately, because this
    // is about the catalog and the file, and being mounted is about the machine.
    static bool allowsUserRequestedUpdate(DiskFreshness f);

    // One line for a status column. Deliberately says "may be" for the
    // ambiguous case rather than accusing the user of being out of date.
    static const char* describe(DiskFreshness f);

    // JSON, as one object keyed by folded filename. Round-trips through
    // serialize/deserialize; a text that will not parse deserialises to an EMPTY
    // ledger rather than a half one, so a truncated write can never read back as
    // "everything is current".
    std::string serialize() const;
    static DiskLedger deserialize(const std::string& text);

    const std::map<std::string, DiskRecord>& records() const { return m_records; }

private:
    std::map<std::string, DiskRecord> m_records;
};
