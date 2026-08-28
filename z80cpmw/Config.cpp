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

    // Last, and after j["disks"], so that a section the loader could not read
    // replaces whatever the lines above just wrote for it. This is the whole of
    // the fix for "a wrongly-typed section is still lost to a later save": the
    // type guards in from_json below skip such a section without reading one
    // thing out of it, so whatever the lines above wrote for it is something the
    // application supplied and nothing the file did - the built-in default at
    // load, or whatever has been put there since. Of the two directions, the
    // user's text over ours is the one that cannot destroy anything the file
    // ever held.
    //
    // Unconditional HERE, and gated one level up. This function is handed an
    // AppConfig and no destination, so it cannot tell z80cpmw.json from a
    // profile; ConfigManager::saveToFile is the one place that knows both, and
    // it clears the carry before serialising when the file being written is not
    // the file AppConfig::unreadSectionsFrom names. Doing it there rather than
    // by passing a path into to_json keeps nlohmann's ADL signature intact -
    // json(config) has to stay a legal thing to write, and referenceDocument()
    // and tests/test_config.cpp both write it.
    //
    // WHOLE SECTIONS, NEVER A MERGE. A carried section is text the application
    // never understood, so there is no way to put a value it computed into that
    // section without guessing what the text around it meant, and it does not
    // guess. The cost is the other half of the
    // same rule and renderBlock() says it out loud: while a section is carried,
    // nothing the application does to it reaches the file either - mount a disk
    // while "disks" is an object and the mount is gone at the next save. The
    // alternative was a per-section "the app has written this since" flag that
    // releases the carry; rejected because every writer of an AppConfig would
    // have to set it, and the one writer that forgot would restore the silent
    // overwrite this exists to end, in one section, with nothing able to see it.
    for (const auto& kv : c.unreadSections) {
        // The throwing parse, not json::parse(text, nullptr, false): a discarded
        // value dumps as "<discarded>", which is not JSON, and saveToFile would
        // write it into z80cpmw.json as though it were. Throwing instead makes
        // saveToFile return false and leaves the file alone. It should not be
        // reachable - the text is nlohmann's own dump() of a node its parser
        // accepted, and the parser rejects the malformed UTF-8 that is the one
        // thing dump() refuses - but the failure if it ever were is silent
        // corruption of the file this whole exercise is protecting.
        j[json::json_pointer(kv.first)] = json::parse(kv.second);
    }
}

void from_json(const json& j, AppConfig& c) {
    // A document never contains a carry, so reading one must never leave a
    // carry standing. get<AppConfig>() hands this function a fresh object and
    // would be safe on its own, but j.get_to(existing) hands it a LIVE one, and
    // the value that would survive there is the previous file's unreadable text
    // still pointed at the previous file. Two lines here make
    // "a carry cannot outlive the load that produced it" a property of the
    // conversion rather than of every caller remembering to clear it;
    // ConfigManager::loadFromFile refills both immediately afterwards.
    c.unreadSections.clear();
    c.unreadSectionsFrom.clear();

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
    //
    // The "i < 4" half of the bound is a guard in its own right, and it is the
    // only one in this function that passes over part of a document WITHOUT
    // leaving a carry behind: AppConfig::disks is four units, so a fifth entry
    // is stepped over here and to_json writes four entries back, and the fifth
    // is gone at the next save. It is not silent - collectExcessDiskProblems()
    // reports every element from index 4 on as an UnknownMember, which is
    // exactly the promise ("ignored now, dropped at the next save") that the
    // behaviour keeps. Carrying "/disks/4" instead was rejected: a splice at
    // that pointer appends to the four-element array to_json writes, so the
    // entry would come back in every saved file forever while remaining a unit
    // the application can never mount, and the report could only say so once.
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

// Where the walk below has got to, in the two notations the answer needs.
//
// `path` is the dotted-and-subscripted notation a person reads back into their
// editor, and is what Diagnostic::path carries. `pointer` is the RFC 6901 JSON
// pointer to_json splices a carried section back in at. They are stepped
// together in one struct because two prefixes advanced in two places is the
// first thing that would drift, and a carried section spliced at the wrong
// pointer would write the user's text over a setting the app really did read.
//
// Nothing is escaped into `pointer`, and nothing needs to be. A pointer only
// reaches AppConfig::unreadSections after the walk has descended, and the walk
// descends only into an array element or into a member the REFERENCE has - a
// member it does not know is reported and stepped over - so every component of
// a pointer that is ever USED is an index or one of to_json's own names, and no
// name to_json writes contains the '/' or '~' a pointer would have to escape.
// One is built for an unknown member too, a line before that member turns out
// to be unknown; it is thrown away unused.
struct WalkPos {
    std::string path;
    std::string pointer;
};

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
                                  const WalkPos& at, Diagnostics& out,
                                  UnreadSections* unread) {
    if ((actual.is_structured() || reference.is_structured()) &&
        actual.type() != reference.type()) {
        Diagnostic d;
        d.problem = Problem::TypeMismatch;
        d.path = at.path;
        d.detail = std::string("expected ") + reference.type_name() +
                   ", found " + actual.type_name();
        // Set from the same condition the carry below is taken on, so the two
        // are one decision written once. inspectDocument() clears it again for
        // the whole list if the document turns out to be unreadable, which is
        // the other half of the answer and is not knowable here.
        d.carried = !at.pointer.empty();
        out.push_back(d);

        // The text of the section, for AppConfig::unreadSections. Taken here
        // because this is the one place that has the node and its pointer at the
        // same time, and taken for every TypeMismatch rather than for a list of
        // paths written out a second time.
        //
        // The ROOT is not a section. An empty pointer addresses the whole
        // document, so carrying it would have to_json replace everything the
        // application knows with whatever the user's file happened to be. It
        // cannot arise in a document that gets as far as being kept - a root of
        // the wrong type cannot be turned into an AppConfig either, and
        // loadFromFile discards the carry along with the rest of an unreadable
        // file - but the cost of being wrong about that is the entire
        // configuration, so it is refused here rather than reasoned about there.
        //
        // The return below is also what keeps two carried sections from ever
        // overlapping: a mismatch stops the walk at that node, so nothing
        // underneath it is reported, and nothing underneath it is carried.
        if (unread && !at.pointer.empty()) (*unread)[at.pointer] = actual.dump();
        return;
    }

    if (freeFormPaths().count(at.path)) return;

    if (actual.is_object()) {
        for (auto it = actual.begin(); it != actual.end(); ++it) {
            WalkPos next{ joinPath(at.path, it.key()),
                          at.pointer + "/" + it.key() };
            auto known = reference.find(it.key());
            if (known == reference.end()) {
                Diagnostic d;
                d.problem = Problem::UnknownMember;
                d.path = next.path;
                // Not "near": windows.h still defines that as an empty macro
                // for the 16-bit memory models, so the declaration vanishes.
                std::string nearest = nearestName(reference, it.key());
                if (!nearest.empty()) d.detail = "did you mean \"" + nearest + "\"?";
                out.push_back(d);
                continue;
            }
            collectMemberProblems(it.value(), *known, next, out, unread);
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
            WalkPos next{ at.path + "[" + std::to_string(i) + "]",
                          at.pointer + "/" + std::to_string(i) };
            collectMemberProblems(actual[i], reference[0], next, out, unread);
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

// Disk entries past the last unit the loader reads.
//
// collectMemberProblems() cannot see this one. It checks every element of an
// array against the reference's FIRST element, which is right for the shape of
// an entry and says nothing about how many entries there may be; a fifth disk
// is a perfectly well-formed DiskConfig, so the walk finds nothing wrong with
// it and from_json's "i < 4" bound then steps over it in silence. That is the
// same failure the rest of this file exists to end - absorbed without a word,
// deleted at the next save, because to_json always writes exactly four - so it
// is reported here rather than left to be discovered by comparing the file
// before and after a save.
//
// UnknownMember rather than a sixth Problem enumerator. The kind is defined by
// what happens next, not by which line of the loader did it, and "ignored now,
// and the next save will not write it back" is already exactly this. A new
// enumerator would also have to be handled in MainWindow's
// reportConfigDiagnostics(), which maps every kind to its own on-screen notice,
// for a case that shares its outcome with one already there.
//
// A null element is skipped, on the same convention collectMemberProblems()
// uses: to_json writes null for an empty unit, so "disks": [null x 5] loses
// nothing when the fifth is dropped and there is nothing to warn about.
static void collectExcessDiskProblems(const json& doc, Diagnostics& out) {
    // A "disks" that is not an array has no elements to count and is already
    // reported as a TypeMismatch by collectMemberProblems().
    if (!doc.contains("disks") || !doc["disks"].is_array()) return;

    const auto& disks = doc["disks"];
    for (size_t i = 4; i < disks.size(); i++) {
        if (disks[i].is_null()) continue;
        Diagnostic d;
        d.problem = Problem::UnknownMember;
        d.path = "disks[" + std::to_string(i) + "]";
        d.detail = "there are only four disk units (0-3), so this entry is "
                   "never read and the next save drops it";
        out.push_back(d);
    }
}

// Everything wrong with one document, whether or not it came from a file.
//
// `unread`, when it is supplied, comes back holding the text of every section
// reported as a TypeMismatch, keyed by the pointer to_json has to splice it back
// in at. It is CLEARED first, so what comes back is about THIS document and not
// about the last one the map was handed to. ConfigManager::loadFromFile, the
// only caller in the program that supplies it, passes a fresh local map and
// would not notice either way; the clear is what lets the signature mean what it
// says, and tests/test_config.cpp asks the question the manager never does.
//
// The trial get<AppConfig>() is what turns a member from_json REQUIRES rather
// than defaults into a diagnostic. There is exactly one such member in the
// schema - DiskConfig's j.at("path") - and it throws for the whole document, so
// "disks": [ { "isManifest": true } ] loses every other setting in the file
// too. That is worth a sentence to the user rather than a silent fall back to
// defaults. loadFromFile then parses a second time; the cost is one pass over a
// file of a few hundred bytes, and the alternative is that this function cannot
// be asked about a document with no file behind it, which is how the tests ask.
Diagnostics inspectDocument(const json& doc, UnreadSections* unread = nullptr) {
    Diagnostics out;
    if (unread) unread->clear();
    collectMemberProblems(doc, referenceDocument(), WalkPos{}, out, unread);
    collectKeyNameProblems(doc, out);
    collectExcessDiskProblems(doc, out);

    bool readable = true;
    try {
        (void)doc.get<AppConfig>();
    } catch (const std::exception& e) {
        Diagnostic d;
        d.problem = Problem::UnreadableFile;
        d.detail = e.what();   // names the member and the reason; do not discard it
        out.push_back(d);
        readable = false;
    }

    // Whether each skipped section is actually being carried, decided here
    // because this is where both halves of the answer are known at once, and
    // decided by the SAME test ConfigManager::loadFromFile makes - it keeps the
    // carry only when nothing in this list is an UnreadableFile. Working it out
    // in two places is how they would come to disagree, and the disagreement
    // this fixes was exactly that: renderBlock() promised the carry for every
    // TypeMismatch while the loader granted it only to the readable ones, so a
    // document that was both wrongly typed and unreadable was quarantined with
    // its carry discarded and the user was told their text was safe.
    //
    // collectMemberProblems() has already set carried on the entries that took
    // a carry: a mismatch at the ROOT takes none (an empty pointer addresses
    // the whole document), and clearing it here as well would not distinguish
    // that case. `unread` being null changes nothing - a caller that does not
    // want the text still gets the truth about what a loader would do with it,
    // which is what renderBlock() is asked in the tests.
    if (!readable) {
        for (auto& d : out) {
            if (d.problem == Problem::TypeMismatch) d.carried = false;
        }
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
    // of from_json's is_array()/is_object() guards skipped a whole section:
    // "keyboard": { "keys": [ ... ] } is the case that matters, where a save
    // used to replace every hand-written binding with the built-in ones in the
    // same launch that failed to read them. Dropping an UnknownMember at the
    // next save is deliberately NOT in this list and stays as it was - nothing
    // was read out of that member because there is nothing in it to read, and
    // renderBlock() says out loud that it is about to go.
    //
    // Only the save that load() itself performs is suppressed. Every other call
    // to save() still writes - MainWindow's saveSettings(), the window
    // placement recorded when the app closes, the first-run welcome flag. That
    // is where this rule stops, and for an UnreadableFile it is enough on its
    // own, because the file has been renamed out from under those saves.
    //
    // For a TypeMismatch this rule was never the answer, and it is no longer
    // asked to be: the file parses, so nothing is quarantined, and those later
    // saves land on it. What makes them safe is AppConfig::unreadSections, which
    // carries the skipped section's own text through to the splice at the end of
    // to_json(json&, const AppConfig&), so a save writes that section BACK
    // rather than over it. The rule here and the carry answer two different
    // halves: this one covers the file, the carry covers the section.
    //
    // The carry covers those saves because they write the file it came from.
    // Every one of them is save(), which writes getConfigPath(), and the
    // document that raised the TypeMismatch here IS getConfigPath(). A save of
    // some other file - saveAsProfile() - neither writes the section back nor
    // over it: saveToFile() drops a carry that does not belong to the file it
    // is writing, and the profile it creates never held that text in the first
    // place.
    //
    // The suppression stays anyway, and the reason is not caution. This is the
    // one save nobody asked for, and whenever it is suppressed it is the fill
    // loop above that asked for it: needSave is set in exactly two places, and
    // the other one is the branch where there was no config file at all, which
    // raises no diagnostic and leaves `ok` true. So materialising the default
    // key bindings is the whole of what is given up here. A launch that has just
    // reported that it could not read part of the file has the weakest claim of
    // any caller to rewriting the whole of it for that. The cost is that those
    // bindings are not written out on this launch; the launch after the user
    // corrects the file does it.
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
            UnreadSections unread;
            found = inspectDocument(j, &unread);
            bool readable = true;
            for (const auto& d : found) {
                if (d.problem == Problem::UnreadableFile) { readable = false; break; }
            }
            if (readable) {
                m_config = j.get<AppConfig>();
                // AFTER the conversion, and that order is the whole of why a
                // section the user corrects by hand is picked up on the next
                // load. The line above replaces the WHOLE AppConfig, this one
                // included, so a carry from the previous file is gone before
                // this line refills it from the document just read; put the two
                // the other way round and the correction would be read into
                // m_config and then written back out as the old array for the
                // rest of the session. from_json cannot do it itself - it is
                // handed the document and knows nothing of the walk that found
                // the mismatch - which is why it is a second statement at all.
                //
                // Every pointer in here is a section from_json SKIPPED rather
                // than read, for any document that gets this far. from_json
                // passes over part of a document in FIVE places. Four are type
                // guards, and they are the four that produce a carry:
                // j["disks"].is_array(), hw.contains("dazzler") on a "hardware"
                // that is not an object, hw["dazzler"].is_array(), and
                // KeyboardConfig's j["keys"].is_object(). The fifth is the
                // "i < 4 && i < disks.size()" bound on the disks loop, which is
                // a count and not a type: a fifth disk entry is well-formed, so
                // no TypeMismatch is raised for it and nothing is carried - it
                // is reported as an UnknownMember by collectExcessDiskProblems()
                // and dropped at the next save, which is what that kind
                // promises. Every OTHER wrongly-typed section reaches a
                // j.value() or a get<T>() that throws, which is an
                // UnreadableFile and never reaches this line. Measured across
                // the whole schema, and asserted rather than assumed:
                // tests/test_config.cpp's "carried sections are the ones the
                // loader skipped" is that list and its second half is the
                // shapes that are quarantined instead, with the fifth guard in
                // "a fifth disk entry" beside it.
                m_config.unreadSections = std::move(unread);
                // The file the carry may be written back to, and the only one.
                // Kept beside the text rather than in a member of the manager
                // because it is a property of the carry: whoever holds one has
                // to be able to say where it came from. Cleared with it, so an
                // empty map never leaves a stale path behind for a later carry
                // to inherit.
                m_config.unreadSectionsFrom =
                    m_config.unreadSections.empty() ? std::string() : path;
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

        // A CARRIED SECTION GOES BACK TO THE FILE IT CAME FROM AND NOWHERE
        // ELSE.
        //
        // AppConfig::unreadSections is text the application could not parse,
        // kept so that a save writes that section back rather than over it.
        // That is only ever true of ONE file. This function is the only place
        // that knows which file is being written, and m_config moves between
        // files freely: save() always writes getConfigPath() and saveAsProfile()
        // always writes a profile path, so with no test here a profile whose
        // "disks" was an object had that object written into z80cpmw.json by the
        // next save, and a z80cpmw.json whose "keyboard.keys" was an array had
        // the array written into every profile saved afterwards. Both
        // directions measured, not argued: replace the comparison below with
        // "if (false)" and tests/test_config.cpp's "a carry belongs to one
        // file" fails five checks, and the two files it writes then carry each
        // other's unreadable text.
        //
        // The carry is DROPPED for any other file rather than carried into it.
        // A file the text never came out of has nothing of the user's in that
        // section to protect - saveAsProfile() creates the profile it is asked
        // for - so the reason for the carry is absent, while the cost of it is
        // not: the section would be unreadable in the new file too, would be
        // reported against it on every load, and correcting the original would
        // never clear the copy. What the new file gets instead is what the
        // application actually holds for that section, which is the built-in
        // defaults, and that is a file that loads cleanly. Refusing the save
        // outright was rejected: it would make "save a profile" fail for a
        // reason the user cannot act on from the dialog they are in.
        //
        // A copy, not a const_cast or a mutable member: this function is const
        // and saving must not change the configuration in force. The struct is
        // a handful of strings and one small map, copied once per save, against
        // a file write.
        AppConfig forThisFile = m_config;
        if (forThisFile.unreadSectionsFrom != path) {
            forThisFile.unreadSections.clear();
            forThisFile.unreadSectionsFrom.clear();
        }

        // Write to temp file first, then rename (atomic write)
        std::string tempPath = path + ".tmp";
        {
            std::ofstream file(tempPath);
            if (!file.is_open()) return false;

            json j = forThisFile;
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

    // Held rather than cleared. The clear was already off the top of this
    // function so that asking for a profile that does not exist leaves the
    // report about the config actually in force standing; the same reasoning
    // runs one step further, because a profile that will not READ changes no
    // setting either - loadFromFile leaves m_config alone when it fails - and
    // clearing here took down the report about z80cpmw.json, whose settings are
    // still the ones the machine is running on.
    //
    // loadFromFile appends, so it is handed an empty list and its findings come
    // back on their own, ready to be kept or discarded on its answer.
    Diagnostics inForce;
    inForce.swap(m_diagnostics);

    if (loadFromFile(path)) {
        // The profile is the configuration now, complaints and all, and
        // whatever was wrong with the file it replaced is no longer about
        // anything in force. m_diagnostics is already exactly the profile's.
        m_currentProfile = name;
        return true;
    }

    // Failed. The report about the configuration still in force goes back, and
    // this attempt's findings go behind it rather than being dropped: the
    // profile has just been quarantined by loadFromFile, and that diagnostic
    // holds the parser's line and column and the name the file was renamed to.
    // Nothing else in the program knows either. MainWindow's onLoadProfile
    // tells the two apart the way it always has, by matching Diagnostic::path
    // against getProfilePath() - only an UnreadableFile carries a file path.
    //
    // The entries this attempt replaces are the ones naming the same file.
    // Retrying is only possible when the quarantine FAILED, since a renamed
    // profile drops out of listProfiles(); when it is possible, a user who
    // clicks Load Profile twice because nothing appeared to happen should get
    // one report of one broken file rather than two identical ones.
    Diagnostics attempt;
    attempt.swap(m_diagnostics);
    for (size_t i = inForce.size(); i-- > 0; ) {
        if (inForce[i].path == path) inForce.erase(inForce.begin() + i);
    }
    m_diagnostics = std::move(inForce);
    m_diagnostics.insert(m_diagnostics.end(), attempt.begin(), attempt.end());
    return false;
}

// Writes the settings in force into a profile of this name, creating it.
//
// It does NOT write a carried section into a profile the carry did not come out
// of, and the decision is made in saveToFile() by comparing the path against
// AppConfig::unreadSectionsFrom - see the note there. Stated again here because
// this is the caller it is most surprising for: the settings being saved may be
// the ones from a file that has a section nobody could read, and the NEW profile
// written from them will not have it. That is deliberate. The profile is a file
// the user has just asked to be created; there is nothing of theirs in it yet to
// be written over, so the whole reason for the carry is missing, while its cost
// - a section that is unreadable, reported on every load of the profile, and not
// cleared by fixing the original - would follow the copy around. The new profile
// gets what the application actually holds for that section, and loads cleanly.
//
// Saving over the profile the carry DID come out of is the other case and is
// not this one: that is the file the text belongs to, so it is written back
// exactly as ConfigManager::save() writes it back to z80cpmw.json.
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
