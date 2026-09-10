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
 * a roms[] that is absent, empty, reordered or carries nothing this build has
 * heard of, display an unknown status rather than failing on it.
 *
 * THE THIRD IS WHICH RELEASE GETS OFFERED, which is the one decision here that a
 * user can see. A build must offer what its own core says it can boot - not a
 * compiled-in list, because the client and the core are separate repositories
 * and either can be ahead - and when the answer is "none of them", that has to
 * be reportable rather than a silent fallback to something unbootable.
 *
 * The documents below are the REAL published ones, byte for byte out of
 * romwbw_disks/catalog/v0/: the whole index, and the whole 3.5.1 catalog with
 * both its ROMs and all twenty of its disks - including hd1k_ws4, which exists
 * under 3.5.1 and not under 3.6.0 and is therefore the entry that proves ids
 * come and go.
 *
 * THEY GO STALE, AND THAT USED TO BE INVISIBLE. These were pasted in at
 * generation 1, when 3.6.0 was still `"status": "preview"` and 3.5.1 was the
 * index's default; the published documents moved to generation 2 on 2026-09-05
 * and this copy did not, so for two days the suite asserted a ROM hash no
 * catalog served and a default release that was no longer the default - and
 * passed every time, because a self-contained fixture is only ever compared to
 * itself. test_the_fixture_is_not_stale() below is the answer: it reads the
 * sibling romwbw_disks checkout when there is one and fails on any drift, and
 * SKIPS when there is not, so the suite still needs no network and no sibling
 * to run everywhere else.
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
  "help": {
    "index_url": "https://github.com/avwohl/romwbw_disks/releases/download/help-v0/help_index.json",
    "base_url": "https://github.com/avwohl/romwbw_disks/releases/download/help-v0/"
  },
  "romwbw_versions": [
    {
      "romwbw_version": "3.5.1",
      "label": "RomWBW 3.5.1",
      "status": "stable",
      "default": false,
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
      "catalog_sha256": "942803d1ed67bcd8c6e0a9b730f9a08775618535b8e9affc58c56830839a78fd",
      "catalog_size": 11826,
      "generation": 2,
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
      "status": "stable",
      "default": true,
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
      "catalog_sha256": "4b4de2967482ab3f218df0ca065a9bdb9998b895792b720b2be3cddc3a6edbb1",
      "catalog_size": 15062,
      "generation": 2,
      "disks_xml_url": "https://github.com/avwohl/romwbw_disks/releases/download/v0-romwbw-3.6.0/disks-v0-3.6.0.xml",
      "rom_count": 2,
      "disk_count": 24,
      "notes": [
        "Promoted out of preview 2026-09-05. romwbw_emu v1.39 reads the RomWBW version out of the loaded ROM instead of a compile-time pin, so one binary boots this release and 3.5.1 alike; romwbw_disks tools/boot_test.sh asserts the boot, the CBIOS v3.6.0 [WBW] banner, no version-mismatch warning on a matched pair, and an R8/W8 round trip. NOTE: no client in any app store carries that core yet, so a SHIPPED client still cannot load a v3.6.0 ROM - it filters this entry out by hbios.ver_byte, which is what those bytes are in the index for.",
        "hd1k_ws4.img does not exist in v3.6.0; upstream combo.def slice 5 is 'wp' (WordStar / word processing) where v3.5.1 had 'ws4'.",
        "NVRAM checksums do not validate across a version change: RomWBW's NVSW_CHECKSUM XORs the version bytes into the seed, so a blob saved under 3.5.1 silently resets under a 3.6.0 ROM. Clients must namespace their NVRAM store per RomWBW version.",
        "Do NOT build from a v3.6.0-dev snapshot. romwbw_emu's archive/romwbw-v3.6.0/SBC_simh_std_v360.rom was deleted 2026-09-05 for this reason: it is a v3.6.0-dev.46 snapshot from 2025-12-12, not the release, and its HCB reads 36 00 so a version check cannot tell the difference."
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
  "generation": 2,
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
      "sha256": "4b11402a29fad22de304775b7c415eb6a74600df06bd57828b9931a7e9693258",
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
      "sha256": "03e646914628aea507eb8db560497292c728d26a127965b5b3cff6270af5feee",
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
      "id": "hd1k_zsdos",
      "filename": "hd1k_zsdos-v0-3.5.1.img",
      "name": "ZSDOS",
      "description": "Z-System DOS - enhanced CP/M compatible OS with date/time stamping.",
      "size": 8388608,
      "sha256": "0d44decad41fd054dcaf44219d60b0c253e7351e30dec16e902ec76d1a063acc",
      "license": "Mixed",
      "format": "hd1k",
      "bootable": true,
      "cbios": "CBIOS v3.5.1 [WBW]",
      "host_transfer": false,
      "upstream": "Binary/hd1k_zsdos.img"
    },
    {
      "id": "hd1k_zpm3",
      "filename": "hd1k_zpm3-v0-3.5.1.img",
      "name": "ZPM3",
      "description": "Z-System ZPM3 - enhanced CP/M 3 compatible with ZCPR extensions.",
      "size": 8388608,
      "sha256": "7397a518e7cbee06d3d9fd470d9a5185c9b81fad8b757607248de010eced6187",
      "license": "Mixed",
      "format": "hd1k",
      "bootable": true,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_zpm3.img"
    },
    {
      "id": "hd1k_cpm3",
      "filename": "hd1k_cpm3-v0-3.5.1.img",
      "name": "CP/M 3",
      "description": "CP/M Plus (CP/M 3.0), Digital Research's banked successor to CP/M 2.2.",
      "size": 8388608,
      "sha256": "9d2199eeef755f36b36fd3eae886f18da5b44bfd1f0ea1bed4fa49c73950472c",
      "license": "Mixed",
      "format": "hd1k",
      "bootable": true,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_cpm3.img"
    },
    {
      "id": "hd1k_nzcom",
      "filename": "hd1k_nzcom-v0-3.5.1.img",
      "name": "NZCOM",
      "description": "NZCOM - Z-System implementation for CP/M 2.2 environments.",
      "size": 8388608,
      "sha256": "c578ef7264be9cdcddd1b8bee242ef728dcbbdf1043c05ad0d2fac14e873beee",
      "license": "Mixed",
      "format": "hd1k",
      "bootable": true,
      "cbios": "CBIOS v3.5.1 [WBW]",
      "host_transfer": false,
      "upstream": "Binary/hd1k_nzcom.img"
    },
    {
      "id": "hd1k_qpm",
      "filename": "hd1k_qpm-v0-3.5.1.img",
      "name": "QPM",
      "description": "QP/M operating system, a CP/M 2.2 compatible alternative.",
      "size": 8388608,
      "sha256": "3822a1db41b0a3d710a5125a5dca04dabc611fd7ef48ff014d3c42a01357460b",
      "license": "Mixed",
      "format": "hd1k",
      "bootable": true,
      "cbios": "CBIOS v3.5.1 [WBW]",
      "host_transfer": false,
      "upstream": "Binary/hd1k_qpm.img"
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
      "id": "hd1k_aztecc",
      "filename": "hd1k_aztecc-v0-3.5.1.img",
      "name": "Aztec C",
      "description": "Aztec C compiler for CP/M - professional C development environment.",
      "size": 8388608,
      "sha256": "d2e637a562a31fd855281ec3eb8ed598019885ecb127e22c7d4b8e1fc4c44c03",
      "license": "Abandonware",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_aztecc.img"
    },
    {
      "id": "hd1k_bascomp",
      "filename": "hd1k_bascomp-v0-3.5.1.img",
      "name": "BASIC Compilers",
      "description": "Collection of BASIC compilers and interpreters for CP/M.",
      "size": 8388608,
      "sha256": "4a260bcaebe5666e6144c219e22d16a9d784cf328b4e9372c3654808ab689943",
      "license": "Abandonware",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_bascomp.img"
    },
    {
      "id": "hd1k_cowgol",
      "filename": "hd1k_cowgol-v0-3.5.1.img",
      "name": "Cowgol",
      "description": "Cowgol compiler - modern language targeting 8-bit systems.",
      "size": 8388608,
      "sha256": "68fdae49d596799ea0393d17eecac5a2bafea3284b679db6bb7e0278dcc443fd",
      "license": "Open Source",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_cowgol.img"
    },
    {
      "id": "hd1k_fortran",
      "filename": "hd1k_fortran-v0-3.5.1.img",
      "name": "Fortran",
      "description": "Fortran compiler for CP/M - scientific computing language.",
      "size": 8388608,
      "sha256": "a9b977eb88dc71414634cb847eb12eb84f62150d08fe4de60e9e88fb2fc2c27a",
      "license": "Abandonware",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_fortran.img"
    },
    {
      "id": "hd1k_hitechc",
      "filename": "hd1k_hitechc-v0-3.5.1.img",
      "name": "Hi-Tech C",
      "description": "Hi-Tech C compiler - optimizing C compiler for Z80 CP/M.",
      "size": 8388608,
      "sha256": "1423423e124bbc4b90b4152522923c16a41ccef3b3eae9d99fcad19909dc4d74",
      "license": "Freeware",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_hitechc.img"
    },
    {
      "id": "hd1k_tpascal",
      "filename": "hd1k_tpascal-v0-3.5.1.img",
      "name": "Turbo Pascal",
      "description": "Borland Turbo Pascal 3.0 - fast Pascal IDE and compiler.",
      "size": 8388608,
      "sha256": "b349c329b6233ad9cfcdd728075c4e53fe5e737a6d6003f20e5f1eb80e007573",
      "license": "Freeware",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_tpascal.img"
    },
    {
      "id": "hd1k_z80asm",
      "filename": "hd1k_z80asm-v0-3.5.1.img",
      "name": "Z80 Assemblers",
      "description": "Collection of Z80 assemblers and development tools.",
      "size": 8388608,
      "sha256": "46104ce74a53405b99228bf7d17f000880dab5e0aca0fb774ae7fa046559f04d",
      "license": "Mixed",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_z80asm.img"
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
    },
    {
      "id": "hd1k_z3plus",
      "filename": "hd1k_z3plus-v0-3.5.1.img",
      "name": "Z3Plus",
      "description": "Z3Plus - ZCPR3 command processor with enhanced features.",
      "size": 8388608,
      "sha256": "09fa6306b5b38db7f4536c38a192488b8c2e608add0fb869aea274356f2bb760",
      "license": "Mixed",
      "format": "hd1k",
      "bootable": true,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_z3plus.img"
    },
    {
      "id": "hd1k_bp",
      "filename": "hd1k_bp-v0-3.5.1.img",
      "name": "B/P Bios",
      "description": "B/P Bios utilities and tools disk.",
      "size": 8388608,
      "sha256": "37b31a2866bf7b0b6617395be7fd599ba1e287b7037ffe9fd112cb07ae1d60f1",
      "license": "Mixed",
      "format": "hd1k",
      "bootable": true,
      "cbios": "CBIOS v3.5.1 [WBW]",
      "host_transfer": false,
      "upstream": "Binary/hd1k_bp.img"
    },
    {
      "id": "hd1k_msxroms1",
      "filename": "hd1k_msxroms1-v0-3.5.1.img",
      "name": "MSX ROMs 1",
      "description": "MSX ROM images collection - volume 1.",
      "size": 8388608,
      "sha256": "0d0624a444369971c1e6ff68d04ffede7050d157963bbf2821ff1d338a439d5f",
      "license": "Abandonware",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_msxroms1.img"
    },
    {
      "id": "hd1k_msxroms2",
      "filename": "hd1k_msxroms2-v0-3.5.1.img",
      "name": "MSX ROMs 2",
      "description": "MSX ROM images collection - volume 2.",
      "size": 8388608,
      "sha256": "4a8e89b981f2684e9f523010d84d2050db6cf07d031aa9fb200f3bc1b9cf2893",
      "license": "Abandonware",
      "format": "hd1k",
      "bootable": false,
      "cbios": null,
      "host_transfer": false,
      "upstream": "Binary/hd1k_msxroms2.img"
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
    // NOT the default any more, and that is the point of reading it rather than
    // assuming it: 3.5.1 was the index's default until 3.6.0 was promoted out of
    // preview on 2026-09-05, and a client that had hardcoded "the first entry"
    // or "3.5.1" would have gone on fetching the older release for ever.
    checkFalse(entries[0].isDefault, "and it is NO LONGER the index's default");
    checkNum(entries[0].verByte, 0x35, "ver_byte 0x35");
    checkNum(entries[0].updByte, 0x10, "upd_byte 0x10");
    checkNum(entries[0].catalogSize, 11826, "with the catalog size the index publishes");
    checkStr(entries[0].catalogSha256,
             "942803d1ed67bcd8c6e0a9b730f9a08775618535b8e9affc58c56830839a78fd",
             "and the sha256 that catalog is verified against before it is parsed");
    checkStr(entries[0].catalogUrl,
             "https://github.com/avwohl/romwbw_disks/releases/download/"
             "v0-romwbw-3.5.1/catalog-v0-3.5.1.json",
             "the catalog URL is absolute and is NOT built from the release tag");
    checkNum(entries[0].generation, 2, "generation 2 - the HB_BNKCALL rebuild");
    checkNum(entries[0].diskCount, 20, "20 disks");
    checkNum(entries[0].romCount, 2, "2 ROMs");

    checkStr(entries[1].romwbwVersion, "3.6.0", "the second is 3.6.0");
    checkStr(entries[1].status, "stable", "published stable, promoted 2026-09-05");
    checkTrue(entries[1].isDefault, "and it is now the index's default");
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
    checkStr(catalogv0::displayLabel(entries[1]), "RomWBW 3.6.0",
             "and so does the second, now that 3.6.0 is stable too");

    // Both published releases are stable today, so the real documents no longer
    // exercise the marking at all - which is exactly why the made-up entry below
    // exists and must stay. A rule only the fixtures could check would stop
    // being checked the moment the fixtures stopped carrying a preview.
    catalogv0::IndexEntry stillPreview;
    stillPreview.romwbwVersion = "3.7.0";
    stillPreview.label = "RomWBW 3.7.0";
    stillPreview.status = "preview";
    checkStr(catalogv0::displayLabel(stillPreview), "RomWBW 3.7.0 (preview)",
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
    checkNum(catalogv0::chooseVersion(entries, both, ""), 1,
             "and with no preference it takes the index's default, which is 3.6.0 "
             "since the 2026-09-05 promotion - read from the flag, never from the "
             "position, so this moved when the document did");
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
    checkNum(catalog.generation, 2, "at generation 2");
    checkStr(catalog.baseUrl,
             "https://github.com/avwohl/romwbw_disks/releases/download/v0-romwbw-3.5.1/",
             "with a base_url that ends in a slash");

    checkNum(catalog.roms.size(), 2, "two ROMs, either of which this build can fetch and boot");
    if (catalog.roms.size() == 2) {
        checkStr(catalog.roms[0].id, "emu_avw", "the default ROM's id");
        checkTrue(catalog.roms[0].isDefault, "and it is the default");
        checkNum(catalog.roms[0].size, 524288, "512 KB");
        checkStr(catalog.roms[0].sha256,
                 "4b11402a29fad22de304775b7c415eb6a74600df06bd57828b9931a7e9693258",
                 "and the hash every start checks the downloaded file against - "
                 "the generation-2 rebuild, c7abc580 before HB_BNKCALL was fixed");
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

static void test_which_rom_boots() {
    section("which ROM of a release this build boots");

    // The published shape, out of the real 3.5.1 catalog: two ROMs, the first
    // flagged. The flag is what is read - not the position, and not the id.
    catalogv0::Catalog real;
    std::string error;
    checkTrue(catalogv0::parseCatalog(REAL_CATALOG_351, real, error),
              "the real catalog parses");
    size_t pick = catalogv0::chooseRom(real.roms);
    checkTrue(pick < real.roms.size(), "the published catalog names a ROM to boot");
    if (pick < real.roms.size()) {
        checkStr(real.roms[pick].id, "emu_avw", "and it is the entry flagged default");
        checkStr(real.roms[pick].filename, "emu_avw-v0-3.5.1.rom",
                 "whose filename carries the interface and the release, so two "
                 "releases' ROMs coexist in one data folder exactly as their disks do");
    }

    // THE FLAG, NOT THE POSITION. Same two ROMs with the flag moved to the
    // second: a client that took roms[0] would boot the wrong image, and the
    // guest would say nothing about it - both ROMs carry the same HCB bytes, so
    // emu_validate_rom_hcb accepts either.
    std::vector<catalogv0::RomItem> reordered = real.roms;
    checkNum(reordered.size(), 2, "two to reorder");
    if (reordered.size() == 2) {
        reordered[0].isDefault = false;
        reordered[1].isDefault = true;
        pick = catalogv0::chooseRom(reordered);
        checkStr(pick < reordered.size() ? reordered[pick].id : std::string(""),
                 "emu_rcz80",
                 "the flagged entry wins wherever it sits in the array");
    }

    // NO ENTRY FLAGGED. Nothing in CATALOG_SCHEMA promises one, so this is a
    // fallback and not an error path: take the first and carry on.
    std::vector<catalogv0::RomItem> unflagged = real.roms;
    for (auto& r : unflagged) r.isDefault = false;
    pick = catalogv0::chooseRom(unflagged);
    checkStr(pick < unflagged.size() ? unflagged[pick].id : std::string(""), "emu_avw",
             "with nothing flagged the first entry is taken, and that is normal");

    // TWO FLAGGED, which the index-side rule (verify_catalog.py) forbids and a
    // hand-edited document could still produce. Route around it; do not refuse.
    std::vector<catalogv0::RomItem> both = real.roms;
    for (auto& r : both) r.isDefault = true;
    pick = catalogv0::chooseRom(both);
    checkStr(pick < both.size() ? both[pick].id : std::string(""), "emu_avw",
             "two flagged entries take the first flagged, not a refusal");

    // ABSENT roms[]. 6.1 says a client must not assume the array is there, and
    // "no ROM for this release" has to be a value a caller can act on rather
    // than a crash or a silent substitution.
    catalogv0::Catalog noRoms;
    checkTrue(catalogv0::parseCatalog(R"JSON({"base_url":"https://x/","disks":[]})JSON",
                                      noRoms, error),
              "a catalog with no roms[] at all is a catalog");
    checkNum(noRoms.roms.size(), 0, "and carries no ROMs");
    checkTrue(catalogv0::chooseRom(noRoms.roms) == (size_t)-1,
              "so there is no ROM to boot - reportable, never a fallback to another release's");

    // EMPTY roms[]. Indistinguishable from absent after the parse, and it must
    // stay indistinguishable: a caller that treated them differently would be
    // acting on a difference the schema does not make.
    catalogv0::Catalog emptyRoms;
    checkTrue(catalogv0::parseCatalog(R"JSON({"base_url":"https://x/","roms":[],"disks":[]})JSON",
                                      emptyRoms, error),
              "an empty roms[] is a catalog too");
    checkTrue(catalogv0::chooseRom(emptyRoms.roms) == (size_t)-1,
              "and answers the same as an absent one");

    // A CATALOG WITH NO emu_avw IN IT. The one name a client is most likely to
    // have hardcoded; nothing here looks for it.
    catalogv0::Catalog future;
    checkTrue(catalogv0::parseCatalog(
        R"JSON({"base_url":"https://x/","roms":[
            {"id":"emu_future","filename":"emu_future-v0-9.9.9.rom","size":262144,
             "sha256":"aa","default":true}]})JSON", future, error),
        "a catalog publishing a ROM set this build has never heard of");
    pick = catalogv0::chooseRom(future.roms);
    checkStr(pick < future.roms.size() ? future.roms[pick].id : std::string(""),
             "emu_future", "is followed, because the flag is all that is read");
    checkNum(pick < future.roms.size() ? future.roms[pick].size : 0, 262144,
             "and its size is taken from the document - nothing here promises 512 KB, "
             "and upstream v3.6.0 already ships ROM images that are not");

    // THE USER'S OWN CHOICE, which is what makes publishing a second ROM worth
    // anything: emu_rcz80 is in both published catalogs and was unreachable from
    // this client until the Settings dropdown started listing roms[]. Matched on
    // id, so it survives a release switch that renames every file.
    pick = catalogv0::chooseRom(real.roms, "emu_rcz80");
    checkStr(pick < real.roms.size() ? real.roms[pick].id : std::string(""), "emu_rcz80",
             "a stored preference beats the default flag");
    checkStr(pick < real.roms.size() ? real.roms[pick].filename : std::string(""),
             "emu_rcz80-v0-3.5.1.rom",
             "and it is this release's file, not the one the preference was made against");

    // A PREFERENCE THIS RELEASE DOES NOT PUBLISH loses to the default rather
    // than stranding the release. 6.1's "do not assume emu_avw is present" cuts
    // both ways: the set of ROMs is the document's to change, so a client whose
    // stored id has gone must still boot something rather than refuse.
    pick = catalogv0::chooseRom(real.roms, "emu_gone");
    checkStr(pick < real.roms.size() ? real.roms[pick].id : std::string(""), "emu_avw",
             "an id the catalog no longer carries falls back to the flagged entry");

    // The empty preference is the ordinary case - a fresh install, and every
    // configuration written before the ROM choice existed - and must behave
    // exactly as the one-argument form does.
    checkTrue(catalogv0::chooseRom(real.roms, std::string()) == catalogv0::chooseRom(real.roms),
              "no preference is the same answer as not asking");

    // A preference cannot resurrect a release that publishes no ROM at all.
    checkTrue(catalogv0::chooseRom(emptyRoms.roms, "emu_avw") == (size_t)-1,
              "and it cannot invent one where roms[] is empty");

    // The id is matched, NEVER the filename. A client that compared filenames
    // would forget the choice at the first version switch, because every
    // published ROM filename carries its release.
    pick = catalogv0::chooseRom(real.roms, "emu_rcz80-v0-3.5.1.rom");
    checkStr(pick < real.roms.size() ? real.roms[pick].id : std::string(""), "emu_avw",
             "a filename is not an id and does not select");

    // The URL a ROM is fetched from is the same concatenation a disk's is, which
    // is the point of assetUrl having one home.
    checkStr(catalogv0::assetUrl(real.baseUrl, "emu_avw-v0-3.5.1.rom"),
             "https://github.com/avwohl/romwbw_disks/releases/download/"
             "v0-romwbw-3.5.1/emu_avw-v0-3.5.1.rom",
             "a ROM URL is base_url + filename, with nothing inserted");
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

static void test_pointing_at_another_catalog() {
    section("pointing at another catalog, and coming back");

    // The invariant the whole feature rests on. Every path this scopes has to
    // come out byte-identical to what a machine already holds, or one visit to
    // a test catalog would strand the user's library behind a name nothing
    // reads afterwards. So the built-in index adds NOTHING.
    checkStr(catalogv0::indexScope(""), "",
             "no preference stored: the built-in index adds no suffix, so an existing "
             "install finds its files and settings exactly where it left them");
    checkFalse(catalogv0::isCustomIndex(""),
               "and is not reported as custom");
    checkStr(catalogv0::indexUrl(""), catalogv0::INDEX_URL,
             "and resolves to the URL this build ships with");

    // A field a user has cleared, or pasted a stray space into.
    checkStr(catalogv0::indexScope("   "), "",
             "whitespace is no preference, not an index whose host is empty - which "
             "would fail every fetch with a message about the network");

    // Somebody who pastes the built-in URL in by hand has not chosen anything
    // different, and must not be given a second namespace holding a second copy
    // of the same downloads.
    checkStr(catalogv0::indexScope(catalogv0::INDEX_URL), "",
             "the built-in URL typed in by hand is still the built-in index");

    const std::string mine = "https://example.invalid/mine/index-v0.json";
    const std::string other = "https://example.invalid/other/index-v0.json";
    checkTrue(catalogv0::isCustomIndex(mine), "a different URL is reported as custom");
    checkStr(catalogv0::indexUrl(mine), mine, "and is what gets fetched");
    checkTrue(!catalogv0::indexScope(mine).empty(), "and takes a namespace of its own");

    // Two catalogs both publishing "3.6.0" is the ordinary case here, not a
    // corner: same release name, same filenames, different bytes.
    checkTrue(catalogv0::indexScope(mine) != catalogv0::indexScope(other),
          "two different custom indexes get different namespaces - two forks both "
          "publishing 3.6.0 must not write over each other's images");
    checkStr(catalogv0::indexScope(mine), catalogv0::indexScope(mine),
             "and the tag is stable, so returning to a catalog finds what was left there");

    // THE CROSS-CLIENT PROPERTY. ioscpm computes this tag in Swift and cpmdroid
    // in Kotlin, from the same FNV-1a folded the same way, so one index URL
    // produces one tag everywhere. Measured against ioscpm on 2026-09-08: a
    // simulator pointed at the URL below created Documents/Disks@78f588f0.
    // If this fails, the three clients have drifted and a bug report naming a
    // scope no longer means the same thing in each.
    checkStr(catalogv0::fnv1a32(mine), "78f588f0",
             "the tag matches what ioscpm's Swift computes for the same URL");
    checkStr(catalogv0::indexScope(mine), "@78f588f0",
             "and the scope is that tag, prefixed");

    checkTrue(catalogv0::fnv1a32("a") != catalogv0::fnv1a32("b"),
          "the tag distinguishes inputs at all");
    checkTrue(catalogv0::fnv1a32(catalogv0::INDEX_URL).size() == 8,
          "and is 8 hex characters, short enough to sit in a path");

    // The whole of C's isspace() set, because Swift's .whitespacesAndNewlines
    // and Kotlin's .trim() both strip \f and \v: a value those two ports read as
    // "no preference" must not be read here as a host whose name is a form feed.
    checkStr(catalogv0::indexUrl("\f\v \t\r\n"), catalogv0::INDEX_URL,
             "form feed and vertical tab are whitespace here as they are in the siblings");
    checkStr(catalogv0::indexUrl("  " + mine + "\t\r\n"), mine,
             "and a pasted URL is trimmed on both ends rather than fetched with its padding");

    // WHAT GETS STORED, which is not the same question as what gets fetched.
    checkStr(catalogv0::normalizedIndexSetting(""), "",
             "an empty field stores nothing");
    checkStr(catalogv0::normalizedIndexSetting("   "), "",
             "and neither does a field holding only spaces");
    checkStr(catalogv0::normalizedIndexSetting(catalogv0::INDEX_URL), "",
             "the built-in URL PASTED IN stores as empty - the field shows the URL in "
             "use and invites copying, and storing it would pin this install to "
             "today's default for ever");
    checkStr(catalogv0::normalizedIndexSetting("  " + mine + "  "), mine,
             "a real custom URL is stored trimmed");
}

// The environment variable, which is the mechanism the header nominates FIRST
// for pointing at another catalog and was the one thing here with no test at
// all - so the precedence rule the feature rests on was never exercised, and
// the suite quietly FAILED for anyone who had the variable set while working on
// it. Measured on 2026-09-08: eight of these checks went red with
// ROMWBW_INDEX_URL exported, which is the state of every machine actually
// testing this feature.
// The release a v0 filename carries, which is what lets a CONFIGURATION answer
// "which release are the disks in my four slots?" with no catalog, no ROM and no
// migration pass in sight.
//
// That question used to be answered only inside ConfigManager::migrateToInterfaceV0,
// and that pass is gated on interfaceV0Migrated - already true on every machine
// that has launched a build since the storage rename, which is to say on every
// machine that has the problem. Reading the release out of the name instead
// works on a configuration that some earlier build already migrated, and it says
// 3.6.0 for a 3.6.0 library where a constant could only ever say 3.5.1.
static void test_the_release_a_v0_name_carries() {
    section("the release a v0 name carries");

    std::string out;

    out.clear();
    checkTrue(diskv0::releaseOfV0Name("hd1k_combo-v0-3.5.1.img", out),
              "a v0 name answers");
    checkStr(out, "3.5.1", "with the release in it");

    out.clear();
    checkTrue(diskv0::releaseOfV0Name("hd1k_games-v0-3.6.0.img", out),
              "and answers for a release that is not the pre-v0 one");
    checkStr(out, "3.6.0",
             "with 3.6.0 and not a constant - a 3.6.0 library must not be dragged "
             "back to 3.5.1 by the thing that reads it");

    out.clear();
    checkTrue(diskv0::releaseOfV0Name(
                  "C:\\Users\\me\\AppData\\Local\\z80cpmw\\data\\hd1k_combo-v0-3.5.1.img", out),
              "a whole path answers too, which is the shape the four slots actually store");
    checkStr(out, "3.5.1", "reading only the basename");

    out.clear();
    checkTrue(diskv0::releaseOfV0Name("emu_avw-v0-3.5.1.rom", out),
              "a ROM name is a v0 name as much as a disk name is");
    checkStr(out, "3.5.1", "and carries its release the same way");

    out = "untouched";
    checkFalse(diskv0::releaseOfV0Name("hd1k_combo.img", out),
               "a PRE-v0 name carries no release");
    checkStr(out, "untouched", "and leaves the out parameter alone");

    out = "untouched";
    checkFalse(diskv0::releaseOfV0Name("my_own_disk.img", out),
               "and neither does an image the user made themselves");
    checkStr(out, "untouched", "which is what keeps their choice from being overwritten");

    out = "untouched";
    checkFalse(diskv0::releaseOfV0Name("hd1k_combo-v0-.img", out),
               "a tag with nothing after it names no release - the same name "
               "looksLikeV0Name() rejects, so the two cannot disagree");

    out = "untouched";
    checkFalse(diskv0::releaseOfV0Name("", out), "and an empty name answers nothing");

    // Folded, like every other name comparison in this file, so a spelling
    // Windows kept for the user does not become a second release.
    out.clear();
    checkTrue(diskv0::releaseOfV0Name("HD1K_COMBO-V0-3.5.1.IMG", out),
              "an upper-case spelling is still a v0 name");
    checkStr(out, "3.5.1", "and folds to the same release");
}

static void setIndexEnv(const char* value) {
#ifdef _WIN32
    _putenv_s("ROMWBW_INDEX_URL", value ? value : "");
#else
    if (value) setenv("ROMWBW_INDEX_URL", value, 1);
    else       unsetenv("ROMWBW_INDEX_URL");
#endif
}

static void test_index_url_environment() {
    section("the environment variable that wins for one run");

    const std::string mine = "https://example.invalid/mine/index-v0.json";
    const std::string fromEnv = "https://example.invalid/env/index-v0.json";

    // Cleared first, so this section describes what it sets rather than what the
    // machine running it happens to export.
    setIndexEnv(nullptr);
    {
        std::string out = "untouched";
        checkFalse(catalogv0::indexUrlFromEnvironment(out),
                   "unset: the environment has no answer");
        checkStr(out, "untouched", "and the out parameter is left alone");
    }

    setIndexEnv("");
    {
        std::string out = "untouched";
        checkFalse(catalogv0::indexUrlFromEnvironment(out),
                   "set to nothing is not set");
        checkStr(catalogv0::indexUrl(mine), mine, "so the stored setting still decides");
    }

    // The divergence that locked the Settings field for a variable with no
    // effect: the dialog tested `*env != 0` where indexUrl() trims first.
    setIndexEnv("   ");
    {
        std::string out;
        checkFalse(catalogv0::indexUrlFromEnvironment(out),
                   "whitespace-only is not set either - one reader, one answer, so the "
                   "Settings field cannot be disabled for a variable indexUrl() ignores");
        checkStr(catalogv0::indexUrl(""), catalogv0::INDEX_URL,
                 "and the built-in index is still what gets fetched");
    }

    setIndexEnv(fromEnv.c_str());
    {
        std::string out;
        checkTrue(catalogv0::indexUrlFromEnvironment(out), "set: the environment answers");
        checkStr(out, fromEnv, "with the URL it holds");
        checkStr(catalogv0::indexUrl(""), fromEnv,
                 "it beats no stored setting");
        checkStr(catalogv0::indexUrl(mine), fromEnv,
                 "AND it beats a stored one - the precedence romwbw_emu's romwbw-get "
                 "uses, so one set of instructions covers every client");
        checkTrue(catalogv0::isCustomIndex(""),
                  "a machine with nothing stored is still on a custom index while it is set");
        checkStr(catalogv0::indexScope(""), "@" + catalogv0::fnv1a32(fromEnv),
                 "and the scope follows the URL in force, not the one stored");
    }

    setIndexEnv(("  " + fromEnv + "  ").c_str());
    {
        std::string out;
        checkTrue(catalogv0::indexUrlFromEnvironment(out), "a padded variable is still set");
        checkStr(out, fromEnv, "and is trimmed before use");
    }

    // Pointing the variable at the built-in URL is not a custom index.
    setIndexEnv(catalogv0::INDEX_URL);
    checkFalse(catalogv0::isCustomIndex(mine),
               "the variable naming the built-in index makes this the built-in index, "
               "whatever is stored");
    checkStr(catalogv0::indexScope(mine), "", "so it takes no namespace");

    // Left clear, because every other section in this file asks indexUrl() what
    // the built-in answer is and would otherwise inherit this one.
    setIndexEnv(nullptr);
    checkStr(catalogv0::indexUrl(""), catalogv0::INDEX_URL,
             "and the variable is cleared again, so the rest of this suite is hermetic");
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

static void test_the_stored_rom_becomes_an_id() {
    section("what a stored ROM filename becomes");

    // AppConfig::rom held a FILENAME until the bundled ROMs were deleted, and it
    // holds a catalog id now. The conversion runs at parse time rather than in
    // the interfaceV0Migrated pass, because that flag is already true on every
    // machine that has launched since the storage rename - so a migration gated
    // on it would never run on the configurations that need this most.
    std::string id;

    check(diskv0::romIdForStoredName("emu_avw.rom", id),
          "the default packaged name maps", "mapped", "mapped");
    checkStr(id, "emu_avw", "onto the catalog id it was");

    // The two packaged files were byte-identical - both 4b11402a..., measured
    // 2026-09-07 - so the second was always a second name for the first. Mapping
    // it anywhere else would invent a preference the user never expressed.
    id.clear();
    check(diskv0::romIdForStoredName("emu_romwbw.rom", id),
          "and so does the other one, which was the same 512 KB", "mapped", "mapped");
    checkStr(id, "emu_avw", "onto the same id");

    // Case, because Windows kept whatever the user's file was created with and a
    // hand-edited config can say anything.
    id.clear();
    check(diskv0::romIdForStoredName("EMU_AVW.ROM", id),
          "a differently-cased name still maps", "mapped", "mapped");
    checkStr(id, "emu_avw", "onto the canonical id");

    // A v0 ROM filename, which carries its release. The release belongs to
    // romwbwVersion and this field must not carry a second copy of it.
    id.clear();
    check(diskv0::romIdForStoredName("emu_avw-v0-3.6.0.rom", id),
          "a v0 ROM filename loses its release", "mapped", "mapped");
    checkStr(id, "emu_avw", "and comes back as the bare id");

    // IDEMPOTENT. A value that is already an id carries no extension, so it is
    // not a filename and is not this function's to touch. from_json keeps it.
    id = "untouched";
    check(!diskv0::romIdForStoredName("emu_avw", id),
          "a bare id is not a filename and is left alone", "refused", "refused");
    checkStr(id, "untouched", "with the out parameter not written");

    // NOT AN ALLOWLIST OF CATALOG IDS. 6.1 forbids assuming emu_avw is
    // published; this maps two legacy FILENAMES onto the id those files were and
    // asks nothing about what any catalog carries. Everything else is "no
    // preference", which is a real answer - chooseRom then takes the default.
    id = "untouched";
    check(!diskv0::romIdForStoredName("SBC_simh_std.rom", id),
          "the stock hardware ROM no build could load maps to no preference",
          "refused", "refused");
    check(!diskv0::romIdForStoredName("mine.rom", id),
          "and so does a ROM the user put beside the executable themselves",
          "refused", "refused");
    check(!diskv0::romIdForStoredName("", id),
          "an empty stored value is already no preference", "refused", "refused");
    check(!diskv0::romIdForStoredName("emu_avw.img", id),
          "a name that is not a .rom at all is not a ROM filename",
          "refused", "refused");
    checkStr(id, "untouched", "and none of those wrote the out parameter");
}

// The two fixtures above are copies, and a copy that nothing compares to its
// original is a copy that will be wrong eventually. This is the comparison.
//
// It reads the sibling romwbw_disks checkout, which CLAUDE.md already requires
// beside this one for the build, and SKIPS when it is not there - so the suite
// keeps its "no network, no sibling, any machine with a compiler" property and
// still turns red on the machine where the drift would actually be noticed.
// Whitespace-insensitive on purpose: what matters is the document, not how the
// generator chose to indent it.
static std::string readWholeFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return std::string();
    std::string out;
    char buf[4096];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, got);
    fclose(f);
    return out;
}

static std::string squeeze(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (!isspace((unsigned char)c)) out += c;
    }
    return out;
}

static void test_where_the_help_lives() {
    section("the help location comes out of the index, not out of the binary");

    // THE POINT OF THE WHOLE BLOCK. Until 1.0.32 HelpWindow.cpp carried
    //     ".../avwohl/ioscpm/releases/latest/download/help_index.json"
    // and a base URL beside it, so in-app help was the last thing in this
    // application that could only be moved by shipping a new client to Windows,
    // Android, iOS and Linux at once. Reading it from the index means
    // romwbw_disks can rename the tag, re-cut it or change host with no client
    // release - the same promise catalog_url makes for a RomWBW release.
    catalogv0::HelpLocation loc;
    checkTrue(catalogv0::parseHelpLocation(REAL_INDEX, loc),
              "the published index carries a help block");
    checkStr(loc.indexUrl,
             "https://github.com/avwohl/romwbw_disks/releases/download/"
             "help-v0/help_index.json",
             "help_index.json on the help-v0 tag");
    checkStr(loc.baseUrl,
             "https://github.com/avwohl/romwbw_disks/releases/download/help-v0/",
             "and the base a topic filename is appended to");
    checkTrue(loc.ok(), "both halves present, so it is usable");

    // base_url is concatenated with a filename and NOTHING is inserted between
    // them - the schema's rule, and the exact spot section 6 says the three
    // clients have disagreed before.
    checkTrue(!loc.baseUrl.empty() && loc.baseUrl.back() == '/',
              "base_url ends in a slash, so concatenation needs no separator");
    checkStr(catalogv0::assetUrl(loc.baseUrl, "help_qpm.md"),
             "https://github.com/avwohl/romwbw_disks/releases/download/"
             "help-v0/help_qpm.md",
             "a topic URL is base_url + filename");

    // AN INDEX WITHOUT THE BLOCK IS NOT AN ERROR, and this is the case that
    // decides whether a shipped client survives the day the block is removed or
    // meets an index published before it existed. The caller falls back to the
    // topics compiled into the binary; nothing goes in front of a user.
    catalogv0::HelpLocation none;
    checkFalse(catalogv0::parseHelpLocation(
                   R"({"schema":"romwbw-disks-index","romwbw_versions":[]})", none),
               "an index with no help block simply has no location");
    checkTrue(none.indexUrl.empty() && none.baseUrl.empty(),
              "and leaves nothing half-set behind");

    // HALF A BLOCK IS NO BLOCK. A topic list with no way to fetch a topic reads
    // to a user as help that is present and broken, which is worse than help
    // that is offline, so neither half is taken without the other.
    catalogv0::HelpLocation half;
    checkFalse(catalogv0::parseHelpLocation(
                   R"({"help":{"index_url":"https://example.invalid/help_index.json"}})", half),
               "an index_url with no base_url is refused");
    checkTrue(half.indexUrl.empty(), "and stores neither half");
    checkFalse(catalogv0::parseHelpLocation(
                   R"({"help":{"base_url":"https://example.invalid/"}})", half),
               "a base_url with no index_url is refused too");

    // Wrong shapes rather than missing ones, since "ignore what you do not
    // understand" has to cover a key whose VALUE is the surprise.
    checkFalse(catalogv0::parseHelpLocation(R"({"help":"somewhere"})", half),
               "a help that is a string is not a location");
    checkFalse(catalogv0::parseHelpLocation(R"({"help":[]})", half),
               "nor is a help that is an array");
    checkFalse(catalogv0::parseHelpLocation("not json at all", half),
               "and a document that will not parse has no location either");

    // A FORK GETS THIS FOR FREE, which is the second reason it is in the index:
    // a client pointed at another catalog reads that catalog's help block, so a
    // test index serves its own topics with no patched client.
    catalogv0::HelpLocation mine;
    checkTrue(catalogv0::parseHelpLocation(
                  R"({"help":{"index_url":"http://127.0.0.1:8731/help_index.json",)"
                  R"("base_url":"http://127.0.0.1:8731/"}})", mine),
              "a custom index names its own help");
    checkStr(mine.baseUrl, "http://127.0.0.1:8731/",
             "and that is what the Help window would fetch from");
}

static void test_the_fixture_is_not_stale() {
    section("the fixtures still match the published documents");

    // run_tests.bat runs the exe from the repository root, so one "..\" is
    // right; the second candidate is for running it from tests\ by hand. Both
    // rather than one, because a check that silently SKIPs in the ordinary case
    // is a check that never runs - which is the exact failure this section was
    // written to end.
    static const char* const kBases[] = {
        "..\\romwbw_disks\\catalog\\v0\\",
        "..\\..\\romwbw_disks\\catalog\\v0\\",
    };

    std::string liveIndex, liveCatalog;
    for (const char* base : kBases) {
        liveIndex = readWholeFile(std::string(base) + "index.json");
        liveCatalog = readWholeFile(std::string(base) + "3.5.1\\catalog.json");
        if (!liveIndex.empty() && !liveCatalog.empty()) break;
    }

    if (liveIndex.empty() || liveCatalog.empty()) {
        printf("  SKIP - no ..\\romwbw_disks checkout beside this one, so there is\n"
               "         nothing to compare the fixtures against on this machine.\n");
        return;
    }

    checkStr(squeeze(REAL_INDEX), squeeze(liveIndex),
             "REAL_INDEX is still romwbw_disks/catalog/v0/index.json - if this fails, "
             "the published index moved and every assertion below about defaults, "
             "status and hashes is now about a document nobody serves");
    checkStr(squeeze(REAL_CATALOG_351), squeeze(liveCatalog),
             "REAL_CATALOG_351 is still romwbw_disks/catalog/v0/3.5.1/catalog.json");
}

int main() {
    printf("=== Interface-v0 catalog suite ===\n");

    // HERMETIC FIRST. catalogv0::indexUrl() reads $ROMWBW_INDEX_URL, and every
    // section below that asks it what the built-in answer is would otherwise be
    // answering a question about the machine rather than about the code.
    //
    // That is not theoretical: with the variable exported this suite failed
    // EIGHT checks, measured on 2026-09-08 - and the people who export it are
    // exactly the people working on this feature, so the suite went red for the
    // one audience most likely to run it, in a way that looked like their change.
    setIndexEnv(nullptr);

    test_hex_bytes();
    test_real_index();
    test_preview_is_marked();
    test_which_releases_are_offered();
    test_index_tolerance();
    test_real_catalog();
    test_which_rom_boots();
    test_catalog_tolerance();
    test_the_url_that_is_compiled_in();
    test_pointing_at_another_catalog();
    test_index_url_environment();
    test_the_release_a_v0_name_carries();
    test_the_one_equivalent_prior_image();
    test_the_stored_rom_becomes_an_id();
    test_where_the_help_lives();
    test_the_fixture_is_not_stale();

    printf("\n===============================\n");
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
