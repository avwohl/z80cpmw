/*
 * Config.cpp - Configuration Management Implementation
 */

#include "pch.h"
#include "Config.h"
#include "EmulatorEngine.h"
#include "Keymap.h"
#include "include/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace config {

// JSON serialization for DiskConfig
void to_json(json& j, const DiskConfig& d) {
    j = json{
        {"path", d.path},
        {"isManifest", d.isManifest}
    };
}

void from_json(const json& j, DiskConfig& d) {
    j.at("path").get_to(d.path);
    d.isManifest = j.value("isManifest", false);
}

// JSON serialization for DazzlerConfig
void to_json(json& j, const DazzlerConfig& d) {
    j = json{
        {"enabled", d.enabled},
        {"port", d.port},
        {"scale", d.scale}
    };
}

void from_json(const json& j, DazzlerConfig& d) {
    d.enabled = j.value("enabled", false);
    d.port = j.value("port", static_cast<uint8_t>(0x0E));
    d.scale = j.value("scale", 4);
}

// JSON serialization for WindowConfig
void to_json(json& j, const WindowConfig& w) {
    j = json{
        {"x", w.x},
        {"y", w.y},
        {"width", w.width},
        {"height", w.height},
        {"maximized", w.maximized},
        {"monLeft", w.monLeft},
        {"monTop", w.monTop},
        {"monRight", w.monRight},
        {"monBottom", w.monBottom}
    };
}

void from_json(const json& j, WindowConfig& w) {
    w.x = j.value("x", 0);
    w.y = j.value("y", 0);
    w.width = j.value("width", 0);
    w.height = j.value("height", 0);
    w.maximized = j.value("maximized", false);
    w.monLeft = j.value("monLeft", 0);
    w.monTop = j.value("monTop", 0);
    w.monRight = j.value("monRight", 0);
    w.monBottom = j.value("monBottom", 0);
}

// JSON serialization for KeyboardConfig
void to_json(json& j, const KeyboardConfig& k) {
    j = json{
        {"f1ToCpm", k.f1ToCpm},
        {"f5ToCpm", k.f5ToCpm},
        {"ctrlRToCpm", k.ctrlRToCpm},
        {"keys", k.keys}
    };
}

void from_json(const json& j, KeyboardConfig& k) {
    k.f1ToCpm = j.value("f1ToCpm", false);
    k.f5ToCpm = j.value("f5ToCpm", false);
    // Absent from every config written before Ctrl+R was released to CP/M. The
    // default here must match KeyboardConfig's, or upgrading would silently
    // re-arm the shortcut on machines that already have a z80cpmw.json.
    k.ctrlRToCpm = j.value("ctrlRToCpm", true);
    if (j.contains("keys") && j["keys"].is_object()) {
        k.keys = j["keys"].get<std::map<std::string, std::string>>();
    }
}

// JSON serialization for AppConfig
void to_json(json& j, const AppConfig& c) {
    j = json{
        {"version", c.version},
        {"core", {
            {"rom", c.rom},
            {"debug", c.debug},
            {"bootString", c.bootString},
            {"warnManifestWrites", c.warnManifestWrites},
            {"welcomeShown", c.welcomeShown}
        }},
        {"display", {
            {"fontSize", c.fontSize},
            {"fontName", c.fontName},
            {"scrollbackLines", c.scrollbackLines}
        }},
        {"keyboard", c.keyboard},
        {"window", c.window},
        {"hardware", {
            {"dazzler", c.dazzlers}
        }}
    };

    // Serialize disks array (with null for empty slots)
    json disksArray = json::array();
    for (int i = 0; i < 4; i++) {
        if (c.disks[i].has_value()) {
            disksArray.push_back(c.disks[i].value());
        } else {
            disksArray.push_back(nullptr);
        }
    }
    j["disks"] = disksArray;
}

void from_json(const json& j, AppConfig& c) {
    c.version = j.value("version", CURRENT_VERSION);

    // Core settings
    if (j.contains("core")) {
        const auto& core = j["core"];
        c.rom = core.value("rom", "emu_avw.rom");
        c.debug = core.value("debug", false);
        c.bootString = core.value("bootString", "");
        c.warnManifestWrites = core.value("warnManifestWrites", true);
        c.welcomeShown = core.value("welcomeShown", false);
    }

    // Display settings
    if (j.contains("display")) {
        const auto& display = j["display"];
        c.fontSize = display.value("fontSize", 20);
        c.fontName = display.value("fontName", "Consolas");
        c.scrollbackLines = display.value("scrollbackLines", 1000);
        if (c.scrollbackLines < 0 || c.scrollbackLines > 100000) {
            c.scrollbackLines = 1000;
        }
    }

    // Disks
    if (j.contains("disks") && j["disks"].is_array()) {
        const auto& disks = j["disks"];
        for (size_t i = 0; i < 4 && i < disks.size(); i++) {
            if (!disks[i].is_null()) {
                c.disks[i] = disks[i].get<DiskConfig>();
            } else {
                c.disks[i] = std::nullopt;
            }
        }
    }

    // Keyboard
    if (j.contains("keyboard")) {
        c.keyboard = j["keyboard"].get<KeyboardConfig>();
    }

    // Window placement
    if (j.contains("window")) {
        c.window = j["window"].get<WindowConfig>();
    }

    // Hardware
    if (j.contains("hardware")) {
        const auto& hw = j["hardware"];
        if (hw.contains("dazzler") && hw["dazzler"].is_array()) {
            c.dazzlers = hw["dazzler"].get<std::vector<DazzlerConfig>>();
        }
    }
}

// ConfigManager singleton
ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

std::string ConfigManager::getConfigDir() const {
    return EmulatorEngine::getUserDataDirectory();
}

std::string ConfigManager::getConfigPath() const {
    return getConfigDir() + "\\z80cpmw.json";
}

std::string ConfigManager::getProfilesDir() const {
    return getConfigDir() + "\\profiles";
}

std::string ConfigManager::getProfilePath(const std::string& name) const {
    return getProfilesDir() + "\\" + name + ".json";
}

bool ConfigManager::load() {
    std::string jsonPath = getConfigPath();
    std::string iniPath = getConfigDir() + "\\z80cpmw.ini";

    bool ok = true;
    bool needSave = false;

    if (fs::exists(jsonPath)) {
        // Try JSON config first
        ok = loadFromFile(jsonPath);
    } else if (fs::exists(iniPath) && migrateFromINI()) {
        // Migrated from old INI format; back up the original
        try {
            fs::rename(iniPath, iniPath + ".bak");
        } catch (...) {
            // Ignore backup errors
        }
    } else {
        // No config found, use defaults
        m_config = AppConfig{};
        needSave = true;
    }

    // Make the default key bindings visible/editable in z80cpmw.json. Absent on
    // a fresh install or when upgrading a config written before keymaps existed;
    // populate them and persist so the user can customize them. Keys still fall
    // back to built-in defaults at runtime even if this section is removed.
    if (m_config.keyboard.keys.empty()) {
        m_config.keyboard.keys = keymap::defaultBindings();
        needSave = true;
    }

    if (needSave) {
        save();
    }
    return ok;
}

bool ConfigManager::save() {
    return saveToFile(getConfigPath());
}

bool ConfigManager::loadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        json j;
        file >> j;
        AppConfig parsed = j.get<AppConfig>();
        m_config = std::move(parsed);
        return true;
    } catch (const std::exception&) {
        // Parse error - leave m_config untouched so a corrupt profile/settings
        // file can't clobber live state that a later saveSettings() would then
        // persist over the user's good config. (At startup m_config is still
        // the default-constructed AppConfig, so behavior there is unchanged.)
        return false;
    }
}

bool ConfigManager::saveToFile(const std::string& path) const {
    try {
        // Ensure directory exists
        fs::path filePath(path);
        fs::create_directories(filePath.parent_path());

        // Write to temp file first, then rename (atomic write)
        std::string tempPath = path + ".tmp";
        {
            std::ofstream file(tempPath);
            if (!file.is_open()) return false;

            json j = m_config;
            file << j.dump(2);  // Pretty print with 2-space indent
        }

        // Rename temp to final (atomic on most filesystems)
        fs::rename(tempPath, path);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigManager::migrateFromINI() {
    std::string iniPath = getConfigDir() + "\\z80cpmw.ini";
    if (!parseOldINI(iniPath)) {
        return false;
    }

    // Save as new JSON format
    return save();
}

bool ConfigManager::parseOldINI(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;

    m_config = AppConfig{};  // Start with defaults

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // Remove trailing newline/CR
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char* cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        // Parse key=value
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* value = eq + 1;

        // Map old keys to new structure
        if (strcmp(key, "disk0") == 0 || strcmp(key, "disk1") == 0 ||
            strcmp(key, "disk2") == 0 || strcmp(key, "disk3") == 0) {
            int idx = key[4] - '0';
            if (idx >= 0 && idx < 4 && strlen(value) > 0) {
                DiskConfig disk;
                disk.path = value;
                m_config.disks[idx] = disk;
            }
        } else if (strcmp(key, "bootString") == 0) {
            m_config.bootString = value;
        } else if (strcmp(key, "fontSize") == 0) {
            m_config.fontSize = atoi(value);
            if (m_config.fontSize < 10 || m_config.fontSize > 40) {
                m_config.fontSize = 20;
            }
        }
        // Note: rom, debug, dazzler settings weren't in old format
        // They get defaults from AppConfig constructor
    }

    fclose(f);
    return true;
}

std::vector<std::string> ConfigManager::listProfiles() const {
    std::vector<std::string> profiles;
    std::string profilesDir = getProfilesDir();

    try {
        if (!fs::exists(profilesDir)) {
            return profiles;
        }

        for (const auto& entry : fs::directory_iterator(profilesDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                profiles.push_back(entry.path().stem().string());
            }
        }
    } catch (...) {
        // Ignore errors
    }

    return profiles;
}

bool ConfigManager::loadProfile(const std::string& name) {
    std::string path = getProfilePath(name);
    if (!fs::exists(path)) {
        return false;
    }

    if (loadFromFile(path)) {
        m_currentProfile = name;
        return true;
    }
    return false;
}

bool ConfigManager::saveAsProfile(const std::string& name) {
    // Create profiles directory if needed
    std::string profilesDir = getProfilesDir();
    try {
        fs::create_directories(profilesDir);
    } catch (...) {
        return false;
    }

    std::string path = getProfilePath(name);
    if (saveToFile(path)) {
        m_currentProfile = name;
        return true;
    }
    return false;
}

bool ConfigManager::deleteProfile(const std::string& name) {
    std::string path = getProfilePath(name);
    try {
        if (fs::exists(path)) {
            fs::remove(path);
            if (m_currentProfile == name) {
                m_currentProfile.clear();
            }
            return true;
        }
    } catch (...) {
        // Ignore errors
    }
    return false;
}

} // namespace config
