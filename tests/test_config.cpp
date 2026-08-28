/*
 * test_config.cpp - Configuration diagnostics suite.
 *
 * todo.txt: "a mistyped config key is silently absorbed, and then silently
 * deleted at the next save." This suite is what holds the detection of that
 * honest, and it has three jobs.
 *
 * The first is the drift canary. Config.cpp's referenceDocument() is built by
 * running an AppConfig through to_json, so the list of names the walker treats
 * as known is produced by the writer rather than typed out a second time; this
 * suite feeds that document straight back through inspectDocument and requires
 * ZERO diagnostics. Add a setting to to_json and forget from_json, or the other
 * way round, and one of the three canaries below fails by name.
 *
 * The second is the detection itself - a member at every depth, arrays
 * included; a member whose name is right and whose value is the wrong kind of
 * thing; and the one place where a name must NOT be reported as a setting, the
 * user's own key names under keyboard.keys (which are classified as key names
 * instead, so a typo there is still not silent).
 *
 * The third is every path in the loader that could destroy the user's work, and
 * it is the reason most of the sections at the end exist. A z80cpmw.json that
 * cannot be read used to leave m_config default, which made the fill-in-defaults
 * loop set needSave, which made save() write defaults over the file; the rename
 * to z80cpmw.json.bad was the only thing in the way, and a rename can fail. The
 * rule those sections hold to is that load() DOES NOT SAVE over a file it could
 * not read - not one it failed to open, not one that will not parse, and not one
 * whose section it had to skip on a type guard - and every one of them ends by
 * comparing the bytes on disk with what the user wrote.
 *
 * It needs no window and builds no emulator: EmulatorEngine::getUserDataDirectory
 * is stubbed below to point at %TEMP%, and nothing else of the engine is linked.
 * The sibling checkouts are on the include path only because Config.cpp includes
 * EmulatorEngine.h, which includes hbios_cpu.h, which includes qkz80.h.
 *
 * Build and run: tests\run_tests.bat
 */

#include "Config.h"
#include "include/nlohmann/json.hpp"
#include "EmulatorEngine.h"
#include "Keymap.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace config;

// Declared here rather than in Config.h. Config.h is included by MainWindow.h,
// so anything it includes is pulled into most of the GUI, and these two take a
// nlohmann::json; that is the whole reason ConfigReport.h exists and is
// json-free. Config.cpp leaves both with external linkage for this suite.
namespace config {
json referenceDocument();
Diagnostics inspectDocument(const json& doc);

// And the serialisers, for the same reason: nlohmann finds to_json/from_json by
// argument-dependent lookup, and both live in Config.cpp where the schema is.
// Only AppConfig's pair is needed here - the nested types are converted inside
// Config.cpp, which can see its own definitions.
void to_json(json& j, const AppConfig& c);
void from_json(const json& j, AppConfig& c);
}

//=============================================================================
// Harness
//=============================================================================

static int g_checks = 0;
static int g_failed = 0;
static const char* g_section = "";

static void section(const char* name) {
    g_section = name;
    printf("\n-- %s\n", name);
}

static void check(bool ok, const std::string& what,
                  const std::string& got, const std::string& want) {
    g_checks++;
    if (ok) return;
    g_failed++;
    printf("  FAIL [%s] %s\n        got:  %s\n        want: %s\n",
           g_section, what.c_str(), got.c_str(), want.c_str());
}

static void checkTrue(bool ok, const std::string& what) {
    check(ok, what, ok ? "true" : "false", "true");
}

static void checkStr(const std::string& got, const std::string& want,
                     const std::string& what) {
    check(got == want, what, "\"" + got + "\"", "\"" + want + "\"");
}

static void checkInt(long got, long want, const std::string& what) {
    check(got == want, what, std::to_string(got), std::to_string(want));
}

static const char* problemName(Problem p) {
    switch (p) {
    case Problem::UnknownMember:  return "UnknownMember";
    case Problem::TypeMismatch:   return "TypeMismatch";
    case Problem::ReservedKey:    return "ReservedKey";
    case Problem::UnknownKeyName: return "UnknownKeyName";
    case Problem::UnreadableFile: return "UnreadableFile";
    }
    return "?";
}

// Everything a failure message needs to say what actually came back.
static std::string describe(const Diagnostics& d) {
    if (d.empty()) return "(none)";
    std::string s;
    for (const auto& e : d) {
        if (!s.empty()) s += "; ";
        s += problemName(e.problem);
        s += "@";
        s += e.path.empty() ? "(document)" : e.path;
    }
    return s;
}

static size_t countOf(const Diagnostics& d, Problem p) {
    size_t n = 0;
    for (const auto& e : d) if (e.problem == p) n++;
    return n;
}

// The path of the one diagnostic of this kind, when it is also the only
// diagnostic at all. Anything else comes back as a parenthesised description,
// so "exactly one X at Y" is a single string comparison whose failure message
// says what the document really produced.
static std::string soleOf(const Diagnostics& d, Problem p) {
    if (countOf(d, p) != d.size() || d.size() != 1) return "(" + describe(d) + ")";
    return d[0].path;
}

static std::string detailOf(const Diagnostics& d) {
    return d.empty() ? std::string("(none)") : d[0].detail;
}

// The detail of the first diagnostic of a given kind, for the documents that
// produce more than one problem at a time - a wrongly-typed section usually
// also costs the trial conversion, so d[0] is not reliably the one under test.
// A miss comes back as the whole description, so the failure says what the
// document really produced rather than "".
static std::string detailOfKind(const Diagnostics& d, Problem p) {
    for (const auto& e : d) if (e.problem == p) return e.detail;
    return "(" + describe(d) + ")";
}

// The path of the first diagnostic of a given kind, on the same terms.
static std::string pathOfKind(const Diagnostics& d, Problem p) {
    for (const auto& e : d) if (e.problem == p) return e.path;
    return "(" + describe(d) + ")";
}

static Diagnostics inspect(const char* text) {
    return inspectDocument(json::parse(text));
}

//=============================================================================
// The engine stub. Config.cpp asks for one thing and this is it.
//=============================================================================

static std::string g_dir;

std::string EmulatorEngine::getUserDataDirectory() {
    return g_dir;
}

static std::string configPath() { return g_dir + "\\z80cpmw.json"; }

static void writeFile(const std::string& path, const std::string& body) {
    std::ofstream f(path, std::ios::binary);
    f << body;
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

static void resetDir() {
    std::error_code ec;
    fs::remove_all(g_dir, ec);
    fs::create_directories(g_dir, ec);
}

//=============================================================================
// The reference document, and the three drift canaries
//=============================================================================

// A canary that fires on an empty cage is no canary. referenceDocument() is
// only useful if it really does name the members at every depth, so check that
// before trusting the fact that nothing was reported against it.
static void test_reference_document() {
    section("reference document");

    json ref = referenceDocument();

    checkTrue(ref.contains("display") && ref["display"].contains("fontSize"),
              "the reference names display.fontSize");
    checkTrue(ref.contains("display") && ref["display"].contains("bell"),
              "the reference names display.bell");
    checkTrue(ref.contains("keyboard") && ref["keyboard"].contains("keys"),
              "the reference names keyboard.keys");
    checkTrue(ref.contains("window") && ref["window"].contains("monBottom"),
              "the reference names window.monBottom");
    checkTrue(ref.contains("disks") && ref["disks"].is_array() &&
              ref["disks"].size() == 4 && ref["disks"][0].contains("path"),
              "the reference fills all four disk slots and names disks[].path");
    checkTrue(ref.contains("hardware") && ref["hardware"]["dazzler"].is_array() &&
              !ref["hardware"]["dazzler"].empty() &&
              ref["hardware"]["dazzler"][0].contains("port"),
              "the reference carries a Dazzler and names its port");
}

static void test_drift_canary() {
    section("drift canary");

    Diagnostics d = inspectDocument(referenceDocument());
    check(d.empty(), "to_json's own output produces no diagnostics",
          describe(d), "(none)");
}

// Every j.value(name, default) in from_json is a literal that has to agree with
// the initialiser on the matching AppConfig member. Present-but-empty sections
// are what exercises them: from_json gates each section on j.contains(), so
// OMITTING a section skips the block entirely and leaves the struct's own
// defaults in place, which would make this comparison pass no matter what the
// literals said.
static void test_default_literals() {
    section("default literals");

    json empties = {
        {"core", json::object()},
        {"display", json::object()},
        {"keyboard", json::object()},
        {"window", json::object()},
        {"hardware", {{"dazzler", json::array({json::object()})}}},
        // DiskConfig is the one shape in the schema that cannot be empty -
        // from_json uses j.at("path") - so its required member is supplied and
        // nothing else is.
        {"disks", json::array({json{{"path", ""}}, nullptr, nullptr, nullptr})}
    };

    AppConfig expect{};
    expect.disks[0] = DiskConfig{};
    expect.dazzlers.assign(1, DazzlerConfig{});

    json got = json(empties.get<AppConfig>());
    check(got == json(expect),
          "an empty section parses to the struct's own defaults",
          got.dump(), json(expect).dump());
}

// Change every scalar in to_json's output to a different value of the same
// type, and require the loader to hand all of them back.
static void mutateLeaves(json& j) {
    if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) mutateLeaves(it.value());
    } else if (j.is_array()) {
        for (auto& e : j) mutateLeaves(e);
    } else if (j.is_boolean()) {
        j = !j.get<bool>();
    } else if (j.is_number_unsigned()) {
        // Unsigned first: is_number_integer() is true for both, and turning an
        // unsigned into a signed would change the type as well as the value and
        // fail the comparison for the wrong reason. +7 keeps every field this
        // touches in range - scrollbackLines is clamped to 100000, and the
        // Dazzler port is a uint8_t.
        j = j.get<uint64_t>() + 7;
    } else if (j.is_number_integer()) {
        j = j.get<int64_t>() + 7;
    } else if (j.is_string()) {
        j = j.get<std::string>() + "X";
    }
}

static std::string firstDifference(const json& a, const json& b,
                                   const std::string& prefix) {
    std::string here = prefix.empty() ? std::string("(root)") : prefix;
    if (a.type() != b.type()) {
        return here + " (wrote " + a.dump() + ", read back " + b.dump() + ")";
    }
    if (a.is_object()) {
        for (auto it = a.begin(); it != a.end(); ++it) {
            std::string path = prefix.empty() ? it.key() : prefix + "." + it.key();
            auto other = b.find(it.key());
            if (other == b.end()) return path + " (gone after the round trip)";
            std::string d = firstDifference(it.value(), *other, path);
            if (!d.empty()) return d;
        }
        for (auto it = b.begin(); it != b.end(); ++it) {
            if (a.find(it.key()) == a.end()) {
                return (prefix.empty() ? it.key() : prefix + "." + it.key()) +
                       " (appeared from nowhere)";
            }
        }
        return std::string();
    }
    if (a.is_array()) {
        if (a.size() != b.size()) {
            return here + " (" + std::to_string(a.size()) + " entries in, " +
                   std::to_string(b.size()) + " out)";
        }
        for (size_t i = 0; i < a.size(); i++) {
            std::string d = firstDifference(a[i], b[i],
                                            prefix + "[" + std::to_string(i) + "]");
            if (!d.empty()) return d;
        }
        return std::string();
    }
    if (a == b) return std::string();
    return here + " (wrote " + a.dump() + ", read back " + b.dump() + ")";
}

// The half of the drift question inspectDocument cannot answer. Comparing a
// document against a reference built from that same document can never find a
// name the reference does not have, so a member to_json writes and no from_json
// reads is invisible to the walker - it just reverts to its default on the way
// through. Marking every value and looking for one that came back unmarked
// finds exactly that, and names the path where it happened.
static void test_written_but_never_read() {
    section("every written member is read back");

    json marked = referenceDocument();
    mutateLeaves(marked);
    json back = json(marked.get<AppConfig>());

    checkStr(firstDifference(marked, back, std::string()), std::string(),
             "a marked document survives to_json -> from_json -> to_json");
}

//=============================================================================
// Unrecognised members
//=============================================================================

static void test_unknown_members() {
    section("unrecognised members");

    checkStr(soleOf(inspect(R"({"display":{"fontSize":20,"fontsize":20}})"),
                    Problem::UnknownMember),
             "display.fontsize",
             "a case-typo beside the real name is reported once");

    checkStr(detailOf(inspect(R"({"display":{"fontsize":20}})")),
             "did you mean \"fontSize\"?",
             "and the nearest spelling is offered");

    checkTrue(detailOf(inspect(R"({"display":{"wibble":20}})")).empty(),
              "a name with no near neighbour is reported without a guess");

    checkStr(soleOf(inspect(R"({"nonsense":true})"), Problem::UnknownMember),
             "nonsense",
             "an unrecognised member at the top level");

    checkStr(soleOf(inspect(R"({"core":{"rom":"a.rom","romm":"b.rom"}})"),
                    Problem::UnknownMember),
             "core.romm",
             "an unrecognised member inside a section");

    checkStr(soleOf(inspect(R"({"keyboard":{"f5tocpm":true}})"),
                    Problem::UnknownMember),
             "keyboard.f5tocpm",
             "an unrecognised member inside the keyboard section");

    checkStr(soleOf(inspect(R"({"disks":[null,{"path":"a.img","pth":"b.img"}]})"),
                    Problem::UnknownMember),
             "disks[1].pth",
             "an unrecognised member inside an array element");

    checkStr(soleOf(inspect(R"({"hardware":{"dazzler":[{"port":14},{"prt":15}]}})"),
                    Problem::UnknownMember),
             "hardware.dazzler[1].prt",
             "an unrecognised member inside a nested array element");

    checkStr(soleOf(inspect(R"({"window":{"monLeft":0,"monleft":0}})"),
                    Problem::UnknownMember),
             "window.monleft",
             "an unrecognised member inside the window section");

    // A null disk slot has no members, and must not be mistaken for one that
    // has all of them missing.
    checkInt((long)inspect(R"({"disks":[null,null,null,null]})").size(), 0,
             "empty disk slots report nothing");

    Diagnostics many = inspect(R"({"aaa":1,"core":{"zzz":2}})");
    checkInt((long)countOf(many, Problem::UnknownMember), 2,
             "two unrecognised members produce two diagnostics");
}

//=============================================================================
// keyboard.keys - the user's own names, and the ones the app answers first
//=============================================================================

static void test_free_form_key_names() {
    section("keyboard.keys is free-form");

    // "Zorg" is not a key name Keymap.h can resolve, and "fontSize" is a
    // setting name that has wandered into the wrong object. Neither is an
    // unrecognised MEMBER: everything under keyboard.keys is a key name by
    // definition, and reporting them as settings would be nonsense.
    Diagnostics d = inspect(
        R"({"keyboard":{"keys":{"Up":"\\E[A","Zorg":"x","fontSize":"y","Ctrl+Left":"^A"}}})");
    checkInt((long)countOf(d, Problem::UnknownMember), 0,
             "no member under keyboard.keys is ever unrecognised");

    // They ARE reported, as the other thing they are: names that resolve to no
    // key. Both of them, and only them - "Up" and "Ctrl+Left" are real.
    checkInt((long)countOf(d, Problem::UnknownKeyName), 2,
             "but a name that resolves to no key is reported as one");
    checkInt((long)d.size(), 2, "and nothing else is reported for them");

    // The guard is on the path, not on the name "keys" wherever it appears.
    checkStr(soleOf(inspect(R"({"keys":{"Up":"x"}})"), Problem::UnknownMember),
             "keys",
             "a \"keys\" object at the top level is still an unrecognised member");
}

// The typo class the four-row table deleted from Config.cpp could not see. A
// name vkForName rejects binds nothing: KeyMap::build drops it, load()'s fill
// loop steps over it, and the line stays in z80cpmw.json looking like a working
// binding forever. classifyName() is what can tell it apart from a real one.
static void test_unknown_key_names() {
    section("key names that resolve to nothing");

    checkStr(soleOf(inspect(R"({"keyboard":{"keys":{"F13":"^A"}}})"),
                    Problem::UnknownKeyName),
             "keyboard.keys.F13",
             "F13 is out of the F1-F12 range and is reported");

    checkStr(soleOf(inspect(R"({"keyboard":{"keys":{"PgeUp":"^A"}}})"),
                    Problem::UnknownKeyName),
             "keyboard.keys.PgeUp",
             "a misspelled key name is reported");

    // vkForName rejects a trailing non-digit after "F" rather than letting atoi
    // read what it likes, which is the only reason this is visible at all.
    checkStr(soleOf(inspect(R"({"keyboard":{"keys":{"F1x":"^A"}}})"),
                    Problem::UnknownKeyName),
             "keyboard.keys.F1x",
             "and so is a name atoi would have accepted");

    checkStr(soleOf(inspect(R"({"keyboard":{"keys":{"Ctrl_Left":"^A"}}})"),
                    Problem::UnknownKeyName),
             "keyboard.keys.Ctrl_Left",
             "a modifier joined with the wrong character resolves to nothing");

    checkTrue(detailOf(inspect(R"({"keyboard":{"keys":{"F13":"^A"}}})"))
                  .find("CONFIGURATION.md") != std::string::npos,
              "and the detail points at where the names are listed");

    // Every name in the built-in defaults must classify as Ok, or the loader
    // would report its own bindings the first time it wrote them into the file.
    json defaults = json::object();
    for (const auto& kv : keymap::defaultBindings()) defaults[kv.first] = kv.second;
    Diagnostics d = inspectDocument(json{{"keyboard", {{"keys", defaults}}}});
    check(d.empty(), "the built-in bindings report nothing against themselves",
          describe(d), "(none)");
}

static void test_reserved_keys() {
    section("reserved key bindings");

    checkStr(soleOf(inspect(R"({"keyboard":{"keys":{"Ctrl+End":"^A"}}})"),
                    Problem::ReservedKey),
             "keyboard.keys.Ctrl+End",
             "a binding for a combination the terminal answers itself");

    checkTrue(detailOf(inspect(R"({"keyboard":{"keys":{"Ctrl+End":"^A"}}})"))
                  .find("return to the live screen") != std::string::npos,
              "and the diagnostic says what the key is used for");

    // Resolved through keyIdForName, so a different spelling of the same
    // binding is the same binding - the reason KeyMap::build resolves names
    // before merging them.
    checkStr(soleOf(inspect(R"({"keyboard":{"keys":{"control+end":"^A"}}})"),
                    Problem::ReservedKey),
             "keyboard.keys.control+end",
             "an alias spelling of a reserved combination is caught too");

    checkStr(soleOf(inspect(R"({"keyboard":{"keys":{"Shift+PgUp":"^A"}}})"),
                    Problem::ReservedKey),
             "keyboard.keys.Shift+PgUp",
             "and so is an alias of the key itself");

    // "At least these modifiers", matching the view, which reads shift and ctrl
    // independently - Ctrl+Shift+PageUp scrolls back as well, so a sequence
    // bound to it would be just as dead.
    checkStr(soleOf(inspect(R"({"keyboard":{"keys":{"Ctrl+Shift+PageUp":"^A"}}})"),
                    Problem::ReservedKey),
             "keyboard.keys.Ctrl+Shift+PageUp",
             "an extra modifier does not release a reserved combination");

    // The unmodified keys are the guest's, and the default bindings written
    // into every z80cpmw.json include all four of them.
    checkInt((long)inspect(
                 R"({"keyboard":{"keys":{"PageUp":"\\E[5~","Home":"\\E[H","End":"\\E[F","Ctrl+Left":"^A"}}})")
                 .size(),
             0, "the unmodified keys are not reserved");
}

//=============================================================================
// Members with the right name and the wrong kind of value
//=============================================================================

// The three type guards in from_json - j["disks"].is_array(),
// hw["dazzler"].is_array() and KeyboardConfig's j["keys"].is_object() - skip
// the section instead of throwing, so before TypeMismatch existed a wrong type
// in any of them loaded clean, reported nothing, and was replaced by the
// built-in defaults at the save later in the same launch.
static void test_type_mismatch() {
    section("wrongly-typed members");

    checkStr(pathOfKind(inspect(R"({"keyboard":{"keys":["Up","\\E[A"]}})"),
                        Problem::TypeMismatch),
             "keyboard.keys",
             "keyboard.keys as an array is reported");

    checkStr(detailOfKind(inspect(R"({"keyboard":{"keys":["Up","\\E[A"]}})"),
                          Problem::TypeMismatch),
             "expected object, found array",
             "and the detail names both types");

    // freeFormPaths() exempts keyboard.keys from the MEMBER walk only. The
    // exemption used to be tested first, which let an array there out of the
    // inspection altogether - the one free-form path in the schema was also the
    // one place a wrong type could not be seen.
    checkInt((long)countOf(inspect(R"({"keyboard":{"keys":["Up","\\E[A"]}})"),
                           Problem::TypeMismatch),
             1, "the free-form exemption does not cover the type");

    checkStr(pathOfKind(inspect(R"({"disks":{"0":{"path":"a.img"}}})"),
                        Problem::TypeMismatch),
             "disks",
             "disks as an object is reported");

    checkStr(pathOfKind(inspect(R"({"hardware":{"dazzler":{"port":14}}})"),
                        Problem::TypeMismatch),
             "hardware.dazzler",
             "hardware.dazzler as an object is reported");

    checkStr(pathOfKind(inspect(R"({"disks":["a.img",null,null,null]})"),
                        Problem::TypeMismatch),
             "disks[0]",
             "a disk slot holding a bare string is reported by index");

    checkStr(pathOfKind(inspect(R"({"window":42})"), Problem::TypeMismatch),
             "window",
             "a whole section replaced by a scalar is reported");

    // A null slot is an empty disk unit, not a type mistake: to_json writes
    // null for a unit with no image and from_json reads it back as nullopt.
    checkInt((long)inspect(R"({"disks":[null,null,null,null]})").size(), 0,
             "empty disk slots are still not a type mismatch");

    // The false positive the structured-only rule exists to avoid. Everything
    // in the reference that came from an int is number_integer; parse the same
    // document back out of text and every non-negative literal is
    // number_unsigned instead. Comparing json::type() without this restriction
    // would report a mismatch on an ordinary saved window position.
    Diagnostics roundTrip = inspectDocument(json::parse(referenceDocument().dump()));
    check(roundTrip.empty(),
          "a reference document that has been through the parser is still clean",
          describe(roundTrip), "(none)");

    // And the same with a negative number in it, which is number_integer on
    // both sides - the other half of the pair.
    checkInt((long)inspect(R"({"window":{"x":-8,"y":-8,"width":800}})").size(), 0,
             "negative and positive window coordinates are both fine");

    // A wrongly-typed SCALAR is deliberately not reported here. j.value() on it
    // throws, and inspectDocument's trial conversion turns that into an
    // UnreadableFile carrying the library's own words; reporting it twice would
    // put two diagnostics on one mistake.
    Diagnostics scalar = inspect(R"({"display":{"fontSize":"big"}})");
    checkInt((long)countOf(scalar, Problem::TypeMismatch), 0,
             "a wrongly-typed scalar is left to the trial conversion");
    checkInt((long)countOf(scalar, Problem::UnreadableFile), 1,
             "which does report it");
}

//=============================================================================
// Documents that cannot be turned into an AppConfig at all
//=============================================================================

static void test_unreadable_documents() {
    section("unreadable documents");

    Diagnostics d = inspect(R"({"disks":[{"isManifest":true}]})");
    checkStr(soleOf(d, Problem::UnreadableFile), "",
             "a disk entry with no path costs the whole document");
    checkTrue(!detailOf(d).empty(),
              "and the reason is carried rather than discarded");

    checkInt((long)inspect(R"({"disks":[{"path":"a.img","isManifest":true}]})").size(),
             0, "a disk entry with a path is fine");
}

//=============================================================================
// Loading a real file, and what happens to one that will not parse
//=============================================================================

static void test_load_reports() {
    section("loading a file");

    ConfigManager& cm = ConfigManager::instance();

    resetDir();
    writeFile(configPath(), R"({"display":{"fontSize":18}})");
    checkTrue(cm.load(), "a good file loads");
    checkInt(cm.get().fontSize, 18, "and its value is the one in force");
    check(cm.diagnostics().empty(), "and reports nothing",
          describe(cm.diagnostics()), "(none)");
    checkTrue(!fs::exists(configPath() + ".bad"), "and is not moved aside");

    resetDir();
    writeFile(configPath(), R"({"display":{"fontSize":18,"fontsize":9}})");
    checkTrue(cm.load(), "a file with an unrecognised member still loads");
    checkStr(soleOf(cm.diagnostics(), Problem::UnknownMember), "display.fontsize",
             "and the member is reported");
    // The other half of the todo item, demonstrated rather than asserted in
    // prose: the entry really is gone from the file that load() just wrote.
    checkTrue(readFile(configPath()).find("fontsize") == std::string::npos,
              "and the save that follows really does delete it");
}

// The manager is a singleton and every section that loads a file leaves a full
// set of key bindings in it. That matters to everything below: it is the
// fill-in-missing-bindings loop in load() that sets needSave, so leaving them
// in place would suppress the very save these sections exist to prove does not
// happen. Put the manager back to what a fresh process starts with.
static void freshManager(ConfigManager& cm) {
    cm.get() = AppConfig{};
}

static void test_unreadable_file_is_kept() {
    section("an unreadable file is kept");

    ConfigManager& cm = ConfigManager::instance();
    const std::string truncated = R"({"display": {"fontSize": 18,)";

    freshManager(cm);
    resetDir();
    writeFile(configPath(), truncated);
    checkTrue(!cm.load(), "a truncated file does not load");

    const std::string bad = configPath() + ".bad";
    checkTrue(fs::exists(bad), "the original is kept as z80cpmw.json.bad");
    checkStr(readFile(bad), truncated, "byte for byte");

    checkStr(soleOf(cm.diagnostics(), Problem::UnreadableFile), configPath(),
             "the diagnostic names the file");
    checkStr(cm.diagnostics().empty() ? std::string() : cm.diagnostics()[0].backup,
             bad, "and names where it was kept");
    checkTrue(detailOf(cm.diagnostics()).find("parse error") != std::string::npos,
              "and carries the parser's own message");

    // The rule, stated where it can be checked. load() used to fill in the
    // default key bindings, set needSave and save() straight onto the file it
    // had just failed to read; the rename was the only thing in the way of
    // that, and it was one failed rename away from being nothing. Nothing is
    // saved automatically after a file we could not read, so there is no new
    // file here at all - the next explicit save writes one.
    checkTrue(!fs::exists(configPath()),
              "and load() writes nothing in its place");
    checkStr(readFile(bad), truncated, "so the kept copy is untouched");
}

// The same guarantee, with the rename taken away. This is finding 5: the
// quarantine can fail, and when it does the original is the only copy there is.
// Holding the file open with no sharing mode is how it is provoked - it fails
// the open AND the rename in one step, so the two halves are checked together.
static void test_file_that_cannot_be_opened() {
    section("a file that cannot be opened");

    ConfigManager& cm = ConfigManager::instance();
    const std::string mine = R"({"display":{"fontSize":31}})";

    freshManager(cm);
    resetDir();
    writeFile(configPath(), mine);

    // dwShareMode 0: nothing else may read this file while the handle is open.
    HANDLE h = CreateFileA(configPath().c_str(), GENERIC_READ, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    checkTrue(h != INVALID_HANDLE_VALUE, "the file can be locked for the test");
    if (h == INVALID_HANDLE_VALUE) return;

    bool ok = cm.load();
    CloseHandle(h);

    checkTrue(!ok, "a file that will not open does not load");

    // Finding 7: this case used to return false out of loadFromFile before any
    // of the diagnostic machinery ran, so the one problem the enum is named
    // after was the one problem with nothing to say about it.
    checkStr(soleOf(cm.diagnostics(), Problem::UnreadableFile), configPath(),
             "it is reported, and the diagnostic names the file");
    checkTrue(detailOf(cm.diagnostics()).find("could not be opened") !=
                  std::string::npos,
              "and says the open was what failed, not the parse");
    checkTrue(detailOf(cm.diagnostics()).find("denied") != std::string::npos,
              "and carries the system's reason for it");

    // The rename could not work either - the handle is still open - so the
    // backup is empty, which renders as nothing at all. The detail has to say
    // where the file still is or nobody is told.
    checkStr(cm.diagnostics().empty() ? std::string("(none)")
                                      : cm.diagnostics()[0].backup,
             std::string(), "the quarantine could not run");
    checkTrue(detailOf(cm.diagnostics()).find("still there under its own name") !=
                  std::string::npos,
              "so the diagnostic says the original is still where it was");

    // And the whole point: it really is.
    checkStr(readFile(configPath()), mine,
             "and the original bytes are on disk, unchanged");

    // That last check passes on Windows even without the no-save rule, because
    // saveToFile's rename cannot replace a locked file either - so it is not on
    // its own evidence that load() declined to save. This is: saveToFile writes
    // z80cpmw.json.tmp first and does NOT clean it up when the rename fails, so
    // a .tmp sitting here is the fingerprint of a save that was attempted and
    // merely thwarted. Nothing but the original in this directory is the
    // fingerprint of a save that was never started.
    checkTrue(!fs::exists(configPath() + ".tmp"),
              "and no save was attempted at all");
}

// A second failure must not eat the first backup. The old fixed name argued
// that the newest copy is the one worth keeping; on a config broken twice
// months apart, the newest copy is a near-defaults file and the one it replaced
// was the user's real work.
static void test_backup_does_not_clobber() {
    section("a second quarantine keeps the first");

    ConfigManager& cm = ConfigManager::instance();
    const std::string first = R"({"the-users-real-config": )";
    const std::string second = R"({"a-later-mistake": )";

    freshManager(cm);
    resetDir();
    writeFile(configPath(), first);
    cm.load();
    checkStr(readFile(configPath() + ".bad"), first, "the first goes to .bad");

    freshManager(cm);
    writeFile(configPath(), second);
    cm.load();
    checkStr(readFile(configPath() + ".bad"), first, "and stays there");
    checkStr(readFile(configPath() + ".bad2"), second, "the second goes to .bad2");
    checkStr(cm.diagnostics().empty() ? std::string() : cm.diagnostics()[0].backup,
             configPath() + ".bad2", "and the diagnostic names the new one");

    freshManager(cm);
    writeFile(configPath(), R"({"a third: )");
    cm.load();
    checkStr(readFile(configPath() + ".bad"), first, "a third leaves .bad alone");
    checkTrue(fs::exists(configPath() + ".bad3"), "and lands on .bad3");
}

// At the cap the file is left where it is rather than overwriting the last
// backup. Nothing in the quarantine may destroy a copy of the user's work, and
// with the no-save rule in load() leaving it in place is safe.
static void test_backup_cap() {
    section("the backup cap");

    ConfigManager& cm = ConfigManager::instance();
    const std::string mine = R"({"still-mine": )";

    freshManager(cm);
    resetDir();
    for (int n = 1; n <= 20; n++) {
        writeFile(configPath() + ".bad" +
                      (n == 1 ? std::string() : std::to_string(n)),
                  "occupied");
    }
    writeFile(configPath(), mine);
    cm.load();

    checkStr(cm.diagnostics().empty() ? std::string("(none)")
                                      : cm.diagnostics()[0].backup,
             std::string(), "no backup is claimed");
    checkTrue(detailOf(cm.diagnostics()).find("all taken") != std::string::npos,
              "the detail says why");
    checkStr(readFile(configPath()), mine, "the file is left where it is");
    checkStr(readFile(configPath() + ".bad20"), "occupied",
             "and the last backup is not overwritten");
}

// Finding 4, end to end. This is the one that cost a whole keymap: the section
// loads clean, reports nothing, and is rewritten with the built-in defaults by
// the save in the same call to load().
static void test_type_mismatch_suppresses_save() {
    section("a wrongly-typed section is not saved over");

    ConfigManager& cm = ConfigManager::instance();
    const std::string mine = R"({"keyboard": {"keys": ["Up", "\\E[A"]}})";

    freshManager(cm);
    resetDir();
    writeFile(configPath(), mine);

    checkTrue(cm.load(), "the document is readable, so load() succeeds");
    checkStr(pathOfKind(cm.diagnostics(), Problem::TypeMismatch), "keyboard.keys",
             "but the section is reported rather than passed over");
    checkStr(readFile(configPath()), mine,
             "and the file is byte for byte what the user wrote");
    checkTrue(!fs::exists(configPath() + ".bad"),
              "nothing is quarantined - the file parses, it is just wrong");

    // The fill loop really did want to save; the suppression is what stopped
    // it. Without this the check above would pass for the wrong reason.
    checkTrue(cm.get().keyboard.keys.size() > 0,
              "the default bindings were filled in memory, which is what "
              "sets needSave");
}

//=============================================================================
// The block of text a terminal can print
//=============================================================================

// The block is wrapped to 78 columns, so a sentence to look for can be split
// across two lines with a hanging indent in between. Join it back up before
// searching for wording, or a check on the wording silently becomes a check on
// where the wrap happened to land.
static std::string flatten(const std::string& block) {
    std::string out;
    for (size_t i = 0; i < block.size(); i++) {
        if (block.compare(i, 2, "\r\n") == 0) { out += ' '; i++; continue; }
        out += block[i];
    }
    std::string tight;
    for (size_t i = 0; i < out.size(); i++) {
        if (out[i] == ' ' && !tight.empty() && tight.back() == ' ') continue;
        tight += out[i];
    }
    return tight;
}

static void test_render_block() {
    section("report text");

    checkTrue(renderBlock(Diagnostics{}).empty(),
              "nothing to report renders as nothing");

    std::string block = flatten(renderBlock(inspect(R"({"display":{"fontsize":9}})")));
    checkTrue(block.find("display.fontsize") != std::string::npos,
              "the block names the path");
    checkTrue(block.find("fontSize") != std::string::npos,
              "the block offers the nearest spelling");
    checkTrue(block.find("saving settings will drop them") != std::string::npos,
              "the block says the entry is about to be lost");

    // The sentence has to be true PER KIND, which is what one anyIgnored flag
    // could not be. A reserved binding is ignored and KEPT: from_json reads
    // keyboard.keys whole, to_json writes it back whole, and load()'s fill loop
    // only adds names, so nothing prunes it. Telling the user it is about to be
    // deleted was a plain falsehood, and it contradicted the note on
    // KeyMap::build in Keymap.h in the same breath.
    std::string reserved = flatten(
        renderBlock(inspect(R"({"keyboard":{"keys":{"Ctrl+End":"^A"}}})")));
    checkTrue(reserved.find("will drop them") == std::string::npos,
              "a reserved binding is not said to be about to be dropped");
    checkTrue(reserved.find("they are kept") != std::string::npos,
              "it is said to be kept instead");

    std::string unknownKey = flatten(
        renderBlock(inspect(R"({"keyboard":{"keys":{"F13":"^A"}}})")));
    checkTrue(unknownKey.find("will drop them") == std::string::npos,
              "and neither is a key name that resolves to nothing");
    checkTrue(unknownKey.find("they are kept") != std::string::npos,
              "which is kept for the same reason");

    // Both kinds in one document: each gets its own sentence, and neither
    // sentence is applied to the other's entry.
    std::string both = flatten(renderBlock(inspect(
        R"({"display":{"fontsize":9},"keyboard":{"keys":{"Ctrl+End":"^A"}}})")));
    checkTrue(both.find("saving settings will drop them") != std::string::npos &&
                  both.find("they are kept") != std::string::npos,
              "a mixed report carries both sentences");

    std::string mismatch = flatten(
        renderBlock(inspect(R"({"keyboard":{"keys":["Up","\\E[A"]}})")));
    checkTrue(mismatch.find("not saved automatically") != std::string::npos,
              "a wrongly-typed section says the file is not saved over");

    Diagnostic e;
    e.problem = Problem::UnreadableFile;
    e.path = "C:\\Users\\x\\z80cpmw.json";
    e.detail = "parse error at line 4";
    e.backup = "C:\\Users\\x\\z80cpmw.json.bad";
    Diagnostics one{ e };
    std::string quarantined = flatten(renderBlock(one));
    checkTrue(quarantined.find("z80cpmw.json.bad") != std::string::npos,
              "an unreadable file's block names the backup");
    checkTrue(quarantined.find("will drop them") == std::string::npos,
              "and does not claim a setting is about to be dropped");

    // The terminal is 80 columns and wraps mid-word, so a path broken across a
    // wrap is a path the reader cannot copy.
    std::string wide = renderBlock(inspect(
        R"({"core":{"aVeryLongMemberNameThatNobodyWouldEverType":1},"display":{"fontsize":2}})"));
    size_t pos = 0;
    int longLines = 0;
    while (pos < wide.size()) {
        size_t nl = wide.find("\r\n", pos);
        if (nl == std::string::npos) nl = wide.size();
        if (nl - pos > 78) longLines++;
        pos = nl + 2;
    }
    checkInt(longLines, 0, "no rendered line is wider than 78 columns");
}

//=============================================================================
// display.bell (the storage half; nothing consumes it yet)
//=============================================================================

static void test_bell() {
    section("display.bell");

    AppConfig fresh{};
    checkTrue(fresh.bellEnabled, "the bell is on by default");
    checkTrue(json(fresh)["display"].contains("bell"),
              "to_json writes display.bell");
    checkTrue(!json::parse(R"({"display":{"bell":false}})").get<AppConfig>().bellEnabled,
              "from_json reads display.bell");
    checkTrue(json::parse(R"({"display":{}})").get<AppConfig>().bellEnabled,
              "an absent display.bell leaves the bell on");
    checkInt((long)inspect(R"({"display":{"bell":false}})").size(), 0,
             "and display.bell is a recognised member");
}

//=============================================================================

int main() {
    const char* tmp = std::getenv("TEMP");
    if (!tmp || !*tmp) tmp = std::getenv("TMP");
    if (!tmp || !*tmp) tmp = ".";
    g_dir = std::string(tmp) + "\\z80cpmw_test_config";

    printf("Configuration diagnostics suite\n");
    printf("===============================\n");
    printf("scratch directory: %s\n", g_dir.c_str());

    test_reference_document();
    test_drift_canary();
    test_default_literals();
    test_written_but_never_read();
    test_unknown_members();
    test_free_form_key_names();
    test_unknown_key_names();
    test_reserved_keys();
    test_type_mismatch();
    test_unreadable_documents();
    test_load_reports();
    test_unreadable_file_is_kept();
    test_file_that_cannot_be_opened();
    test_backup_does_not_clobber();
    test_backup_cap();
    test_type_mismatch_suppresses_save();
    test_render_block();
    test_bell();

    std::error_code ec;
    fs::remove_all(g_dir, ec);

    printf("\n===============================\n");
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
