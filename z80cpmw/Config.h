/*
 * Config.h - Configuration Management
 *
 * Provides JSON-based configuration persistence with profile support.
 * Handles migration from legacy INI format.
 */

#pragma once

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
    // and a mistyped Reset reboots the machine without asking, so this one
    // defaults the other way: ^R reaches CP/M and Reset stays on the Emulator
    // menu. Set false to get the Ctrl+R shortcut back.
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

    // Keyboard / key bindings
    KeyboardConfig keyboard;

    // Main window placement
    WindowConfig window;

    // Disk units (0-3)
    std::optional<DiskConfig> disks[4];

    // Hardware peripherals
    std::vector<DazzlerConfig> dazzlers;
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
};

} // namespace config
