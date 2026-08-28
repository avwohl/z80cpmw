/*
 * Config.cpp - Configuration Management Implementation
 */

#include "pch.h"
#include "Config.h"
#include "EmulatorEngine.h"
#include "Keymap.h"
#include "include/nlohmann/json.hpp"
#include <cctype>
#include <cerrno>
#include <fstream>
#include <filesystem>
#include <set>
#include <sstream>
#include <system_error>

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
            {"scrollbackLines", c.scrollbackLines},
            {"bell", c.bellEnabled}
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
        // The literal here and AppConfig::bellEnabled's initialiser have to
        // agree, and they are eighty lines apart in two different files.
        // ctrlRToCpm above documents what happens when such a pair drifts: the
        // section is present in every existing config, so from_json's default is
        // the one that wins on upgrade and the struct's is never consulted.
        // tests/test_config.cpp parses a document whose sections are all
        // present but empty and requires the result to equal the defaults the
        // structs themselves declare, which fails the moment any pair in this
        // function drifts - not just this one.
        c.bellEnabled = display.value("bell", true);
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

//=============================================================================
// Diagnostics - what the document said that none of the from_json above read
//=============================================================================

// The schema the loader understands, expressed as a document rather than as a
// list of names.
//
// nlohmann 3.11.3 keeps no record of which members a from_json touched:
// j.value(name, default) is an ordinary lookup that leaves no trace, so the set
// of names that ARE read cannot be observed and has to be supplied. Supplying
// it by hand would put a second copy of every setting in this file, and the
// copy would be wrong the first time somebody added a setting and updated only
// one of them. Serialising an AppConfig through to_json above and taking its
// member names is the same list produced by the writer itself, so the schema
// cannot drift from what is actually written; tests/test_config.cpp asserts the
// consequence, that inspectDocument(referenceDocument()) is empty.
//
// The containers have to be filled first. to_json writes null for an empty disk
// slot and an empty array for no Dazzlers, and neither of those names a single
// member of the thing it is a container for, so a reference built from a plain
// default AppConfig would call disks[0].path and hardware.dazzler[0].port
// unknown. Only the names matter here, never the values.
//
// Deliberately not static: tests/test_config.cpp declares and drives this and
// inspectDocument directly, because neither can be declared in Config.h without
// dragging json.hpp into MainWindow.h behind it.
json referenceDocument() {
    AppConfig shape{};
    for (int i = 0; i < 4; i++) {
        shape.disks[i] = DiskConfig{};
    }
    shape.dazzlers.assign(1, DazzlerConfig{});
    return json(shape);
}

// Paths whose members are the user's names rather than ours, and are therefore
// never reported unknown.
//
// "keyboard.keys" maps a key name to an escape sequence, so its members are
// exactly the names Keymap.h resolves - "Ctrl+Left", "F7", whatever the user
// has bound - and calling one of them an unrecognised setting would be
// nonsense. A name in there that Keymap.h CANNOT resolve is a real problem, but
// a different one: collectKeyNameProblems() below reports it as an
// UnknownKeyName, because the answer is about key names and not about the shape
// of the document.
//
// The exemption is only from the MEMBER walk, and only once the node is the
// shape it is supposed to be - see the type check at the top of
// collectMemberProblems(). It used to be checked first, which exempted
// "keyboard.keys": [ ... ] from inspection entirely: the one free-form path in
// the schema was also the one place a wrong type could not be noticed.
static const std::set<std::string>& freeFormPaths() {
    static const std::set<std::string> paths = { "keyboard.keys" };
    return paths;
}

static std::string joinPath(const std::string& prefix, const std::string& name) {
    return prefix.empty() ? name : prefix + "." + name;
}

// The single guess worth making about a member name.
//
// A case-only difference is the typo this catches in practice - "fontsize" for
// "fontSize", "f5tocpm" for "f5ToCpm" - and it is the only guess that can be
// offered without inventing a similarity threshold. An edit-distance
// suggestion was tried on paper and rejected: at a distance of one it offers
// "port" for "prt" and also "path" for "prt", and a confidently wrong
// suggestion in a diagnostic costs more trust than the occasional right one
// earns.
static std::string nearestName(const json& reference, const std::string& name) {
    for (auto it = reference.begin(); it != reference.end(); ++it) {
        const std::string& known = it.key();
        if (known.size() != name.size()) continue;
        bool same = true;
        for (size_t i = 0; i < name.size(); i++) {
            if (std::tolower((unsigned char)known[i]) !=
                std::tolower((unsigned char)name[i])) {
                same = false;
                break;
            }
        }
        if (same) return known;
    }
    return std::string();
}

// Walk the real document against the reference and report every member the
// reference does not have, and every member whose value is the wrong kind of
// thing.
//
// Recursive, and into arrays as well as objects. A typo in disks[1].pth or
// hardware.dazzler[0].prt is exactly as silent as one at the top level, and an
// array element is where a hand-edited config is most likely to have one:
// nothing in the UI writes a disk entry by hand, so the people editing that
// part of the file are the ones typing the names themselves.
//
// The type check is the half that was missing. This used to recurse only when
// actual and reference were both objects or both arrays, so a node whose type
// differed from the reference fell out of both branches with NOTHING reported -
// and that is exactly where from_json guards on the type instead of letting the
// conversion throw: j["disks"].is_array(), hw["dazzler"].is_array() and
// KeyboardConfig's j["keys"].is_object(). A file containing
//     "keyboard": { "keys": ["Up", "\E[A"] }
// was skipped by the loader, said nothing, and had every custom binding
// replaced by the built-in defaults at the save later in that same launch. That
// is the "absorbed in silence, then silently deleted" failure this whole file
// exists to end, with the user's entire keymap inside it.
//
// Only STRUCTURED mismatches are reported - one side an object or an array and
// the other not the same thing. Two reasons for the restriction, both measured
// against what the schema actually does:
//   - a wrongly-typed SCALAR is not silent. j.value("fontSize", 20) on a string
//     throws type_error.302, and the trial conversion in inspectDocument turns
//     that into an UnreadableFile carrying the library's own words. Reporting
//     it here as well would put two diagnostics on one mistake.
//   - json::type() is far too fine to compare for numbers. The reference's
//     window.x comes from an int and is number_integer; the same field read out
//     of a file as "x": 100 is number_unsigned, because the parser gives a
//     non-negative literal the unsigned type. Comparing those would report a
//     type mismatch on an ordinary saved window position.
static void collectMemberProblems(const json& actual, const json& reference,
                                  const std::string& prefix, Diagnostics& out) {
    if ((actual.is_structured() || reference.is_structured()) &&
        actual.type() != reference.type()) {
        Diagnostic d;
        d.problem = Problem::TypeMismatch;
        d.path = prefix;
        d.detail = std::string("expected ") + reference.type_name() +
                   ", found " + actual.type_name();
        out.push_back(d);
        return;
    }

    if (freeFormPaths().count(prefix)) return;

    if (actual.is_object()) {
        for (auto it = actual.begin(); it != actual.end(); ++it) {
            std::string path = joinPath(prefix, it.key());
            auto known = reference.find(it.key());
            if (known == reference.end()) {
                Diagnostic d;
                d.problem = Problem::UnknownMember;
                d.path = path;
                // Not "near": windows.h still defines that as an empty macro
                // for the 16-bit memory models, so the declaration vanishes.
                std::string nearest = nearestName(reference, it.key());
                if (!nearest.empty()) d.detail = "did you mean \"" + nearest + "\"?";
                out.push_back(d);
                continue;
            }
            collectMemberProblems(it.value(), *known, path, out);
        }
        return;
    }

    if (actual.is_array() && !reference.empty()) {
        // Every element is checked against the reference's first element:
        // referenceDocument() populates both arrays with entries of the one
        // shape they can hold, and a JSON array in this schema is a list of
        // like things rather than a tuple.
        for (size_t i = 0; i < actual.size(); i++) {
            // A null element is an empty slot, not a wrongly-typed one:
            // to_json writes null for a disk unit with no image in it and
            // from_json reads it back as std::nullopt, so it has no members to
            // misspell and no type to get wrong. Skipped BEFORE the check at
            // the top of the recursion, which would otherwise call every empty
            // disk slot a mismatch against the reference's DiskConfig object.
            // "hardware": { "dazzler": [null] } has no such convention and is
            // skipped here too, but it does not go unreported: DazzlerConfig's
            // from_json throws on a null and the trial conversion in
            // inspectDocument turns that into an UnreadableFile.
            if (actual[i].is_null()) continue;
            collectMemberProblems(actual[i], reference[0],
                                  prefix + "[" + std::to_string(i) + "]", out);
        }
    }
}

// Everything wrong with the NAMES in "keyboard.keys".
//
// keymap::classifyName() answers this, and the four-row copy of Keymap.h's
// reservedKeys() table that used to live here has been deleted. The copy was
// written because the two changes landed in parallel and neither could depend
// on the other; both have landed, and Config.cpp already includes Keymap.h.
// Keeping it would have meant that reserving a fifth combination took an edit
// in two files, and that forgetting the second one made this diagnostic go
// quiet in exactly the release where the key stopped working.
//
// Names are still resolved rather than compared as strings, and the modifier
// test is still a mask rather than an equality - both are now properties of
// keyIdForName() and reservedFor() rather than of anything written here.
//
// classifyName also answers the case the local table could not: a name
// vkForName rejects outright. Those are reported as UnknownKeyName, which is
// new. It is worth reporting because it is invisible from every other angle -
// "F13", "PgeUp" and "Ctrl_Left" resolve to nothing, so KeyMap::build drops
// them on the floor and load()'s fill loop steps over them, and the line sits
// in z80cpmw.json looking exactly like a binding that works. It is reported and
// NOT dropped: to_json writes the keys object back whole, so the misspelling
// survives for the user to correct. That is the same treatment a reserved
// binding gets, which is why renderBlock() covers both with one sentence.
static void collectKeyNameProblems(const json& doc, Diagnostics& out) {
    if (!doc.contains("keyboard") || !doc["keyboard"].is_object()) return;
    const auto& kb = doc["keyboard"];
    // A "keys" that is not an object has no names in it to classify, and is
    // already reported as a TypeMismatch by collectMemberProblems().
    if (!kb.contains("keys") || !kb["keys"].is_object()) return;

    for (auto it = kb["keys"].begin(); it != kb["keys"].end(); ++it) {
        const char* why = nullptr;
        keymap::NameStatus status = keymap::classifyName(it.key(), nullptr, &why);
        if (status == keymap::NameStatus::Ok) continue;

        Diagnostic d;
        d.path = "keyboard.keys." + it.key();
        if (status == keymap::NameStatus::Reserved) {
            d.problem = Problem::ReservedKey;
            d.detail = std::string("the terminal answers this itself (") +
                       (why ? why : "reserved") +
                       "), so the sequence would never reach CP/M";
        } else {
            d.problem = Problem::UnknownKeyName;
            d.detail = "no key of that name - see \"Bindable key names\" in "
                       "docs/CONFIGURATION.md";
        }
        out.push_back(d);
    }
}

// Everything wrong with one document, whether or not it came from a file.
//
// The trial get<AppConfig>() is what turns a member from_json REQUIRES rather
// than defaults into a diagnostic. There is exactly one such member in the
// schema - DiskConfig's j.at("path") - and it throws for the whole document, so
// "disks": [ { "isManifest": true } ] loses every other setting in the file
// too. That is worth a sentence to the user rather than a silent fall back to
// defaults. loadFromFile then parses a second time; the cost is one pass over a
// file of a few hundred bytes, and the alternative is that this function cannot
// be asked about a document with no file behind it, which is how the tests ask.
Diagnostics inspectDocument(const json& doc) {
    Diagnostics out;
    collectMemberProblems(doc, referenceDocument(), std::string(), out);
    collectKeyNameProblems(doc, out);

    try {
        (void)doc.get<AppConfig>();
    } catch (const std::exception& e) {
        Diagnostic d;
        d.problem = Problem::UnreadableFile;
        d.detail = e.what();   // names the member and the reason; do not discard it
        out.push_back(d);
    }
    return out;
}

// Move a config file that could not be read out of the way, returning where it
// went - or an empty string, with `whyNot` filled in, if it could not be moved.
//
// This is no longer the thing standing between a stray comma and the loss of
// the user's work, and it should not be: it used to be, and it was standing
// there alone. load() discarded loadFromFile's return value, so when this
// rename failed the file stayed where it was AND the save at the end of load()
// landed on it - no backup and no original. The rule is now in load() itself:
// nothing is saved automatically after a file we could not read. See the save
// at the end of ConfigManager::load().
//
// The rename stays because that guarantee only covers the AUTOMATIC save. A
// Settings dialog, or the window placement written when the app closes, calls
// save() on its own later in the same session, and that would still land on the
// broken file. Renaming first makes the worst case a file the user renames
// back.
//
// Non-clobbering, which the fixed z80cpmw.json.bad was not. The note here used
// to argue for one fixed name on the grounds that "if a second load fails the
// copy worth keeping is the one that just failed" - which assumes the user saw
// the first quarantine and dealt with it. Nothing makes that true; the report
// is printed into a terminal window that scrolls. So the second failure's
// rename replaced a backup of the user's real configuration, saved weeks
// earlier, with a near-defaults file written yesterday. Numbering keeps both.
// A timestamped name is still rejected, for the reason it always was:
// z80cpmw.json.bad is a name the diagnostic can print and the user can find,
// and z80cpmw.json.2026-08-28T09-15-02.bad is litter nobody ever deletes.
//
// The pile does not grow on its own. Once the file has been renamed there is
// nothing at z80cpmw.json, so the next launch writes a fresh one and the launch
// after that has nothing to quarantine; reaching .bad3 takes three separate
// occasions of breaking the file by hand. The cap is for the pathological case,
// and AT THE CAP THE FILE IS LEFT WHERE IT IS rather than overwriting .bad20 -
// the entire point of this function is that nothing in it destroys a copy of
// the user's work, and the diagnostic says so instead.
//
// exists() then rename() is not atomic. Losing that race needs a second process
// writing into one user's config directory during startup, and it costs one
// overwritten backup rather than the live file, so it is not worth a lock file.
static std::string quarantineUnreadable(const std::string& path,
                                        std::string& whyNot) {
    const int maxBackups = 20;
    for (int n = 1; n <= maxBackups; n++) {
        std::string backup = path + ".bad" +
                             (n == 1 ? std::string() : std::to_string(n));
        std::error_code ec;
        if (fs::exists(backup, ec)) continue;
        fs::rename(path, backup, ec);
        if (!ec) return backup;
        whyNot = ec.message();
        return std::string();
    }
    whyNot = fs::path(path).filename().string() + ".bad and .bad2 through .bad" +
             std::to_string(maxBackups) + " are all taken";
    return std::string();
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
    // Whatever the previous load found is about to stop being true.
    m_diagnostics.clear();

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
    //
    // Fill in MISSING names rather than only populating a wholly empty map. The
    // empty-map test never fires for someone who already has a config, so a
    // default added in a later version - the four Ctrl+arrows, say - stayed
    // invisible in their file forever, even though the app honoured it. Only
    // names that are absent are added, so every override the user has written
    // survives, and a key they deliberately unbound with "" keeps its entry and
    // stays unbound.
    // Match by the KEY a name resolves to, not by the name itself. "Ctrl+Left",
    // "Control+Left" and "ctrl+left" are three spellings of one binding, and
    // adding the canonical spelling beside a user's alias would leave two
    // entries for the same key in one file - with no defined answer as to which
    // one the app then honours.
    std::set<long> present;
    for (const auto& kv : m_config.keyboard.keys) {
        long id = keymap::keyIdForName(kv.first);
        if (id >= 0) present.insert(id);
    }
    for (const auto& kv : keymap::defaultBindings()) {
        long id = keymap::keyIdForName(kv.first);
        if (id < 0 || present.count(id)) continue;
        m_config.keyboard.keys[kv.first] = kv.second;
        present.insert(id);
        needSave = true;
    }

    // DO NOT SAVE OVER A FILE WE FAILED TO READ.
    //
    // That is the rule everything else in this file's handling of a broken
    // config is built on, and it is the one that was missing. load() used to
    // throw loadFromFile's answer away: a file that could not be opened or
    // could not be parsed left m_config default-constructed, the fill loop
    // above then found every default binding absent and set needSave, and
    // save() wrote defaults straight onto the original bytes. Where the rename
    // in quarantineUnreadable had also failed - a read-only z80cpmw.json.bad
    // already sitting there, a locked directory - there was then no backup AND
    // no original, and the user's configuration was simply gone. With this
    // rule, whatever the rename does, nothing is destroyed.
    //
    // Stated over the diagnostics rather than over `ok`, because loadFromFile
    // returns true for a document it only PARTLY read. A TypeMismatch means one
    // of from_json's is_array()/is_object() guards skipped a whole section, and
    // saving would write our defaults over text nothing ever looked at:
    // "keyboard": { "keys": [ ... ] } is the case that matters, where the save
    // replaces every hand-written binding with the built-in ones in the same
    // launch that failed to read them. Dropping an UnknownMember at the next
    // save is deliberately NOT in this list and stays as it was - nothing was
    // read out of that member because there is nothing in it to read, and
    // renderBlock() says out loud that it is about to go.
    //
    // Only the save that load() itself performs is suppressed. Every other call
    // to save() still writes - MainWindow's saveSettings(), the window
    // placement recorded when the app closes, the first-run welcome flag. That
    // is where this rule stops, and for an UnreadableFile that is enough,
    // because the file has been renamed out from under those saves. For a
    // TypeMismatch it is NOT a complete answer: the file parses, so it is not
    // quarantined, and a save later in the session still writes our defaults
    // over the section that was skipped. Closing that properly means carrying
    // the text we could not read through to the next to_json, which is a change
    // to the shape of the format rather than to its loader, and is not
    // attempted here. What this buys is that a launch which only reads settings
    // no longer destroys them on its own.
    //
    // The cost of suppressing this one save is that the default key bindings
    // are not materialised into the file on this launch; the next launch that
    // reads the file cleanly does it.
    bool partlyUnread = !ok;
    for (const auto& d : m_diagnostics) {
        if (d.problem == Problem::UnreadableFile ||
            d.problem == Problem::TypeMismatch) {
            partlyUnread = true;
            break;
        }
    }

    if (needSave && !partlyUnread) {
        save();
    }
    return ok;
}

bool ConfigManager::save() {
    return saveToFile(getConfigPath());
}

bool ConfigManager::loadFromFile(const std::string& path) {
    Diagnostics found;
    try {
        // A file that will not open is reported, not returned from silently.
        // This function used to begin "if (!file.is_open()) return false;",
        // ahead of everything else in it: no diagnostic was appended, the
        // quarantine below was never reached, and load() went on to write
        // defaults over the file. The enum case is called UnreadableFile and
        // problemLabel() renders it "could not be read:", and the one case
        // where the file genuinely could not be READ was the case with nothing
        // to say. Reproduced by holding the file open without FILE_SHARE_READ.
        //
        // errno carries the reason, and the reason is what distinguishes the
        // two causes in the report. It is worth having even though it is coarse
        // here: MSVC's ifstream opens through _wfsopen, so a Windows sharing
        // violation and a genuine permissions problem both arrive as EACCES -
        // "permission denied" at least tells the reader which half of the
        // report to believe, where a bare "could not be read" does not.
        errno = 0;
        std::ifstream file(path);
        if (!file.is_open()) {
            Diagnostic d;
            d.problem = Problem::UnreadableFile;
            d.detail = "the file could not be opened";
            if (errno != 0) {
                d.detail += " (";
                d.detail += std::error_code(errno, std::generic_category()).message();
                d.detail += ")";
            }
            found.push_back(d);
            // Falls through to the quarantine below, which is the point.
        } else {
            json j;
            file >> j;

            // Inspect before parsing, and parse only if the inspection found
            // the document readable. inspectDocument runs the same
            // get<AppConfig>() to discover whether it throws, so doing these
            // the other way round would report one failure twice.
            found = inspectDocument(j);
            bool readable = true;
            for (const auto& d : found) {
                if (d.problem == Problem::UnreadableFile) { readable = false; break; }
            }
            if (readable) {
                m_config = j.get<AppConfig>();
                m_diagnostics.insert(m_diagnostics.end(), found.begin(), found.end());
                return true;
            }
        }
    } catch (const std::exception& e) {
        // Bind the exception. Its text - "[json.exception.parse_error.101]
        // parse error at line 4, column 3: syntax error while parsing value" -
        // names the line the user has to fix, and the handler this replaces
        // caught it as (...) and dropped it on the floor.
        found.clear();
        Diagnostic d;
        d.problem = Problem::UnreadableFile;
        d.detail = e.what();
        found.push_back(d);
    }

    // Unreadable. m_config is left untouched so a corrupt profile cannot
    // clobber live state that a later save would then persist over the user's
    // good config, and the file itself is moved aside so that a save later in
    // the session cannot land on top of it. The move applies to profiles as
    // well as to the main config: a profile that cannot be parsed is not
    // loadable either, and preserving it costs nothing.
    for (auto& d : found) {
        if (d.problem != Problem::UnreadableFile) continue;
        d.path = path;
        std::string whyNot;
        d.backup = quarantineUnreadable(path, whyNot);
        if (d.backup.empty()) {
            // An empty Diagnostic::backup renders as nothing at all in
            // renderBlock(), so if the fact that the file is still sitting
            // there under its own name is not said here it is not said
            // anywhere. It is the thing the reader most needs: it is the file
            // they have to fix, move or delete, and until they do, every launch
            // will refuse to save settings for the same reason.
            d.detail += "; it could not be moved aside (" + whyNot +
                        "), so the original is still there under its own name";
        }
    }
    m_diagnostics.insert(m_diagnostics.end(), found.begin(), found.end());
    return false;
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

    // Cleared here rather than at the top, so that asking for a profile that
    // does not exist leaves the diagnostics from the config actually in force.
    m_diagnostics.clear();

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
