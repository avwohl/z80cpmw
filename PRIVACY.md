# Privacy Policy for Z80CPM

**Last updated:** September 10, 2026

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

## Network Access

**Z80CPM needs the network to run at all.** This section said the opposite until
September 10, 2026 — that network access was optional and that "if you never use
these features, the app makes no network connections" — and that stopped being
true on September 7, when the application stopped carrying a ROM of its own.

Nothing bootable is installed with the app. Every request goes to public GitHub
release assets, and there are three:

- **The catalog index and a release's catalog** — the documents that list what is
  on offer. Read whenever the app needs to know what it can fetch, including on
  the way to starting the machine.
- **The ROM** — the firmware the emulated machine runs. It is downloaded the first
  time you start the machine and checked against the size and SHA-256 the catalog
  publishes; a ROM that does not match is not loaded. **A machine that has never
  reached the network has no ROM it is allowed to load and will not start.**
- **Disk images** — downloaded when you ask for them, and checked the same way.
- **In-app help** — fetching the latest help topics (a bundled offline copy is
  used if there is no network).

**No personal information is transmitted in any of these requests.** They are
ordinary anonymous HTTPS downloads of public files: no account, no identifier, no
telemetry, and nothing about you or your machine beyond what any download sends —
your IP address and a user-agent naming the app and its version. GitHub, as the
host of those files, sees those requests; its privacy statement governs what
GitHub does with them. Nothing is sent to the developer.

You can point the app at a different catalog — a mirror, or your own — with the
**Catalog index** setting under Settings → Disk Images, or the
`ROMWBW_INDEX_URL` environment variable. It then contacts that host instead, and
whoever runs it sees the requests in GitHub's place.

## Contact

If you have questions about this privacy policy, please open an issue at:
https://github.com/avwohl/z80cpmw/issues

## Changes

Any changes to this privacy policy will be posted to this page.
