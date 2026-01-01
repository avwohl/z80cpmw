# z80cpmw Development Notes

## W8/R8 Host File Transfer (December 2024)

### Current Implementation

W8/R8 are CP/M utilities that transfer files between CP/M and the host system using HBIOS extension traps (RST 8):

```
H_OPEN_R = 0xE1  ; Open host file for reading (DE=filename)
H_OPEN_W = 0xE2  ; Open host file for writing (DE=filename)
H_READ   = 0xE3  ; Read byte, returns in A
H_WRITE  = 0xE4  ; Write byte (E=byte)
H_CLOSE  = 0xE5  ; Close file (C=0 read, C=1 write)
```

Files are read/written to the data folder: `%LocalAppData%\z80cpmw\data\`

### MP/M2 Limitation

The current implementation uses **global state** for file transfers:
- Only one file transfer can be active at a time
- In MP/M2 (multi-user), concurrent W8/R8 from different users would conflict

### Future Options (if needed)

1. **Add handles to protocol** - H_OPEN returns handle (0-3), H_WRITE/H_CLOSE take handle
2. **Serialize access** - Return error if transfer in progress
3. **Per-process state** - Track by MP/M process ID

### Current Workaround

Users can use **XMODEM** for file transfers in MP/M2 scenarios. The XMODEM protocol works through the terminal and doesn't require host-side file access.

## Data Directory Structure

All user data is stored in `%LocalAppData%\z80cpmw\`:

```
%LocalAppData%\z80cpmw\
  z80cpmw.ini          - Settings file
  data\                - Disk images and file transfers
    hd1k_combo.img     - Downloaded disk images
    hd1k_games.img
    <files from W8>    - Exported files from CP/M
    <files for R8>     - Files to import to CP/M
```

This location is used because Microsoft Store apps cannot write to Program Files.

## Store App Compatibility

- App install directory is read-only
- All writable files go to LocalAppData
- ROMs are read from app install directory (read-only resources)
