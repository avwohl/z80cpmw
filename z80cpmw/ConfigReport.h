/*
 * ConfigReport.h - What the configuration file said that nothing read
 *
 * A mistyped setting in z80cpmw.json used to be absorbed in total silence and
 * then deleted. Config.cpp's from_json reads every field as j.value(name,
 * default), so a member nobody asks for is simply never looked at; to_json
 * writes only the names it knows, so the next save drops it. "fontsize": 28 in
 * a hand-edited file therefore did nothing, said nothing, and was gone by the
 * time the user reopened the file to check what they had typed.
 *
 * This header is the vocabulary for saying so. It is deliberately free of
 * nlohmann/json.hpp: Config.h is included by MainWindow.h, so anything Config.h
 * pulls in is pulled into most of the GUI, and json.hpp is 25,000 lines of
 * templates. The detection itself lives in Config.cpp, where the JSON already
 * is - see collectMemberProblems() and inspectDocument() - and hands back these
 * plain structures.
 */

#pragma once

#include <string>
#include <vector>

namespace config {

// What is wrong with one entry in the document.
//
// The order below is the order the kinds get worse in, which is also the order
// renderBlock() explains them in: ignored and dropped, ignored and kept, not
// read at all.
enum class Problem {
    // A member no from_json in Config.cpp reads. It is ignored now and the next
    // save will not write it back.
    UnknownMember,

    // A member whose NAME is one we know and whose value is the wrong kind of
    // thing - "keys" as an array, "disks" as an object, "keyboard" as a string.
    //
    // This is the quietest failure in the schema and the reason the kind
    // exists. from_json guards on the type in three places rather than letting
    // the conversion throw (j["disks"].is_array(), hw["dazzler"].is_array(),
    // KeyboardConfig's j["keys"].is_object()), so a wrong type there skipped
    // the whole section without a word and the next save replaced it with the
    // built-in defaults. Nothing is read out of such a member, so load() does
    // not save automatically after finding one - see the save at the end of
    // ConfigManager::load().
    TypeMismatch,

    // A "keyboard.keys" entry whose name resolves to a combination the
    // application answers before the keymap is consulted, so the bytes bound to
    // it could never reach CP/M.
    ReservedKey,

    // A "keyboard.keys" entry whose name resolves to no key at all - "F13",
    // "PgeUp", "Ctrl_Left". keymap::vkForName rejects it, so KeyMap::build
    // skips it and load()'s fill loop skips it, and the line sits in the file
    // looking exactly like a binding that works.
    UnknownKeyName,

    // The file could not be turned into an AppConfig at all - it could not be
    // opened, or it is a syntax error, or a member from_json requires rather
    // than defaults (DiskConfig's j.at("path") is the one such member in the
    // whole schema).
    UnreadableFile,
};

struct Diagnostic {
    Problem problem = Problem::UnknownMember;

    // Where in the document, in the dotted-and-subscripted notation a person
    // can follow back into the file: "display.fontsize", "disks[1].pth",
    // "hardware.dazzler[0].prt". For UnreadableFile there is no member to
    // point at, so this carries the path of the file instead.
    std::string path;

    // Why, in the user's terms: the nearest known spelling, what the reserved
    // combination is used for, what type was expected, or the parser's own
    // message (which names the line and column of a syntax error and is the
    // single most useful thing the old catch-and-discard handler threw away).
    std::string detail;

    // Where the unreadable file was moved to, for UnreadableFile only. Empty
    // for every other problem, and empty if the rename itself failed.
    //
    // An empty value renders as NOTHING in renderBlock() below, which is why
    // ConfigManager::loadFromFile appends the "still there under its own name"
    // sentence to `detail` when the rename fails rather than leaving the reader
    // to infer it from a line that was not printed.
    std::string backup;
};

using Diagnostics = std::vector<Diagnostic>;

// Human-readable name for the kind of problem, short enough to lead a line.
inline const char* problemLabel(Problem p) {
    switch (p) {
    case Problem::UnknownMember:  return "unrecognised setting:";
    case Problem::TypeMismatch:   return "wrong kind of value:";
    case Problem::ReservedKey:    return "reserved key:";
    case Problem::UnknownKeyName: return "unknown key name:";
    case Problem::UnreadableFile: return "could not be read:";
    }
    return "problem:";
}

// Append one logical line, wrapped to 78 columns with a hanging indent.
//
// The terminal this is printed on is 80 columns and wraps mid-word, and a
// diagnostic line is mostly one long path plus a sentence - exactly the shape
// that gets broken in the middle of the thing the reader has to copy back into
// their editor. Breaking on spaces here costs ten lines and keeps every path
// whole unless the path alone is wider than the screen.
inline void appendWrapped(std::string& out, const std::string& text,
                          const std::string& firstIndent,
                          const std::string& contIndent) {
    const size_t width = 78;
    std::string indent = firstIndent;
    size_t pos = 0;
    do {
        size_t room = width > indent.size() ? width - indent.size() : 1;
        size_t take = text.size() - pos;
        if (take > room) {
            // rfind from the last column that would still fit, so the break is
            // at the last space that fits rather than the first one that does.
            size_t brk = text.rfind(' ', pos + room);
            take = (brk != std::string::npos && brk > pos) ? brk - pos : room;
        }
        out += indent;
        out += text.substr(pos, take);
        // CRLF, not LF. TerminalView gives 0x0A an implicit carriage return so
        // the CR costs nothing there, but a plain CP/M console does not, and
        // this same text is worth being able to paste into a bug report on
        // Windows without every line running together.
        out += "\r\n";
        pos += take;
        while (pos < text.size() && text[pos] == ' ') ++pos;
        indent = contIndent;
    } while (pos < text.size());
}

// Turn a set of diagnostics into the block of text a terminal can print.
// Empty in, empty out, so a caller can print the result unconditionally and
// say nothing when there is nothing to say.
inline std::string renderBlock(const Diagnostics& diags) {
    if (diags.empty()) return std::string();

    std::string out = "Configuration report: ";
    out += std::to_string(diags.size());
    out += (diags.size() == 1 ? " problem" : " problems");
    out += " while loading settings.\r\n\r\n";

    // What happens next is not the same for every kind, so it is not said in
    // one sentence. The single flag this replaces was set for everything that
    // was not UnreadableFile and then printed "Entries listed above are
    // ignored, and saving settings will drop them", which is false for a
    // reserved or misspelled key name: from_json reads keyboard.keys whole,
    // to_json writes it back whole, and load()'s fill loop only ever ADDS names
    // to it. Nothing in the loader prunes that object, ever - the same fact
    // KeyMap::build in Keymap.h records from the other side ("Dropping such
    // entries at load would make a Settings dialog lossy"). Telling users their
    // hand-written binding is about to be erased when it is not is how they
    // stop believing the rest of the report.
    bool anyDropped = false;   // ignored, and gone at the next save
    bool anyKept    = false;   // ignored, but written back untouched
    bool anyUnread  = false;   // never reached the configuration at all

    for (const auto& d : diags) {
        std::string line = problemLabel(d.problem);
        line += "  ";
        // An UnreadableFile whose path was never filled in is the whole
        // document, which happens when inspectDocument is asked about JSON that
        // has no file behind it.
        line += d.path.empty() ? "(the whole document)" : d.path;
        if (!d.detail.empty()) {
            line += " - ";
            line += d.detail;
        }
        appendWrapped(out, line, "  ", "      ");
        if (!d.backup.empty()) {
            appendWrapped(out, "the original was kept as " + d.backup, "      ", "      ");
        }

        switch (d.problem) {
        case Problem::UnknownMember:  anyDropped = true; break;
        case Problem::ReservedKey:
        case Problem::UnknownKeyName: anyKept = true; break;
        case Problem::TypeMismatch:
        case Problem::UnreadableFile: anyUnread = true; break;
        }
    }

    // The sentences the whole exercise exists to be able to say. Without them
    // the list reads as a warning about something that might happen later, when
    // in fact the setting is already being ignored - and in one of the three
    // cases is about to be erased.
    if (anyDropped || anyKept || anyUnread) out += "\r\n";
    if (anyDropped) {
        appendWrapped(out,
                      "Unrecognised settings listed above are ignored, and "
                      "saving settings will drop them.",
                      "", "");
    }
    if (anyKept) {
        appendWrapped(out,
                      "Key bindings listed above are ignored, but they are "
                      "kept: nothing removes an entry from \"keyboard.keys\", "
                      "so the line is still in the file to be corrected.",
                      "", "");
    }
    if (anyUnread) {
        appendWrapped(out,
                      "Settings listed above were not read at all. Settings "
                      "are not saved automatically after that, so nothing has "
                      "been written over what you typed.",
                      "", "");
    }
    return out;
}

} // namespace config
