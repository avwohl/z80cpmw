/*
 * resource.h - Resource definitions
 */

#pragma once

// Application icon
#define IDI_APPICON             1

// Main menu IDs
#define IDR_MAINMENU            100

// File menu
#define ID_FILE_LOADROM         1001
#define ID_FILE_LOADDISK0       1002
#define ID_FILE_LOADDISK1       1003
#define ID_FILE_SAVEDISK0       1004
#define ID_FILE_SAVEDISK1       1005
#define ID_FILE_SAVEDISKS       1006
#define ID_FILE_LOADPROFILE     1007
#define ID_FILE_SAVEPROFILE     1008
#define ID_FILE_EXIT            1010

// Emulator menu
#define ID_EMU_START            2001
#define ID_EMU_STOP             2002
#define ID_EMU_RESET            2003
#define ID_EMU_SETTINGS         2004

// View menu
#define ID_VIEW_FONT14          3001
#define ID_VIEW_FONT16          3002
#define ID_VIEW_FONT18          3003
#define ID_VIEW_FONT20          3004
#define ID_VIEW_FONT24          3005
#define ID_VIEW_FONT28          3006
#define ID_VIEW_DAZZLER         3010

// Help menu
#define ID_HELP_TOPICS          4000
#define ID_HELP_ABOUT           4001

// ROM selection submenu
#define ID_ROM_EMU_AVW          5001
#define ID_ROM_EMU_ROMWBW       5002
#define ID_ROM_SBC_SIMH         5003

// Status bar
#define IDC_STATUSBAR           6001

// Timer
#define IDT_EMULATOR            7001

// The eight help assets compiled in from ..\ioscpm\release_assets: the index
// and the seven topics it lists, in the index's own order. z80cpmw.rc turns
// each into an RCDATA resource and help_assets::bundledTopic reads them back;
// the name-to-id table lives in HelpAssets.cpp, so a new topic needs a line
// here, a line in the .rc and a line in that table.
//
// 8000 rather than an id near the menu's: these are data, not user interface,
// and nothing dispatches on them, so they are kept clear of the WM_COMMAND
// ranges above where a collision would be a silently wrong menu item.
#define IDR_HELP_INDEX          8000
#define IDR_HELP_QUICK_START    8001
#define IDR_HELP_CPM22          8002
#define IDR_HELP_ZSDOS          8003
#define IDR_HELP_NZCOM          8004
#define IDR_HELP_ZPM3           8005
#define IDR_HELP_QPM            8006
#define IDR_HELP_FILE_TRANSFER  8007

