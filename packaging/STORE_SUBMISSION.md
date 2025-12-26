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
- [x] App Name: **Z80CPM**
- [x] Publisher: **avwohl**
- [x] Package Identity: `avwohl.z80cpmw`
- [ ] Update `Publisher` in AppxManifest.xml with your actual certificate CN after Store registration

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

Required if app collects any user data. Z80CPM:
- Does NOT collect personal data
- Does NOT require internet (disk catalog is optional)
- Stores settings locally only

If no data collection, you can state:
```
This app does not collect, store, or transmit any personal information.
```

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

Output: `dist\z80cpmw.msix`

### NSIS Installer (for direct distribution)

```powershell
# Install NSIS first from https://nsis.sourceforge.io/

cd packaging\scripts
.\build-nsis.ps1 -Configuration Release
```

Output: `dist\z80cpmw-1.0.0-setup.exe`

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

## Files Structure

```
packaging/
├── msix/
│   ├── AppxManifest.xml      # MSIX package manifest
│   ├── mapping.txt           # File mapping for package
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
