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
 * That rule only ever covered the save load() makes itself, which left the last
 * hole: a file that PARSES is not quarantined, so the next save from anywhere
 * else - saveWindowPlacement() at WM_CLOSE, the welcome flag - wrote the
 * built-in defaults over the section from_json's type guards had skipped. The
 * sections around test_wrongly_typed_section_survives_a_save are the ones that
 * hold the answer to that honest: AppConfig::unreadSections keeps the text and
 * to_json splices it back, so a save writes such a section BACK rather than
 * over it, and correcting the file by hand is all it takes to be rid of it.
 *
 * A carry is text out of ONE file and the three sections that hold its limits
 * are the ones that came last. test_carry_belongs_to_one_file requires it to
 * reach that file and no other, in both directions, because the settings move
 * between files and the text must not follow them. test_render_block requires
 * the report to say what really happened to it, including for a document that
 * is wrongly typed AND unreadable, where there is no carry at all and the block
 * used to promise one. And test_fifth_disk is the case that is none of the
 * above: a guard that is a count rather than a type, reported and dropped and
 * never carried.
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
// The default argument is repeated here rather than in a header for the same
// reason the declaration is: there is no header both this file and Config.cpp
// can share that is allowed to name a nlohmann::json.
Diagnostics inspectDocument(const json& doc, UnreadSections* unread = nullptr);

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

// Whether EVERY diagnostic of a kind says its section is being carried. All of
// them rather than the first, because the answer is a property of the document
// - loadFromFile keeps the carry or discards it for the whole of one file - so
// a mixture would mean the two halves of the decision had come apart. A kind
// that is not present answers false, which is why every caller checks that the
// kind is there first.
static bool carriedFlagOfKind(const Diagnostics& d, Problem p) {
    bool any = false;
    for (const auto& e : d) {
        if (e.problem != p) continue;
        if (!e.carried) return false;
        any = true;
    }
    return any;
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
// Carrying a section the loader could not read through to the next save
//=============================================================================

// What inspectDocument hands back for AppConfig::unreadSections, asked directly.
// The manager-level sections below cannot reach the documents that never load,
// and those are half of the rule.
static UnreadSections carriedBy(const char* text) {
    UnreadSections u;
    (void)inspectDocument(json::parse(text), &u);
    return u;
}

static std::string carriedAt(const UnreadSections& u, const std::string& pointer) {
    auto it = u.find(pointer);
    if (it != u.end()) return it->second;
    std::string keys;
    for (const auto& kv : u) { if (!keys.empty()) keys += ", "; keys += kv.first; }
    return "(carried: " + (keys.empty() ? std::string("nothing") : keys) + ")";
}

static void test_carried_text() {
    section("the text of a section that was not read");

    checkStr(carriedAt(carriedBy(R"({"keyboard":{"keys":["Up","\\E[A"]}})"),
                       "/keyboard/keys"),
             R"(["Up","\\E[A"])",
             "a wrongly-typed section is carried, as the section itself");

    checkInt((long)carriedBy(R"({"display":{"fontSize":18}})").size(), 0,
             "a document with nothing wrong carries nothing");

    checkInt((long)carriedBy(R"({"display":{"fontsize":18}})").size(), 0,
             "and neither does an unrecognised member, which has nothing to keep");

    // The pointer, not the dotted path: it is what to_json follows, and the two
    // notations are stepped together in WalkPos precisely so this cannot drift.
    checkStr(carriedAt(carriedBy(R"({"hardware":{"dazzler":{"port":14}}})"),
                       "/hardware/dazzler"),
             R"({"port":14})",
             "a nested section is keyed by its JSON pointer");

    checkStr(carriedAt(carriedBy(R"({"disks":["a.img",null,null,null]})"), "/disks/0"),
             R"("a.img")",
             "and an array element by its index, with no path parsing anywhere");

    // A mismatch stops the walk at that node, so a section and something inside
    // it can never both be carried - which is what keeps to_json's splices from
    // depending on the order they are applied in.
    UnreadSections nested = carriedBy(R"({"hardware":[{"dazzler":5}]})");
    checkInt((long)nested.size(), 1, "a mismatch carries one section, not two");
    checkStr(carriedAt(nested, "/hardware"), R"([{"dazzler":5}])",
             "and it is the outermost one");

    // The empty pointer addresses the WHOLE document. Carrying it would have
    // to_json replace everything the application knows with the user's file.
    checkInt((long)carriedBy(R"([1,2,3])").size(), 0,
             "a document whose root is an array carries nothing at all");
    checkInt((long)carriedBy(R"("hello")").size(), 0,
             "and neither does one that is a bare string");

    // Why the root refusal in collectMemberProblems() cannot be observed from
    // outside, and is belt beside braces rather than a live branch: a root that
    // is the wrong type cannot be turned into an AppConfig either - every
    // from_json in the schema starts with a j.value() or a contains() that
    // needs an object - so a root mismatch ALWAYS arrives with an
    // UnreadableFile beside it, and inspectDocument would clear the carried
    // flag for the whole list anyway. Both routes to the same false, pinned
    // here so that the pair is on the record as agreeing.
    for (const char* root : { "[1,2,3]", R"("hello")", "42", "true", "null" }) {
        Diagnostics r = inspect(root);
        checkInt((long)countOf(r, Problem::UnreadableFile), 1,
                 std::string("a root of the wrong type is unreadable: ") + root);
        checkTrue(!carriedFlagOfKind(r, Problem::TypeMismatch),
                  std::string("so nothing in it is said to be carried: ") + root);
    }

    // Cleared, not added to: what comes back describes the document just
    // inspected and not the one before it. ConfigManager::loadFromFile hands
    // over a fresh local map and would not notice either way, so this is the
    // only thing holding that half of the signature up.
    UnreadSections reused;
    (void)inspectDocument(json::parse(R"({"keyboard":{"keys":[]}})"), &reused);
    (void)inspectDocument(json::parse(R"({"keyboard":{"keys":{"F7":"^G"}}})"), &reused);
    checkInt((long)reused.size(), 0,
             "a second document empties the map the first one filled");
}

// The item this closes, end to end: the file is read, the section is skipped,
// and a save that knows nothing about any of it - saveWindowPlacement() at
// WM_CLOSE, the first-run welcome flag, an NVRAM change - writes the section
// back rather than over it.
static void test_wrongly_typed_section_survives_a_save() {
    section("a wrongly-typed section survives a later save");

    ConfigManager& cm = ConfigManager::instance();
    // The measured case: a keymap written as an array. The user's binding was
    // gone within twelve seconds of boot. f5ToCpm sits beside it as a setting
    // the loader really does read, so the checks below can tell the splice from
    // a copy of the whole file.
    const std::string mine =
        R"({"keyboard": {"f5ToCpm": true, "keys": ["Up", "\\E[A"]}})";

    freshManager(cm);
    resetDir();
    writeFile(configPath(), mine);
    checkTrue(cm.load(), "the document is readable, so load() succeeds");
    checkTrue(cm.get().keyboard.f5ToCpm,
              "the rest of the section was read normally");

    // Not the save load() declines to make - that one is test_type_mismatch_
    // suppresses_save above. This is the one that used to destroy the section.
    checkTrue(cm.save(), "an explicit save writes");

    json written = json::parse(readFile(configPath()));
    checkStr(written["keyboard"]["keys"].dump(),
             json::parse(mine)["keyboard"]["keys"].dump(),
             "and keyboard.keys in the file is the value the user wrote");
    checkTrue(written["keyboard"]["f5ToCpm"] == true,
              "while the member beside it is the one the application holds");

    // Nothing of the carry's own appears anywhere: it is spliced in at the path
    // it came from and written under no name of its own, so the saved file has
    // exactly the one problem the original had and no new ones.
    Diagnostics after = inspectDocument(written);
    checkStr(soleOf(after, Problem::TypeMismatch), "keyboard.keys",
             "the saved file has the same one problem and no other");

    // Idempotent: the file the second save writes is the file the first one
    // wrote, so nothing accumulates and nothing degrades launch by launch.
    const std::string once = readFile(configPath());
    freshManager(cm);
    checkTrue(cm.load(), "the file we just wrote loads again");
    checkTrue(cm.save(), "and saves again");
    checkStr(readFile(configPath()), once,
             "byte for byte the same file the second time round");
    checkStr(pathOfKind(cm.diagnostics(), Problem::TypeMismatch), "keyboard.keys",
             "and the reload reports the same section, still there to be fixed");

    // The cost, stated in renderBlock() and checked here: while the section is
    // carried, nothing the application computes for it reaches the file either.
    // A merge is the one thing that must not happen - the app has no idea what
    // the user meant by that text - so the disk mounted below is dropped.
    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"disks":{"0":{"path":"a.img"}}})");
    checkTrue(cm.load(), "a wrongly-typed disks section still loads");
    DiskConfig mounted;
    mounted.path = "mounted.img";
    cm.get().disks[0] = mounted;
    checkTrue(cm.save(), "and a save after mounting a disk writes");
    // Verbatim, and this is what verbatim costs and buys at once: the section
    // goes back with no "isManifest" beside the path, because to_json's DiskConfig
    // never ran on it. Nothing the application knows about a disk entry is added
    // to text the application could not read.
    checkStr(json::parse(readFile(configPath()))["disks"].dump(),
             R"({"0":{"path":"a.img"}})",
             "the carried section is what reaches the file, unembellished");
    checkTrue(readFile(configPath()).find("mounted.img") == std::string::npos,
              "and nothing the application computed is merged into it");
}

// Correcting the file by hand is the whole exit from the state above, so it has
// to work without the user doing anything else.
static void test_hand_fix_is_picked_up() {
    section("a hand-fixed section is read on the next load");

    ConfigManager& cm = ConfigManager::instance();

    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"keyboard":{"keys":["Up","\\E[A"]}})");
    checkTrue(cm.load(), "the wrongly-typed file loads");
    checkInt((long)cm.get().unreadSections.count("/keyboard/keys"), 1,
             "and the section is carried");

    // Deliberately NO freshManager() here. The carry from the load above is
    // still in m_config, and what has to get rid of it is the ORDER of the two
    // statements in loadFromFile: "m_config = j.get<AppConfig>()" replaces the
    // whole struct, carry included, and only then is the carry refilled from
    // this document. Storing it the other way round pins keyboard.keys to the
    // old array for the rest of the session and the correction looks ignored,
    // which is what a mutation swapping those two lines does to the checks
    // below.
    writeFile(configPath(), R"({"keyboard":{"keys":{"F7":"^G"}}})");
    checkTrue(cm.load(), "the corrected file loads");
    checkInt((long)countOf(cm.diagnostics(), Problem::TypeMismatch), 0,
             "with nothing left to report");
    checkInt((long)cm.get().unreadSections.size(), 0,
             "and nothing left carried");
    checkStr(cm.get().keyboard.keys["F7"], "^G",
             "the binding is in force");

    // And it reaches the file as an object, which is the check that fails if
    // the stale carry is still there: the splice would put the array back.
    checkTrue(cm.save(), "a save after the correction writes");
    json written = json::parse(readFile(configPath()));
    checkTrue(written["keyboard"]["keys"].is_object(),
              "keyboard.keys is an object in the saved file");
    checkStr(written["keyboard"]["keys"].value("F7", std::string()), "^G",
              "and still holds the corrected binding");
}

// Which sections are carried is not a list written out a second time; it is
// every TypeMismatch. That is only safe because a wrongly-typed section that
// from_json really DOES read makes the whole document unreadable, so it never
// reaches the assignment in loadFromFile. Both halves are checked here, because
// it is the second half that keeps the first one honest: carrying a section the
// loader had also read would put the user's text back over a setting the
// application really holds.
static void test_carry_matches_what_the_loader_skips() {
    section("carried sections are the ones the loader skipped");

    ConfigManager& cm = ConfigManager::instance();

    // The four TYPE guards, in the shapes that reach them:
    // j["disks"].is_array(), hw.contains("dazzler") on a "hardware" that is not
    // an object, hw["dazzler"].is_array(), and KeyboardConfig's
    // j["keys"].is_object(). They are four of the five places from_json passes
    // over part of a document, and the four that leave a carry; the fifth is
    // the "i < 4" bound on the disks loop, which raises no TypeMismatch and
    // carries nothing, and is test_fifth_disk below.
    static const struct { const char* text; const char* pointer; } skipped[] = {
        { R"({"keyboard":{"keys":["Up","x"]}})",     "/keyboard/keys"    },
        { R"({"keyboard":{"keys":5}})",              "/keyboard/keys"    },
        { R"({"disks":{"0":{"path":"a.img"}}})",     "/disks"            },
        { R"({"disks":"a.img"})",                    "/disks"            },
        { R"({"hardware":{"dazzler":{"port":14}}})", "/hardware/dazzler" },
        { R"({"hardware":"none"})",                  "/hardware"         },
    };
    for (const auto& c : skipped) {
        freshManager(cm);
        resetDir();
        writeFile(configPath(), c.text);
        checkTrue(cm.load(), std::string("readable: ") + c.text);
        checkInt((long)cm.get().unreadSections.size(), 1,
                 std::string("one section carried from ") + c.text);
        checkInt((long)cm.get().unreadSections.count(c.pointer), 1,
                 std::string("at ") + c.pointer);
        // And the report agrees with the loader about it, which is the thing
        // renderBlock() has to be able to trust: the sentence it prints for
        // this kind promises the carry, so the flag it prints it from must be
        // the same answer m_config just recorded.
        checkTrue(carriedFlagOfKind(cm.diagnostics(), Problem::TypeMismatch),
                  std::string("and the report says it is carried: ") + c.text);
    }

    // Every other wrongly-typed section reaches a j.value() or a get<T>() that
    // throws. Those are UnreadableFile, so the file is quarantined and
    // loadFromFile never assigns the carry at all.
    static const char* quarantined[] = {
        R"({"window":42})",
        R"({"core":[1,2]})",
        R"({"display":[1,2]})",
        R"({"keyboard":"x"})",
        R"({"version":[1]})",
        R"({"disks":["a.img",null,null,null]})",
        R"({"hardware":{"dazzler":["x"]}})",
        R"([1,2,3])",
    };
    for (const char* text : quarantined) {
        freshManager(cm);
        resetDir();
        writeFile(configPath(), text);
        checkTrue(!cm.load(), std::string("not readable: ") + text);
        checkInt((long)countOf(cm.diagnostics(), Problem::UnreadableFile), 1,
                 std::string("reported as unreadable rather than carried: ") + text);
        checkTrue(fs::exists(configPath() + ".bad"),
                  std::string("and moved aside instead: ") + text);
        checkInt((long)cm.get().unreadSections.size(), 0,
                 std::string("with nothing carried out of it: ") + text);
        // Several of these shapes ALSO raise a TypeMismatch on their way to the
        // exception. Those are the ones renderBlock() used to promise the carry
        // for, and the promise was false in exactly this branch, so the flag has
        // to say so wherever one appears.
        if (countOf(cm.diagnostics(), Problem::TypeMismatch) > 0) {
            checkTrue(!carriedFlagOfKind(cm.diagnostics(), Problem::TypeMismatch),
                      std::string("and the report does not claim one: ") + text);
        }
    }

    // A load that FAILS leaves the carry alone for the same reason it leaves
    // m_config alone: the settings in force are still the ones from the file
    // that did load, and the section that came with them is part of them.
    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"keyboard":{"keys":["Up","x"]}})");
    cm.load();
    writeFile(configPath(), R"({"display": {"fontSize": 9,)");
    checkTrue(!cm.load(), "a file that will not parse does not load");
    checkInt((long)cm.get().unreadSections.count("/keyboard/keys"), 1,
             "and does not take the carry in force down with it");
}

//=============================================================================
// The fifth disk: the guard that is a count rather than a type
//=============================================================================

// Defined with the rest of the report-text helpers at the bottom, and used from
// here as well: the sections below ask what the report SAYS about a document
// they have just loaded, and the answer has to be un-wrapped before it can be
// searched for wording.
static std::string flatten(const std::string& block);

// from_json passes over part of a document in five places, not the four type
// guards the section above enumerates. The fifth is the "i < 4 &&
// i < disks.size()" bound on the disks loop, and it behaves like none of the
// others: a fifth disk entry is a well-formed DiskConfig, so the walk finds
// nothing wrong with its shape, no TypeMismatch is raised and nothing is
// carried - and to_json writes exactly four entries, so the entry is gone at
// the next save. Silently, until collectExcessDiskProblems() existed.
//
// What this section pins is all three answers at once: it IS reported, it is
// NOT carried, and it IS dropped. Any two of those without the third is a
// broken promise - reported without being dropped would be a false alarm,
// dropped without being reported is the failure the whole suite exists for, and
// carried would put a disk unit that can never be mounted back into every file
// the app ever writes.
static void test_fifth_disk() {
    section("a fifth disk entry");

    const char* five =
        R"({"disks":[{"path":"a.img"},{"path":"b.img"},{"path":"c.img"},)"
        R"({"path":"d.img"},{"path":"e.img"}]})";

    Diagnostics d = inspect(five);
    checkStr(soleOf(d, Problem::UnknownMember), "disks[4]",
             "the entry past the last unit is reported, and is the only problem");
    checkTrue(detailOfKind(d, Problem::UnknownMember).find("four disk units") !=
                  std::string::npos,
              "and the reason says how many units there are");

    // Not a TypeMismatch: there is nothing wrong with the entry's shape, and
    // calling it one would carry it at /disks/4 for ever.
    checkInt((long)countOf(d, Problem::TypeMismatch), 0,
             "it is not called a wrongly-typed section");
    checkInt((long)carriedBy(five).size(), 0, "and nothing is carried");

    // Every entry past the fourth, not just the first of them.
    Diagnostics six = inspect(
        R"({"disks":[null,null,null,null,{"path":"e.img"},{"path":"f.img"}]})");
    checkInt((long)countOf(six, Problem::UnknownMember), 2,
             "a sixth entry is reported too");
    checkStr(six.size() > 1 ? six[1].path : describe(six), "disks[5]",
             "and named by its own index");

    // A null past the end is an empty unit. to_json writes null for one anyway,
    // so dropping it loses nothing and warning about it would be crying wolf.
    checkInt((long)inspect(R"({"disks":[null,null,null,null,null]})").size(), 0,
             "a null fifth slot holds nothing to lose and is not reported");

    // Four is still four.
    checkInt((long)inspect(
                 R"({"disks":[{"path":"a.img"},null,null,{"path":"d.img"}]})").size(),
             0, "four entries are fine");

    // A "disks" that is not an array is a TypeMismatch, and the count check has
    // to leave it alone rather than report a second problem on the same
    // section.
    checkInt((long)countOf(inspect(R"({"disks":{"0":{"path":"a.img"}}})"),
                           Problem::UnknownMember),
             0, "a wrongly-typed disks section is not also counted");

    // And the is_array() guard is load-bearing, not decorative. nlohmann's
    // size() answers for an object too, so a "disks" OBJECT with five members
    // gets past a guard that only tests contains() - and operator[](size_t) on
    // an object throws type_error.305, out of a function whose whole job is to
    // turn bad documents into sentences. Five members is what it takes to reach
    // the subscript, so five is what is written here. Caught rather than left to
    // escape, so a guard that stops working reads as a failed check with the
    // library's own words in it rather than as the whole suite aborting.
    Diagnostics fatObject;
    std::string threw;
    try {
        fatObject = inspect(R"({"disks":{"a":1,"b":2,"c":3,"d":4,"e":5}})");
    } catch (const std::exception& e) {
        threw = e.what();
    }
    checkStr(threw, "", "an object with five members is not subscripted");
    checkStr(pathOfKind(fatObject, Problem::TypeMismatch), "disks",
             "it is a mismatch, and only a mismatch");
    checkInt((long)countOf(fatObject, Problem::UnknownMember), 0,
             "with no index reported against it");

    // And the end-to-end half: what the loader does with such a file. There is
    // no TypeMismatch and no UnreadableFile, so load() does NOT suppress its own
    // save - which makes this the same demonstration the unrecognised-member
    // section makes, that the entry really is gone from the file afterwards.
    ConfigManager& cm = ConfigManager::instance();
    freshManager(cm);
    resetDir();
    writeFile(configPath(), five);
    checkTrue(cm.load(), "a five-disk file loads");
    checkStr(cm.get().disks[3] ? cm.get().disks[3]->path : std::string("(none)"),
             "d.img", "the four units the loader has are filled");
    checkStr(pathOfKind(cm.diagnostics(), Problem::UnknownMember), "disks[4]",
             "the fifth is reported against the file");
    checkInt((long)cm.get().unreadSections.size(), 0,
             "and is not carried");

    const std::string afterLoad = readFile(configPath());
    checkTrue(afterLoad.find("e.img") == std::string::npos,
              "and the save that follows really does drop it");
    checkInt((long)json::parse(afterLoad)["disks"].size(), 4,
             "leaving four entries in the file");
    std::string report = flatten(renderBlock(cm.diagnostics()));
    checkTrue(report.find("disks[4]") != std::string::npos &&
                  report.find("saving settings will drop them") !=
                      std::string::npos,
              "and the report names it and says that is what happens to it");
}

//=============================================================================
// Profiles
//=============================================================================

static void writeProfile(ConfigManager& cm, const std::string& name,
                         const std::string& body) {
    std::error_code ec;
    fs::create_directories(cm.getProfilesDir(), ec);
    writeFile(cm.getProfilePath(name), body);
}

// loadProfile() used to clear m_diagnostics before it could fail, which took
// down the report about z80cpmw.json - settings that are still in force, since
// a failed profile load changes nothing.
static void test_failed_profile_keeps_the_report() {
    section("a failed profile keeps the report in force");

    ConfigManager& cm = ConfigManager::instance();

    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"display":{"fontSize":18,"fontsize":9}})");
    checkTrue(cm.load(), "the main config loads with a complaint");
    checkStr(soleOf(cm.diagnostics(), Problem::UnknownMember), "display.fontsize",
             "which is what the report says");

    writeProfile(cm, "broken", R"({"display": {"fontSize": 9,)");
    checkTrue(!cm.loadProfile("broken"), "a profile that will not parse fails");
    checkInt(cm.get().fontSize, 18, "and changes no setting");
    checkStr(pathOfKind(cm.diagnostics(), Problem::UnknownMember),
             "display.fontsize",
             "so the report about the settings in force is still standing");

    // And this attempt is behind it rather than dropped: nothing else in the
    // program knows the parser's line and column or where the file went, and
    // MainWindow's onLoadProfile decides whether to print anything at all by
    // looking for a diagnostic whose path is this profile's.
    checkStr(pathOfKind(cm.diagnostics(), Problem::UnreadableFile),
             cm.getProfilePath("broken"),
             "with the failed profile named behind it");
    checkInt((long)cm.diagnostics().size(), 2,
             "the report is those two things and nothing else");
    checkTrue(fs::exists(cm.getProfilePath("broken") + ".bad"),
              "the profile was quarantined like any other unreadable file");

    // A name with no file at all returns before any of that, which is what the
    // clear was moved off the top of the function for in the first place.
    size_t before = cm.diagnostics().size();
    checkTrue(!cm.loadProfile("no-such-profile"), "a missing profile fails");
    checkInt((long)cm.diagnostics().size(), (long)before,
             "and says nothing at all");

    // A profile that LOADS replaces the report outright: the previous file's
    // complaints are not about anything in force any more.
    writeProfile(cm, "good", R"({"display":{"fontSize":26}})");
    checkTrue(cm.loadProfile("good"), "a good profile loads");
    checkInt(cm.get().fontSize, 26, "and its settings are in force");
    check(cm.diagnostics().empty(), "and the report is the profile's alone",
          describe(cm.diagnostics()), "(none)");
}

// Retrying is only possible when the quarantine could not run - a renamed
// profile has dropped out of listProfiles(). Holding the file open with no
// sharing mode fails the open and the rename in one step, which is exactly that
// state, and is also how a user ends up clicking Load Profile twice.
static void test_retrying_a_profile_does_not_double_the_report() {
    section("retrying an unreadable profile reports it once");

    ConfigManager& cm = ConfigManager::instance();

    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"display":{"fontSize":18,"fontsize":9}})");
    cm.load();
    writeProfile(cm, "locked", R"({"display":{"fontSize":9}})");

    const std::string path = cm.getProfilePath("locked");
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    checkTrue(h != INVALID_HANDLE_VALUE, "the profile can be locked for the test");
    if (h == INVALID_HANDLE_VALUE) return;

    checkTrue(!cm.loadProfile("locked"), "the first attempt fails");
    checkTrue(!cm.loadProfile("locked"), "and so does the second");
    CloseHandle(h);

    checkInt((long)countOf(cm.diagnostics(), Problem::UnreadableFile), 1,
             "the profile is named once, not twice");
    checkInt((long)cm.diagnostics().size(), 2,
             "so two attempts leave two entries, not three");
    checkStr(pathOfKind(cm.diagnostics(), Problem::UnknownMember),
             "display.fontsize",
             "and the report in force is still there behind it");
}

// A carried section is text out of ONE file, and it goes back into that file
// and no other. Without AppConfig::unreadSectionsFrom the carry followed the
// settings instead, and the settings move between files: save() always writes
// z80cpmw.json and saveAsProfile() always writes a profile, so a section the
// application could not read in one file was written into another where the
// user had never typed it, could not be cleared by correcting the original, and
// was reported against the innocent file on every load of it.
//
// Both directions are checked, because the mechanism is one comparison and a
// mistake in it leaks one way or the other rather than failing outright.
static void test_carry_belongs_to_one_file() {
    section("a carry belongs to one file");

    ConfigManager& cm = ConfigManager::instance();

    // Direction one: a profile's carry must not reach z80cpmw.json.
    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"display":{"fontSize":18}})");
    checkTrue(cm.load(), "the main config loads cleanly");
    checkInt((long)cm.get().unreadSections.size(), 0, "carrying nothing");

    writeProfile(cm, "wrong", R"({"keyboard":{"keys":["Up","\\E[A"]}})");
    checkTrue(cm.loadProfile("wrong"), "a profile with a wrongly-typed section loads");
    checkInt((long)cm.get().unreadSections.count("/keyboard/keys"), 1,
             "and its section is carried");
    checkStr(cm.get().unreadSectionsFrom, cm.getProfilePath("wrong"),
             "attached to the profile it was read out of");

    checkTrue(cm.save(), "a save of the main config writes");
    json main1 = json::parse(readFile(configPath()));
    checkTrue(main1["keyboard"]["keys"].is_object(),
              "and z80cpmw.json's keyboard.keys is an object, not the profile's array");
    checkTrue(readFile(configPath()).find("\\\\E[A") == std::string::npos,
              "the profile's text is nowhere in it");
    check(inspectDocument(main1).empty(),
          "so the file it wrote loads cleanly",
          describe(inspectDocument(main1)), "(none)");

    // The profile itself is untouched by any of that: the carry exists to
    // protect the file it came from, and that file is still the one it protects.
    checkTrue(cm.saveAsProfile("wrong"), "saving the profile back writes");
    checkStr(json::parse(readFile(cm.getProfilePath("wrong")))["keyboard"]["keys"].dump(),
             R"(["Up","\\E[A"])",
             "and the profile still holds what the user typed");

    // Direction two: the main config's carry must not reach a profile.
    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"keyboard":{"keys":["Up","\\E[A"]}})");
    checkTrue(cm.load(), "a wrongly-typed main config loads");
    checkStr(cm.get().unreadSectionsFrom, configPath(),
             "the carry is attached to z80cpmw.json");

    checkTrue(cm.saveAsProfile("copy"), "saving it as a profile writes");
    json prof = json::parse(readFile(cm.getProfilePath("copy")));
    checkTrue(prof["keyboard"]["keys"].is_object(),
              "and the new profile's keyboard.keys is an object");
    check(inspectDocument(prof).empty(),
          "the profile the user asked for loads cleanly",
          describe(inspectDocument(prof)), "(none)");

    // Dropping it there is the decision, not an accident of the copy: the
    // profile is a file the user has just asked to be created, so there is
    // nothing of theirs in that section to be written over, while a copy of the
    // unreadable text would be reported against the profile for ever and would
    // outlive the fix to the original. The carry itself is untouched by the
    // saveAsProfile - it still belongs to z80cpmw.json - which is what the next
    // three checks are for.
    checkInt((long)cm.get().unreadSections.count("/keyboard/keys"), 1,
             "saving a profile does not take the carry away");
    checkStr(cm.get().unreadSectionsFrom, configPath(),
             "nor move it to the file that was just written");
    checkTrue(cm.save(), "and a save of the main config writes");
    checkStr(json::parse(readFile(configPath()))["keyboard"]["keys"].dump(),
             R"(["Up","\\E[A"])",
             "putting the user's text back where they typed it");

    // A carry cannot outlive the load that produced it. The wholesale
    // replacement in loadFromFile is one half; from_json clearing both members
    // is the other, and it is the half that covers get_to() into a LIVE
    // AppConfig, where the previous file's text would otherwise survive still
    // pointed at the previous file.
    AppConfig live;
    live.unreadSections["/disks"] = "{}";
    live.unreadSectionsFrom = "somewhere else";
    json::parse(R"({"display":{"fontSize":18}})").get_to(live);
    checkInt((long)live.unreadSections.size(), 0,
             "reading a document into a live config clears the carry it had");
    checkStr(live.unreadSectionsFrom, "",
             "and the file that carry belonged to");

    // A profile that loads cleanly takes the carry down with it, source and
    // all, so nothing is left attached to a file nobody is running on.
    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"keyboard":{"keys":["Up","\\E[A"]}})");
    cm.load();
    writeProfile(cm, "clean", R"({"display":{"fontSize":26}})");
    checkTrue(cm.loadProfile("clean"), "a clean profile loads");
    checkInt((long)cm.get().unreadSections.size(), 0, "and carries nothing");
    checkStr(cm.get().unreadSectionsFrom, "", "attached to nothing");
    checkTrue(cm.save(), "a save after it writes");
    checkTrue(json::parse(readFile(configPath()))["keyboard"]["keys"].is_object(),
              "with no trace of the section the previous file could not read");

    // A profile that FAILS changes no setting, so the carry in force stays -
    // and stays attached to the file it came from, which is still the file the
    // machine is running on.
    freshManager(cm);
    resetDir();
    writeFile(configPath(), R"({"keyboard":{"keys":["Up","\\E[A"]}})");
    cm.load();
    writeProfile(cm, "broken", R"({"display": {"fontSize": 9,)");
    checkTrue(!cm.loadProfile("broken"), "a profile that will not parse fails");
    checkInt((long)cm.get().unreadSections.count("/keyboard/keys"), 1,
             "the carry in force is untouched");
    checkStr(cm.get().unreadSectionsFrom, configPath(),
             "and still names the file it came from");
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

    // A wrongly-typed section used to share the unreadable file's sentence,
    // which promised only that no AUTOMATIC save would follow. That was true
    // and it was not the question the user was asking: the next explicit save
    // wrote the built-in defaults over the section anyway. Now that
    // AppConfig::unreadSections carries the section through every save, the
    // sentence this kind gets is its own, and it says both halves: the user's
    // text is safe, and nothing else gets into that section either.
    std::string mismatch = flatten(
        renderBlock(inspect(R"({"keyboard":{"keys":["Up","\\E[A"]}})")));
    checkTrue(mismatch.find("writes such a section back rather than over it") !=
                  std::string::npos,
              "a wrongly-typed section is said to be written back, not over");
    checkTrue(mismatch.find("nothing the application changes in that section "
                            "is saved either") != std::string::npos,
              "and what that costs is said in the same breath");
    checkTrue(mismatch.find("not saved automatically") == std::string::npos,
              "and the unreadable file's promise is not applied to it");
    checkTrue(mismatch.find("not kept") == std::string::npos,
              "a section that really is carried is not marked as not kept");

    // The promise the carry makes is conditional, and this is the condition.
    // A document that is BOTH wrongly typed and unreadable never reaches the
    // assignment to AppConfig::unreadSections in loadFromFile: the conversion
    // throws, the file is quarantined, nothing of it is in force, and the next
    // save writes the built-in defaults for that section. This block used to
    // print "what you typed is safe" for it anyway - the promise was emitted
    // for ANY TypeMismatch, while only a readable document ever got one - which
    // told the user the exact opposite of what had just happened to their file.
    std::string unreadPair = flatten(renderBlock(inspect(
        R"({"keyboard":{"keys":[]},"window":42})")));
    checkTrue(unreadPair.find("writes such a section back rather than over it") ==
                  std::string::npos,
              "a wrongly-typed section in an unreadable file is not called safe");
    checkTrue(unreadPair.find("not carried through a save") != std::string::npos,
              "it is said not to be carried");
    checkTrue(unreadPair.find("writes the built-in defaults for that section") !=
                  std::string::npos,
              "and what the next save does to it is said");
    checkTrue(unreadPair.find("not saved automatically") != std::string::npos,
              "while the unreadable file keeps its own sentence");
    checkTrue(unreadPair.find("not kept - the file it is in could not be read") !=
                  std::string::npos,
              "and the line itself is marked, not only the sentence");

    // Which sections are carried is decided per diagnostic, and it has to be:
    // MainWindow's reportConfigDiagnostics() splits the list BY KIND and calls
    // renderBlock() once per kind, so the TypeMismatch block it renders never
    // holds the UnreadableFile that decided the answer. Anything worked out
    // from "does this list also contain an UnreadableFile" would pass the check
    // above and do nothing at all in the app.
    Diagnostics justMismatches;
    for (const auto& d : inspect(R"({"keyboard":{"keys":[]},"window":42})")) {
        if (d.problem == Problem::TypeMismatch) justMismatches.push_back(d);
    }
    checkInt((long)justMismatches.size(), 2, "the document has two mismatches");
    std::string split = flatten(renderBlock(justMismatches));
    checkTrue(split.find("not carried through a save") != std::string::npos &&
                  split.find("writes such a section back rather than over it") ==
                      std::string::npos,
              "rendered on their own, away from the unreadable file, they still "
              "say they were not kept");

    // The other direction, which no whole-list rule could get right either: a
    // section carried out of z80cpmw.json listed beside a PROFILE that would not
    // load. That list is what ConfigManager::loadProfile leaves behind, and the
    // carry really is still in force, so the promise really is still true.
    Diagnostic carried;
    carried.problem = Problem::TypeMismatch;
    carried.path = "keyboard.keys";
    carried.carried = true;
    Diagnostic elsewhere;
    elsewhere.problem = Problem::UnreadableFile;
    elsewhere.path = "C:\\Users\\x\\profiles\\p.json";
    elsewhere.detail = "parse error at line 2";
    std::string twoFiles = flatten(renderBlock(Diagnostics{ carried, elsewhere }));
    checkTrue(twoFiles.find("writes such a section back rather than over it") !=
                  std::string::npos,
              "a carried section beside another file's failure is still called safe");
    checkTrue(twoFiles.find("not carried through a save") == std::string::npos,
              "and is not also said to have been dropped");

    // Both fates in one block, which is what MainWindow renders when the config
    // in force has a carried mismatch and a profile that would not load had one
    // of its own. Two sentences that both began "sections listed above" would be
    // individually true and jointly useless, so the LINE carries the mark and
    // the sentence points at it.
    Diagnostic lost;
    lost.problem = Problem::TypeMismatch;
    lost.path = "disks";
    lost.carried = false;
    std::string bothFates = flatten(renderBlock(Diagnostics{ carried, lost }));
    checkTrue(bothFates.find("writes such a section back rather than over it") !=
                  std::string::npos &&
                  bothFates.find("not carried through a save") != std::string::npos,
              "one block can hold both fates and says both");
    size_t firstMark = bothFates.find("not kept - the file it is in");
    checkTrue(firstMark != std::string::npos &&
                  bothFates.find("not kept - the file it is in",
                                 firstMark + 1) == std::string::npos,
              "with exactly one line marked, the one that was not kept");
    // "Sections listed above" would otherwise sweep in the marked line, so the
    // carried sentence names the exception - and only when there is one on the
    // screen to name.
    checkTrue(bothFates.find("Except where a line is marked as not kept") !=
                  std::string::npos,
              "and the safe sentence excepts it rather than covering it");
    checkTrue(mismatch.find("Except where a line is marked") == std::string::npos,
              "while a report with nothing marked does not mention a mark");

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
    checkTrue(quarantined.find("not saved automatically") != std::string::npos,
              "and keeps the sentence about the save that does not follow");

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
    test_carried_text();
    test_wrongly_typed_section_survives_a_save();
    test_hand_fix_is_picked_up();
    test_carry_matches_what_the_loader_skips();
    test_fifth_disk();
    test_failed_profile_keeps_the_report();
    test_retrying_a_profile_does_not_double_the_report();
    test_carry_belongs_to_one_file();
    test_render_block();
    test_bell();

    std::error_code ec;
    fs::remove_all(g_dir, ec);

    printf("\n===============================\n");
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
