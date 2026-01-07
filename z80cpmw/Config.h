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

namespace config {

// Configuration version for format migration
constexpr int CURRENT_VERSION = 2;

// Disk unit configuration
struct DiskConfig {
    std::string path;           // Full path to disk image
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

    // Display settings
    int fontSize = 20;
    std::string fontName = "Consolas";

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
