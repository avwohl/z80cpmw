# Microsoft Store Submission Guide for Z80CPM

This guide covers all requirements for submitting Z80CPM to the Microsoft Store.

## Prerequisites

1. **Microsoft Partner Center Account**
   - Register at: https://partner.microsoft.com/dashboard
   - One-time registration fee: ~$19 USD (individual) or ~$99 USD (company)

2. **Windows SDK** (for MSIX packaging)
   - Install via Visual Studio Installer or standalone SDK

3. **NSIS** (optional, for traditional installer)
   - Download from: https://nsis.sourceforge.io/

## Store Requirements Checklist

### App Identity

These are the values Partner Center reserved for this app; they are already in
`packaging/msix/AppxManifest.xml` and **must not be changed** for a Store upload:

- [x] App Name (DisplayName): **Z80CPM**
- [x] Publisher display name: **Aaron Wohl**
- [x] Package Identity `Name`: `AaronWohl.Z80CPM`
- [x] Package Identity `Publisher`: `CN=724C9014-DD22-420E-9BB4-F2740D082EB0`

The `Publisher` is a Microsoft-assigned GUID, **not** a certificate you own — see
[Signing](#signing-who-signs-what) below.

### Required Assets

The assets in `packaging/msix/Assets` are final, not placeholders. The package
logos were produced by `packaging\scripts\convert-icons.ps1` from the shared app
icon and are the ones in the current 1.0.22 build. Do **not** run
`generate-icons.ps1` over that folder: its default output directory is that same
folder and it writes placeholders on top of the real artwork. `build-msix.ps1`
generates icons only when `Assets\StoreLogo.png` is missing.

| Asset | Size | Status |
|-------|------|--------|
| Square44x44Logo.png | 44x44 | Final (shipped) |
| Square71x71Logo.png | 71x71 | Final (shipped) |
| Square150x150Logo.png | 150x150 | Final (shipped) |
| Square310x310Logo.png | 310x310 | Final (shipped) |
| Wide310x150Logo.png | 310x150 | Final (shipped) |
| StoreLogo.png | 50x50 | Final (shipped) |
| SplashScreen.png | 620x300 | Final (shipped) |

**Additional scaled versions** (for high DPI):
- Scale-100, 125, 150, 200, 400 versions of `Square44x44Logo` and `Square150x150Logo`

The same folder also holds the Store listing artwork. Nothing in the manifest
references it, but `build-msix.ps1` copies the whole folder, so it rides along
inside the package:

| Asset | Size | Status |
|-------|------|--------|
| AppTile_300x300.png | 300x300 | Final (listing art) |
| BoxArt_1080x1080.png, BoxArt_2160x2160.png | 1080x1080, 2160x2160 | Final (`create-boxart.ps1`) |
| Poster_720x1080.png, Poster_1440x2160.png | 720x1080, 1440x2160 | Final (`create-poster.ps1`) |
| screenshot1.png | 824x656 | Under the minimum listed below; recapture before uploading |

### Store Listing Content

Prepare the following for Partner Center:

**Description (up to 10,000 characters):**
```
Z80CPM is a Z80 CPU emulator for running CP/M and other vintage operating systems on modern Windows PCs.

Features:
- Full Z80 CPU emulation
- RomWBW HBIOS compatibility
- VT100 terminal emulation with customizable fonts
- Support for multiple disk images (.img, .dsk)
- Disk catalog with downloadable disk images
- ROM and disk images fetched from the published RomWBW catalog and checked against its SHA-256
- New ROMs and disk images appear in the app without an update

Perfect for:
- Retro computing enthusiasts
- CP/M software preservation
- Educational purposes
- Running vintage software

Needs an internet connection: nothing bootable is installed with the app. The ROM and the CP/M and Z-System disk images are downloaded from the public RomWBW catalog on GitHub and checked against it before the machine runs.
```

Two lines that used to be in that copy — "Built-in ROM files for immediate
use" and "Includes CP/M and Z-System disk images ready to boot" — were false
and must not come back. No disk image has been in the package for some time, and since
2026-09-07 no ROM is either: `build-msix.ps1` stages `z80cpmw.exe`, the DLLs
beside it and `Assets\`, and makes no `roms\` or `disks\` directory at all,
while `z80cpmw.nsi` installs no `.rom` file. Nothing in the install boots by
itself: at startup `MainWindow::loadDefaultROM` loads no ROM and only posts a
notice saying the ROM is downloaded on the first Start, and
`MainWindow::romReadyToStart` refuses to start until a catalog has been read and
the ROM it names has been fetched and checked against the size and sha256 the
catalog carries. Listing copy that promises a ready-to-boot install therefore
describes the one thing the package cannot do, to the reader most likely to try
it with no connection.

**Short Description (up to 100 characters):**
```
Z80 CPU emulator for running CP/M and vintage operating systems
```

**Keywords:**
```
Z80, CP/M, emulator, retro computing, vintage, 8-bit, terminal, VT100, RomWBW
```

**Category:** Developer Tools > Development utilities
**Subcategory:** Emulator

### Screenshots

Prepare 1-10 screenshots:
- Minimum size: 1366x768 or 768x1366
- Maximum size: 3840x2160
- Format: PNG, JPG, BMP, or GIF

Recommended screenshots:
1. Main terminal window with CP/M booted
2. File loading dialog
3. Settings/configuration dialog
4. Running a classic CP/M program

### Age Rating

Complete the IARC questionnaire in Partner Center. Z80CPM should qualify for:
- **PEGI 3** / **ESRB E (Everyone)**
- No violence, gambling, user interaction, or objectionable content

### Privacy Policy

The published policy is [PRIVACY.md](../PRIVACY.md). In summary, Z80CPM:
- Does NOT collect personal data, run analytics, or require accounts
- Does NOT send anything to the developer automatically
- Uses the internet for the catalog in `avwohl/romwbw_disks` — which carries
  the ROM as well as the disk images — and for in-app help, both public GitHub
  release assets. The catalog stopped being optional on 2026-09-07: with no ROM
  in the package, a machine that has never reached it has nothing it is allowed
  to boot
- Stores everything locally, including settings, disk images, R8/W8 transfers, and
  diagnostic/crash files (which are only shared if the user chooses to send them)

The Store listing can state:
```
This app does not collect, store, or transmit any personal information.
```

## Signing (who signs what)

**You do not sign the Store package.** The Microsoft Store **re-signs** every
submission with its own certificate during ingestion, and distributes the app with
*that* signature — trusted on all Windows machines. This is why the very first
*unsigned* upload worked: the Store signs it for you.

Consequences worth remembering:

- **Upload the Store package unsigned.** `signtool` requires the signing cert's
  subject to equal the manifest `Publisher`, which for the Store is the assigned
  GUID `CN=724C9014-…` — a certificate only Microsoft holds. So a Store-identity
  package physically cannot be self-signed; it goes up unsigned and Microsoft signs
  it. The default `build-msix.ps1` output is exactly this.
- **Uploading a *signed* package does not "use" your signature** — the Store
  discards it and re-signs. Signing a Store submission yourself buys nothing.
- **Do NOT upload the beta MSIX to the Store.** `build-msix.ps1 -Beta` rewrites the
  `Publisher` to your Trusted Signing cert subject (`CN=Aaron Wohl, …`) and signs it
  for **sideloading**. Partner Center will reject it — not because it is signed, but
  because its identity/publisher no longer matches your reservation
  (`CN=724C9014-…`). The beta package is a separate identity that installs
  side-by-side with the Store build; it is for GitHub/direct distribution only.

See [docs/CODE_SIGNING.md](../docs/CODE_SIGNING.md) for the full two-vehicle policy
and the beta signing commands.

## Building the Packages

### MSIX Package (for Store)

```powershell
# From the repository root
cd packaging\scripts

# The Store assets and z80cpmw\z80cpmw.ico are committed and final. Do NOT run
# generate-icons.ps1 or create-ico.ps1 over them - both write placeholders, and
# create-ico.ps1 overwrites the .ico that is compiled into the exe.

# Build MSIX package (Store identity, unsigned)
.\build-msix.ps1 -Configuration Release
```

Output: `dist\z80cpmw.msix` — this is the **Store** package (Store identity,
unsigned). Upload it as-is; Microsoft signs it. For the signed **beta** package
(sideloading), run `.\build-msix.ps1 -Beta` instead, which emits
`dist\z80cpmw-<version>-beta.msix` (currently `dist\z80cpmw-1.0.23-beta.msix`,
the same binary as the 1.0.23 Store package signed under our own publisher) — do **not**
upload that one to the Store (see [Signing](#signing-who-signs-what)).

The beta build is done in two stages, because the signed one is irreversible: it
spends an Azure Trusted Signing call and writes the published `-beta.msix` name.

```powershell
.\build-msix.ps1 -Beta -SkipBuild -SkipSign   # stage 1: rehearsal, always safe
.\build-msix.ps1 -Beta                        # stage 2: only on an unshipped version
```

Stage 1 does everything stage 2 does except sign, and names its outputs
`dist\z80cpmw-<version>-beta-unsigned.msix` and `…-beta-unsigned.pdb` so a dry run
can never overwrite or be mistaken for a shipped package. Delete both when the run
is checked. Before stage 2, compare `z80cpmw\Version.h` against the published
releases: the version guard only proves `bin\Release\z80cpmw.exe` matches
`Version.h`, so re-running on a shipped version replaces that artifact with a
different binary carrying the same number and reports nothing wrong. `-SkipSign` is
the dry run; `build-msix.ps1` rejects `-WhatIf` outright rather than pretending to
honour it. (`build-nsis.ps1` carries the same `[CmdletBinding()]` for the same
reason and rejects `-WhatIf` too, though it has no `-SkipSign`-shaped dry run to
offer in its place.) Full recipe in
[docs/CODE_SIGNING.md](../docs/CODE_SIGNING.md).

### NSIS Installer (for direct distribution)

```powershell
# Install NSIS first from https://nsis.sourceforge.io/

cd packaging\scripts
.\build-nsis.ps1 -Configuration Release
```

Output: `dist\z80cpmw-<version>-setup.exe`

The NSIS installer is not the current distribution vehicle: direct downloads are
served as the signed beta MSIX, and no `-setup.exe` has been released since
v1.0.14.

## Store Submission Steps

### First submission — already completed, do not repeat

1. **Reserve App Name**
   - Go to Partner Center > Apps and games > New product
   - Select "MSIX or PWA app"
   - Reserve "Z80CPM"

2. **Update Package Identity**
   - After reservation, Partner Center provides:
     - Package/Identity/Name
     - Package/Identity/Publisher
     - Package/Properties/PublisherDisplayName
   - Done: those values are committed in `packaging\msix\AppxManifest.xml`
     (`Name="AaronWohl.Z80CPM"`,
     `Publisher="CN=724C9014-DD22-420E-9BB4-F2740D082EB0"`, PublisherDisplayName
     `Aaron Wohl`) and must not be edited. The committed `Version` stays
     `0.0.0.0` by design: `build-msix.ps1` injects the real version into a staged
     copy of the manifest and never writes to the committed file.

3. **Create Submission**
   - Pricing: Free or Paid
   - Markets: Select target countries
   - Upload screenshots and descriptions
   - Complete age rating questionnaire

### Every update — the current flow

The Store carried 1.0.14, then 1.0.19, 1.0.22, 1.0.23, 1.0.25 and now **1.0.29**.
**Do not trust that sentence either - MEASURE it.** `tools/check-store-version.sh`
asks the Store what it actually serves, and a CI job runs it on every push; this
paragraph has been wrong four times, because a submission going out leaves no
trace in this repository. 1.0.29 is what it reported on 2026-09-10, and
`z80cpmw/Version.h` is at **1.0.31**, whose unsigned package is built and awaiting
upload.

1. **Pick the version**
   - Bump `z80cpmw/Version.h` to a number free on both channels (see
     [Version numbers](#version-numbers)). Nothing else records a version.

2. **Rebuild Release**
   - `build-msix.ps1` refuses to package a `bin\Release\z80cpmw.exe` whose
     version does not match `Version.h`.

3. **Nothing to do — no ROM and no disk images ship in the package**
   - **As of 1.0.23 the package contains no `disks\` folder, and since
     2026-09-07 no `roms\` folder either.** Both come from the interface-v0
     catalog in **`avwohl/romwbw_disks`**, which is two documents:
     `index-v0.json`, at the one URL compiled into the binary
     (`z80cpmw/CatalogV0.cpp:24`), listing the RomWBW releases; and a
     `catalog-v0-<ver>.json` per release, carrying that release's `base_url`,
     `roms[]` and `disks[]`. No `RELEASE_TAG` constant is left in this tree —
     `git grep RELEASE_TAG` finds only comments explaining that it was deleted
     rather than repointed — and nothing interpolates a tag into an asset URL:
     every URL but the index is read out of a document. That is the design; a
     bundled copy is a second source of the same file that can only disagree
     with the first.
   - **The three ROMs went with the `roms\` directory on 2026-09-07.**
     `emu_avw.rom` and `emu_romwbw.rom` were byte-identical to each other and to
     the catalog's `emu_avw-v0-3.5.1.rom` (sha256 `4b11402a29fad22d…`, measured
     2026-09-07), so the package paid twice to ship one image; `SBC_simh_std.rom`
     was a stock hardware ROM the emulator could not run and nothing staged it.
     Three files stopped moving them: `z80cpmw.vcxproj`'s PostBuildEvent no
     longer copies `roms\emu_*.rom` into `$(OutDir)roms` (the Release arm still
     copies the app-local CRT DLLs), `build-msix.ps1` no longer creates or fills
     a `roms\` staging directory, and `z80cpmw.nsi` has lost its two `File`
     lines and its `SetOutPath $INSTDIR\roms`. The uninstaller still *deletes*
     all three names, which is cleanup for installs that had them and not
     evidence that anything still ships them.
   - **The cost of that, which the listing copy must not paper over:** a first
     launch with no network can no longer start the machine. Every ROM now comes
     from the catalog and is checked against the size and sha256 only the catalog
     carries, so a machine that has never reached the network has no ROM it is
     allowed to load — the offline branch that used to answer "the release this
     build ships a ROM for" is gone from `MainWindow::romReadyToStart` along with
     the ROM it named. The app says so and offers to retry; it never boots
     unverified bytes.
   - Up to and including 1.0.22 both vehicles *did* bundle `hd1k_combo.img` and
     `hd1k_games.img`, and **nothing ever read them**. The only function that
     looked in the install directory's `disks\` was `loadDefaultDisks()`, which
     wanted `cpm_wbw.img` and `zsys_wbw.img` — neither of which was staged — and
     it had no caller; it was deleted in 1.0.24, and the two images it wanted
     stopped being tracked in the repository in 1.0.25.
     `downloadAndStartWithDefaults()`, which is the path a
     real user takes, looks in the *user data* folder and downloads what is
     missing. So the two images cost 57 MB of payload (12.7 MB → 7.07 MB
     packaged) and bought nothing.
   - What the user actually runs is therefore decided by what `romwbw_disks`
     publishes — not by this build, and no longer by a constant inside it. A new
     ROM or a new disk image within a release the client already offers reaches
     users with no packaging step and no release here at all; that is what makes
     the catalog's second ROM, `emu_rcz80`, selectable without a build of this
     application. What still needs a build is a new RomWBW *version*, because
     which versions are offered is decided by the emulator core rather than by
     this repository: `DiskCatalog::fetchCatalogInto` filters the index through
     `emu_romwbw_release_supported()`, which answers from
     `ROMWBW_SUPPORTED_RELEASES` in `..\romwbw_emu\src\romwbw_pin.h` (3.5.1
     and 3.6.0 today, both recorded there as checked 2026-09-05). See
     `CLAUDE.md`.
   - Images are verified where they are published: `romwbw_disks`
     `tools/verify_release.sh` checks every ROM's HCB, every bootable image's
     CBIOS banner and the `06 E9 CF` `HBF_HOST_CAPS` probe in every `w8.com`,
     against the catalog that names them. `packaging/scripts/verify-disk-assets.sh`
     did that here and was deleted on 2026-09-05: this package has no images to
     check, and had none for some time before that. Since 2026-09-07 it has no
     ROM to check either.

4. **Build the package**
   - `cd packaging\scripts` then `.\build-msix.ps1 -Configuration Release`,
     which writes `dist\z80cpmw.msix`.

5. **Upload Package**
   - Partner Center > Z80CPM > New submission, and upload `dist\z80cpmw.msix`
     **unsigned** — Microsoft validates and re-signs it.

6. **Submit for Certification**
   - Update the release notes, then submit.
   - Microsoft reviews within 1-3 business days
   - Address any certification failures

## Certification Tips

Common rejection reasons and how to avoid them:

1. **App doesn't launch** - Test thoroughly on clean Windows install
2. **Missing functionality** - Ensure all menu items work
3. **Poor metadata** - Use accurate, complete descriptions
4. **Inappropriate content** - N/A for this app
5. **Privacy policy missing** - Required if collecting data

Item 1 acquired a new failure mode on 2026-09-07. A clean install with no network
still reaches a window, a terminal and the startup text, but F5 does not boot it:
with no ROM in the package and no catalog read, `MainWindow::startEmulator`'s
`hasROM()` guard puts up "No ROM is loaded, so there is nothing to run" and
`offerRomChoice` explains that the ROM is downloaded the first time the machine
starts. A certification pass run where github.com is unreachable would therefore
see an app that never boots anything, and a screenshot taken there would show an
error box. Say in the listing that the first start downloads a ROM, rather than
leaving a tester to find that out.

## Post-Submission

After approval:
- App available in Store within 24 hours
- Monitor reviews and ratings
- Push updates via Partner Center
- Respond to user feedback

## Version numbers

The version lives in exactly one file: `z80cpmw/Version.h`. Edit the four
`#define`s there and nothing else. `build-msix.ps1` and `build-nsis.ps1` parse
them and inject the result into the package manifest and the installer, and both
refuse to run if the number does not match the compiled `bin\Release\z80cpmw.exe`.

A `-beta` suffix names the signed sideload/GitHub package and a bare number names
the Microsoft Store release. `build-msix.ps1 -Beta` rewrites the manifest
`Publisher` to the signing-cert subject, so the two are separate package
identities that install side by side. They share a version number only when they
carry the same build — as 1.0.23 does, where `dist\z80cpmw.msix` and
`dist\z80cpmw-1.0.23-beta.msix` hold the same `z80cpmw.exe` (sha256
`800715614bd5e20f…` inside both), because the beta was cut with `-SkipBuild` off
the build the Store package was made from; where the builds differ, the numbers
must differ too. As of 2026-09-10 the Store carries **1.0.29**, the newest published
sideload package is **v1.0.28-beta**, and **1.0.30-beta** (signed) and **1.0.31**
(the unsigned Store package) are both built and published on neither channel - so
the next change on either channel takes 1.0.32 or later. Note that 1.0.30-beta and
1.0.31 are the same SOURCE and different BUILDS, which is exactly why they carry
different numbers. Check both channels before bumping, and measure the Store one: the recent Store releases (1.0.19, 1.0.22) carry no git tag and no
GitHub release, while the older ones (1.0.10, 1.0.14) do, so `git tag` and
`gh release list` are not evidence of what has shipped.

The `Version` in the committed `AppxManifest.xml` is a `0.0.0.0` placeholder, so
a package built by any route other than `build-msix.ps1` carries a version the
Store will reject rather than a wrong one it would accept. `z80cpmw.nsi` has no
default version at all and fails to compile unless the script supplies it.

`VERSION_BUILD` (the fourth field) stays `0`: the Store reserves the revision
field.

## Files Structure

```
packaging/
├── msix/
│   ├── AppxManifest.xml      # MSIX package manifest (version is a placeholder)
│   └── Assets/               # Store icons and listing art (final)
├── nsis/
│   └── z80cpmw.nsi          # NSIS installer script
├── scripts/
│   ├── convert-icons.ps1    # Produced the final Store assets and z80cpmw.ico
│   ├── create-boxart.ps1    # Store listing box art
│   ├── create-poster.ps1    # Store listing poster art
│   ├── generate-icons.ps1   # Placeholder icons (do not run over the final assets)
│   ├── create-ico.ps1       # Placeholder .ico (superseded by convert-icons.ps1)
│   ├── build-msix.ps1       # Build MSIX package
│   ├── build-nsis.ps1       # Build NSIS installer
└── STORE_SUBMISSION.md      # This file
```

## Support

- Repository: https://github.com/avwohl/z80cpmw
- Issues: https://github.com/avwohl/z80cpmw/issues
