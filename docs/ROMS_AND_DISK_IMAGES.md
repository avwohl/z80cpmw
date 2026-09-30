# ROMs and Disk Images

**Nothing is bundled.** The package carries the executable, the wxWidgets and
VC++ runtime DLLs, and (in a build made with the optional `..\ioscpm` checkout)
the help text. It carries no ROM and no disk image.

Both come from the interface-v0 catalog in
[romwbw_disks](https://github.com/avwohl/romwbw_disks). The only content address
compiled into this application is the index that catalog starts from
(`z80cpmw/CatalogV0.cpp`); the help base URL comes out of that index at run
time, and the index names every published RomWBW release and points at that
release's own list of ROMs and disk images, so no download URL is ever assembled
here from a version number.

Everything fetched lands in the data folder and is measured against the size and
SHA-256 the catalog publishes: a download whose bytes do not match is deleted
rather than kept, and a ROM is refused outright if the catalog carries no
checksum for it, because fifteen banks of unknown bytes under a CPU is not a
risk worth the convenience.

Three things are yours to choose, all under **Emulator → Settings**:

- **Which RomWBW release** - the picker at the top of the **Disk Images** page.
  That list is the index: every release `romwbw_disks` publishes is offered, and
  **a new RomWBW release reaches you with no new build of this application**,
  exactly as a new ROM or a new disk image does. Pre-releases are the one
  exception, and they are not releases - see **Show pre release** below.
  This paragraph said the opposite until 2026-09-17, and the thing it described
  was real: the list was filtered through the emulator core's
  `emu_romwbw_release_supported()`, which answered from a compile-time list in
  `romwbw_emu/src/romwbw_pin.h`, so a 3.7.0 entry was dropped by any binary built
  before somebody added 3.7.0 there. romwbw_emu v1.44 deleted the function, the
  list and the header. The reasoning is in `romwbw_emu/DOWNSTREAM.md`: the
  release number is the pairing between a ROM and a disk image - which the guest
  enforces itself, by printing `*** WARNING: HBIOS/CBIOS Version Mismatch ***` -
  and not a property of the emulator. What the emulator depends on is the
  emulator-to-ROM interface, and that is versioned by the name of the catalog
  this app reads: `index-v0.json`. An interface change the core could not service
  would be published as `index-v1.json`, which this build would never open.
  What the release picker still owes you is a matched pair, and it says so: pick
  a release whose ROM is not the one in the banks and the note under the picker
  tells you Start will offer to fetch that release's ROM.
- **Show pre release** - the checkbox under the release picker, off by default.
  `romwbw_disks` publishes RomWBW pre-releases beside the releases; upstream has
  not released them and they are never the catalog's default. Tick the box and
  they join the list. **Unticking it moves a machine that is on one** back to the
  catalog's default, and the ROM and the four disks follow, the same way they do
  when you change the release by hand. Nothing is deleted: the pre-release's
  images stay in the data folder.
- **Which ROM** - the dropdown on the **Machine** page, filled from the selected
  release's ROMs. Publishing a new ROM in `romwbw_disks`, into a release the
  picker already offers, makes it selectable with no new release of this
  application.
- **Which catalog** - the *Catalog index* field under the release picker. Leave
  it empty for the catalog this build ships with, or point it at another
  `romwbw_disks` index to try a release before it is published.
  `ROMWBW_INDEX_URL` in the environment overrides it for one run and stores
  nothing; the field is disabled and says so while it is set. The order -
  environment, then setting, then built-in - matches `romwbw_emu`'s
  `romwbw-get`. Every catalog reads and writes the same data folder, so two
  catalogs publishing an image under one name share one file;
  [`docs/CONFIGURATION.md`](CONFIGURATION.md) has the details.

## The first run

No ROM may be loaded before the catalog carrying its size and checksum has been
read, so a machine that has never reached the network has nothing it is allowed
to boot. Press **F5** on a fresh install and it fetches the catalog, downloads
the two default disk images (`hd1k_combo` and `hd1k_games`) and then the ROM
(about 512 KB); it starts when that ROM has arrived and matched. With no network
and nothing cached it says so and asks you to check the connection and press F5
again. After one success everything is cached, and later starts cost one small
catalog request and one checksum. If the catalog cannot be read later on, it
mounts whichever default disks are already cached and tries to start on those;
whether it gets there depends on the ROM gate, which needs a cached ROM for the
selected release. With neither, it says so instead of running empty banks.

A ROM placed by hand is searched for before the data folder's copy, in this
order: `<appDir>\roms\`, `<appDir>\`, `<appDir>\..\roms\`, then the data folder.
It is still used only after the catalog has been read and the file has matched
the published size and checksum.
