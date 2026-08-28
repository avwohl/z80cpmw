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
// The order below is the order renderBlock() explains them in: ignored and
// dropped, skipped whole, ignored and kept, not read at all. Five kinds and six
// sentences - the skipped one is explained twice, because whether such a
// section is written back or lost with its file is decided per diagnostic and
// both answers can appear in one report.
//
// It is no longer also the order the kinds get worse in, which is
// what this note used to say - a TypeMismatch stopped being a setting on its
// way out of the file when AppConfig::unreadSections started carrying it - and
// the enumerators are left where they are rather than reordered for a sentence.
enum class Problem {
    // A member no from_json in Config.cpp reads. It is ignored now and the next
    // save will not write it back.
    //
    // Also an array element past the end of what the loader reads, which is the
    // same outcome reached by a different route. AppConfig::disks is four units
    // and from_json's loop is bounded "i < 4 && i < disks.size()", so a fifth
    // disk entry is stepped over; to_json then writes exactly four, so the entry
    // is gone at the next save. collectExcessDiskProblems() in Config.cpp
    // reports it, because "ignored now, dropped at the next save" is precisely
    // what this kind already means.
    UnknownMember,

    // A member whose NAME is one we know and whose value is the wrong kind of
    // thing - "keys" as an array, "disks" as an object, "keyboard" as a string.
    //
    // This is the quietest failure in the schema and the reason the kind
    // exists. from_json guards on the type in three places rather than letting
    // the conversion throw (j["disks"].is_array(), hw["dazzler"].is_array(),
    // KeyboardConfig's j["keys"].is_object()), and a "hardware" that is not an
    // object answers contains("dazzler") with false to the same effect, so a
    // wrong type in any of them skipped the whole section without a word and
    // the next save replaced it with the built-in defaults.
    //
    // Nothing is read out of such a section, so there is nothing the
    // application knows that could honestly go back into it:
    // AppConfig::unreadSections keeps the text the loader could not use and
    // to_json splices it back where it came from, which is what makes a later
    // save safe for this kind. ConfigManager::load() also declines to save on
    // its own after finding one - see the save at the end of it - but that only
    // ever covered the save load() itself makes; the carry is what covers every
    // other one.
    //
    // The carry is NOT unconditional, and Diagnostic::carried below is where
    // one of these says whether it got one. Two limits: a document that also
    // produces an UnreadableFile is never turned into an AppConfig at all, so
    // there is nothing to attach a carry to and the whole file is quarantined
    // instead; and a carry is written back only to the file it was read out of
    // (AppConfig::unreadSectionsFrom), never into a profile or a main config
    // that never held that text.
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

    // TypeMismatch only: whether this section's own text is being carried, so
    // that a save writes it BACK rather than over it. False for every other
    // kind, and false for a mismatch in a document that also produced an
    // UnreadableFile - the conversion threw, ConfigManager::loadFromFile never
    // reached the assignment to AppConfig::unreadSections, the file was
    // quarantined, and the next save writes the built-in defaults for that
    // section. inspectDocument() decides it, from the same `readable` test the
    // loader uses, so the two cannot answer differently.
    //
    // Per-diagnostic rather than worked out inside renderBlock(), because
    // renderBlock() cannot see enough to work it out. MainWindow's
    // reportConfigDiagnostics() splits the list BY KIND and calls renderBlock()
    // once per kind, so the TypeMismatch block it renders never contains the
    // UnreadableFile that decided the answer. A flag computed from "does this
    // list also hold an UnreadableFile" would therefore be correct in the tests
    // and inert in the shipping app. It also settles the other direction, which
    // no whole-list test could get right either: a mismatch carried out of
    // z80cpmw.json listed beside an UnreadableFile for a profile that would not
    // load is still carried, and says so.
    bool carried = false;
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
    //
    // A TypeMismatch was split off from the last of these for the same reason,
    // one change later. It shared "nothing has been written over what you
    // typed" with an UnreadableFile, which was true of both only because
    // ConfigManager::load() suppresses its own save - a narrower promise than
    // the sentence sounds, and the wrong one now that AppConfig::unreadSections
    // carries the section through every save of the file it came from. The two
    // kinds reach the same outcome by different machinery, one by a rename and
    // one by a carry, and they no longer stop being true at the same moment
    // either: the section a carry protects is still wrong after a save and
    // still worth saying so, while a file the quarantine really did move is no
    // longer at that path to be wrong at all.
    //
    // The skipped case then had to be split again, and for the same reason a
    // third time: the sentence was emitted for EVERY TypeMismatch, while
    // ConfigManager::loadFromFile only takes the carry when the document is
    // otherwise readable. A file that is both wrongly typed and unreadable had
    // its carry discarded, was renamed to .bad, and got the built-in defaults
    // written for that section at the next save - while this block told the
    // user their text was safe. Diagnostic::carried is what each one now says
    // for itself.
    bool anyDropped = false;   // ignored, and gone at the next save
    bool anyCarried = false;   // skipped whole, and written back as it stands
    bool anyLost    = false;   // skipped whole, and NOT carried anywhere
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
        // Marked on the LINE, not left to the sentences below, because one
        // report can hold both fates at once: a mismatch carried out of
        // z80cpmw.json, and a mismatch in a profile that would not load and was
        // therefore dropped, arrive in one list from ConfigManager::loadProfile
        // and MainWindow renders them in one block. Two sentences that both
        // begin "sections listed above" would then be individually true and
        // jointly unreadable. The sentence for this case names this mark.
        if (d.problem == Problem::TypeMismatch && !d.carried) {
            appendWrapped(out, "not kept - the file it is in could not be read "
                               "as a whole", "      ", "      ");
        }

        switch (d.problem) {
        case Problem::UnknownMember:  anyDropped = true; break;
        case Problem::TypeMismatch:
            if (d.carried) anyCarried = true; else anyLost = true;
            break;
        case Problem::ReservedKey:
        case Problem::UnknownKeyName: anyKept = true; break;
        case Problem::UnreadableFile: anyUnread = true; break;
        }
    }

    // The sentences the whole exercise exists to be able to say. Without them
    // the list reads as a warning about something that might happen later, when
    // in fact the setting is already being ignored - and in two of the five
    // cases is about to be erased.
    if (anyDropped || anyCarried || anyLost || anyKept || anyUnread) out += "\r\n";
    if (anyDropped) {
        appendWrapped(out,
                      "Unrecognised settings listed above are ignored, and "
                      "saving settings will drop them.",
                      "", "");
    }
    if (anyCarried) {
        // Both halves of the carry, because the second half is the surprise.
        // The user's text is safe from every save of the file they typed it in
        // - which is every save this sentence is ever printed about, since a
        // save of any OTHER file writes neither their text nor over it - and
        // the price of that is that nothing else gets into that section either:
        // a disk mounted while "disks" is an object is gone at the next save,
        // and this sentence is the only warning of it the user is ever given.
        std::string s = "Sections listed above were not read at all - the "
                        "value is the wrong kind of thing, so the whole "
                        "section was skipped. ";
        // "Sections listed above" sweeps in a section that was NOT carried when
        // one of each is in the block, so the exception is named - but only
        // then. In the ordinary report there is nothing marked for the reader
        // to except, and a qualifier pointing at a mark that is not on the
        // screen is a puzzle rather than a precision.
        s += anyLost
                 ? "Except where a line is marked as not kept, saving settings "
                   "writes such a section back rather than over it"
                 : "Saving settings writes such a section back rather than "
                   "over it";
        s += ", so what you typed is safe; until it is corrected, nothing the "
             "application changes in that section is saved either.";
        appendWrapped(out, s, "", "");
    }
    if (anyLost) {
        // The opposite outcome, and the one the single sentence above used to
        // be printed for as well. Nothing is promised about a backup here: the
        // rename can fail, and when it does the UnreadableFile's own line says
        // so instead.
        appendWrapped(out,
                      "A section marked above as not kept is not carried "
                      "through a save: the file it is in could not be read as a "
                      "whole, so nothing from it is in force and the next save "
                      "writes the built-in defaults for that section.",
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
