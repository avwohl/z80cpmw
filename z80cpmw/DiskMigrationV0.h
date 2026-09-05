/*
 * DiskMigrationV0.h - What a pre-v0 disk name becomes under interface v0, and
 * every decision the storage migration makes about one.
 *
 * The catalog moved from `avwohl/ioscpm` to `avwohl/romwbw_disks`, and with it
 * every published filename gained a suffix naming the interface and the RomWBW
 * release the image was built for: `hd1k_combo.img` -> `hd1k_combo-v0-3.5.1.img`.
 * That is not cosmetic. A 3.5.1 disk booted against a 3.6.0 ROM makes the guest
 * CBIOS print `*** WARNING: HBIOS/CBIOS Version Mismatch ***`, so the two
 * generations have to be able to sit in one data folder without one overwriting
 * the other, and only the filename can keep them apart.
 *
 * Nothing here changes where a file comes FROM: this is the rename half alone,
 * and it maps names for a data folder rather than for a URL. The other half -
 * the switch from `RELEASE_TAG` to the two-document catalog in
 * `avwohl/romwbw_disks` - is CatalogV0.h, and the two are written apart on
 * purpose so that a mistake in one is not searched for in the other.
 *
 * The two halves are in the same tree now, which changes one thing worth naming
 * here: the file a stored path is rewritten onto is the one the catalog will
 * name too, because BUNDLED_ROMWBW below is the release this build's bundled ROM
 * is and that is the release the index's `default: true` entry selects. A user
 * who then chooses another release in Settings gets a second set of images
 * beside these, under their own `-v0-<ver>` names, and this pass never touches
 * them - it renames pre-v0 names and nothing else.
 *
 * ## The three rules, and why each of them is a rule
 *
 * RENAME, NEVER COPY. DiskLedger::measurementApplies is exactly
 * `measuredSize == f.size && measuredModified == f.modified`, and
 * measuredModified is raw FILETIME ticks. MoveFileEx within one directory
 * preserves both, so a renamed image keeps its cached measurement and its
 * recorded provenance; a copy-then-delete resets the write time and costs every
 * user a re-hash of the whole library - 210,763,776 bytes across the twenty
 * entries - for nothing. Nineteen of those twenty images are byte-identical
 * between the ioscpm v1.4.12 catalog and the v0 one (only hd1k_combo's bytes
 * moved, 89b8ae1a... -> 0ca4ec60...), so carrying the ledger key across the
 * rename is what makes this migration free rather than expensive.
 *
 * ONLY NAMES THE OLD CATALOG PUBLISHED. The data folder is not the app's alone:
 * R8 and W8 read and write the user's own host files into that very directory,
 * and users drop their own images there by hand. So the pass is driven by
 * legacyCatalogFilenames() below - a fixed list of twenty - and never by
 * enumerating the folder and matching a pattern. A pattern match renames
 * somebody's transferred file.
 *
 * NEVER DELETE. If a file already sits under the v0 name, the pre-v0 one is
 * left exactly where it is rather than replaced. Whatever it is, it is not this
 * migration's to throw away.
 *
 * ## No Win32, no file system, no JSON
 *
 * For the same reason DiskLedger.cpp has none (DiskLedger.h:12-14): it is the
 * only way tests/test_diskledger.cpp and tests/test_config.cpp can check any of
 * this. The facts come in as values - which file exists is decided by the
 * caller and handed back in a LandedNames map - and every function here is a
 * pure function of its arguments.
 */

#pragma once

#include "DiskLedger.h"

#include <map>
#include <string>
#include <vector>

namespace diskv0 {

// The interface these names carry, and the RomWBW release this build's bundled
// ROM is. Both appear in the filename: <stem>-<INTERFACE>-<BUNDLED_ROMWBW>.<ext>.
//
// BUNDLED_ROMWBW is a fact about roms/emu_avw.rom, whose HCB reads 35 10. It is
// written down rather than read out of the ROM because the migration has to run
// before any ROM question is asked, and because a rename that changed with the
// loaded ROM would move a user's files somewhere else on the next launch.
// Offering more than one release is a later step, and it is where
// emu_romwbw_release_of_image() belongs; here the answer must not vary.
extern const char* const INTERFACE;
extern const char* const BUNDLED_ROMWBW;

// The twenty filenames the pre-v0 catalog published, and the only names this
// migration is allowed to touch. Read out of ioscpm's release_assets/disks.xml
// at version="13", which is the catalog both v1.4.5 and v1.4.12 serve; every one
// of them is <id>.img for an id the v0 3.5.1 catalog still carries, with nothing
// left over on either side.
//
// Lowercase, and that is the canonical spelling: a rename target is built from
// the entry matched here and never from the user's own spelling, so a
// HD1K_COMBO.IMG that Windows happily kept for them lands as
// hd1k_combo-v0-3.5.1.img - the name the catalog uses, which is what the
// Settings dropdown and isDiskDownloaded() will be asking about afterwards.
const std::vector<std::string>& legacyCatalogFilenames();

// Whether a name already carries a -v0- suffix. True means "leave it alone":
// the pass has already run over this name, or the file arrived under a v0 name
// to begin with. Running the migration twice has to be harmless, and this is
// the test that makes it so.
// True when 'provenance' names a pre-v0 image already known to be equivalent to
// the image 'catalogSha256' names. Both are compared lowercased.
//
// There is exactly one such pair, and it exists because two toolchains built the
// same disk. hd1k_combo is the only one of the twenty images whose bytes differ
// between ioscpm v1.4.12 and catalog-v0-3.5.1.json, and the difference is not in
// anything a guest can reach: both are 51,380,224 bytes, slices 1-5 are
// byte-identical, slice 0's directory lists the same 94 files, and all 94 extract
// byte-identical - r8.com and w8.com included, at 1,792 bytes each. The 2,342
// bytes that differ are CP/M slack between those files: unallocated blocks still
// holding a deleted file's content, plus a little 0xE5 directory padding.
// Measured, not assumed - romwbw_disks docs/FINDINGS.md section 10 has the ranges.
//
// So a migrated machine holds an image whose every file already matches the
// catalog, under a hash that does not. Refreshing it would spend 49 MB of
// somebody's connection to replace 2,342 bytes of garbage with different garbage.
//
// Keyed on PROVENANCE, which is what makes this an exception rather than a hole:
// a download records the catalog it was fetched against, so only the migration
// can put the pre-v0 hash in that field. It cannot bless a corrupt, truncated or
// unrelated image, and it stops applying once a machine fetches the canonical one.
bool isEquivalentPriorImage(const std::string& provenance,
                            const std::string& catalogSha256);

bool looksLikeV0Name(const std::string& filename);

// The v0 filename for a bare pre-v0 one. False - and 'out' untouched - when the
// name already looks like a v0 name, or when it is not one legacyCatalogFilenames()
// carries, which covers every image the user imported themselves.
bool v0NameFor(const std::string& filename, std::string& out);

// Path splitting, on both separators because a path can reach the config from a
// Browse dialog or from the old INI file. Neither touches the file system.
std::string basenameOf(const std::string& path);
std::string parentOf(const std::string& path);

// Whether 'path' names a file sitting DIRECTLY in 'dir', compared with
// DiskLedger::fold so that the answer does not depend on how either was typed.
//
// Stricter than the _strnicmp prefix test in MainWindow::applyConfig, on
// purpose. That one decides whether to apply a completeness check, where
// over-matching costs nothing; this one decides whether a stored path is the
// migration's to rewrite, and a prefix test would also claim
// <dataDir>\mine\hd1k_combo.img and even <dataDir>_old\hd1k_combo.img. The data
// folder is flat, so "directly in" is the whole of what the catalog ever writes
// there.
bool isDirectlyIn(const std::string& path, const std::string& dir);

// Folded pre-v0 filename -> the catalog's v0 filename, for every name whose v0
// file is actually present in the data folder once the file pass has finished.
//
// Only names that LANDED belong in here, and that is the whole contract: a
// stored path or a ledger key is rewritten because the file is now there under
// the new name, never because the mapping exists. A rename that failed, or a
// name neither of whose files is present, leaves everything that refers to it
// exactly as it was - a dangling path that still says what the user chose beats
// one this pass invented.
//
// Keyed on the folded name because that is the key DiskLedger stores under and
// the only spelling both halves can agree on.
using LandedNames = std::map<std::string, std::string>;

// One stored disk path. False - and 'out' untouched - unless the path names a
// file directly in the data folder whose basename landed. A path from Browse,
// from File > Load Disk or from the INI era pointing anywhere else is the
// user's own and is never rewritten.
//
// The path's own directory text is kept and only the basename replaced, so a
// path the user typed in a different case comes back looking the way they left
// it.
bool migratedDiskPath(const std::string& storedPath,
                      const std::string& dataDir,
                      const LandedNames& landed,
                      std::string& out);

// Move each landed name's ledger record onto the new key, carrying the
// provenance and the measurement facts unchanged - which is exactly what makes
// the rename free: the file's size and write time did not move, so
// measurementApplies stays true and nineteen of the twenty images come out
// Current against the v0 catalog rather than being re-hashed.
//
// A record already stored under the v0 key wins and the pre-v0 one is left in
// place: there is no evidence here about which of two records describes the file
// on disk, and removing the loser would destroy provenance that cannot be
// recovered. Returns how many records moved.
int migrateLedgerKeys(DiskLedger& ledger, const LandedNames& landed);

}  // namespace diskv0
