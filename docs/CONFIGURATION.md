# z80cpmw Configuration (`z80cpmw.json`)

z80cpmw keeps its settings in a JSON file you can edit by hand:

```
%LOCALAPPDATA%\z80cpmw\z80cpmw.json
```

**Finding it:** *Emulator → Settings → Open Folder* opens the **data** folder
(`...\z80cpmw\data`, where disks and R8/W8 transfers live). The `z80cpmw.json`
file is one level up, in the `z80cpmw` folder. On the Microsoft Store build both
are redirected under `...\Packages\<package>\LocalCache\Local\z80cpmw\`.

Close z80cpmw before editing the file, then restart for changes to take effect.
(The same settings, including the keyboard map, are also viewable in-app from
*Help → Help Topics → Configuration File*.)

## Keyboard map

CP/M is pure ASCII and has no built-in function or navigation keys. Each CP/M
terminal historically defined its **own** escape sequences for them, so there is
no single standard — the correct bytes depend on the terminal your CP/M software
expects (VT100, ADM-3A, Televideo, Kaypro, …).

z80cpmw therefore lets you bind each special key to whatever bytes you choose,
written as **termcap-style escape strings** (the same notation used in
termcap/terminfo entries). The bindings live under `keyboard` in `z80cpmw.json`:

```json
"keyboard": {
  "f1ToCpm": false,
  "f5ToCpm": false,
  "keys": {
    "Insert": "\\E[2~",
    "F2": "\\EOQ"
  }
}
```

> **Backslashes are doubled.** JSON uses the backslash for its own escaping, so
> every backslash is written **twice** in the file. The Escape character (`\E`
> in termcap) becomes `\\E`, so Insert is stored as `"\\E[2~"`.

### Escape syntax

| Notation | Meaning |
| --- | --- |
| `\E`            | Escape, `0x1B` (written `\\E` in JSON) |
| `\n` `\r` `\t`  | Newline, Return, Tab |
| `\b` `\f` `\s`  | Backspace, Form-feed, Space |
| `\NNN`          | One byte in octal, e.g. `\033` = Escape |
| `^X`            | Control-X, e.g. `^C` = `0x03` |
| `^?`            | Delete, `0x7F` |

Any other character stands for itself. An **empty value unbinds** that key.

### Bindable key names

`Up`, `Down`, `Left`, `Right`, `Home`, `End`, `Insert`, `Delete`, `PageUp`,
`PageDown`, and `F1` through `F12`. Names are case-insensitive; `Ins`, `Del`,
`PgUp` and `PgDn` also work.

### Default bindings (VT220 / xterm)

Shown as written in the file (doubled backslashes):

| Key | Sends | Key | Sends |
| --- | --- | --- | --- |
| Up       | `\\E[A`  | F1  | `\\EOP`   |
| Down     | `\\E[B`  | F2  | `\\EOQ`   |
| Right    | `\\E[C`  | F3  | `\\EOR`   |
| Left     | `\\E[D`  | F4  | `\\EOS`   |
| Home     | `\\E[H`  | F5  | `\\E[15~` |
| End      | `\\E[F`  | F6  | `\\E[17~` |
| Insert   | `\\E[2~` | F7  | `\\E[18~` |
| Delete   | `^?`     | F8  | `\\E[19~` |
| PageUp   | `\\E[5~` | F9  | `\\E[20~` |
| PageDown | `\\E[6~` | F10 | `\\E[21~` |
|          |          | F11 | `\\E[23~` |
|          |          | F12 | `\\E[24~` |

These match the VT100-style sequences the terminal already used for the arrow
keys. Change any line to suit your CP/M program. For example, to make `F1`–`F4`
easier to parse in a hand-written key reader, give them the same `CSI` form as
the rest:

```json
"F1": "\\E[11~", "F2": "\\E[12~", "F3": "\\E[13~", "F4": "\\E[14~"
```

### F1 and F5

`F1` opens Help and `F5` / `Shift+F5` Start and Stop the emulator, so by default
those keys are **not** sent to CP/M. To deliver them to CP/M instead:

| Setting | Effect |
| --- | --- |
| `"f1ToCpm": true` | `F1` is sent to CP/M (Help stays on the Help menu) |
| `"f5ToCpm": true` | `F5` and `Shift+F5` are sent to CP/M |

`F10` normally opens the Windows menu bar; z80cpmw delivers it to CP/M when it is
bound in the keymap.

## Mouse copy and paste

Drag with the mouse to select text in the terminal, then **right-click** for
Copy and Paste. `Ctrl+C` and `Ctrl+V` are deliberately left untouched so they
still reach CP/M as `^C` and `^V`. Paste works only while the emulator is
running.

## Other settings

| Setting | Meaning |
| --- | --- |
| `display.fontSize` | Terminal font size, in points |
| `display.scrollbackLines` | Lines of terminal history kept for scrollback (0 = off) |
| `core.rom`         | ROM image to load at startup |
| `core.bootString`  | Text typed automatically at the boot menu |
| `disks`            | Disk images assigned to units 0–3 |

Most of these are easier to change from *Emulator → Settings*.
