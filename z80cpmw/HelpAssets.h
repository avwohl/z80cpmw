/*
 * HelpAssets.h - The state-free half of the help system.
 *
 * Everything here takes strings and returns strings. It touches no window, no
 * thread and no socket, so tests\test_help.cpp can call all of it on a machine
 * with no interactive desktop and without either sibling checkout - which is
 * why the help suite can sit second in tests\run_tests.bat, ahead of the two
 * blocks that exit /b 1 when ..\romwbw_emu or ..\cpmemu are missing.
 *
 * The split is not tidying for its own sake. todo.txt asks for the seven remote
 * topics to be bundled and cached on disk, and both halves of that job are
 * built out of exactly these functions: bundling makes markdownToText's output
 * the only thing a reader ever sees of those topics, in every build and with
 * the network down, and caching turns a filename that arrived over the network
 * into a path on the user's machine. Whatever the renderer gets wrong today is
 * shipped and taken offline by the first of those, so it is fixed and pinned
 * before either is written.
 */

#pragma once

#include <string>
#include <vector>

namespace help_assets {

// One entry from help_index.json.
struct HelpTopic {
    std::string id;
    std::string title;
    std::string description;
    std::string filename;
};

// Read the "topics" array out of help_index.json. Returns false and sets error
// when nothing usable was found; topics is cleared on entry either way.
//
// This is a scanner rather than a JSON parser: it takes each brace-delimited
// run after the "topics" key and pulls four quoted values out of it by name. It
// therefore gets a nested object or an escaped quote wrong, and does not stop
// at the closing bracket of the array. That is the behaviour that has shipped;
// it is moved here unchanged, and the suite pins the shape the published index
// actually has rather than asserting anything about the shapes it would
// mishandle. nlohmann::json is already in the tree (Config.cpp uses it) and
// would fix all three, but swapping the parser is a behaviour change that
// belongs in its own commit.
bool parseIndexJson(const std::string& json,
                    std::vector<HelpTopic>& topics,
                    std::string& error);

// Render markdown to the plain text the help pane's read-only EDIT control
// shows. Line endings are CRLF because that is what an EDIT control needs to
// break a line at all.
std::string markdownToText(const std::string& markdown);

// True when name is safe to paste into a download URL and into a cache path.
//
// This has no caller yet and is here deliberately: the next commit builds both
// of those out of HelpTopic::filename, which is a string that arrived over the
// network in help_index.json. It is a whitelist - a plain file name, no
// separators, no drive letter, no dot-dot - because a blacklist of the ways a
// path escapes a directory on Windows is a list nobody finishes.
bool isSafeAssetName(const std::string& name);

// Decode UTF-8 to UTF-16 for the Win32 W entry points.
//
// The thing this replaces is std::wstring(s.begin(), s.end()), which widens
// each char one at a time; char is signed on MSVC, so every byte over 0x7F
// becomes a negative value and then a wchar_t up near 0xFF80-0xFFFF. Of the
// eight assets published in avwohl/ioscpm exactly one is not ASCII -
// help_file_transfer.md, 30 bytes making 10 characters: U+2014 twice, U+2026
// twice, U+2192 six times - so the old conversion turned each of those into two
// or three pieces of garbage.
//
// Invalid UTF-8 is not rejected. MultiByteToWideChar is called without
// MB_ERR_INVALID_CHARS, so a bad sequence becomes U+FFFD and the reader still
// gets the rest of the topic; failing the call would hand the pane an empty
// string, which looks exactly like a topic that loaded and said nothing.
std::wstring toWide(const std::string& utf8);

}  // namespace help_assets
