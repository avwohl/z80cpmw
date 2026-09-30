# Using z80cpmw

The README has the first steps. This page covers the boot menu, the keyboard,
copy and paste, and R8/W8 file transfer.

## Boot Menu Keys

Every command is a line, so it needs Enter.

- `2` - boot the first hard disk, slice 0; `2.3` for slice 3
- `d` - list the devices
- `h` - the command set. On RomWBW 3.6.0 this lists the ROM applications too;
  on 3.5.1 they are under `l`, which 3.6.0 answers with `*** Invalid command`

Units 0 and 1 are the on-board RAM and ROM memory disks and carry no operating
system, so they boot nothing.

## Keyboard

Arrow keys, Home/End, Insert, PageUp/PageDown and the function keys (F1-F12)
send VT100/xterm escape sequences to CP/M. Because CP/M is pure ASCII with no
standard for these keys, every binding is configurable - see
[Configuration](CONFIGURATION.md).

By default `F1` opens Help and `F5` / `Shift+F5` start and stop the emulator, so
those two are not passed to CP/M unless you enable them in the config. Reset has
no shortcut by default: `Ctrl+R` is a character CP/M itself uses, so it goes to
the guest and Reset stays on the **Emulator** menu. Set `"ctrlRToCpm": false` to
claim `Ctrl+R` for Reset instead.

## Mouse Copy/Paste

Drag to select text in the terminal, then right-click for **Copy** and
**Paste**. `Ctrl+C` / `Ctrl+V` are left untouched so they still reach CP/M as
`^C` / `^V`.

## File Transfer (R8 / W8)

`R8 name` imports a file from the host into CP/M; `W8 name` exports one. On
Windows both take a **full path**, and `W8` prints the path the file really went
to - which on any MSIX install is the redirected `LocalCache` location, not the
one you typed:

```
R8 C:\Users\me\Desktop\getkey2.com
W8 REPORT.TXT C:\Users\me\Desktop\report.txt
```

A bare name (`W8 out.com`) goes to the app's data folder, whose real location
the app shows in **Emulator → Settings** (with an **Open Folder** button),
**Help → About**, and the boot banner.

Both utilities come from the disk catalog, not from this app, and only the
`hd1k_combo` image carries them - boot a single-OS image and there is no `R8` or
`W8` to run. [docs/FILE_TRANSFER.md](FILE_TRANSFER.md) covers where
exported files land on the Store build and on the other ports.
