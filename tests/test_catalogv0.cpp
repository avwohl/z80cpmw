/*
 * test_catalogv0.cpp - The interface-v0 catalog documents.
 *
 * This is the suite for the half of the URL migration that can be wrong
 * silently. Everything else about it is loud: a wrong host does not resolve, a
 * wrong path is a 404. What is quiet is a document read slightly wrong - the
 * 3.6.0 entry offered to a build that cannot boot a 3.6.0 ROM, a preview release
 * shown as though it were recommended, a base_url concatenated with a separator
 * that was already there, a version byte read out of "0x35" as the number 0.
 *
 * It has three jobs.
 *
 * THE FIRST IS THAT THE PARSER NEVER THROWS. DiskCatalog fetches these documents
 * on a DETACHED thread, and the parser it replaces called std::stoull on a
 * <size> element straight out of the HTTP response - so one malformed catalog
 * was std::terminate with no dump, no message and no callback. Every section
 * below that feeds this code something ill-formed exists for that reason: the
 * required answer is "false, with a sentence", never an exception.
 *
 * THE SECOND IS CATALOG_SCHEMA.md's compatibility rules, which are promises this
 * client has to keep rather than checks it may skip: ignore unknown fields at
 * every level, key on id, tolerate entries appearing and disappearing, tolerate
 * a roms[] this build never looks at, display an unknown status rather than
 * failing on it.
 *
 * THE THIRD IS WHICH RELEASE GETS OFFERED, which is the one decision here that a
 * user can see. A build must offer what its own core says it can boot - not a
 * compiled-in list, because the client and the core are separate repositories
 * and either can be ahead - and when the answer is "none of them", that has to
 * be reportable rather than a silent fallback to something unbootable.
 *
 * The documents below are the REAL published ones, byte for byte out of
 * romwbw_disks/catalog/v0/. The index is whole; the catalog is an excerpt of the
 * 3.5.1 one carrying its header, both ROMs and four of its twenty disks -
 * including hd1k_ws4, which exists under 3.5.1 and not under 3.6.0 and is
 * therefore the entry that proves ids come and go.
 *
 * It needs no window, no data folder, no network and no Windows: CatalogV0.cpp
 * holds no Win32, no WinHTTP and no threads, which is the whole reason this
 * suite can exist at all.
 *
 * Build and run: tests\run_tests.bat
 */

#include "CatalogV0.h"
#include "DiskMigrationV0.h"

#include <cctype>
#include <cstdio>
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

static void checkNum(unsigned long long got, unsigned long long want,
                     const std::string& what) {
    check(got == want, what, std::to_string(got), std::to_string(want));
}

//=============================================================================
// The real documents
//=============================================================================

// romwbw_disks/catalog/v0/index.json, verbatim. The committed copy is
// byte-identical to the asset published on the catalog-v0 tag, so this is the
// document a shipped build will actually be handed.
static const char* const REAL_INDEX = R"JSON({
  "schema": "romwbw-disks-index",
  "schema_version": 1,
  "interface": "v0",
  "repo": "https://github.com/avwohl/romwbw_disks",
  "index_url": "https://github.com/avwohl/romwbw_disks/releases/download/catalog-v0/index-v0.json",
  "romwbw_versions": [
    {
      "romwbw_version": "3.5.1",
      "label": "RomWBW 3.5.1",
      "status": "stable",
      "default": true,
      "released": "2025-05-21",
      "hbios": {
        "major": 3,
        "minor": 5,
        "update": 1,
        "patch": 0,
        "ver_byte": "0x35",
        "upd_byte": "0x10",
        "sysver_de": "0x3510"
      },
      "release_tag": "v0-romwbw-3.5.1",
      "catalog_url": "https://github.com/avwohl/romwbw_disks/releases/download/v0-romwbw-3.5.1/catalog-v0-3.5.1.json",
      "catalog_sha256": "7a5411b329be606c2bcc7b8d2b051b8fca9a2906f780d65fc98221cb6b61ed65",
      "catalog_size": 11826,
      "generation": 1,
      "disks_xml_url": "https://github.com/avwohl/romwbw_disks/releases/download/v0-romwbw-3.5.1/disks-v0-3.5.1.xml",
      "rom_count": 2,
      "disk_count": 20,
      "notes": [
        "The RomWBW release every shipped client is pinned to today.",
        "CBIOS banner in the boot slices reads 'CBIOS v3.5.1 [WBW]'."
      ]
    },
    {
      "romwbw_version": "3.6.0",
      "label": "RomWBW 3.6.0",
      "status": "preview",
      "default": false,
      "released": "2026-03-28",
      "hbios": {
        "major": 3,
        "minor": 6,
        "update": 0,
        "patch": 0,
        "ver_byte": "0x36",
        "upd_byte": "0x00",
        "sysver_de": "0x3600"
      },
      "release_tag": "v0-romwbw-3.6.0",
      "catalog_url": "https://github.com/avwohl/romwbw_disks/releases/download/v0-romwbw-3.6.0/catalog-v0-3.6.0.json",
      "catalog_sha256": "3907ba2f23f2307fdbc220fd20e3209b877357b5df1057b86db86a905090191f",
      "catalog_size": 14694,
      "generation": 1,
      "disks_xml_url": "https://github.com/avwohl/romwbw_disks/releases/download/v0-romwbw-3.6.0/disks-v0-3.6.0.xml",
      "rom_count": 2,
      "disk_count": 24,
      "notes": [
        "PREVIEW. No released client can load a v3.6.0 ROM yet: romwbw_emu's emu_validate_rom_hcb (src/emu_init.cc:52-60) refuses any ROM whose HCB version bytes differ from the compile-time ROMWBW_PIN_STR. See docs/CLIENT_MIGRATION.md.",
        "hd1k_ws4.img does not exist in v3.6.0; upstream combo.def slice 5 is 'wp' (WordStar / word processing) where v3.5.1 had 'ws4'.",
        "NVRAM checksums do not validate across a version change: RomWBW's NVSW_CHECKSUM XORs the version bytes into the seed, so a blob saved under 3.5.1 silently resets under a 3.6.0 ROM. Clients must namespace their NVRAM store per RomWBW version.",
        "Do NOT build from archive/romwbw-v3.6.0/SBC_simh_std_v360.rom in romwbw_emu: it is a v3.6.0-dev.46 snapshot from 2025-12-12, not the release, and its HCB reads 36 00 so a version check cannot tell the difference."
      ]
    }
  ]
})JSON";

// An excerpt of romwbw_disks/catalog/v0/3.5.1/catalog.json: its header, both
// roms[] entries, and four of its twenty disks. Every value is the published
// one. The four disks are the two defaults, one plain single-slice image, and
// hd1k_ws4 - which 3.6.0 does not carry.
static const char* const REAL_CATALOG_351 = R"JSON({
    "schema": "romwbw-disks-catalog",
    "schema_version": 1,
    "interface": "v0",
    "romwbw_version": "3.5.1",
    "generation": 1,
    "status": "stable",
    "release_tag": "v0-romwbw-3.5.1",
    "base_url": "https://github.com/avwohl/romwbw_disks/releases/download/v0-romwbw-3.5.1/",
    "hbios": {
        "major": 3,
        "minor": 5,
        "update": 1,
        "patch": 0,
        "ver_byte": "0x35",
        "upd_byte": "0x10",
        "sysver_de": "0x3510"
    },
    "upstream": {
        "tag": "v3.5.1",
        "package_url": "https://github.com/wwarthen/RomWBW/releases/download/v3.5.1/RomWBW-v3.5.1-Package.zip",
        "package_sha256": "e696ff2faf8f6420367ae3d0ad14c9daf1d7b08727b2699d005e877cc755da20"
    },
    "notes": [
        "The RomWBW release every shipped client is pinned to today.",
        "CBIOS banner in the boot slices reads 'CBIOS v3.5.1 [WBW]'."
    ],
    "roms": [
        {
            "id": "emu_avw",
            "filename": "emu_avw-v0-3.5.1.rom",
            "name": "EMU AVW",
            "description": "Standard emulator ROM. Our HBIOS proxy in bank 0 over the RomWBW SBC_simh_std ROM disk in banks 1-15. This is the ROM every shipped client bundles today.",
            "size": 524288,
            "sha256": "c7abc580b3285a33e439c0d6724a9d64dd3e93733a4fc2c1b80b0bfd91f9c580",
            "default": true,
            "hcb": {
                "marker": "57 A8",
                "version": "0x35",
                "update": "0x10",
                "platform": 0
            },
            "built_from": {
                "bank0": "src/emu_hbios.asm",
                "banks_1_15": "Binary/SBC_simh_std.rom"
            }
        },
        {
            "id": "emu_rcz80",
            "filename": "emu_rcz80-v0-3.5.1.rom",
            "name": "EMU RCZ80",
            "description": "Alternate emulator ROM. Same HBIOS proxy in bank 0, but banks 1-15 come from the RomWBW RCZ80_std ROM disk, so the ROM-resident applications match an RC2014 Z80 build.",
            "size": 524288,
            "sha256": "ee3adea5caa9b3da4005e6a3d627e3eaf4ebd56f5795a5c41f6a90492850c4a7",
            "default": false,
            "hcb": {
                "marker": "57 A8",
                "version": "0x35",
                "update": "0x10",
                "platform": 0
            },
            "built_from": {
                "bank0": "src/emu_hbios.asm",
                "banks_1_15": "Binary/RCZ80_std.rom"
            }
        }
    ],
    "disks": [
        {
            "id": "hd1k_combo",
            "filename": "hd1k_combo-v0-3.5.1.img",
            "name": "Combo (Recommended)",
            "description": "Six-slice disk: CP/M 2.2, ZSDOS, NZCOM, CP/M 3, ZPM3 and a WordStar 4 applications slice, plus R8/W8 host file transfer on slice 0. Best starter disk.",
            "size": 51380224,
            "sha256": "0ca4ec60cb8bca71b8f0287c4b634c3126887be483db9b59b41bdff424f89303",
            "license": "Mixed",
            "format": "hd1k_combo",
            "bootable": true,
            "cbios": "CBIOS v3.5.1 [WBW]",
            "host_transfer": true,
            "upstream": "Binary/hd1k_combo.img",
            "slices": 6,
            "defaultSlot": 0
        },
        {
            "id": "hd1k_cpm22",
            "filename": "hd1k_cpm22-v0-3.5.1.img",
            "name": "CP/M 2.2",
            "description": "Digital Research CP/M 2.2 operating system with standard utilities.",
            "size": 8388608,
            "sha256": "bfe32f3b5d6ebc8c9d5615a3390d61bba4cb565039d4fb144a65a9502515cbe6",
            "license": "Mixed",
            "format": "hd1k",
            "bootable": true,
            "cbios": "CBIOS v3.5.1 [WBW]",
            "host_transfer": false,
            "upstream": "Binary/hd1k_cpm22.img"
        },
        {
            "id": "hd1k_games",
            "filename": "hd1k_games-v0-3.5.1.img",
            "name": "Games",
            "description": "Collection of classic CP/M games including adventures and arcade titles.",
            "size": 8388608,
            "sha256": "7f33738c4c8be0655ee9452370fe450146492e9174347c22b3300ac2377d0abd",
            "license": "Abandonware",
            "format": "hd1k",
            "bootable": false,
            "cbios": null,
            "host_transfer": false,
            "upstream": "Binary/hd1k_games.img"
        },
        {
            "id": "hd1k_ws4",
            "filename": "hd1k_ws4-v0-3.5.1.img",
            "name": "WordStar 4",
            "description": "WordStar 4.0 - classic word processor for CP/M.",
            "size": 8388608,
            "sha256": "fcdf308753142d2d2957636ac74721f8e3ef27a4f0aeec81f41196f631c1f2c9",
            "license": "Abandonware",
            "format": "hd1k",
            "bootable": false,
            "cbios": null,
            "host_transfer": false,
            "upstream": "Binary/hd1k_ws4.img"
        }
    ]
})JSON";

// The cores this suite pretends to be. The real one is
// emu_romwbw_release_supported() out of the linked romwbw_emu; these three are
// the three shapes it can have, and the third is not hypothetical - it is what a
// build too old or too new for the repository looks like.
static bool supportsBoth(unsigned char ver, unsigned char upd) {
    return (ver == 0x35 && upd == 0x10) || (ver == 0x36 && upd == 0x00);
}
static bool supports351Only(unsigned char ver, unsigned char upd) {
    return ver == 0x35 && upd == 0x10;
}
static bool supportsNothing(unsigned char, unsigned char) { return false; }

//=============================================================================
// Sections
//=============================================================================

static void test_hex_bytes() {
    section("the two version bytes, which are hex STRINGS");

    // The index writes ver_byte and upd_byte as "0x35" and "0x10" while the four
    // integers beside them are numbers, and a ROM's hcb writes `version` as a
    // string next to `platform` as an integer. CATALOG_SCHEMA.md says the
    // asymmetry is real and not to assume a uniform encoding, so these are read
    // as what they are.
    unsigned char b = 0;
    checkTrue(catalogv0::parseHexByte("0x35", b), "the 3.5.1 version byte reads");
    checkNum(b, 0x35, "as 0x35");
    checkTrue(catalogv0::parseHexByte("0x10", b), "and its update byte");
    checkNum(b, 0x10, "as 0x10");
    checkTrue(catalogv0::parseHexByte("0x00", b), "an update byte of zero is a value");
    checkNum(b, 0x00, "and reads as one");
    checkTrue(catalogv0::parseHexByte("0X3F", b), "an upper-case 0X is accepted");
    checkNum(b, 0x3F, "with upper-case digits");

    // The refusals matter more than the acceptances. A number accessor asked for
    // "0x35" yields 0, and 0 is a perfectly plausible upd_byte - 3.6.0's is
    // exactly that - so a silently wrong answer here is one that looks right.
    b = 0xAA;
    checkFalse(catalogv0::parseHexByte("35", b), "no 0x prefix is not this encoding");
    checkFalse(catalogv0::parseHexByte("0x350", b), "nor is a third digit");
    checkFalse(catalogv0::parseHexByte("0x", b), "nor is a prefix with no digits");
    checkFalse(catalogv0::parseHexByte("", b), "nor is nothing at all");
    checkFalse(catalogv0::parseHexByte("0xzz", b), "nor are digits that are not digits");
    checkNum(b, 0xAA, "and a refusal leaves the caller's byte alone");
}

static void test_real_index() {
    section("the published index, as a client meets it");

    std::vector<catalogv0::IndexEntry> entries;
    std::string error;
    checkTrue(catalogv0::parseIndex(REAL_INDEX, entries, error), "the real index parses");
    checkStr(error, "", "with nothing to say about it");
    checkNum(entries.size(), 2, "and carries the two published RomWBW releases");
    if (entries.size() != 2) return;

    checkStr(entries[0].romwbwVersion, "3.5.1", "the first is 3.5.1");
    checkStr(entries[0].status, "stable", "published stable");
    checkTrue(entries[0].isDefault, "and it is the index's default");
    checkNum(entries[0].verByte, 0x35, "ver_byte 0x35");
    checkNum(entries[0].updByte, 0x10, "upd_byte 0x10");
    checkNum(entries[0].catalogSize, 11826, "with the catalog size the index publishes");
    checkStr(entries[0].catalogSha256,
             "7a5411b329be606c2bcc7b8d2b051b8fca9a2906f780d65fc98221cb6b61ed65",
             "and the sha256 that catalog is verified against before it is parsed");
    checkStr(entries[0].catalogUrl,
             "https://github.com/avwohl/romwbw_disks/releases/download/"
             "v0-romwbw-3.5.1/catalog-v0-3.5.1.json",
             "the catalog URL is absolute and is NOT built from the release tag");
    checkNum(entries[0].generation, 1, "generation 1");
    checkNum(entries[0].diskCount, 20, "20 disks");
    checkNum(entries[0].romCount, 2, "2 ROMs");

    checkStr(entries[1].romwbwVersion, "3.6.0", "the second is 3.6.0");
    checkStr(entries[1].status, "preview", "published preview");
    checkFalse(entries[1].isDefault, "and is not the default");
    checkNum(entries[1].verByte, 0x36, "ver_byte 0x36");
    checkNum(entries[1].updByte, 0x00, "upd_byte 0x00 - a zero that is a value");
    checkNum(entries[1].diskCount, 24, "24 disks, four more than 3.5.1");
}

static void test_preview_is_marked() {
    section("a preview release has to LOOK like one");

    std::vector<catalogv0::IndexEntry> entries;
    std::string error;
    if (!catalogv0::parseIndex(REAL_INDEX, entries, error) || entries.size() != 2) {
        checkTrue(false, "the index parsed");
        return;
    }

    checkStr(catalogv0::displayLabel(entries[0]), "RomWBW 3.5.1",
             "a stable release says nothing extra");
    checkStr(catalogv0::displayLabel(entries[1]), "RomWBW 3.6.0 (preview)",
             "a preview release says so, in the menu, where the user chooses");

    // `status` is free text copied from the version metadata, not a closed set.
    // An unknown value has to reach the user's eyes rather than be dropped as
    // unrecognised or - worse - treated as stable.
    catalogv0::IndexEntry made;
    made.romwbwVersion = "3.7.0";
    made.label = "RomWBW 3.7.0";
    made.status = "experimental";
    checkStr(catalogv0::displayLabel(made), "RomWBW 3.7.0 (experimental)",
             "and so does a status this build has never heard of");

    made.label.clear();
    made.status.clear();
    checkStr(catalogv0::displayLabel(made), "RomWBW 3.7.0",
             "an entry with no label still has a name to show");
}

static void test_which_releases_are_offered() {
    section("which releases this build offers, and who decides");

    std::vector<catalogv0::IndexEntry> entries;
    std::string error;
    if (!catalogv0::parseIndex(REAL_INDEX, entries, error) || entries.size() != 2) {
        checkTrue(false, "the index parsed");
        return;
    }

    // A core that can boot both - which is what romwbw_emu v1.39 is today.
    std::vector<size_t> both = catalogv0::runnableVersions(entries, supportsBoth);
    checkNum(both.size(), 2, "a core that boots both is offered both");
    checkNum(catalogv0::chooseVersion(entries, both, ""), 0,
             "and with no preference it takes the index's default, 3.5.1");
    checkNum(catalogv0::chooseVersion(entries, both, "3.6.0"), 1,
             "a stored preference for 3.6.0 is honoured");

    // A core built for one release, which is every SHIPPED client today.
    std::vector<size_t> one = catalogv0::runnableVersions(entries, supports351Only);
    checkNum(one.size(), 1, "a core that boots only 3.5.1 is offered only 3.5.1");
    checkNum(catalogv0::chooseVersion(entries, one, ""), 0, "and gets it");
    checkNum(catalogv0::chooseVersion(entries, one, "3.6.0"), 0,
             "a preference it cannot boot falls back rather than failing - a user "
             "who downgrades the app must still get a working catalog");

    // The reportable one. This is not a network failure and must not be dressed
    // as one: it means the client and the repository have drifted apart, and
    // quietly fetching some other release's images would download disks this
    // machine cannot boot and hand the user an HBIOS/CBIOS mismatch instead of
    // an explanation.
    std::vector<size_t> none = catalogv0::runnableVersions(entries, supportsNothing);
    checkNum(none.size(), 0, "a core that boots neither is offered neither");
    checkTrue(catalogv0::chooseVersion(entries, none, "3.5.1") == (size_t)-1,
              "and choosing from nothing yields nothing, not the first entry");

    // An entry whose hbios pair could not be read can never be run: the pair is
    // the whole of what decides whether the core can boot it.
    catalogv0::IndexEntry blind;
    blind.romwbwVersion = "3.9.9";
    blind.haveHbios = false;
    std::vector<catalogv0::IndexEntry> withBlind = entries;
    withBlind.push_back(blind);
    checkNum(catalogv0::runnableVersions(withBlind, supportsBoth).size(), 2,
             "an entry with no readable version bytes is never offered");
}

static void test_index_tolerance() {
    section("what an index is allowed to do to a shipped build");

    std::vector<catalogv0::IndexEntry> entries;
    std::string error;

    // Adding a field is explicitly not an interface break, so a build that
    // refused an unknown key would break on the next publication.
    checkTrue(catalogv0::parseIndex(
        R"JSON({"schema":"romwbw-disks-index","invented_later":{"a":[1,2]},
                "romwbw_versions":[{"romwbw_version":"3.5.1","catalog_url":"u",
                "hbios":{"ver_byte":"0x35","upd_byte":"0x10","invented":1},
                "also_new":true}]})JSON", entries, error),
        "unknown fields are ignored at every level");
    checkNum(entries.size(), 1, "and the entry still arrives");

    // A NEW entry shaped in a way this build cannot use must not take the ones
    // it can use down with it.
    checkTrue(catalogv0::parseIndex(
        R"JSON({"romwbw_versions":[
                {"romwbw_version":"3.5.1","catalog_url":"u",
                 "hbios":{"ver_byte":"0x35","upd_byte":"0x10"}},
                {"romwbw_version":"3.7.0"},
                {"catalog_url":"u2","hbios":{"ver_byte":"0x37","upd_byte":"0x00"}},
                "not even an object"]})JSON", entries, error),
        "an entry this build cannot use is skipped, not fatal");
    checkNum(entries.size(), 1, "and only the usable one survives");

    // The refusals. Each of these is a document that would otherwise be read as
    // an empty but valid index, which reads to a user as "there are no disks".
    checkFalse(catalogv0::parseIndex("", entries, error), "an empty response is not an index");
    checkFalse(catalogv0::parseIndex("{\"romwbw_versions\":[", entries, error),
               "nor is a truncated one");
    checkFalse(catalogv0::parseIndex("[1,2,3]", entries, error), "nor is an array");
    checkFalse(catalogv0::parseIndex("{\"romwbw_versions\":{}}", entries, error),
               "nor is a versions member that is not an array");
    checkFalse(catalogv0::parseIndex(
        R"JSON({"schema":"romwbw-disks-catalog","base_url":"x/"})JSON", entries, error),
        "and a per-version CATALOG read as an index is refused by name");
    checkTrue(!error.empty(), "every refusal carries a sentence for the status line");
}

static void test_real_catalog() {
    section("the published 3.5.1 catalog");

    catalogv0::Catalog catalog;
    std::string error;
    checkTrue(catalogv0::parseCatalog(REAL_CATALOG_351, catalog, error),
              "the real catalog parses");
    checkStr(catalog.romwbwVersion, "3.5.1", "for RomWBW 3.5.1");
    checkStr(catalog.releaseTag, "v0-romwbw-3.5.1", "on its own immutable tag");
    checkNum(catalog.generation, 1, "at generation 1");
    checkStr(catalog.baseUrl,
             "https://github.com/avwohl/romwbw_disks/releases/download/v0-romwbw-3.5.1/",
             "with a base_url that ends in a slash");

    checkNum(catalog.roms.size(), 2, "two ROMs, which this build reads and does not fetch");
    if (catalog.roms.size() == 2) {
        checkStr(catalog.roms[0].id, "emu_avw", "the default ROM's id");
        checkTrue(catalog.roms[0].isDefault, "and it is the default");
        checkNum(catalog.roms[0].size, 524288, "512 KB");
        checkStr(catalog.roms[0].sha256,
                 "c7abc580b3285a33e439c0d6724a9d64dd3e93733a4fc2c1b80b0bfd91f9c580",
                 "and the hash the bundled roms/emu_avw.rom already has");
        checkTrue(catalog.roms[0].haveHcb, "its HCB bytes are published");
        checkNum(catalog.roms[0].hcbVersion, 0x35, "version 0x35");
        checkNum(catalog.roms[0].hcbUpdate, 0x10, "update 0x10 - checkable before a 512 KB fetch");
    }

    checkStr(catalog.disks.empty() ? "" : catalog.disks[0].id, "hd1k_combo",
             "the first disk is the combo");
    if (!catalog.disks.empty()) {
        const catalogv0::DiskItem& combo = catalog.disks[0];
        checkStr(combo.filename, "hd1k_combo-v0-3.5.1.img",
                 "under its interface-v0 name, which carries the release");
        checkNum(combo.size, 51380224, "49 MB");
        checkStr(combo.sha256,
                 "0ca4ec60cb8bca71b8f0287c4b634c3126887be483db9b59b41bdff424f89303",
                 "and the v0 hash, which is the ONE of the twenty that moved");
        checkStr(combo.format, "hd1k_combo", "a six-slice image");
        checkTrue(combo.bootable, "bootable");
        checkTrue(combo.hostTransfer, "and the only image carrying R8/W8");
        checkTrue(combo.haveDefaultSlot, "it names a default slot");
        checkNum(combo.defaultSlot, 0, "slot 0");

        checkStr(catalogv0::assetUrl(catalog.baseUrl, combo.filename),
                 "https://github.com/avwohl/romwbw_disks/releases/download/"
                 "v0-romwbw-3.5.1/hd1k_combo-v0-3.5.1.img",
                 "and base_url + filename is the whole of an asset URL - NOTHING "
                 "is inserted between them, which is the client-side fixup v0 "
                 "exists to delete");
    }

    // The absent optionals, which are absent on nineteen of the twenty.
    bool sawSingleSlice = false;
    for (const auto& d : catalog.disks) {
        if (d.id != "hd1k_cpm22") continue;
        sawSingleSlice = true;
        checkFalse(d.haveDefaultSlot,
                   "a single-slice image names no default slot, and absent is not slot 0");
        checkFalse(d.hostTransfer, "and carries no host transfer");
    }
    checkTrue(sawSingleSlice, "the single-slice image is in the excerpt");

    // The entry that proves ids come and go between releases. A client that
    // hardcoded twenty, or indexed by position, breaks on 3.6.0.
    bool sawWs4 = false;
    for (const auto& d : catalog.disks) {
        if (d.id == "hd1k_ws4") sawWs4 = true;
    }
    checkTrue(sawWs4, "hd1k_ws4 is in 3.5.1 - and it is NOT in 3.6.0");
}

static void test_catalog_tolerance() {
    section("what a catalog is allowed to do to a shipped build");

    catalogv0::Catalog catalog;
    std::string error;

    // The one field whose absence makes the document useless. Everything else
    // this build reads has a sane absence.
    checkFalse(catalogv0::parseCatalog(R"JSON({"disks":[]})JSON", catalog, error),
               "a catalog with no base_url is refused - there is no URL without it");

    checkTrue(catalogv0::parseCatalog(
        R"JSON({"base_url":"https://x/","roms":[],"disks":[]})JSON", catalog, error),
        "an empty roms[] and an empty disks[] are a real answer, not an error");
    checkNum(catalog.roms.size(), 0, "no ROMs");
    checkNum(catalog.disks.size(), 0, "no disks");

    // "do not assume emu_avw is present" - nothing here looks for it by name, so
    // a catalog of one unfamiliar ROM parses like any other.
    checkTrue(catalogv0::parseCatalog(
        R"JSON({"base_url":"https://x/","roms":[{"id":"emu_future","filename":"f.rom"}]})JSON",
        catalog, error), "a catalog with no emu_avw is still a catalog");
    checkNum(catalog.roms.size(), 1, "and its one unfamiliar ROM is read");
    checkFalse(catalog.roms.empty() ? true : catalog.roms[0].haveHcb,
               "a ROM with no hcb block is readable, and says it has no version bytes");

    // THE THROWING CASES. Every one of these is a value of the wrong JSON type
    // where the schema names another, which is what `.get<std::string>()` and
    // std::stoull turn into a dead process on a detached thread. The required
    // behaviour is that the wrong type reads exactly as an absent one.
    checkTrue(catalogv0::parseCatalog(
        R"JSON({"base_url":"https://x/","generation":"one","disks":[
                {"id":"a","filename":"a.img","size":"8388608","sha256":123,
                 "bootable":"yes","defaultSlot":"first","license":[],"format":null}]})JSON",
        catalog, error), "every field of the wrong type is survivable");
    checkNum(catalog.disks.size(), 1, "and the entry still arrives");
    if (!catalog.disks.empty()) {
        checkNum(catalog.disks[0].size, 0, "a size that is a string reads as no size");
        checkStr(catalog.disks[0].sha256, "",
                 "a sha256 that is a number reads as no hash, which normalizedHash "
                 "already knows how to mean");
        checkFalse(catalog.disks[0].bootable, "a bootable that is a string reads as false");
        checkFalse(catalog.disks[0].haveDefaultSlot,
                   "and a defaultSlot that is a string is no default slot");
    }
    checkNum(catalog.generation, 0, "a generation that is a string reads as 0");

    // A negative size must not wrap into the largest number there is: the size
    // is what isDiskDownloaded compares a cached file against, and 2^64-1 would
    // report every image in the library as truncated.
    checkTrue(catalogv0::parseCatalog(
        R"JSON({"base_url":"https://x/","disks":[{"id":"a","filename":"a.img","size":-1}]})JSON",
        catalog, error), "a negative size parses");
    checkNum(catalog.disks.empty() ? 1 : catalog.disks[0].size, 0,
             "and reads as no size rather than wrapping to 2^64-1");

    // An entry with no id has no stable key, and one with no filename names no
    // asset. Skipped rather than fatal, so a future field that this build cannot
    // read does not cost the entries it can.
    checkTrue(catalogv0::parseCatalog(
        R"JSON({"base_url":"https://x/","disks":[
                {"filename":"nameless.img"},{"id":"b"},
                {"id":"c","filename":"c.img"},"not an object"]})JSON",
        catalog, error), "unusable disk entries are skipped");
    checkNum(catalog.disks.size(), 1, "and only the usable one survives");

    checkFalse(catalogv0::parseCatalog("", catalog, error), "an empty response is not a catalog");
    checkFalse(catalogv0::parseCatalog("{\"base_url\":", catalog, error),
               "nor is a truncated one");
    checkFalse(catalogv0::parseCatalog(
        R"JSON({"schema":"romwbw-disks-index","romwbw_versions":[]})JSON", catalog, error),
        "and an INDEX read as a catalog is refused by name");
}

static void test_the_url_that_is_compiled_in() {
    section("the one URL in the binary");

    // There used to be two, both interpolated from RELEASE_TAG = "v1.4.12".
    // There is now one, and it is the index; everything else is read out of a
    // document. If this string is ever wrong the application fetches nothing at
    // all, which is the failure mode worth having - a stale tag fetched the
    // wrong disks and said nothing.
    checkStr(catalogv0::INDEX_URL,
             "https://github.com/avwohl/romwbw_disks/releases/download/"
             "catalog-v0/index-v0.json",
             "index-v0.json on the catalog-v0 tag, and nothing else is compiled in");
    checkStr(catalogv0::INTERFACE, "v0", "the interface these documents describe");
}

static void test_the_one_equivalent_prior_image() {
    section("the one pre-v0 image accepted as equivalent");

    const std::string v0Combo =
        "0ca4ec60cb8bca71b8f0287c4b634c3126887be483db9b59b41bdff424f89303";
    const std::string priorCombo =
        "89b8ae1aaa6867dc515c3511b34c4f0c311a77e99ff71066f5a774bef99cde1d";

    check(diskv0::isEquivalentPriorImage(priorCombo, v0Combo),
          "the v1.4.12 combo stands in for the v0 one - same 94 files byte for byte, "
          "differing only in the CP/M slack between them",
          "accepted", "accepted");

    std::string upperPrior = priorCombo, upperCat = v0Combo;
    for (auto& c : upperPrior) c = (char)toupper((unsigned char)c);
    for (auto& c : upperCat) c = (char)toupper((unsigned char)c);
    check(diskv0::isEquivalentPriorImage(upperPrior, upperCat),
          "and the comparison folds case, because nothing guarantees how a hash was stored",
          "accepted", "accepted");

    check(!diskv0::isEquivalentPriorImage(v0Combo, priorCombo),
          "the relation is one-way: the v0 image is not a stand-in for the older one",
          "refused", "refused");
    check(!diskv0::isEquivalentPriorImage(std::string(64, '0'), v0Combo),
          "an unrelated hash is refused - this must never become a way to bless a "
          "corrupt or truncated image",
          "refused", "refused");
    check(!diskv0::isEquivalentPriorImage(priorCombo, std::string(64, 'f')),
          "and it is keyed on the catalog hash too, so it cannot leak onto another image",
          "refused", "refused");
    check(!diskv0::isEquivalentPriorImage("", ""),
          "two empty hashes are not equivalent to each other",
          "refused", "refused");
}

int main() {
    printf("=== Interface-v0 catalog suite ===\n");

    test_hex_bytes();
    test_real_index();
    test_preview_is_marked();
    test_which_releases_are_offered();
    test_index_tolerance();
    test_real_catalog();
    test_catalog_tolerance();
    test_the_url_that_is_compiled_in();
    test_the_one_equivalent_prior_image();

    printf("\n===============================\n");
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
