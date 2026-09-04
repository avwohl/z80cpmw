/*
 * test_diskledger.cpp - Disk provenance suite.
 *
 * todo.txt: "shipping the repin does not refresh an image already on a machine."
 * DiskLedger is what decides whether it should be, and this suite is what holds
 * that decision honest. It has three jobs.
 *
 * The first is the rule that keeps the user's work: an image the user has
 * written to must NEVER reach the automatic path, and the only verdict that does
 * reach it is one proven to still hash to what we ourselves downloaded. A
 * downloaded disk is a writable CP/M volume, so "the bytes differ from the
 * catalog" is not evidence of staleness - it is the normal state of a disk
 * somebody has saved a file on. Every section below that names `Modified` or
 * `Differs` exists because getting this backwards silently destroys 49 MB of
 * somebody's work.
 *
 * The second is the migration case, which is every install in service: no ledger
 * exists, so provenance is unknowable, and an image that does not hash to the
 * catalog is either the superseded one or the user's own. The suite requires
 * that case to be ambiguous rather than guessed - and requires the one case that
 * DOES resolve, an image already hashing to the catalog, to be adopted so a
 * migrating install does not re-hash nineteen files for ever.
 *
 * The third is the measurement cache. It is keyed on (size, mtime) and it has to
 * invalidate on either, or a re-downloaded file is judged on the previous file's
 * hash.
 *
 * It needs no window, no data folder, no catalog and no network:
 * DiskLedger.cpp holds no Win32 and no file system, which is the whole reason
 * this suite can exist. Every fact arrives as a value.
 *
 * Build and run: tests\run_tests.bat
 */

#include "DiskLedger.h"
#include "DiskHash.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

//=============================================================================
// Harness
//=============================================================================

static int g_checks = 0;
static int g_failed = 0;
static const char* g_section = "";

static void section(const char* name) {
    g_section = name;
    printf("\n-- %s\n", name);
}

static void check(bool ok, const std::string& what,
                  const std::string& got, const std::string& want) {
    g_checks++;
    if (ok) return;
    g_failed++;
    printf("  FAIL [%s] %s\n        got:  %s\n        want: %s\n",
           g_section, what.c_str(), got.c_str(), want.c_str());
}

static void checkTrue(bool ok, const std::string& what) {
    check(ok, what, ok ? "true" : "false", "true");
}

static void checkFalse(bool ok, const std::string& what) {
    check(!ok, what, ok ? "true" : "false", "false");
}

static void checkStr(const std::string& got, const std::string& want,
                     const std::string& what) {
    check(got == want, what, "\"" + got + "\"", "\"" + want + "\"");
}

static const char* freshnessName(DiskFreshness f) {
    switch (f) {
    case DiskFreshness::Unverifiable:             return "Unverifiable";
    case DiskFreshness::NotInstalled:             return "NotInstalled";
    case DiskFreshness::Current:                  return "Current";
    case DiskFreshness::SupersededPristine:       return "SupersededPristine";
    case DiskFreshness::SupersededModified:       return "SupersededModified";
    case DiskFreshness::UnknownProvenanceMatches: return "UnknownProvenanceMatches";
    case DiskFreshness::UnknownProvenanceDiffers: return "UnknownProvenanceDiffers";
    case DiskFreshness::NeedsMeasurement:         return "NeedsMeasurement";
    }
    return "?";
}

static const char* actionName(DiskRefreshAction a) {
    switch (a) {
    case DiskRefreshAction::None:                 return "None";
    case DiskRefreshAction::Measure:              return "Measure";
    case DiskRefreshAction::OfferUpdate:          return "OfferUpdate";
    case DiskRefreshAction::OfferUpdateLossy:     return "OfferUpdateLossy";
    case DiskRefreshAction::RefreshAutomatically: return "RefreshAutomatically";
    }
    return "?";
}

static const char* planName(DiskRefreshPlan p) {
    switch (p) {
    case DiskRefreshPlan::DoNothing:        return "DoNothing";
    case DiskRefreshPlan::OfferUpdate:      return "OfferUpdate";
    case DiskRefreshPlan::OfferUpdateLossy: return "OfferUpdateLossy";
    case DiskRefreshPlan::RefreshNow:       return "RefreshNow";
    case DiskRefreshPlan::DeferredMounted:  return "DeferredMounted";
    }
    return "?";
}

static void checkFreshness(DiskFreshness got, DiskFreshness want, const std::string& what) {
    check(got == want, what, freshnessName(got), freshnessName(want));
}

static void checkAction(DiskRefreshAction got, DiskRefreshAction want, const std::string& what) {
    check(got == want, what, actionName(got), actionName(want));
}

static void checkPlan(DiskRefreshPlan got, DiskRefreshPlan want, const std::string& what) {
    check(got == want, what, planName(got), planName(want));
}

//=============================================================================
// The hashes this suite works in
//=============================================================================

// The REAL published hash of hd1k_combo.img in the ioscpm v1.4.12 catalog, read
// out of ioscpm/release_assets/disks.xml, and the one DiskCatalog.cpp's comment
// names as 89b8ae1a....
static const char* CATALOG_V1412 =
    "89b8ae1aaa6867dc515c3511b34c4f0c311a77e99ff71066f5a774bef99cde1d";

// Stands in for the v1.4.5 image's hash, which begins be19984e... The rest is
// SYNTHETIC - only the prefix of the real one has ever been recorded in this
// repository, and inventing the tail and calling it measured is exactly what
// CLAUDE.md forbids. Nothing here depends on its value, only on it differing.
static const char* CATALOG_OLD =
    "be19984e00000000000000000000000000000000000000000000000000000000";

// A third hash, for "the user has written to this volume". Synthetic by nature:
// it is whatever somebody's saved file happened to make it.
static const char* USER_WRITTEN =
    "1111111111111111111111111111111111111111111111111111111111111111";

static const char* COMBO = "hd1k_combo.img";

static DiskFileFacts facts(uint64_t size, int64_t modified) {
    DiskFileFacts f;
    f.size = size;
    f.modified = modified;
    return f;
}

// The real size of hd1k_combo.img, and an arbitrary write time.
static const uint64_t COMBO_SIZE = 51380224;
static const int64_t  T0 = 133000000000000000LL;
static const int64_t  T1 = 133000000000000001LL;

//=============================================================================
// Hash hygiene
//=============================================================================

static void test_normalized_hash() {
    section("normalizedHash");

    std::string out;
    checkTrue(DiskLedger::normalizedHash(CATALOG_V1412, out), "a real catalog hash is accepted");
    checkStr(out, CATALOG_V1412, "and comes back unchanged");

    checkTrue(DiskLedger::normalizedHash("  " + std::string(CATALOG_V1412) + "\n", out),
              "surrounding whitespace is trimmed");
    checkStr(out, CATALOG_V1412, "and does not survive into the value");

    std::string upper;
    for (const char* p = CATALOG_V1412; *p; ++p) {
        upper += static_cast<char>(*p >= 'a' && *p <= 'z' ? *p - 32 : *p);
    }
    checkTrue(DiskLedger::normalizedHash(upper, out), "an uppercase hash is accepted");
    checkStr(out, CATALOG_V1412, "and is folded to lowercase for comparison");

    // The case the whole function exists for. <sha256></sha256> parses to the
    // empty string, and "non-empty" is not the test - a caller checking that
    // would compare "" against "" and call an unverifiable entry Current.
    checkFalse(DiskLedger::normalizedHash("", out), "the empty string is not a hash");
    checkFalse(DiskLedger::normalizedHash("   ", out), "nor is whitespace");
    checkFalse(DiskLedger::normalizedHash("89b8ae1a", out), "nor is a 8-character prefix");
    checkFalse(DiskLedger::normalizedHash(std::string(63, 'a'), out), "nor 63 hex digits");
    checkFalse(DiskLedger::normalizedHash(std::string(65, 'a'), out), "nor 65");
    checkFalse(DiskLedger::normalizedHash(std::string(64, 'z'), out),
               "nor 64 characters that are not hex");
}

//=============================================================================
// Case folding
//=============================================================================

static void test_case_insensitive() {
    section("filenames fold");

    DiskLedger ledger;
    ledger.recordInstall(COMBO, CATALOG_V1412, nullptr);

    checkTrue(ledger.record("HD1K_COMBO.IMG") != nullptr,
              "a record stored lowercase is found by an uppercase name");
    checkTrue(ledger.record("Hd1K_Combo.Img") != nullptr, "and by a mixed-case one");

    DiskFileFacts f = facts(COMBO_SIZE, T0);
    checkFreshness(ledger.freshness("HD1K_COMBO.IMG", CATALOG_V1412, &f),
                   DiskFreshness::Current,
                   "and the verdict does not change with the case of the name");

    ledger.removeRecord("HD1K_COMBO.IMG");
    checkTrue(ledger.record(COMBO) == nullptr, "removal folds too");
}

//=============================================================================
// The verdicts
//=============================================================================

static void test_freshness_matrix() {
    section("freshness");

    DiskFileFacts f = facts(COMBO_SIZE, T0);

    // No catalog hash: nothing can be decided, whatever else is true.
    {
        DiskLedger ledger;
        ledger.recordInstall(COMBO, CATALOG_V1412, &f);
        checkFreshness(ledger.freshness(COMBO, "", &f), DiskFreshness::Unverifiable,
                       "an entry with no <sha256> is unverifiable even with provenance");
        checkFreshness(ledger.freshness(COMBO, "not-a-hash", &f), DiskFreshness::Unverifiable,
                       "and so is one whose <sha256> is malformed");
    }

    // No file.
    {
        DiskLedger ledger;
        checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, nullptr),
                       DiskFreshness::NotInstalled, "no file on disk is NotInstalled");
        ledger.recordInstall(COMBO, CATALOG_V1412, &f);
        checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, nullptr),
                       DiskFreshness::NotInstalled,
                       "and stays NotInstalled even with a record left behind");
    }

    // Provenance recorded and equal to the catalog.
    {
        DiskLedger ledger;
        ledger.recordInstall(COMBO, CATALOG_V1412, &f);
        checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, &f), DiskFreshness::Current,
                       "a verified download of the current image is Current");
    }

    // THE CASE THE WHOLE FILE EXISTS FOR: downloaded under v1.4.5, catalog now
    // names v1.4.12, and the user has not touched the volume.
    {
        DiskLedger ledger;
        ledger.recordInstall(COMBO, CATALOG_OLD, &f);
        checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, &f),
                       DiskFreshness::SupersededPristine,
                       "a repinned catalog supersedes an untouched download");
    }

    // Same, but the user has saved a file into the volume: same provenance, and
    // the bytes have moved. This must NOT be silently replaced.
    {
        DiskLedger ledger;
        ledger.recordInstall(COMBO, CATALOG_OLD, &f);
        // The guest wrote: new bytes, new mtime, same size (a CP/M volume is
        // fixed-size, which is why size alone can never detect this).
        DiskLedger written = ledger;
        written.recordMeasurement(COMBO, USER_WRITTEN, facts(COMBO_SIZE, T1));
        DiskFileFacts after = facts(COMBO_SIZE, T1);
        checkFreshness(written.freshness(COMBO, CATALOG_V1412, &after),
                       DiskFreshness::SupersededModified,
                       "a superseded volume the user wrote to is SupersededModified");
    }

    // Migration: a file with no ledger entry at all.
    {
        DiskLedger ledger;
        checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, &f),
                       DiskFreshness::NeedsMeasurement,
                       "an install with no ledger must be measured before anything is said");

        ledger.recordMeasurement(COMBO, CATALOG_V1412, f);
        checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, &f),
                       DiskFreshness::UnknownProvenanceMatches,
                       "measuring it to the catalog settles it without provenance");

        DiskLedger other;
        other.recordMeasurement(COMBO, USER_WRITTEN, f);
        checkFreshness(other.freshness(COMBO, CATALOG_V1412, &f),
                       DiskFreshness::UnknownProvenanceDiffers,
                       "measuring it to anything else is genuinely ambiguous");
    }

    // Provenance present, measurement stale: nothing may be decided.
    {
        DiskLedger ledger;
        ledger.recordInstall(COMBO, CATALOG_OLD, &f);
        DiskFileFacts moved = facts(COMBO_SIZE, T1);
        checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, &moved),
                       DiskFreshness::NeedsMeasurement,
                       "a superseded file whose mtime moved must be re-measured, not assumed");
    }
}

//=============================================================================
// What may be done about each verdict
//=============================================================================

static void test_actions() {
    section("action");

    checkAction(DiskLedger::action(DiskFreshness::Unverifiable),
                DiskRefreshAction::None, "nothing is offered for an unverifiable entry");
    checkAction(DiskLedger::action(DiskFreshness::NotInstalled),
                DiskRefreshAction::None, "nor for one that is not installed");
    checkAction(DiskLedger::action(DiskFreshness::Current),
                DiskRefreshAction::None, "nor for a current one");
    checkAction(DiskLedger::action(DiskFreshness::UnknownProvenanceMatches),
                DiskRefreshAction::None,
                "nor for one that hashes to the catalog whoever fetched it");
    checkAction(DiskLedger::action(DiskFreshness::NeedsMeasurement),
                DiskRefreshAction::Measure, "an unmeasured file is measured");

    // The load-bearing pair.
    checkAction(DiskLedger::action(DiskFreshness::SupersededPristine),
                DiskRefreshAction::RefreshAutomatically,
                "ONLY a pristine superseded file may be refreshed unasked");
    checkAction(DiskLedger::action(DiskFreshness::SupersededModified),
                DiskRefreshAction::OfferUpdateLossy,
                "a superseded file the user wrote to is offered, never taken");
    checkAction(DiskLedger::action(DiskFreshness::UnknownProvenanceDiffers),
                DiskRefreshAction::OfferUpdateLossy,
                "and an ambiguous one is offered with the cost said out loud");

    section("no verdict but one reaches the automatic path");

    const DiskFreshness all[] = {
        DiskFreshness::Unverifiable, DiskFreshness::NotInstalled,
        DiskFreshness::Current, DiskFreshness::SupersededPristine,
        DiskFreshness::SupersededModified, DiskFreshness::UnknownProvenanceMatches,
        DiskFreshness::UnknownProvenanceDiffers, DiskFreshness::NeedsMeasurement,
    };
    int automatic = 0;
    for (DiskFreshness f : all) {
        if (DiskLedger::action(f) == DiskRefreshAction::RefreshAutomatically) automatic++;
    }
    check(automatic == 1, "exactly one of the eight verdicts refreshes unasked",
          std::to_string(automatic), "1");
}

static void test_plan_and_user_request() {
    section("plan folds in what the emulator is doing");

    checkPlan(DiskLedger::plan(DiskFreshness::SupersededPristine, false),
              DiskRefreshPlan::RefreshNow, "an unmounted pristine superseded file refreshes");
    checkPlan(DiskLedger::plan(DiskFreshness::SupersededPristine, true),
              DiskRefreshPlan::DeferredMounted,
              "but not while the machine is running off it - the flush would undo it");
    checkPlan(DiskLedger::plan(DiskFreshness::SupersededModified, true),
              DiskRefreshPlan::OfferUpdateLossy,
              "a lossy offer is unaffected by mounting; it was never automatic");
    checkPlan(DiskLedger::plan(DiskFreshness::Current, true),
              DiskRefreshPlan::DoNothing, "and a current file is left alone either way");
    checkPlan(DiskLedger::plan(DiskFreshness::NeedsMeasurement, false),
              DiskRefreshPlan::DoNothing,
              "measuring is not a plan the UI acts on");

    section("an explicit click");

    checkTrue(DiskLedger::allowsUserRequestedUpdate(DiskFreshness::SupersededPristine),
              "is allowed for a superseded file");
    checkTrue(DiskLedger::allowsUserRequestedUpdate(DiskFreshness::SupersededModified),
              "and for one the user wrote to - the control says what it costs");
    checkTrue(DiskLedger::allowsUserRequestedUpdate(DiskFreshness::UnknownProvenanceDiffers),
              "and for an ambiguous one, which is the migration case");
    checkFalse(DiskLedger::allowsUserRequestedUpdate(DiskFreshness::Current),
               "and refused for a current file");
    checkFalse(DiskLedger::allowsUserRequestedUpdate(DiskFreshness::Unverifiable),
               "and for one there is no hash to verify a replacement against");
    checkFalse(DiskLedger::allowsUserRequestedUpdate(DiskFreshness::NotInstalled),
               "and for one that is not there - that is Download, not Update");
    checkFalse(DiskLedger::allowsUserRequestedUpdate(DiskFreshness::NeedsMeasurement),
               "and before it has been measured");
}

//=============================================================================
// The measurement cache
//=============================================================================

static void test_measurement_cache() {
    section("measurementApplies");

    DiskRecord r;
    r.installedCatalogSha256 = CATALOG_V1412;
    r.hasMeasurement = true;
    r.measuredSha256 = CATALOG_V1412;
    r.measuredSize = COMBO_SIZE;
    r.measuredModified = T0;

    checkTrue(DiskLedger::measurementApplies(r, facts(COMBO_SIZE, T0)),
              "a measurement applies to the file it was taken against");
    checkFalse(DiskLedger::measurementApplies(r, facts(COMBO_SIZE, T1)),
               "a moved write time invalidates it");
    checkFalse(DiskLedger::measurementApplies(r, facts(COMBO_SIZE - 1, T0)),
               "and so does a changed size");

    DiskRecord none;
    none.installedCatalogSha256 = CATALOG_V1412;
    checkFalse(DiskLedger::measurementApplies(none, facts(COMBO_SIZE, T0)),
               "a record carrying no measurement never applies");

    section("recordInstall takes the measurement for free");

    DiskLedger ledger;
    DiskFileFacts f = facts(COMBO_SIZE, T0);
    ledger.recordInstall(COMBO, CATALOG_V1412, &f);
    const DiskRecord* got = ledger.record(COMBO);
    checkTrue(got != nullptr, "the record exists");
    if (got) {
        checkStr(got->installedCatalogSha256, CATALOG_V1412, "provenance is the catalog hash");
        checkTrue(got->hasMeasurement, "and the download counts as a measurement");
        checkStr(got->measuredSha256, CATALOG_V1412,
                 "of the same value - the transfer was verified against it");
    }

    DiskLedger noFacts;
    noFacts.recordInstall(COMBO, CATALOG_V1412, nullptr);
    const DiskRecord* bare = noFacts.record(COMBO);
    checkTrue(bare != nullptr, "a stat failure still records provenance");
    if (bare) {
        checkFalse(bare->hasMeasurement, "but claims no measurement it cannot key");
    }

    section("an unusable hash records nothing");

    DiskLedger junk;
    junk.recordInstall(COMBO, "", &f);
    checkTrue(junk.record(COMBO) == nullptr,
              "recordInstall with an empty <sha256> writes no provenance");
    junk.recordMeasurement(COMBO, "nonsense", f);
    checkTrue(junk.record(COMBO) == nullptr, "and recordMeasurement writes no measurement");
}

static void test_measurement_preserves_provenance() {
    section("a measurement does not disturb provenance");

    DiskLedger ledger;
    DiskFileFacts f = facts(COMBO_SIZE, T0);
    ledger.recordInstall(COMBO, CATALOG_OLD, &f);
    ledger.recordMeasurement(COMBO, USER_WRITTEN, facts(COMBO_SIZE, T1));

    const DiskRecord* got = ledger.record(COMBO);
    checkTrue(got != nullptr, "the record survives");
    if (got) {
        checkStr(got->installedCatalogSha256, CATALOG_OLD,
                 "provenance is still where the bytes came from");
        checkStr(got->measuredSha256, USER_WRITTEN, "and the measurement is what they are now");
    }
}

//=============================================================================
// Migration
//=============================================================================

static void test_adopt_provenance() {
    section("adoptProvenanceIfCurrent");

    DiskFileFacts f = facts(COMBO_SIZE, T0);

    // The nineteen-of-twenty case: an image installed before any ledger existed
    // that already hashes to the catalog.
    DiskLedger ledger;
    ledger.recordMeasurement(COMBO, CATALOG_V1412, f);
    checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, &f),
                   DiskFreshness::UnknownProvenanceMatches, "before adoption it is provenance-less");

    ledger.adoptProvenanceIfCurrent(COMBO, CATALOG_V1412);
    checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, &f), DiskFreshness::Current,
                   "after adoption it is simply Current");

    // The case adoption must NOT touch: bytes that do not hash to the catalog.
    // Adopting there would label the superseded image current for ever.
    DiskLedger stale;
    stale.recordMeasurement(COMBO, USER_WRITTEN, f);
    stale.adoptProvenanceIfCurrent(COMBO, CATALOG_V1412);
    checkFreshness(stale.freshness(COMBO, CATALOG_V1412, &f),
                   DiskFreshness::UnknownProvenanceDiffers,
                   "a file that does not hash to the catalog is never adopted");

    // And it must not overwrite provenance that is already known.
    DiskLedger known;
    known.recordInstall(COMBO, CATALOG_OLD, &f);
    known.adoptProvenanceIfCurrent(COMBO, CATALOG_V1412);
    const DiskRecord* got = known.record(COMBO);
    checkTrue(got != nullptr, "the record is still there");
    if (got) {
        checkStr(got->installedCatalogSha256, CATALOG_OLD,
                 "recorded provenance is never overwritten by adoption");
    }

    // Nothing recorded at all: adoption has no measurement to work from.
    DiskLedger empty;
    empty.adoptProvenanceIfCurrent(COMBO, CATALOG_V1412);
    checkTrue(empty.record(COMBO) == nullptr, "adoption invents no record");
}

//=============================================================================
// Persistence
//=============================================================================

static void test_serialization() {
    section("round trip");

    DiskLedger ledger;
    DiskFileFacts f = facts(COMBO_SIZE, T0);
    ledger.recordInstall(COMBO, CATALOG_V1412, &f);
    ledger.recordInstall("hd1k_games.img", CATALOG_OLD, nullptr);

    DiskLedger back = DiskLedger::deserialize(ledger.serialize());
    check(back.records().size() == 2, "both records survive",
          std::to_string(back.records().size()), "2");

    const DiskRecord* combo = back.record(COMBO);
    checkTrue(combo != nullptr, "the combo record is found");
    if (combo) {
        checkStr(combo->installedCatalogSha256, CATALOG_V1412, "provenance round-trips");
        checkTrue(combo->hasMeasurement, "the measurement round-trips");
        checkStr(combo->measuredSha256, CATALOG_V1412, "with its hash");
        check(combo->measuredSize == COMBO_SIZE, "and its size",
              std::to_string(combo->measuredSize), std::to_string(COMBO_SIZE));
        check(combo->measuredModified == T0, "and its write time exactly",
              std::to_string(combo->measuredModified), std::to_string(T0));
    }

    const DiskRecord* games = back.record("hd1k_games.img");
    checkTrue(games != nullptr, "the games record is found");
    if (games) {
        checkFalse(games->hasMeasurement, "and still carries no measurement");
    }

    // The write time is the field that has to survive EXACTLY. A value that
    // comes back a hair off invalidates every measurement on every launch and
    // re-hashes the whole 211 MB library for ever - which is why it is an
    // integer here rather than a floating-point time.
    DiskFileFacts still = facts(COMBO_SIZE, T0);
    checkFreshness(back.freshness(COMBO, CATALOG_V1412, &still), DiskFreshness::Current,
                   "so a reloaded ledger needs no re-measure");

    section("a ledger that will not parse reads as empty");

    checkTrue(DiskLedger::deserialize("").records().empty(), "empty text");
    checkTrue(DiskLedger::deserialize("{").records().empty(), "truncated JSON");
    checkTrue(DiskLedger::deserialize("not json at all").records().empty(), "garbage");
    checkTrue(DiskLedger::deserialize("[1,2,3]").records().empty(), "a JSON array");
    checkTrue(DiskLedger::deserialize("{\"a\": 5}").records().empty(),
              "an entry that is not an object is skipped");

    section("a half-written measurement is dropped, not half-read");

    // Size and hash but no write time: nothing can ever show it still applies,
    // and reading it back would only force a re-measure. Better to say so.
    DiskLedger partial = DiskLedger::deserialize(
        "{\"hd1k_combo.img\":{\"installedCatalogSha256\":\"" + std::string(CATALOG_OLD) +
        "\",\"measuredSha256\":\"" + std::string(CATALOG_OLD) + "\",\"measuredSize\":51380224}}");
    const DiskRecord* got = partial.record(COMBO);
    checkTrue(got != nullptr, "the record is kept");
    if (got) {
        checkStr(got->installedCatalogSha256, CATALOG_OLD, "with its provenance");
        checkFalse(got->hasMeasurement, "and without the incomplete measurement");
    }
    DiskFileFacts f2 = facts(COMBO_SIZE, T0);
    checkFreshness(partial.freshness(COMBO, CATALOG_V1412, &f2),
                   DiskFreshness::NeedsMeasurement,
                   "so it is measured rather than judged on a partial record");
}

//=============================================================================
// The whole scenario, end to end
//=============================================================================

static void test_the_repin_scenario() {
    section("the v1.4.5 -> v1.4.12 repin, as a user meets it");

    // 1. A user on 1.0.23 downloads hd1k_combo.img from the v1.4.5 catalog.
    DiskLedger ledger;
    DiskFileFacts atDownload = facts(COMBO_SIZE, T0);
    ledger.recordInstall(COMBO, CATALOG_OLD, &atDownload);
    checkFreshness(ledger.freshness(COMBO, CATALOG_OLD, &atDownload), DiskFreshness::Current,
                   "on the catalog of the day it is current");

    // 2. The app is updated and the pin moves to v1.4.12. No download happens:
    //    the file is already there, and every check the old code had passes.
    checkFreshness(ledger.freshness(COMBO, CATALOG_V1412, &atDownload),
                   DiskFreshness::SupersededPristine,
                   "after the repin the same file is superseded");
    checkPlan(DiskLedger::plan(ledger.freshness(COMBO, CATALOG_V1412, &atDownload), false),
              DiskRefreshPlan::RefreshNow,
              "and, untouched and unmounted, it is refreshed without being asked");

    // 3. Had the user saved anything into that volume first, the same repin must
    //    reach a different answer.
    DiskLedger written = ledger;
    DiskFileFacts afterSave = facts(COMBO_SIZE, T1);
    written.recordMeasurement(COMBO, USER_WRITTEN, afterSave);
    checkPlan(DiskLedger::plan(written.freshness(COMBO, CATALOG_V1412, &afterSave), false),
              DiskRefreshPlan::OfferUpdateLossy,
              "a volume with the user's work in it is offered, never overwritten");

    // 4. And the user who is on 1.0.23 today, with no ledger at all, gets the
    //    honest answer rather than a guess.
    DiskLedger migrating;
    checkFreshness(migrating.freshness(COMBO, CATALOG_V1412, &atDownload),
                   DiskFreshness::NeedsMeasurement, "a migrating install is measured first");
    migrating.recordMeasurement(COMBO, CATALOG_OLD, atDownload);
    checkFreshness(migrating.freshness(COMBO, CATALOG_V1412, &atDownload),
                   DiskFreshness::UnknownProvenanceDiffers,
                   "and then told its copy differs, without being told why");
    checkTrue(DiskLedger::allowsUserRequestedUpdate(
                  migrating.freshness(COMBO, CATALOG_V1412, &atDownload)),
              "with a control that works");
    checkPlan(DiskLedger::plan(migrating.freshness(COMBO, CATALOG_V1412, &atDownload), false),
              DiskRefreshPlan::OfferUpdateLossy,
              "and no automatic replacement, because it might be their work");
}

static void test_describe() {
    section("what the status column says");

    checkStr(DiskLedger::describe(DiskFreshness::NotInstalled), "Available",
             "not installed");
    checkStr(DiskLedger::describe(DiskFreshness::Current), "Downloaded", "current");
    checkStr(DiskLedger::describe(DiskFreshness::UnknownProvenanceMatches), "Downloaded",
             "matching the catalog without provenance reads the same as current");
    checkStr(DiskLedger::describe(DiskFreshness::NeedsMeasurement), "Downloaded",
             "and an unmeasured file claims nothing");
    checkStr(DiskLedger::describe(DiskFreshness::Unverifiable), "Downloaded",
             "and neither does one with no hash to compare");
    checkStr(DiskLedger::describe(DiskFreshness::SupersededPristine), "Update available",
             "superseded and pristine");
    checkStr(DiskLedger::describe(DiskFreshness::SupersededModified),
             "Update available (overwrites your changes)",
             "superseded and written to - the cost is in the words");
    checkStr(DiskLedger::describe(DiskFreshness::UnknownProvenanceDiffers),
             "Differs from catalog",
             "and the ambiguous case says what is known, not what is guessed");
}

//=============================================================================
// The measurements themselves
//
// These are the half of the machinery that touches a file, and the half whose
// failure is invisible by reading: a wrong hash marks every image in the
// library as differing from the catalog, and the user is told their disks are
// stale when they are not. They were private statics on DiskCatalog, where no
// suite could reach them; DiskHash.h says why they moved.
//=============================================================================

static std::string g_dir;

static std::string writeTempFile(const char* name, const std::string& bytes) {
    const std::string path = g_dir + "\\" + name;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return "";
    if (!bytes.empty()) fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    return path;
}

// SHA-256 of a buffer through BCrypt's ONE-SHOT api, which is a different code
// path from the incremental one sha256File drives. Used below to check the
// 64KB chunking against something that does no chunking at all.
static bool oneShotSha256(const std::string& bytes, std::string& hexOut) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        return false;
    }
    UCHAR digest[32] = {};
    NTSTATUS status = BCryptHash(alg,
                                 nullptr, 0,
                                 reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
                                 static_cast<ULONG>(bytes.size()),
                                 digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!BCRYPT_SUCCESS(status)) return false;

    static const char* kHex = "0123456789abcdef";
    hexOut.clear();
    for (UCHAR b : digest) {
        hexOut += kHex[b >> 4];
        hexOut += kHex[b & 0x0F];
    }
    return true;
}

static void test_sha256_file() {
    section("sha256File against the published vectors");

    // FIPS 180-4's own two, which is what makes this a check of the hash rather
    // than a check that the code agrees with itself.
    std::string hash;
    checkTrue(diskhash::sha256File(writeTempFile("empty.bin", ""), hash),
              "an empty file hashes");
    checkStr(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
             "and is SHA-256 of the empty string");

    checkTrue(diskhash::sha256File(writeTempFile("abc.bin", "abc"), hash), "\"abc\" hashes");
    checkStr(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
             "and is the published SHA-256 of \"abc\"");

    section("sha256File reads the whole file, not the first block");

    // The failure this exists for: a 49MB disk image is read in 64KB blocks, and
    // a loop that stopped after one - or that mistook a read error for EOF -
    // would return a confident hash of a PREFIX. Checked against the one-shot
    // API, which chunks nothing.
    for (size_t size : {(size_t)65535, (size_t)65536, (size_t)65537, (size_t)200000}) {
        std::string bytes;
        bytes.reserve(size);
        for (size_t i = 0; i < size; i++) {
            bytes += static_cast<char>((i * 31 + (i >> 8)) & 0xFF);
        }
        const std::string name = "block_" + std::to_string(size) + ".bin";

        std::string fromFile, fromBuffer;
        checkTrue(diskhash::sha256File(writeTempFile(name.c_str(), bytes), fromFile),
                  std::to_string(size) + " bytes hashes");
        checkTrue(oneShotSha256(bytes, fromBuffer),
                  std::to_string(size) + " bytes hashes one-shot");
        checkStr(fromFile, fromBuffer,
                 "chunked and one-shot agree at " + std::to_string(size) + " bytes");
    }

    section("sha256File on a file that is not there");

    std::string untouched = "sentinel";
    checkFalse(diskhash::sha256File(g_dir + "\\no_such_file.bin", untouched),
               "a missing file fails");
    checkStr(untouched, "sentinel", "and writes nothing into the output");
    checkFalse(diskhash::sha256File(g_dir, untouched), "and so does a directory");
}

static void test_stat_file() {
    section("statFile");

    const std::string path = writeTempFile("stat.bin", std::string(1234, 'x'));

    DiskFileFacts facts;
    checkTrue(diskhash::statFile(path, facts), "a real file stats");
    check(facts.size == 1234, "with its size", std::to_string(facts.size), "1234");
    checkTrue(facts.modified > 0, "and a write time that is not zero");

    // The pair has to move together with the file, or a re-downloaded image is
    // judged on the previous image's hash.
    DiskFileFacts again;
    checkTrue(diskhash::statFile(path, again), "stating it twice works");
    check(again.modified == facts.modified, "and gives the same write time",
          std::to_string(again.modified), std::to_string(facts.modified));

    writeTempFile("stat.bin", std::string(999, 'y'));
    DiskFileFacts rewritten;
    checkTrue(diskhash::statFile(path, rewritten), "the rewritten file stats");
    check(rewritten.size == 999, "with the new size", std::to_string(rewritten.size), "999");

    // And the two together are what measurementApplies compares, so a rewrite
    // must invalidate a measurement taken before it.
    DiskRecord r;
    r.hasMeasurement = true;
    r.measuredSha256 = CATALOG_V1412;
    r.measuredSize = facts.size;
    r.measuredModified = facts.modified;
    checkFalse(DiskLedger::measurementApplies(r, rewritten),
               "so a measurement taken before the rewrite no longer applies");

    DiskFileFacts none;
    checkFalse(diskhash::statFile(g_dir + "\\no_such_file.bin", none),
               "a missing file does not stat");
    checkFalse(diskhash::statFile(g_dir, none), "and neither does a directory");
}

// The two halves meeting: a real file, really hashed, driving a real verdict.
static void test_measurement_end_to_end() {
    section("a real file drives a real verdict");

    const std::string path = writeTempFile("volume.img", std::string(4096, 'A'));

    std::string measured;
    checkTrue(diskhash::sha256File(path, measured), "the file hashes");
    DiskFileFacts facts;
    checkTrue(diskhash::statFile(path, facts), "and stats");

    // Downloaded when the catalog named exactly these bytes.
    DiskLedger ledger;
    ledger.recordInstall("volume.img", measured, &facts);
    checkFreshness(ledger.freshness("volume.img", measured, &facts), DiskFreshness::Current,
                   "against that catalog it is current");

    // The publisher moves the image. Ours has not been touched.
    checkFreshness(ledger.freshness("volume.img", CATALOG_V1412, &facts),
                   DiskFreshness::SupersededPristine,
                   "against a moved catalog it is superseded, and pristine");

    // Now the guest writes to the volume. Re-measured from the real file, the
    // same ledger must reach the answer that does not destroy the write.
    writeTempFile("volume.img", std::string(4096, 'B'));
    std::string after;
    DiskFileFacts afterFacts;
    checkTrue(diskhash::sha256File(path, after), "the written file hashes");
    checkTrue(diskhash::statFile(path, afterFacts), "and stats");
    checkTrue(after != measured, "to something different");

    ledger.recordMeasurement("volume.img", after, afterFacts);
    checkFreshness(ledger.freshness("volume.img", CATALOG_V1412, &afterFacts),
                   DiskFreshness::SupersededModified,
                   "and the verdict becomes the one that will not overwrite it");
    checkPlan(DiskLedger::plan(ledger.freshness("volume.img", CATALOG_V1412, &afterFacts), false),
              DiskRefreshPlan::OfferUpdateLossy, "so nothing happens without being asked");
}

//=============================================================================

int main() {
    const char* tmp = std::getenv("TEMP");
    if (!tmp || !*tmp) tmp = std::getenv("TMP");
    if (!tmp || !*tmp) tmp = ".";
    g_dir = std::string(tmp) + "\\z80cpmw_test_ledger";
    CreateDirectoryA(g_dir.c_str(), nullptr);

    printf("=== Disk provenance suite ===\n");

    test_normalized_hash();
    test_case_insensitive();
    test_freshness_matrix();
    test_actions();
    test_plan_and_user_request();
    test_measurement_cache();
    test_measurement_preserves_provenance();
    test_adopt_provenance();
    test_serialization();
    test_the_repin_scenario();
    test_describe();
    test_sha256_file();
    test_stat_file();
    test_measurement_end_to_end();

    // Leave nothing behind; the files above are a few hundred KB in %TEMP%.
    WIN32_FIND_DATAA found = {};
    HANDLE search = FindFirstFileA((g_dir + "\\*").c_str(), &found);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            DeleteFileA((g_dir + "\\" + found.cFileName).c_str());
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    RemoveDirectoryA(g_dir.c_str());

    printf("\n===============================\n");
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
