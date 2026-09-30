# Building z80cpmw

**Requirements:**

- Visual Studio 18 or later (the project sets `PlatformToolset` to `v145`)
- Windows SDK 10.0 or later
- **wxWidgets 3.3** through vcpkg, one triplet per architecture you build:
  `wxwidgets:x64-windows` for `Platform=x64`, `wxwidgets:arm64-windows` for
  `Platform=ARM64`. An x64 machine builds both; the ARM64 one is a cross-build.
  `z80cpmw.vcxproj` looks under `C:\temp\vcpkg\installed\<triplet>\`; install
  vcpkg there or pass `/p:WxVcpkgRoot=<your vcpkg>`. Run `vcpkg integrate
  install` once as well - that is what copies the wx DLLs beside the exe.
  Without the triplet the build stops at `SettingsDialogWx.h(9): fatal error
  C1083: Cannot open include file: 'wx/wx.h'`.
- [`cpmemu`](https://github.com/avwohl/cpmemu) and
  [`romwbw_emu`](https://github.com/avwohl/romwbw_emu) checked out **next to**
  this one: the project compiles emulator sources directly from `..\cpmemu\src`
  and `..\romwbw_emu\src`.
- **Optional:** [`ioscpm`](https://github.com/avwohl/ioscpm), also next to this
  one. When present, `z80cpmw.rc` compiles the shared help assets from
  `..\ioscpm\release_assets` into the executable as `RCDATA`, so the online help
  topics can be read with no network and an empty cache. Without it the build
  succeeds and defines `NO_BUNDLED_HELP_ASSETS`; what is lost is only the
  offline floor for those topics. `/p:BundleHelpAssets=false` forces that build
  on a machine that does have the checkout.

Open `z80cpmw.sln` in Visual Studio and build the solution.
`tests\run_tests.bat` needs `..\cpmemu` and `..\romwbw_emu` for its last three
suites and stops without them; without `..\ioscpm` the help suite skips its
bundled-asset section and runs the rest.

Packaging is scripted: `packaging\scripts\build-msix.ps1` builds the unsigned
Store package, and `-Beta` builds and signs the sideload package instead.
`-SkipBuild` packages an existing `bin\Release` without rebuilding, checking the
binary's version against `z80cpmw/Version.h` and refusing a mismatch. See
[packaging/STORE_SUBMISSION.md](../packaging/STORE_SUBMISSION.md) and
[docs/CODE_SIGNING.md](CODE_SIGNING.md).
