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
- Built-in ROM files for immediate use
- Disk catalog with downloadable disk images

Perfect for:
- Retro computing enthusiasts
- CP/M software preservation
- Educational purposes
- Running vintage software

Includes CP/M and Z-System disk images ready to boot.
```

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
- Uses the internet only for two optional, on-demand features (disk catalog and
  in-app help), both from public GitHub assets
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

The Store carried 1.0.14, then 1.0.19, and now **1.0.22**, published 2026-08-23.
1.0.23 is packaged and awaiting submission.

1. **Pick the version**
   - Bump `z80cpmw/Version.h` to a number free on both channels (see
     [Version numbers](#version-numbers)). Nothing else records a version.

2. **Rebuild Release**
   - `build-msix.ps1` refuses to package a `bin\Release\z80cpmw.exe` whose
     version does not match `Version.h`.

3. **Nothing to do — no disk images ship in the package**
   - **As of 1.0.23 the package contains no `disks\` folder at all.** Every port
     gets its disk images from the **ioscpm release area**, through the catalog
     pinned in `DiskCatalog.cpp`'s `RELEASE_TAG`. That is the design; a bundled
     copy is a second source of the same file that can only disagree with the
     first.
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
   - The image the user actually runs is therefore governed by `RELEASE_TAG`,
     not by this build. Changing which images users get is a **code change**
     with its own release, not a packaging step. See `CLAUDE.md`.
   - `packaging/scripts/verify-disk-assets.sh` is still the right tool for
     checking a set of images before they are published, and
     `romwbw_emu/disks/verify_disk_utils.sh` is its upstream twin — but neither
     is a gate on *this* package any more, because this package has no images
     to check.

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
must differ too. As of 2026-09-03 the Store still carries **1.0.22** and
1.0.23 is built on both vehicles but published on neither, so the next change on
either channel takes 1.0.24 or later. Check both channels before
bumping: the recent Store releases (1.0.19, 1.0.22) carry no git tag and no
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
│   └── verify-disk-assets.sh # Check bin/Release/disks before packaging (sh)
└── STORE_SUBMISSION.md      # This file
```

## Support

- Repository: https://github.com/avwohl/z80cpmw
- Issues: https://github.com/avwohl/z80cpmw/issues
