/*
 * Config.h - Configuration Management
 *
 * Provides JSON-based configuration persistence with profile support.
 * Handles migration from legacy INI format.
 */

#pragma once

#include "ConfigReport.h"

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <map>

namespace config {

// Configuration version for format migration
constexpr int CURRENT_VERSION = 2;

// Disk unit configuration
struct DiskConfig {
    std::string path;           // Full path to disk image
    bool isManifest = false;    // True if from catalog (may be overwritten on update)
};

// Main window position and size, so it reopens where the user left it.
// width/height of 0 means "not saved yet, use the default size". The monitor
// bounds it was saved on are recorded too: if the monitor layout changed (or
// the saved spot is now off-screen), the saved position is discarded.
struct WindowConfig {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool maximized = false;

    // Bounds (virtual-screen coords) of the monitor the window was on when saved.
    int monLeft = 0;
    int monTop = 0;
    int monRight = 0;
    int monBottom = 0;
};

// Keyboard configuration: how special keys are delivered to CP/M.
struct KeyboardConfig {
    // F1 and F5 double as application shortcuts (Help, and emulator Start/Stop).
    // When false (default), those shortcuts win and the key is NOT sent to CP/M.
    // When true, the shortcut is released so the key reaches CP/M via the keymap.
    bool f1ToCpm = false;   // false: F1 = Help.  true: F1 -> CP/M
    bool f5ToCpm = false;   // false: F5/Shift+F5 = Start/Stop.  true: -> CP/M

    // Ctrl+R is the Reset shortcut, but unlike F1/F5 it is also ordinary ASCII:
    // 0x12 is a CP/M console line-editing character and the WordStar family of
    // editors binds it too. Reserving it costs the user a key CP/M really uses,
    // so this one defaults the other way: ^R reaches CP/M and Reset stays on
    // the Emulator menu. Set false to get the Ctrl+R shortcut back.
    //
    // Half of that justification is gone; the default is deliberately not.
    // This note used to end "and a mistyped Reset reboots the machine without
    // asking", which stopped being true: onEmulatorReset() now puts up a
    // Yes/No box with MB_DEFBUTTON2 whenever the emulator isRunning(), so a
    // Ctrl+R nobody meant costs a dialog rather than the session. The default
    // stays true on the half that remains, which was always the load-bearing
    // half: the confirmation only limits the damage of a keystroke the user did
    // NOT intend, while reserving ^R takes a working CP/M key away every time
    // they do intend it, for the whole session. Reset is rare and has a menu
    // item; retyping a command line is not. Anyone who wants the accelerator
    // more than the key sets this false.
    bool ctrlRToCpm = true;

    // Key name -> termcap-style escape string (see Keymap.h). Empty on a fresh
    // load; populated with the built-in defaults so it is visible/editable in
    // z80cpmw.json. Any name absent here falls back to the built-in default.
    std::map<std::string, std::string> keys;
};

// Dazzler graphics card configuration
struct DazzlerConfig {
    bool enabled = false;       // Whether this Dazzler instance is active
    uint8_t port = 0x0E;        // Base I/O port
    int scale = 4;              // Display scale factor
};

// Sections of a configuration file that none of the from_json functions in
// Config.cpp could use, kept as the text they arrived as.
//
// Keyed by RFC 6901 JSON pointer - "/keyboard/keys", "/disks", "/hardware" -
// because that is what to_json splices an entry back in at, and a pointer is
// followed by the library rather than parsed here. The value is the section as
// nlohmann's own dump() renders it, which is the parsed VALUE and not the
// user's bytes: their spacing and their order of members inside an object are
// gone by the time the document reaches us either way.
using UnreadSections = std::map<std::string, std::string>;

// Complete application configuration
struct AppConfig {
    int version = CURRENT_VERSION;

    // Core emulator settings
    std::string rom = "emu_avw.rom";
    bool debug = false;
    std::string bootString;
    bool warnManifestWrites = true;  // Warn when writing to downloaded catalog disks
    bool welcomeShown = false;       // True once the Getting Started help has been auto-shown

    // Display settings
    int fontSize = 20;
    std::string fontName = "Consolas";
    int scrollbackLines = 1000;       // terminal history capacity, in lines (0 = off)

    // Whether BEL (0x07) sounds. Default true, because a terminal that has
    // always beeped and silently stops is a bug report, whereas a terminal that
    // beeps too much is a setting the user can find. It is worth having a
    // setting at all: CP/M software rings the bell freely - WordStar on every
    // rejected keystroke - and MessageBeep is the system sound, not a soft
    // click. Settings > Terminal carries the checkbox and TerminalView holds the
    // live state; this field is only where it is written down between runs.
    bool bellEnabled = true;

    // Keyboard / key bindings
    KeyboardConfig keyboard;

    // Main window placement
    WindowConfig window;

    // Disk units (0-3)
    std::optional<DiskConfig> disks[4];

    // Hardware peripherals
    std::vector<DazzlerConfig> dazzlers;

    // What the file this configuration was read from said that nothing above
    // could be made out of it. Filled by ConfigManager::loadFromFile from the
    // TypeMismatch findings inspectDocument reports.
    //
    // It belongs to ONE FILE - the one named in unreadSectionsFrom below - and
    // it is written back to that file and to no other. Only one member of this
    // struct is held at all: a load that succeeds replaces the whole of it
    // before refilling this member from the document it just read, so a section
    // the user has corrected leaves nothing behind, and a section that is wrong
    // in two files read in one session is held once, by the file that won. A
    // file that could not be read replaces nothing - loadFromFile leaves
    // m_config exactly as it was - so the carry stays with the settings it came
    // in with, still attached to the file those settings came from.
    //
    // It is not a setting and it is not part of the document. Nothing outside
    // Config.cpp reads it, and to_json writes each entry back at the path it
    // came from rather than under a name of this member's, so no name of its own
    // ever appears in z80cpmw.json. That is also why it is invisible to the
    // drift canary in tests/test_config.cpp, which builds its idea of the schema
    // by running an AppConfig through to_json: a member to_json does not write
    // is not in that schema. Adding it there would put a bookkeeping entry in
    // the user's file for no one to read.
    UnreadSections unreadSections;

    // The path unreadSections was read out of, exactly as ConfigManager handed
    // it to loadFromFile. Empty whenever unreadSections is.
    //
    // Without it the carry followed the SETTINGS rather than the file, and the
    // settings move between files: ConfigManager::save() always writes
    // getConfigPath() and saveAsProfile() always writes a profile path, so a
    // profile whose "disks" was an object put that object into z80cpmw.json at
    // the next save, and a main config whose "keyboard.keys" was an array put
    // the array into every profile saved afterwards. One file's unparseable
    // text ended up in another file, where the user had never typed it and
    // where correcting the first file would never clear it.
    //
    // Compared with == and nothing cleverer. Both sides are produced by
    // getConfigPath()/getProfilePath() from one getConfigDir(), so the two
    // strings are byte-identical when they name the same file; a case-folding
    // or canonicalising comparison would only matter for a caller that built a
    // path some other way, and ConfigManager::saveToFile has no such caller.
    // Getting it wrong in the safe direction costs a carry that is dropped
    // rather than one that is misfiled.
    std::string unreadSectionsFrom;
};

// Singleton configuration manager
class ConfigManager {
public:
    // Get singleton instance
    static ConfigManager& instance();

    // Load configuration (handles migration from INI if needed)
    bool load();

    // Save current configuration
    bool save();

    // Profile management
    std::vector<std::string> listProfiles() const;
    bool loadProfile(const std::string& name);
    bool saveAsProfile(const std::string& name);
    bool deleteProfile(const std::string& name);
    std::string currentProfileName() const { return m_currentProfile; }

    // Configuration access
    AppConfig& get() { return m_config; }
    const AppConfig& get() const { return m_config; }

    // What the last load() or loadProfile() found in the file that it could not
    // use. Empty when the file was understood completely.
    //
    // It describes the configuration NOW IN FORCE. load(), and a loadProfile()
    // that succeeds, replace it outright, so a file that loads cleanly takes the
    // previous one's complaints down with it rather than leaving them standing
    // over settings nobody is using any more.
    //
    // A loadProfile() that FAILS changes no setting - loadFromFile leaves
    // m_config alone when it cannot read a file - so it keeps the report about
    // the configuration still in force and puts this attempt's behind it. The
    // added entry is an UnreadableFile naming the profile, and it is the only
    // place the parser's line and column, and the name the file was quarantined
    // to, are ever said. Retrying the same profile replaces that entry rather
    // than adding a second copy of it; a second DIFFERENT profile that will not
    // read adds one, which is the only way this list grows within a session.
    const Diagnostics& diagnostics() const { return m_diagnostics; }

    // Path utilities
    std::string getConfigDir() const;
    std::string getConfigPath() const;
    std::string getProfilesDir() const;
    std::string getProfilePath(const std::string& name) const;

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Migration from legacy INI format
    bool migrateFromINI();
    bool parseOldINI(const std::string& path);

    // File I/O
    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;

    AppConfig m_config;
    std::string m_currentProfile;  // Empty = using main config
    Diagnostics m_diagnostics;
};

} // namespace config
