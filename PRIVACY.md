# Privacy Policy for Z80CPM

**Last updated:** August 23, 2026

## Overview

Z80CPM is a Z80 CPU emulator for running CP/M and vintage operating systems. It
is designed with privacy in mind.

## Data Collection

**Z80CPM does not collect, store, or transmit any personal information.**

Specifically, the application:
- Does not collect personal data
- Does not track usage or analytics
- Does not require user accounts
- Does not send any data to the developer automatically
- Does not share any data with third parties

## Local Storage

The application stores the following on your device only, in its user data folder
(`%LOCALAPPDATA%\z80cpmw\`, redirected into the package container —
`…\Packages\AaronWohl.Z80CPM_<hash>\LocalCache\Local\z80cpmw\` — on any MSIX
install, whether from the Microsoft Store or the signed sideload beta):

- **Settings** — your preferences (fonts, window position, keyboard map, ROM/disk
  assignments) in `z80cpmw.json`.
- **Disk images** — any images you download from the catalog or create, plus files
  you transfer with the R8/W8 utilities, under the `data\` subfolder.
- **Diagnostics** — a rolling log file (`z80cpmw.log`) and, if the app hits an
  internal error, a crash report (`z80cpmw-crash-*.dmp`). A crash report is a
  memory snapshot used to diagnose the fault; it can contain whatever the program
  had in memory at the time.

None of this data leaves your device unless you choose to send it — for example,
by attaching a crash report to a bug report. The app never uploads it for you.

## Optional Network Access

Z80CPM contacts the network only for two optional, on-demand features, both served
from public GitHub release assets:

- **Disk catalog** — downloading prebuilt disk images.
- **In-app help** — fetching the latest help topics (a bundled offline copy is used
  if there is no network).

No personal information is transmitted during either request. If you never use
these features, the app makes no network connections.

## Contact

If you have questions about this privacy policy, please open an issue at:
https://github.com/avwohl/z80cpmw/issues

## Changes

Any changes to this privacy policy will be posted to this page.
