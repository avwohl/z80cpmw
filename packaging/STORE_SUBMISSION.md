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

Run `packaging\scripts\generate-icons.ps1` to create placeholder icons, then replace with final designs:

| Asset | Size | Status |
|-------|------|--------|
| Square44x44Logo.png | 44x44 | Generated (placeholder) |
| Square71x71Logo.png | 71x71 | Generated (placeholder) |
| Square150x150Logo.png | 150x150 | Generated (placeholder) |
| Square310x310Logo.png | 310x310 | Generated (placeholder) |
| Wide310x150Logo.png | 310x150 | Generated (placeholder) |
| StoreLogo.png | 50x50 | Generated (placeholder) |
| SplashScreen.png | 620x300 | Generated (placeholder) |

**Additional scaled versions** (for high DPI):
- Scale-100, 125, 150, 200, 400 versions of key icons

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

# Generate icons (first time only)
.\generate-icons.ps1

# Create icon file
.\create-ico.ps1

# Build MSIX package
.\build-msix.ps1 -Configuration Release
```

Output: `dist\z80cpmw.msix` — this is the **Store** package (Store identity,
unsigned). Upload it as-is; Microsoft signs it. For the signed **beta** package
(sideloading), run `.\build-msix.ps1 -Beta` instead, which emits
`dist\z80cpmw-<version>-beta.msix` — do **not** upload that one to the Store (see
[Signing](#signing-who-signs-what)).

### NSIS Installer (for direct distribution)

```powershell
# Install NSIS first from https://nsis.sourceforge.io/

cd packaging\scripts
.\build-nsis.ps1 -Configuration Release
```

Output: `dist\z80cpmw-<version>-setup.exe`

## Store Submission Steps

1. **Reserve App Name**
   - Go to Partner Center > Apps and games > New product
   - Select "MSIX or PWA app"
   - Reserve "Z80CPM"

2. **Update Package Identity**
   - After reservation, Partner Center provides:
     - Package/Identity/Name
     - Package/Identity/Publisher
     - Package/Properties/PublisherDisplayName
   - Update `packaging\msix\AppxManifest.xml` with these values

3. **Create Submission**
   - Pricing: Free or Paid
   - Markets: Select target countries
   - Upload screenshots and descriptions
   - Complete age rating questionnaire

4. **Upload Package**
   - Upload the `.msix` file
   - Partner Center validates and signs the package

5. **Submit for Certification**
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
│   └── Assets/               # Store icons (generated)
├── nsis/
│   └── z80cpmw.nsi          # NSIS installer script
├── scripts/
│   ├── generate-icons.ps1   # Generate Store icons
│   ├── create-ico.ps1       # Create Windows .ico
│   ├── build-msix.ps1       # Build MSIX package
│   └── build-nsis.ps1       # Build NSIS installer
└── STORE_SUBMISSION.md      # This file
```

## Support

- Repository: https://github.com/avwohl/z80cpmw
- Issues: https://github.com/avwohl/z80cpmw/issues
