/*
 * test_help.cpp - Help renderer, index parser, asset-name and cache suite.
 *
 * todo.txt: "the seven remote help topics have no bundled copy and no on-disk
 * cache." The renderer sections came first, before either half could bake a
 * defect into a binary or into a file on the user's disk; the cache sections
 * further down cover the second half, and the bundled-asset section at the
 * bottom covers the third, which is the one that puts the renderer's output in
 * front of a reader with no network at all.
 *
 * What this suite would have caught, measured over the eight assets published
 * in avwohl/ioscpm at ..\ioscpm\release_assets:
 *
 *   170 fence lines across the eight, 60 of them in help_cpm22.md, printed as
 *   literal backticks because markdownToText had no branch for a fence at all.
 *
 *   63 bullet lines and 6 table rows carrying ** or a backtick, printed with
 *   the markers showing, because the bullet and table branches ran "continue"
 *   before the inline passes at the bottom of the loop.
 *
 *   30 bytes making 10 characters in help_file_transfer.md - U+2014 twice,
 *   U+2026 twice, U+2192 six times - mangled into two or three wchar_t each by
 *   std::wstring(text.begin(), text.end()).
 *
 * Those assets are read in ONE section, the bundled-asset one at the bottom,
 * and that section is compiled out when ..\ioscpm is not there. Everything else
 * needs no sibling checkout and no window station, which is what lets the suite
 * sit second in tests\run_tests.bat: the host-file and HBIOS blocks below it
 * exit /b 1 when ..\romwbw_emu or ..\cpmemu are missing, so a block appended
 * after them is unreachable on a machine that has only this repository. The
 * index JSON in test_parse_index is a copy of the published one rather than a
 * read of it, for the same reason.
 *
 * The cache sections do touch the file system - that is what they are for - but
 * only under %TEMP%\z80cpmw_help_cache_<pid>, which help_assets::setCacheRoot
 * points them at and removeScratch() takes away at the end of main(). Nothing
 * is written under the user's profile and no network is used.
 *
 * Build and run: tests\run_tests.bat
 */

#include "HelpAssets.h"
#include "HelpWindow.h"
// For the IDR_HELP_* ids the bundled-asset section checks the module's RCDATA
// resources against. Harmless in a build without them: it is #defines only.
#include "resource.h"
// For keymap::defaultBindings() and keymap::reservedKeys(), which the
// Configuration topic's two tables are checked against instead of against a
// copy of them written out here. Header-only over windows.h, <string>, <map>,
// <cctype> and <cstdlib>, so it adds nothing to link and needs no sibling.
#include "Keymap.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

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

static void checkFalse(bool ok, const std::string& what) {
    check(!ok, what, ok ? "true" : "false", "false");
}

static void checkStr(const std::string& got, const std::string& want,
                     const std::string& what) {
    check(got == want, what, "\"" + got + "\"", "\"" + want + "\"");
}

static void checkInt(long got, long want, const std::string& what) {
    check(got == want, what, std::to_string(got), std::to_string(want));
}

//=============================================================================
// Helpers
//=============================================================================

// The rendered text, split on the CRLF pairs the EDIT control needs. A single
// trailing empty line is dropped: every rendered line ends in CRLF, so the
// split always leaves one.
static std::vector<std::string> renderLines(const std::string& markdown) {
    std::string text = help_assets::markdownToText(markdown);
    std::vector<std::string> lines;
    size_t pos = 0;
    while (true) {
        size_t nl = text.find("\r\n", pos);
        if (nl == std::string::npos) {
            lines.push_back(text.substr(pos));
            break;
        }
        lines.push_back(text.substr(pos, nl - pos));
        pos = nl + 2;
    }
    if (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

// The SOURCE lines of a markdown string, for the invariants the bundled topics
// are written to satisfy.
static std::vector<std::string> sourceLines(const std::string& markdown) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos <= markdown.size()) {
        size_t nl = markdown.find('\n', pos);
        std::string line = (nl == std::string::npos)
                               ? markdown.substr(pos)
                               : markdown.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return lines;
}

static std::string trimmed(const std::string& s) {
    size_t first = s.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" \t");
    return s.substr(first, last - first + 1);
}

static std::string rtrim(const std::string& s) {
    size_t last = s.find_last_not_of(" \t");
    if (last == std::string::npos) return "";
    return s.substr(0, last + 1);
}

static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

static bool anyLine(const std::vector<std::string>& lines, const std::string& want) {
    for (const auto& l : lines) {
        if (l == want) return true;
    }
    return false;
}

// A wide string printed as code points, so a failure message says which
// characters came back rather than mojibake in whatever the console is set to.
static std::string describeWide(const std::wstring& w) {
    std::string s = "[";
    for (size_t i = 0; i < w.size(); ++i) {
        char buf[16];
        snprintf(buf, sizeof(buf), i ? " U+%04X" : "U+%04X", (unsigned)w[i]);
        s += buf;
    }
    return s + "]";
}

//=============================================================================
// The two bundled topics
//
// These are the only help text this build can show with the network down, and
// the previous commit rewrote the File Transfer section of the first one after
// measuring what the shipped W8.COM actually does. The checks here are the ones
// that would have caught the claim it removed coming back.
//=============================================================================

static void test_bundled_file_transfer() {
    section("bundled topics: File Transfer");

    const std::string gs = help_topics::gettingStartedMarkdown();
    const std::string cfg = help_topics::configurationMarkdown();

    checkTrue(!gs.empty(), "Getting Started markdown is not empty");
    checkTrue(!cfg.empty(), "Configuration markdown is not empty");
    checkTrue(contains(gs, "## File Transfer (R8 / W8)"),
              "Getting Started still has a File Transfer section");

    // The removed claim. "Give a full path (recommended)" sat under a heading
    // naming R8 and W8 together, so it read as a recommendation for both, and
    // it is true of R8 only.
    checkFalse(contains(gs, "Give a full path"),
               "the recommendation that covered W8 too is gone");
    checkFalse(contains(cfg, "Give a full path"),
               "and did not reappear in the Configuration topic");

    // Whatever wording replaces it, the line that says a full path works must
    // be about R8. One line mentions a full path; it names R8 and not W8.
    std::vector<std::string> hits;
    for (const auto& line : sourceLines(gs)) {
        if (contains(line, "full path")) hits.push_back(line);
    }
    checkInt((long)hits.size(), 1, "exactly one line offers a full path");
    if (hits.size() == 1) {
        checkTrue(contains(hits[0], "R8"), "that line names R8: " + hits[0]);
        checkFalse(contains(hits[0], "W8"), "that line does not name W8: " + hits[0]);
    }

    // The two hazards of the utilities on the shipped images, and the games
    // disk that carries neither. Named so that deleting a block without its
    // condition being met is a test failure rather than a quiet loss.
    checkTrue(contains(gs, "W8 does not take a host path yet"),
              "the W8 host-path block is present");
    checkTrue(contains(gs, "Two cautions until then"),
              "the R8 wildcard / W8 1Ah caution block is present");
    checkTrue(contains(gs, "The games disk carries neither utility"),
              "the games-disk sentence is present");
}

static void test_bundled_render_invariants() {
    section("bundled topics: renderer invariants");

    // The previous commit wrote its replacement text to avoid the renderer's
    // defects: no bullet marker, no ** and no backtick anywhere in either
    // bundled topic. Those defects are fixed as of this commit, so the text no
    // longer HAS to obey - but it still does, and pinning it keeps the two
    // topics readable in any renderer and makes a regression in either half
    // visible on its own. One check per property per topic, naming the first
    // offending line.
    struct { const char* name; std::string md; } topics[] = {
        { "Getting Started",  help_topics::gettingStartedMarkdown() },
        { "Configuration",    help_topics::configurationMarkdown()  },
    };

    for (const auto& t : topics) {
        std::string bullet, stars, tick;
        for (const auto& line : sourceLines(t.md)) {
            std::string tr = trimmed(line);
            if (bullet.empty() && tr.size() >= 2 &&
                (tr[0] == '-' || tr[0] == '*') && tr[1] == ' ') {
                bullet = line;
            }
            if (stars.empty() && contains(line, "**")) stars = line;
            if (tick.empty() && line.find('`') != std::string::npos) tick = line;
        }
        check(bullet.empty(), std::string(t.name) + ": no line starts with a bullet marker",
              "\"" + bullet + "\"", "(none)");
        check(stars.empty(), std::string(t.name) + ": no line contains a double-star",
              "\"" + stars + "\"", "(none)");
        check(tick.empty(), std::string(t.name) + ": no line contains a backtick",
              "\"" + tick + "\"", "(none)");
    }

    // And they survive the renderer intact. The Configuration topic's default
    // bindings are shown with the doubled backslashes the user types into
    // z80cpmw.json, so the renderer must not touch a backslash.
    std::vector<std::string> gsLines = renderLines(help_topics::gettingStartedMarkdown());
    std::vector<std::string> cfgLines = renderLines(help_topics::configurationMarkdown());
    checkTrue(gsLines.size() > 50, "Getting Started renders to more than 50 lines");
    checkTrue(cfgLines.size() > 50, "Configuration renders to more than 50 lines");
    checkTrue(anyLine(gsLines, "Getting Started"), "the H1 renders as its own text");
    checkTrue(anyLine(gsLines, "==============="), "and is underlined to its own width");
    checkTrue(contains(help_assets::markdownToText(help_topics::configurationMarkdown()),
                       "\\\\E[A"),
              "the doubled backslash of a default binding survives the renderer");
}

static void test_bundled_configuration_matches_docs() {
    section("bundled topics: the settings the Configuration topic documents");

    // todo.txt: the in-app copy of the settings table "has drifted from the
    // file it documents: there is no display.bell row, and nothing tells the
    // reader that keys can be bound from Emulator > Settings > Keyboard rather
    // than by hand." Both are fixed, and pinned here because the copy is a raw
    // string literal that needs a build to look at - the same property that let
    // the last correction land in docs\CONFIGURATION.md and not in this string.
    //
    // These are substring checks and not a diff against docs\CONFIGURATION.md.
    // A diff is not possible: the topic is deliberately NOT that file - it
    // carries none of the markdown constructs the EDIT-control renderer handles
    // badly, and it is shorter. What is checkable is that each fact the item
    // named is present in some wording, which is what fails when one is dropped
    // again.
    const std::string cfg = help_topics::configurationMarkdown();

    checkTrue(contains(cfg, "| display.bell |"),
              "the Other Settings table has a display.bell row");
    checkTrue(contains(cfg, "BEL (character 7)"),
              "and says what it does, in docs\\CONFIGURATION.md's words");
    checkTrue(contains(cfg, "default true"),
              "and gives its default, which is AppConfig::bellEnabled's");

    checkTrue(contains(cfg, "Emulator > Settings > Keyboard"),
              "the reader is told keys can be bound from the Keyboard page");
    checkTrue(contains(cfg, "You do not have to edit the file to change a key"),
              "and told it before the hand-editing instructions, not after");

    // The Default Bindings table, walked against keymap::defaultBindings()
    // rather than against a list written out here. That is the difference
    // between pinning the four Ctrl+arrows that had gone missing and pinning
    // the PROPERTY that went wrong - a fifth default added to Keymap.h and not
    // to this topic fails the suite on its own, with no test to remember to
    // update. Keymap.h costs nothing to include: it is header-only over
    // windows.h and four standard headers, and links nothing.
    //
    // The topic shows each sequence with its backslashes DOUBLED, because that
    // is how the user types it into z80cpmw.json, so the comparison doubles
    // them too. Everything else about the row is literal.
    for (const auto& binding : keymap::defaultBindings()) {
        std::string shown;
        for (char ch : binding.second) {
            shown += ch;
            if (ch == '\\') shown += '\\';
        }
        std::string row = "| " + binding.first + " | " + shown + " |";
        checkTrue(contains(cfg, row), "the default bindings table has " + row);
    }
    checkTrue(contains(cfg, "Ctrl+, Shift+ and Alt+ prefixes"),
              "and the modifier prefixes a Ctrl+arrow row needs are explained");

    // The reserved combinations, walked the same way out of
    // keymap::reservedKeys(). Keymap.h's own comment records that
    // docs\CONFIGURATION.md carries a hand-maintained copy of this list which
    // nothing checks against the table - that is still true of the .md; it is
    // no longer true of the in-app copy.
    size_t reservedCount = 0;
    const keymap::ReservedKey* reserved = keymap::reservedKeys(&reservedCount);
    checkTrue(reservedCount > 0, "keymap has reserved combinations to document");
    for (size_t i = 0; i < reservedCount; ++i) {
        std::string name = keymap::nameForKeyId(
            (reserved[i].mods << 16) | (unsigned)reserved[i].vk);
        checkTrue(!name.empty(), "reserved row " + std::to_string(i) + " has a name");
        checkTrue(contains(cfg, name), name + " is named as reserved");
    }
    checkTrue(contains(cfg, "kept by the app and cannot be bound"),
              "and they are said to be unbindable, not merely listed");
}

//=============================================================================
// markdownToText
//=============================================================================

static void test_render_headers() {
    section("renderer: headers");

    std::vector<std::string> h1 = renderLines("# Hello");
    checkInt((long)h1.size(), 2, "an H1 renders to a line and a rule");
    if (h1.size() == 2) {
        checkStr(h1[0], "Hello", "H1 text");
        checkStr(h1[1], "=====", "H1 rule is = to the text's width");
    }

    std::vector<std::string> h2 = renderLines("## Sub");
    checkInt((long)h2.size(), 2, "an H2 renders to a line and a rule");
    if (h2.size() == 2) {
        checkStr(h2[0], "Sub", "H2 text");
        checkStr(h2[1], "---", "H2 rule is - to the text's width");
    }

    std::vector<std::string> h3 = renderLines("### Deep");
    checkInt((long)h3.size(), 1, "an H3 renders to a bare line");
    if (h3.size() == 1) checkStr(h3[0], "Deep", "H3 text");

    // The header branch ran "continue" before the inline passes too, so a
    // header carrying markup showed it - and the rule was measured on the
    // SOURCE, which would now be four characters too long.
    std::vector<std::string> hm = renderLines("# The **bold** `code` header");
    checkInt((long)hm.size(), 2, "a header with inline markup still renders to two lines");
    if (hm.size() == 2) {
        checkStr(hm[0], "The bold code header", "header markers are stripped");
        checkStr(hm[1], std::string(20, '='), "the rule matches the RENDERED width");
    }
}

static void test_render_bullets() {
    section("renderer: bullets");

    checkStr(renderLines("- plain").at(0), "  * plain", "a dash bullet");
    checkStr(renderLines("* plain").at(0), "  * plain", "a star bullet");

    // The defect: the bullet branch emitted "  * " and continued, so the bold
    // and backtick passes at the bottom of the loop never ran on it. This is
    // help_cpm22.md's own line 49.
    checkStr(renderLines("- `*` matches any characters").at(0),
             "  * * matches any characters",
             "a bullet's backticks are stripped");
    checkStr(renderLines("- **Bold** item").at(0), "  * Bold item",
             "a bullet's double-stars are stripped");
    checkStr(renderLines("  - indented `code` bullet").at(0),
             "  * indented code bullet",
             "an indented bullet is recognised and rendered");

    // A dash that is not a bullet marker needs the space after it.
    checkStr(renderLines("-not a bullet").at(0), "-not a bullet",
             "a dash with no space is ordinary text");
}

static void test_render_inline() {
    section("renderer: bold and inline code");

    checkStr(renderLines("Use **DIR** and `TYPE` now.").at(0),
             "Use DIR and TYPE now.", "markers are stripped from ordinary text");
    checkStr(renderLines("**a** and **b**").at(0), "a and b",
             "two bold runs on one line");

    // An unpaired marker is prose. Deleting the rest of the line after one
    // would be worse than showing it.
    checkStr(renderLines("5 ** 3 is not bold").at(0), "5 ** 3 is not bold",
             "an unpaired double-star is left alone");
    checkStr(renderLines("a lone ` backtick").at(0), "a lone ` backtick",
             "an unpaired backtick is left alone");

    // Indentation is how the bundled topics mark a code block, so the
    // fall-through prints the whole line, not the trimmed one.
    checkStr(renderLines("    R8 C:\\Users\\me\\getkey2.com").at(0),
             "    R8 C:\\Users\\me\\getkey2.com",
             "an indented block keeps its indent and its backslashes");
}

static void test_render_tables() {
    section("renderer: tables");

    std::vector<std::string> t = renderLines(
        "| Key | Action |\n"
        "| --- | --- |\n"
        "| F5 | Start emulator |\n"
        "| Shift+F5 | Stop emulator |\n");
    checkInt((long)t.size(), 4, "header, rule and two rows");
    if (t.size() == 4) {
        // Each column is as wide as its widest cell: 8 for "Shift+F5" and 14
        // for "Start emulator", with two spaces between them.
        checkStr(t[1], std::string(8, '-') + "  " + std::string(14, '-'),
                 "the rule is drawn to the measured column widths");
        checkStr(rtrim(t[0]), "Key       Action", "header row is padded to the widths");
        checkStr(rtrim(t[2]), "F5        Start emulator", "first data row");
        checkStr(rtrim(t[3]), "Shift+F5  Stop emulator", "second data row");
    }

    // The table branch had the same defect as the bullet branch, and it shows
    // up twice: the markers were printed, and the column was measured with them
    // still in it. help_quick_start.md has six rows of this shape.
    std::vector<std::string> b = renderLines(
        "| Cmd | Meaning |\n"
        "| --- | --- |\n"
        "| `DIRECTORY` | List files |\n");
    checkInt((long)b.size(), 3, "header, rule and one row");
    if (b.size() == 3) {
        checkStr(b[1], std::string(9, '-') + "  " + std::string(10, '-'),
                 "the column is measured on DIRECTORY, not on `DIRECTORY`");
        checkStr(rtrim(b[2]), "DIRECTORY  List files", "the row's backticks are gone");
    }

    // A table ends at a blank line and what follows is ordinary text again.
    std::vector<std::string> after = renderLines(
        "| A | B |\n"
        "| --- | --- |\n"
        "| 1 | 2 |\n"
        "\n"
        "After.\n");
    checkTrue(anyLine(after, "After."), "text after a table is not swallowed by it");
}

static void test_render_fences() {
    section("renderer: fenced code blocks");

    // There was no branch for a fence at all, so both marker lines reached the
    // fall-through and printed their own backticks.
    std::vector<std::string> f = renderLines(
        "Try this:\n"
        "```\n"
        "A>DIR\n"
        "```\n"
        "Done.\n");
    checkInt((long)f.size(), 3, "the two fence markers produce no lines of their own");
    std::string ticks, bare;
    for (const auto& l : f) {
        if (ticks.empty() && l.find('`') != std::string::npos) ticks = l;
        if (bare.empty() && trimmed(l) == "```") bare = l;
    }
    check(ticks.empty(), "no rendered line contains a backtick", "\"" + ticks + "\"", "(none)");
    check(bare.empty(), "no rendered line is a bare fence", "\"" + bare + "\"", "(none)");
    checkTrue(anyLine(f, "    A>DIR"),
              "fenced content is indented like the bundled topics' code blocks");
    checkTrue(anyLine(f, "Try this:") && anyLine(f, "Done."),
              "the prose either side is untouched");

    // An info string is consumed with its fence rather than printed.
    std::vector<std::string> info = renderLines("```asm\nLD A,1\n```\n");
    checkInt((long)info.size(), 1, "```asm is a fence, not a line of text");
    if (info.size() == 1) checkStr(info[0], "    LD A,1", "and its content still renders");

    // A fence says "these characters are not markdown", so nothing inside it is
    // interpreted - not a leading dash (a diff or a shell transcript has them),
    // not a star, not a backtick.
    std::vector<std::string> lit = renderLines(
        "```\n"
        "- not a bullet\n"
        "**stars** and `ticks`\n"
        "# not a header\n"
        "| not | a table |\n"
        "```\n");
    checkInt((long)lit.size(), 4, "four content lines, verbatim");
    if (lit.size() == 4) {
        checkStr(lit[0], "    - not a bullet", "a dash inside a fence is not a bullet");
        checkStr(lit[1], "    **stars** and `ticks`", "inline markers inside a fence are kept");
        checkStr(lit[2], "    # not a header", "a hash inside a fence is not a header");
        checkStr(lit[3], "    | not | a table |", "a pipe inside a fence is not a table");
    }

    // A blank line inside a fence stays blank rather than being indented.
    std::vector<std::string> blank = renderLines("```\na\n\nb\n```\n");
    checkInt((long)blank.size(), 3, "a fenced block keeps its blank line");
    if (blank.size() == 3) checkStr(blank[1], "", "the blank line carries no indent");

    // An unterminated fence runs to the end of the document. That is a defect
    // in the document; printing the rest of it indented beats printing stray
    // backticks.
    std::vector<std::string> open = renderLines("```\nstill open\n");
    checkInt((long)open.size(), 1, "an unterminated fence swallows the rest");
    if (open.size() == 1) checkStr(open[0], "    still open", "and still shows no backticks");
}

//=============================================================================
// isSafeAssetName
//
// No caller yet: the next commit builds a download URL and a cache path out of
// HelpTopic::filename, which is a string that arrived over the network.
//=============================================================================

static void test_safe_asset_name() {
    section("isSafeAssetName");

    checkTrue(help_assets::isSafeAssetName("help_index.json"), "accepts help_index.json");
    checkTrue(help_assets::isSafeAssetName("help_cpm22.md"), "accepts help_cpm22.md");
    checkTrue(help_assets::isSafeAssetName("help_file_transfer.md"),
              "accepts help_file_transfer.md");
    checkTrue(help_assets::isSafeAssetName("a"), "accepts a one-character name");

    checkFalse(help_assets::isSafeAssetName("../evil.md"), "refuses ../evil.md");
    checkFalse(help_assets::isSafeAssetName("..\\evil.md"), "refuses ..\\evil.md");
    checkFalse(help_assets::isSafeAssetName(".."), "refuses .. alone");
    checkFalse(help_assets::isSafeAssetName("a/b.md"), "refuses a/b.md");
    checkFalse(help_assets::isSafeAssetName("a\\b.md"), "refuses a\\b.md");
    checkFalse(help_assets::isSafeAssetName("C:\\Windows\\win.ini"),
               "refuses an absolute drive path");
    checkFalse(help_assets::isSafeAssetName("C:/Windows/win.ini"),
               "refuses it with forward slashes too");
    checkFalse(help_assets::isSafeAssetName("\\\\host\\share\\x.md"), "refuses a UNC path");
    checkFalse(help_assets::isSafeAssetName("help.md:stream"),
               "refuses an alternate data stream");
    checkFalse(help_assets::isSafeAssetName(""), "refuses an empty name");
    checkFalse(help_assets::isSafeAssetName(".hidden"), "refuses a leading dot");
    checkFalse(help_assets::isSafeAssetName("-rf"), "refuses a leading dash");
    checkFalse(help_assets::isSafeAssetName("help*.md"), "refuses a wildcard");
    checkFalse(help_assets::isSafeAssetName("help ?.md"), "refuses a space and a question mark");
    checkFalse(help_assets::isSafeAssetName("help\nx.md"), "refuses a control character");
    checkFalse(help_assets::isSafeAssetName("hel\xE2\x80\x94lo.md"),
               "refuses a byte over 0x7F");
    checkFalse(help_assets::isSafeAssetName(std::string(200, 'a') + ".md"),
               "refuses an absurdly long name");

    // Windows resolves a device name through its extension, so "NUL.md" is the
    // null device and a cache write to it succeeds and stores nothing.
    checkFalse(help_assets::isSafeAssetName("NUL.md"), "refuses NUL.md");
    checkFalse(help_assets::isSafeAssetName("con.md"), "refuses con.md, case-insensitively");
    checkFalse(help_assets::isSafeAssetName("COM1"), "refuses COM1");
    checkFalse(help_assets::isSafeAssetName("lpt9.txt"), "refuses lpt9.txt");
    checkTrue(help_assets::isSafeAssetName("console.md"),
              "but not a name that merely starts with one");
    checkTrue(help_assets::isSafeAssetName("com10.md"), "and not COM10, which is not a device");
}

//=============================================================================
// toWide
//=============================================================================

static void test_to_wide() {
    section("toWide");

    checkTrue(help_assets::toWide("").empty(), "an empty string converts to empty");
    checkTrue(help_assets::toWide("ASCII text") == L"ASCII text", "ASCII round-trips");

    // The three characters help_file_transfer.md actually uses, spelled as the
    // UTF-8 bytes that arrive off the wire: U+2014 EM DASH, U+2026 HORIZONTAL
    // ELLIPSIS, U+2192 RIGHTWARDS ARROW.
    const std::string utf8 = "\xE2\x80\x94 \xE2\x80\xA6 \xE2\x86\x92";
    const std::wstring want = L"\x2014 \x2026 \x2192";
    std::wstring got = help_assets::toWide(utf8);
    check(got == want, "the three non-ASCII characters round-trip",
          describeWide(got), describeWide(want));
    checkInt((long)got.size(), 5, "nine bytes and two spaces become five characters");

    // What it replaces, so the defect is pinned rather than described: widening
    // char by char turns each three-byte sequence into three wchar_t, and char
    // is signed on MSVC, so each one lands up near 0xFF80-0xFFFF.
    std::wstring naive(utf8.begin(), utf8.end());
    checkInt((long)naive.size(), 11, "the old conversion produced eleven");
    checkTrue(naive[0] > 0xFF00, "and its first character was not U+2014: " + describeWide(naive));

    // Invalid UTF-8 is not rejected. MultiByteToWideChar is called without
    // MB_ERR_INVALID_CHARS, so a stray byte becomes U+FFFD and the reader still
    // gets the rest of the topic - failing the call would hand the pane an
    // empty string, which looks exactly like a topic that loaded and said
    // nothing. Measured, not assumed: this is what the call actually returns.
    std::wstring bad = help_assets::toWide("a\x80z");
    checkInt((long)bad.size(), 3, "a stray 0x80 keeps its neighbours");
    check(bad == L"a\xFFFDz", "and becomes U+FFFD rather than nothing",
          describeWide(bad), describeWide(L"a\xFFFDz"));
}

//=============================================================================
// parseIndexJson
//
// A copy of the published help_index.json, not a read of it: this suite must
// run with no sibling checkout. parseIndexJson is a scanner, not a parser - see
// HelpAssets.h - so what is pinned here is the shape the published index has.
//=============================================================================

static const char* kPublishedIndex = R"JSON({
  "version": 1,
  "base_url": "https://github.com/avwohl/ioscpm/releases/latest/download/",
  "topics": [
    { "id": "quick_start",   "title": "Quick Start Guide",    "description": "Getting started with iOSCPM",              "filename": "help_quick_start.md" },
    { "id": "cpm22",         "title": "CP/M 2.2 User Guide",  "description": "Complete guide to CP/M 2.2 operating system", "filename": "help_cpm22.md" },
    { "id": "zsdos",         "title": "ZSDOS User Guide",     "description": "Z-System DOS with date/time stamping",     "filename": "help_zsdos.md" },
    { "id": "nzcom",         "title": "NZCOM User Guide",     "description": "Z-System for CP/M 2.2 environments",       "filename": "help_nzcom.md" },
    { "id": "zpm3",          "title": "ZPM3 User Guide",      "description": "Z-System Plus/M3 enhanced CP/M 3",         "filename": "help_zpm3.md" },
    { "id": "qpm",           "title": "QPM User Guide",       "description": "QP/M operating system",                    "filename": "help_qpm.md" },
    { "id": "disk_transfer", "title": "File Transfer (R8/W8)","description": "Transfer files between host and CP/M",     "filename": "help_file_transfer.md" }
  ]
})JSON";

static void test_parse_index() {
    section("parseIndexJson");

    std::vector<help_assets::HelpTopic> topics;
    std::string error = "untouched";
    checkTrue(help_assets::parseIndexJson(kPublishedIndex, topics, error),
              "the published index parses");
    checkInt((long)topics.size(), 7, "seven topics");
    if (topics.size() == 7) {
        checkStr(topics[0].id, "quick_start", "first id");
        checkStr(topics[0].title, "Quick Start Guide", "first title");
        checkStr(topics[0].filename, "help_quick_start.md", "first filename");
        checkStr(topics[6].id, "disk_transfer", "last id");
        checkStr(topics[6].filename, "help_file_transfer.md", "last filename");
    }

    // The tie between the two halves of this file: every name the published
    // index carries is one the cache path in the next commit may use.
    std::string rejected;
    for (const auto& t : topics) {
        if (!help_assets::isSafeAssetName(t.filename)) { rejected = t.filename; break; }
    }
    check(rejected.empty(), "every published filename is a safe asset name",
          "\"" + rejected + "\"", "(none)");

    // Failures. The topics vector is cleared on entry either way, so a failed
    // parse cannot leave the caller holding the previous index.
    topics.clear();
    error.clear();
    checkFalse(help_assets::parseIndexJson("{\"version\":1}", topics, error),
               "a document with no topics array fails");
    checkStr(error, "No topics array found", "and says which part was missing");
    checkTrue(topics.empty(), "and returns no topics");

    error.clear();
    checkFalse(help_assets::parseIndexJson("{\"topics\": []}", topics, error),
               "an empty topics array fails");
    checkStr(error, "No valid topics found", "with the other message");

    error.clear();
    checkFalse(help_assets::parseIndexJson("{\"topics\": 3}", topics, error),
               "topics that is not an array fails");
    checkStr(error, "Invalid topics format", "with the third message");

    // An entry missing an id or a title is dropped rather than shown as a blank
    // row in the list box.
    topics.push_back(help_assets::HelpTopic());
    error.clear();
    checkTrue(help_assets::parseIndexJson(
                  "{\"topics\": ["
                  "{\"title\": \"No id\", \"filename\": \"a.md\"},"
                  "{\"id\": \"b\", \"title\": \"B\", \"filename\": \"b.md\"}]}",
                  topics, error),
              "a partly bad array still yields its good entries");
    checkInt((long)topics.size(), 1, "the entry with no id is dropped");
    if (topics.size() == 1) checkStr(topics[0].id, "b", "and the good one survives");
}

//=============================================================================
// The on-disk cache
//
// This is the half of the todo item that is not blocked. It writes real files,
// which is why help_assets::setCacheRoot exists: the suite points the cache at
// a scratch directory under %TEMP% and the round trip below is the real one -
// CreateFileW, WriteFile, MoveFileExW - and not a simulation of it. Nothing is
// written under the user's profile, and removeScratch() takes the directory
// away at the end of main().
//=============================================================================

static std::wstring wide(const std::string& s) {
    return help_assets::toWide(s);
}

static bool fileExists(const std::string& path) {
    DWORD attr = GetFileAttributesW(wide(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Read a file without going through readCached, so an assertion about what is
// ON DISK does not rest on the function it is checking.
static std::string readRaw(const std::string& path) {
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::string();
    std::string out;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(h, buf, (DWORD)sizeof(buf), &n, nullptr) && n > 0) out.append(buf, n);
    CloseHandle(h);
    return out;
}

// Plant a file, for the half-written scratch file the cache must never publish.
static bool writeRaw(const std::string& path, const std::string& content) {
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD n = 0;
    BOOL ok = WriteFile(h, content.data(), (DWORD)content.size(), &n, nullptr);
    CloseHandle(h);
    return ok && n == (DWORD)content.size();
}

// The file names in the scratch directory. Narrowed the crude way on purpose:
// every name this suite creates is ASCII, and a byte over 0x7F would be a
// finding in itself, so it is mapped to '?' and shows up in the message.
static std::vector<std::string> dirEntries(const std::string& dir) {
    std::vector<std::string> names;
    WIN32_FIND_DATAW fd = {};
    HANDLE h = FindFirstFileW(wide(dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return names;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string name;
        for (const wchar_t* p = fd.cFileName; *p; ++p) {
            name += (*p >= 32 && *p < 127) ? (char)*p : '?';
        }
        names.push_back(name);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return names;
}

// %TEMP%\z80cpmw_help_cache_<pid>. The process id is in the name so two agents
// running this suite at once do not share a cache directory - which would make
// the "the directory gained no files" checks below read each other's writes.
static std::string scratchDir() {
    static std::string cached;
    if (!cached.empty()) return cached;

    wchar_t tmp[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, tmp);
    if (n == 0 || n >= MAX_PATH) return std::string();

    std::wstring w(tmp, n);
    if (!w.empty() && w.back() != L'\\') w += L'\\';
    wchar_t leaf[64];
    swprintf(leaf, 64, L"z80cpmw_help_cache_%lu", (unsigned long)GetCurrentProcessId());
    w += leaf;

    int need = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                   nullptr, 0, nullptr, nullptr);
    if (need <= 0) return std::string();
    std::string out((size_t)need, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &out[0], need, nullptr, nullptr);
    cached = out;
    return cached;
}

// The directory the scratch directory sits in, which is where "../evil.md"
// would land if isSafeAssetName ever stopped refusing it.
static std::string scratchParent() {
    std::string dir = scratchDir();
    size_t sep = dir.find_last_of('\\');
    return sep == std::string::npos ? dir : dir.substr(0, sep);
}

// Recursive, because the "a missing directory is created" check below leaves
// two nested levels behind.
static void removeTree(const std::wstring& dir) {
    if (dir.empty()) return;
    WIN32_FIND_DATAW fd = {};
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                removeTree(dir + L"\\" + name);
            } else {
                DeleteFileW((dir + L"\\" + name).c_str());
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

static void removeScratch() {
    removeTree(wide(scratchDir()));
}

static bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static void test_cache_paths() {
    section("cache: paths");

    // The default. The shipping app does not reach it - MainWindow::onCreate
    // calls setCacheRoot first - so this suite is what keeps it honest.
    // Needs LOCALAPPDATA, which every interactive Windows session has; with it
    // unset the default is empty by design and these two checks are the ones
    // that would say so.
    help_assets::setCacheRoot("");
    std::string dflt = help_assets::cacheDir();
    checkTrue(!dflt.empty(), "an unconfigured cache still names a directory");
    checkTrue(endsWith(dflt, "\\z80cpmw\\help"),
              "and it is where getUserDataDirectory() + \"\\help\" would be: " + dflt);

    const std::string dir = scratchDir();
    checkTrue(!dir.empty(), "the suite can name a scratch directory");

    help_assets::setCacheRoot(dir);
    checkStr(help_assets::cacheDir(), dir, "setCacheRoot moves the cache");
    checkStr(help_assets::cachePath("help_qpm.md"), dir + "\\help_qpm.md",
             "a cache path is the directory and the asset name");

    // isSafeAssetName is called by cachePath itself, so there is no way to get
    // a path out of this file without it having said yes.
    checkTrue(help_assets::cachePath("../evil.md").empty(),
              "no path at all for ../evil.md");
    checkTrue(help_assets::cachePath("..\\evil.md").empty(),
              "none for ..\\evil.md either");
    checkTrue(help_assets::cachePath("NUL.md").empty(), "none for a device name");
    checkTrue(help_assets::cachePath("").empty(), "none for an empty name");
    checkTrue(help_assets::cacheTempPath("../evil.md").empty(),
              "and none from cacheTempPath, which applies the same test");

    // The scratch name must differ from the real one, or the rename is not a
    // rename and a half-written file is visible under the name a reader reads.
    std::string temp = help_assets::cacheTempPath("help_qpm.md");
    checkTrue(!temp.empty(), "a safe name does get a scratch path");
    checkTrue(temp != help_assets::cachePath("help_qpm.md"),
              "the scratch path is not the real path");
    checkTrue(endsWith(temp, ".tmp"), "and is marked as scratch: " + temp);
    checkTrue(temp.rfind(dir + "\\", 0) == 0,
              "it sits in the cache directory, so the rename stays on one volume");
}

static void test_cache_creates_its_directory() {
    section("cache: the directory is created on demand");

    // The first write on a new machine happens into a directory nobody has
    // made: %LOCALAPPDATA%\z80cpmw exists but its help subdirectory does not.
    // Two levels deep here rather than one, because writeCached's helper walks
    // up only when Windows says a component ABOVE the leaf is missing, and one
    // level never exercises that walk.
    const std::string deep = scratchDir() + "\\made\\on\\demand";
    help_assets::setCacheRoot(deep);
    checkStr(help_assets::cacheDir(), deep, "the cache is pointed at a path that does not exist");
    checkTrue(GetFileAttributesW(wide(deep).c_str()) == INVALID_FILE_ATTRIBUTES,
              "and it really does not exist yet");

    checkTrue(help_assets::writeCached("help_qpm.md", "# QPM\n"),
              "writeCached makes the missing directories");
    checkTrue(fileExists(deep + "\\help_qpm.md"), "and the file lands in the deepest one");
    std::string back;
    checkTrue(help_assets::readCached("help_qpm.md", back), "and reads back from there");
    checkStr(back, "# QPM\n", "with its content");

    // readCached creates nothing: a miss must not leave a directory behind.
    const std::string never = scratchDir() + "\\never\\made";
    help_assets::setCacheRoot(never);
    std::string nothing;
    checkFalse(help_assets::readCached("help_qpm.md", nothing), "a read from a missing directory misses");
    checkTrue(GetFileAttributesW(wide(never).c_str()) == INVALID_FILE_ATTRIBUTES,
              "and did not create the directory it looked in");

    help_assets::setCacheRoot(scratchDir());
}

static void test_cache_round_trip() {
    section("cache: round trip");

    help_assets::setCacheRoot(scratchDir());
    const std::string name = "help_qpm.md";

    // Bytes, not text. A CRLF, a bare LF, the UTF-8 em dash that really is in
    // help_file_transfer.md, and an embedded NUL: what comes back must be what
    // went in, because markdownToText is fed the cached copy and the downloaded
    // copy alike and must not be able to tell them apart.
    std::string content = "# QPM\r\nline\nem dash \xE2\x80\x94 here\n";
    content.push_back('\0');
    content += "after the NUL\n";

    checkTrue(help_assets::writeCached(name, content), "writeCached stores a topic");
    checkTrue(fileExists(help_assets::cachePath(name)),
              "the file is on disk under its real name");
    checkTrue(readRaw(help_assets::cachePath(name)) == content,
              "and holds the bytes it was given, with no line-ending translation");

    std::string back = "left over from before";
    checkTrue(help_assets::readCached(name, back), "readCached finds it");
    checkInt((long)back.size(), (long)content.size(), "the same number of bytes");
    checkTrue(back == content, "byte for byte, NUL and em dash included");

    std::string missing = "left over from before";
    checkFalse(help_assets::readCached("help_zpm3.md", missing),
               "a topic never written is a miss");
    checkTrue(missing.empty(),
              "and the caller's string is cleared rather than left holding the last topic");

    checkTrue(help_assets::writeCached(name, "second"), "a second write replaces the first");
    back.clear();
    checkTrue(help_assets::readCached(name, back), "readCached answers after the replace");
    checkStr(back, "second", "with the new copy");

    // An HTTP 200 with an empty body reaches writeCached as an empty string.
    // Storing it would replace the reader's only offline copy with a file
    // readCached then reports as absent.
    checkFalse(help_assets::writeCached(name, ""), "an empty topic is refused");
    back.clear();
    checkTrue(help_assets::readCached(name, back), "and the copy that was there survives");
    checkStr(back, "second", "unchanged");

    // The largest published asset is 5,468 bytes (help_cpm22.md). Something far
    // larger still has to survive the write and the read.
    std::string big(300000, 'x');
    checkTrue(help_assets::writeCached("help_cpm22.md", big), "a 300 KB topic is stored");
    back.clear();
    checkTrue(help_assets::readCached("help_cpm22.md", back), "and found again");
    checkInt((long)back.size(), 300000, "at its full length");
    checkTrue(back == big, "with its content intact");
}

static void test_cache_refuses_unsafe_names() {
    section("cache: an unsafe name reaches no file");

    help_assets::setCacheRoot(scratchDir());

    // Where "../evil.md" would land if the whitelist ever stopped refusing it.
    const std::string escapee = scratchParent() + "\\evil.md";
    DeleteFileW(wide(escapee).c_str());
    checkFalse(fileExists(escapee), "nothing at the escape target to begin with");

    std::vector<std::string> before = dirEntries(scratchDir());

    checkFalse(help_assets::writeCached("../evil.md", "x"), "writeCached refuses ../evil.md");
    checkFalse(fileExists(escapee), "and no file appeared above the cache directory");
    checkFalse(help_assets::writeCached("..\\evil.md", "x"), "refuses ..\\evil.md");
    checkFalse(fileExists(escapee), "still nothing above the cache directory");
    checkFalse(help_assets::writeCached("C:\\Windows\\z80cpmw_help.md", "x"),
               "refuses an absolute path");
    checkFalse(fileExists("C:\\Windows\\z80cpmw_help.md"),
               "and wrote nothing where it pointed");

    // The reason the device names are on isSafeAssetName's list at all: a write
    // to NUL.md succeeds and stores nothing, so without the refusal the cache
    // would report a topic saved that can never be read back.
    checkFalse(help_assets::writeCached("NUL.md", "x"), "refuses NUL.md");
    std::string junk = "left over from before";
    checkFalse(help_assets::readCached("NUL.md", junk), "and will not read it back either");

    checkFalse(help_assets::writeCached("help .md", "x"), "refuses a name with a space");
    checkFalse(help_assets::writeCached(".hidden.md", "x"), "refuses a leading dot");

    std::vector<std::string> after = dirEntries(scratchDir());
    checkInt((long)after.size(), (long)before.size(),
             "and the cache directory gained no files at all");
}

static void test_cache_resolve_order() {
    section("cache: download, then cache, then the binary");

    help_assets::setCacheRoot(scratchDir());
    const std::string name = "help_zsdos.md";
    const std::string BUNDLED  = "# ZSDOS\n\nthe copy compiled into the binary\n";
    const std::string DOWNLOAD = "# ZSDOS\n\nthe copy fetched from the network\n";

    DeleteFileW(wide(help_assets::cachePath(name)).c_str());
    checkFalse(fileExists(help_assets::cachePath(name)), "no cached copy to start with");

    // Step three on its own.
    help_assets::ResolvedTopic r = help_assets::resolveTopic(name, "", BUNDLED);
    checkTrue(r.source == help_assets::TopicSource::Bundled,
              "with no download and no cache, the bundled copy");
    checkStr(r.content, BUNDLED, "and its text");
    checkTrue(r.savedWhen.empty(), "a bundled copy carries no saved-at time");
    checkStr(help_assets::sourceLabel(r.source), "bundled with the app",
             "which the status line calls what it is");
    checkFalse(fileExists(help_assets::cachePath(name)),
               "and nothing was cached - the cache holds what was downloaded, "
               "not what the binary already has");

    // Step one, which also writes step two.
    r = help_assets::resolveTopic(name, DOWNLOAD, BUNDLED);
    checkTrue(r.source == help_assets::TopicSource::Downloaded,
              "a download beats both of the others");
    checkStr(r.content, DOWNLOAD, "and is what the reader is shown");
    checkStr(help_assets::sourceLabel(r.source), "downloaded", "labelled as fresh");
    checkTrue(fileExists(help_assets::cachePath(name)),
              "and it was cached on the way past");
    checkStr(readRaw(help_assets::cachePath(name)), DOWNLOAD,
             "with the downloaded bytes, not the bundled ones");

    // Step two: this is the whole point of the commit. Offline, with a bundled
    // copy available, the reader still gets the topic they actually read.
    r = help_assets::resolveTopic(name, "", BUNDLED);
    checkTrue(r.source == help_assets::TopicSource::Cached,
              "with the download gone, the cache");
    checkStr(r.content, DOWNLOAD, "and it is the DOWNLOADED text, not the bundled text");
    checkStr(help_assets::sourceLabel(r.source), "offline copy", "labelled as offline");
    checkInt((long)r.savedWhen.size(), 16,
             "a cached copy says when it was saved: \"" + r.savedWhen + "\"");
    checkTrue(r.savedWhen.compare(0, 2, "20") == 0,
              "as a local YYYY-MM-DD HH:MM: \"" + r.savedWhen + "\"");

    // And back to step three once the file is gone.
    DeleteFileW(wide(help_assets::cachePath(name)).c_str());
    r = help_assets::resolveTopic(name, "", BUNDLED);
    checkTrue(r.source == help_assets::TopicSource::Bundled,
              "the bundled copy again after the cache file is deleted");
    checkStr(r.content, BUNDLED, "and its text again");

    // What all seven remote topics do today: no bundled copy exists, so the
    // chain really ends at the cache.
    r = help_assets::resolveTopic(name, "", "");
    checkTrue(r.source == help_assets::TopicSource::None,
              "no download, no cache and no bundled copy is None");
    checkTrue(r.content.empty(), "with no content for the pane");
    checkStr(help_assets::sourceLabel(r.source), "unavailable", "and a label saying so");

    // An empty download is an absent one, not a topic. This is the HTTP 200
    // with an empty body again, seen from the resolve side.
    r = help_assets::resolveTopic(name, "", BUNDLED);
    checkTrue(r.source == help_assets::TopicSource::Bundled,
              "an empty download does not count as a download");

    // A name the whitelist refuses must not cost the reader the text that was
    // downloaded - it costs them only the offline copy.
    r = help_assets::resolveTopic("../evil.md", DOWNLOAD, "");
    checkTrue(r.source == help_assets::TopicSource::Downloaded,
              "an unsafe name still shows what was downloaded");
    checkStr(r.content, DOWNLOAD, "in full");
    checkFalse(fileExists(scratchParent() + "\\evil.md"),
               "but nothing was written above the cache directory");
    r = help_assets::resolveTopic("../evil.md", "", BUNDLED);
    checkTrue(r.source == help_assets::TopicSource::Bundled,
              "and with no download it falls straight past the cache it cannot name");
}

static void test_cache_half_written_file() {
    section("cache: a half-written file is never read whole");

    help_assets::setCacheRoot(scratchDir());
    const std::string name = "help_nzcom.md";
    const std::string GOOD = "# NZCOM\n\nthe whole topic\n";
    const std::string NEXT = "# NZCOM\n\nthe next whole topic\n";

    checkTrue(help_assets::writeCached(name, GOOD), "a good copy is cached");

    // What a killed writer, a full disk or a power loss leaves behind: a
    // scratch file holding part of a topic. It is not under the real name, so
    // no reader can reach it - that is what the rename buys.
    // Deliberately LONGER than what the next write stores. A write that opened
    // the scratch file without truncating it would then leave the tail of this
    // one behind the new topic, which is exactly what the "no leftovers" check
    // below is looking for; with a shorter plant that mutation survives.
    const std::string temp = help_assets::cacheTempPath(name);
    checkTrue(writeRaw(temp, "# NZCOM\n\nthe first half of a topic whose writer "
                             "was killed before it could be renamed into place\n"),
              "a half-written scratch file is planted");
    checkTrue(fileExists(temp), "and it is really there");

    std::string back;
    checkTrue(help_assets::readCached(name, back), "readCached still answers");
    checkStr(back, GOOD, "with the whole copy, not the half-written one");

    // CREATE_ALWAYS, so the stale scratch file is truncated rather than
    // appended to: without that the next write would publish the leftovers of
    // the last one followed by the new topic.
    checkTrue(help_assets::writeCached(name, NEXT),
              "the next write succeeds over the stale scratch file");
    back.clear();
    checkTrue(help_assets::readCached(name, back), "and the topic is readable");
    checkStr(back, NEXT, "as exactly the new content, with no leftovers in front of it");
    checkFalse(fileExists(temp), "and the scratch file is gone - the rename consumed it");

    int forTopic = 0;
    for (const auto& entry : dirEntries(scratchDir())) {
        if (entry.rfind(name, 0) == 0) forTopic++;
    }
    checkInt(forTopic, 1, "the topic leaves exactly one file in the cache directory");

    // The other shape a killed writer leaves, and the one that gets past the
    // rename: a file of the right name and no length. readCached calls it a
    // miss - a reader shown a blank pane cannot tell it from a topic that
    // loaded and said nothing - so the resolve falls through to the next step
    // rather than publishing a blank topic as this one.
    checkTrue(writeRaw(help_assets::cachePath(name), ""),
              "a zero-byte file is planted under the real name");
    checkTrue(fileExists(help_assets::cachePath(name)), "and it is really there");
    back = "left over from before";
    checkFalse(help_assets::readCached(name, back), "readCached calls an empty file a miss");
    checkTrue(back.empty(), "and clears the caller's string rather than leaving it stale");

    const std::string BUNDLED = "# NZCOM\n\nthe copy compiled into the binary\n";
    help_assets::ResolvedTopic r = help_assets::resolveTopic(name, "", BUNDLED);
    checkTrue(r.source == help_assets::TopicSource::Bundled,
              "and the resolve steps over it to the bundled copy");
    checkStr(r.content, BUNDLED, "showing the bundled text and not a blank pane");
}

//=============================================================================
// A download that did not all arrive
//
// HelpWindow::downloadToString used to end its read loop with an unconditional
// "success = true", so a body cut off part-way was handed back as a document.
// With the cache in place that stopped being a one-session annoyance: fetchTopic
// passes a successful download to resolveTopic, which writes it over the
// complete offline copy and labels the pane "(downloaded)". The fragment became
// the durable copy of the topic.
//
// downloadToString itself needs a live WinHTTP session and cannot be called
// from here. help_assets::downloadIsComplete is the seam it was split at: the
// RULE is covered below, the WIRING - that downloadToString queries the real
// Content-Length, counts the real bytes and passes a real error code - is
// argued in the comment on the read loop and not tested here.
//
// The numbers below are the measured ones. help_cpm22.md is 5147 bytes from
// the release URL HelpWindow fetches; 5468/2000 and the chunked 500 with
// WinHTTP error 12152 are what a throwaway localhost server and a WinHTTP probe
// produced on 2026-08-28 - see HelpAssets.h for what each of those two shapes
// proved and why one check could not cover both.
//=============================================================================

static void test_download_completeness() {
    section("download: judging a body against what the response announced");

    std::string error = "left over from an earlier failure";
    checkTrue(help_assets::downloadIsComplete(5147, 5147, 0, error),
              "a body of exactly the announced length is complete");
    checkStr(error, "left over from an earlier failure",
             "and a complete body does not touch the caller's error string, so "
             "no stale reason can be printed beside it");

    error.clear();
    checkFalse(help_assets::downloadIsComplete(5468, 2000, 0, error),
               "2000 bytes of an announced 5468 is not a document - the shape "
               "WinHTTP reports as a clean end of body");
    checkTrue(contains(error, "2000") && contains(error, "5468"),
              "and the reason names both lengths: \"" + error + "\"");

    error.clear();
    checkFalse(help_assets::downloadIsComplete(5147, 5148, 0, error),
               "one byte MORE than announced is a mismatch too, not a pass");
    checkTrue(contains(error, "5148") && contains(error, "5147"),
              "with both lengths again: \"" + error + "\"");

    error.clear();
    checkFalse(help_assets::downloadIsComplete(5468, 0, 0, error),
               "and nothing at all against an announced 5468 is a failure, not "
               "an empty topic");

    // The chunked case: no Content-Length to compare, so the read error is the
    // only evidence. Refusing a response for having no length would take the
    // remote help system offline the day the asset host switched to chunked.
    error = "untouched";
    checkTrue(help_assets::downloadIsComplete(-1, 1000, 0, error),
              "no Content-Length and a clean read is complete, because a "
              "chunked response carries no length to check");
    checkStr(error, "untouched", "with the error string left alone");

    error.clear();
    checkFalse(help_assets::downloadIsComplete(-1, 500, 12152, error),
               "no Content-Length and a failed read is NOT complete - this is "
               "the truncated chunked body, ERROR_WINHTTP_INVALID_SERVER_RESPONSE");
    checkTrue(contains(error, "12152"),
              "and the WinHTTP code reaches the reader: \"" + error + "\"");

    error.clear();
    checkFalse(help_assets::downloadIsComplete(5468, 5468, 12152, error),
               "a failed read is a failed download even when the announced "
               "length was reached: the rule is absolute so it does not have to "
               "be re-argued whenever the read loop changes");

    // An empty 200 stays what it was: a complete response with nothing in it.
    // resolveTopic is the one that calls an empty download absent, and that is
    // what keeps an empty body from displacing the cached copy - see the
    // resolve-order section. Pinned here so the two do not both start
    // claiming it, or both stop.
    error = "untouched";
    checkTrue(help_assets::downloadIsComplete(0, 0, 0, error),
              "\"Content-Length: 0\" with an empty body is complete here");
    checkStr(error, "untouched", "and says nothing about it");
    error = "untouched";
    checkTrue(help_assets::downloadIsComplete(-1, 0, 0, error),
              "and so is an empty body with no announced length");
    checkStr(error, "untouched", "with nothing said about that either");
}

static void test_download_truncation_leaves_the_cache_alone() {
    section("download: a truncated body does not become the offline copy");

    help_assets::setCacheRoot(scratchDir());

    // Two topics, so the "what the defect did" half below cannot damage what
    // the "what happens now" half is asserting.
    const std::string FIXED  = "help_cpm3.md";
    const std::string BROKEN = "help_zcpr.md";
    const std::string WHOLE =
        "# CP/M Plus\n\nthe complete topic, read yesterday and cached\n";
    const std::string FRAGMENT = WHOLE.substr(0, 20);

    checkTrue(help_assets::writeCached(FIXED, WHOLE),
              "a complete copy of the topic is in the cache from an earlier read");

    // What downloadToString now does with a body that stopped short, and what
    // fetchTopic does with the answer: it passes resolveTopic an empty
    // download, exactly as it does for a connection that never opened. A FAILED
    // download and an EMPTY one take the same path there.
    std::string error;
    bool downloaded = help_assets::downloadIsComplete(
        (long long)WHOLE.size(), FRAGMENT.size(), 0, error);
    checkFalse(downloaded, "the short body is refused");

    help_assets::ResolvedTopic r = help_assets::resolveTopic(
        FIXED, downloaded ? FRAGMENT : std::string(), "");
    checkTrue(r.source == help_assets::TopicSource::Cached,
              "so the reader gets the cached copy");
    checkStr(r.content, WHOLE, "which is the WHOLE topic, not the fragment");
    checkStr(help_assets::sourceLabel(r.source), "offline copy",
             "and the status line does not call it a fresh download");
    std::string back;
    checkTrue(help_assets::readCached(FIXED, back), "the cache file is still there");
    checkStr(back, WHOLE, "still holding the whole topic");

    // And the shape being fixed, so it is clear WHERE the guard has to live.
    // resolveTopic cannot tell a fragment from a topic - both are non-empty
    // strings - so it caches the fragment and reports Downloaded. Nothing below
    // downloadToString can catch this; that is why the length check is up
    // there and not here.
    checkTrue(help_assets::writeCached(BROKEN, WHOLE),
              "the second topic starts with a complete copy cached too");
    help_assets::ResolvedTopic bad = help_assets::resolveTopic(BROKEN, FRAGMENT, "");
    checkTrue(bad.source == help_assets::TopicSource::Downloaded,
              "handed a fragment as a successful download, resolveTopic calls "
              "it downloaded");
    checkStr(help_assets::sourceLabel(bad.source), "downloaded",
             "and the status line would say so");
    back.clear();
    checkTrue(help_assets::readCached(BROKEN, back),
              "and it has written it to the cache");
    checkStr(back, FRAGMENT,
             "over the complete copy - the truncation outliving the session is "
             "the whole reason downloadToString has to refuse it");
}

//=============================================================================
// The seven remote topics compiled into the binary
//
// z80cpmw.rc turns each file of ..\ioscpm\release_assets into an RCDATA
// resource; help_assets::bundledIndexJson and bundledTopic read them back, and
// HelpWindow hands the latter to resolveTopic as its third step. The checks
// below are what stops that wiring being wrong in the two ways it can be
// without anything crashing: a topic the index names that no resource answers
// for - the reader gets the failure page offline, exactly as before bundling -
// and a resource nothing will ever ask for, which is bytes in every binary for
// nothing.
//
// THIS SECTION IS THE ONLY PART OF THE SUITE THAT NEEDS ..\ioscpm, and it is
// compiled only when tests\run_tests.bat found that checkout and linked the
// resources in; see the help block there. Everything above still runs on a
// machine holding this repository and nothing else, which is what lets the
// suite sit second, ahead of the blocks that exit /b 1 for a missing sibling.
//
// It sits here, below the cache sections, for one reason: check 7 runs the real
// resolveTopic, whose second step reads the cache. Up with the other bundled-
// topic sections it would consult whatever help the person running the suite
// happens to have cached in their own profile. scratchDir() is declared above,
// so down here it can point the cache at an empty directory instead.
//=============================================================================

#ifdef HELP_BUNDLED_ASSETS
// Where run_tests.bat's rc.exe invocation compiled the blobs FROM, relative to
// the repository root it cds to. Comparing against these files is the point of
// the section: a resource that is not byte-for-byte the file it was made from
// means the .rc, the resource compiler's include path or the id table in
// HelpAssets.cpp is pointing somewhere other than where it claims.
static const char* const kAssetDir = "..\\ioscpm\\release_assets";

static bool readWholeFile(const std::string& path, std::string& out) {
    out.clear();
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > (1 << 20)) {
        CloseHandle(h);
        return false;
    }

    out.resize((size_t)size.QuadPart);
    DWORD got = 0;
    bool ok = ReadFile(h, &out[0], (DWORD)out.size(), &got, nullptr) &&
              got == (DWORD)out.size();
    CloseHandle(h);
    if (!ok) out.clear();
    return ok;
}

// Every RT_RCDATA resource in this executable, by integer id. One given a STRING
// name is recorded as -1 rather than dropped, so it still shows up as an orphan:
// this list is compared against the eight ids resource.h defines, and "not an
// integer id at all" is not one of them.
static BOOL CALLBACK collectRcdataId(HMODULE, LPCWSTR, LPWSTR name, LONG_PTR param) {
    std::vector<long>* ids = reinterpret_cast<std::vector<long>*>(param);
    ids->push_back(IS_INTRESOURCE(name) ? (long)(ULONG_PTR)name : -1);
    return TRUE;
}

static std::vector<long> rcdataIds() {
    std::vector<long> ids;
    // MAKEINTRESOURCEW(10) rather than RT_RCDATA for the reason spelled out on
    // kResourceTypeRcData in HelpAssets.cpp: RT_RCDATA is the ANSI spelling in
    // a translation unit that does not define UNICODE, and this suite is one.
    //
    // Nothing is asserted about the return value. FALSE with
    // ERROR_RESOURCE_TYPE_NOT_FOUND is what a module carrying no RCDATA at all
    // gives back, and it leaves ids empty - which is exactly the answer the
    // caller wants for that case, and one the count check below then fails on.
    EnumResourceNamesW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(10),
                       collectRcdataId, (LONG_PTR)&ids);
    std::sort(ids.begin(), ids.end());
    return ids;
}

static std::string joined(const std::vector<std::string>& v) {
    std::string s;
    for (const auto& item : v) {
        if (!s.empty()) s += ", ";
        s += item;
    }
    return s.empty() ? "(none)" : s;
}
#endif  // HELP_BUNDLED_ASSETS

static void test_bundled_remote_assets() {
    section("bundled assets: the compiled-in copies of the seven remote topics");

#ifndef HELP_BUNDLED_ASSETS
    printf("  SKIP: built without ..\\ioscpm\\release_assets, so this binary "
           "carries no help resources to check\n");
#else
    // 1. The index is there, and is the file it was compiled from.
    const std::string index = help_assets::bundledIndexJson();
    checkTrue(!index.empty(), "the bundled help_index.json is not empty");

    std::string onDisk;
    checkTrue(readWholeFile(std::string(kAssetDir) + "\\help_index.json", onDisk),
              "and the file it was compiled from can be read");
    check(index == onDisk, "the bundled index is byte-identical to that file",
          std::to_string(index.size()) + " bytes",
          std::to_string(onDisk.size()) + " bytes");

    // 2. It parses to exactly seven topics, through the same parser fetchIndex
    //    runs on the downloaded copy - so the fallback list and the online list
    //    are built by the same code and cannot differ in shape.
    std::vector<help_assets::HelpTopic> topics;
    std::string error;
    checkTrue(help_assets::parseIndexJson(index, topics, error),
              "the bundled index parses: " + error);
    checkInt((long)topics.size(), 7, "and yields exactly seven topics");

    // 3. Every topic the index names resolves to a resource holding the file's
    //    bytes. This is the direction that decides whether an offline reader
    //    gets the topic at all.
    std::vector<std::string> named;
    for (const auto& topic : topics) {
        named.push_back(topic.filename);

        checkTrue(help_assets::isSafeAssetName(topic.filename),
                  topic.filename + ": the index names it safely");

        std::string blob;
        bool got = help_assets::bundledTopic(topic.filename, blob);
        checkTrue(got, topic.filename + ": a bundled copy exists");
        if (!got) continue;

        std::string file;
        checkTrue(readWholeFile(std::string(kAssetDir) + "\\" + topic.filename, file),
                  topic.filename + ": the file it was compiled from can be read");
        check(blob == file, topic.filename + ": the blob is byte-identical to it",
              std::to_string(blob.size()) + " bytes",
              std::to_string(file.size()) + " bytes");
    }

    // 4. And the other direction. A one-way check passes just as happily on a
    //    build that bundles a file nobody will ever ask for, so the id table is
    //    held to the index's names AND to its order - the order is what a
    //    renumbering breaks while every individual lookup still succeeds.
    std::vector<std::string> table = help_assets::bundledTopicNames();
    checkInt((long)table.size(), 7, "the name-to-id table holds seven names");
    check(table == named, "and holds exactly the index's names, in its order",
          joined(table), joined(named));

    // 5. No orphan resource: eight RCDATA ids, and exactly the eight resource.h
    //    defines. A ninth is bytes in every binary that nothing can reach.
    std::vector<long> want = {
        IDR_HELP_INDEX, IDR_HELP_QUICK_START, IDR_HELP_CPM22, IDR_HELP_ZSDOS,
        IDR_HELP_NZCOM, IDR_HELP_ZPM3, IDR_HELP_QPM, IDR_HELP_FILE_TRANSFER,
    };
    std::sort(want.begin(), want.end());
    std::vector<long> have = rcdataIds();
    checkInt((long)have.size(), 8, "the module carries eight RCDATA resources");
    checkTrue(have == want, "and they are exactly the eight resource.h defines");

    // 6. The wording this todo item was held open for. Compiling text written
    //    for iOS into a Windows binary would have made the wrong wording
    //    durable and offline, so the strings that named the problem are
    //    asserted absent and the Windows wording that replaced them asserted
    //    present. If avwohl/ioscpm ever goes back, this fails here instead of
    //    shipping.
    //
    //    The three strings were measured against the assets attached to
    //    https://github.com/avwohl/ioscpm/releases/latest/download/ on
    //    2026-08-28, which at that date still carried the pre-fix text:
    //    "tap the **gear icon** (Settings)" and "Return to main screen and tap
    //    **Play**" in help_cpm22.md, "Files app -> iOSCPM -> Imports" and a
    //    com.awohl.iOSCPM container path in help_file_transfer.md, "This guide
    //    covers using CP/M 2.2 in the Z80CPM emulator on iOS and macOS" at the
    //    top of help_cpm22.md, and "Getting started with iOSCPM" as Quick
    //    Start's description in the index. "gear icon" and not "tap the gear
    //    icon" because the published line has the bold markers inside it.
    checkFalse(contains(index, "iOSCPM"),
               "the bundled index does not describe a topic as iOSCPM's");
    for (const auto& name : named) {
        std::string blob;
        if (!help_assets::bundledTopic(name, blob)) continue;
        checkFalse(contains(blob, "gear icon"), name + ": no \"gear icon\"");
        checkFalse(contains(blob, "iOSCPM"), name + ": no \"iOSCPM\"");
        checkFalse(contains(blob, "on iOS and macOS"),
                   name + ": no \"on iOS and macOS\"");
    }

    // And the positive half, on the three topics that carry platform-specific
    // instructions at all. The other four - the ZSDOS, NZCOM, ZPM3 and QPM
    // guides - describe an operating system and name no platform, so requiring
    // "Windows" of them would be requiring a sentence that has no business
    // being there. Measured the same day: those three named Windows zero times
    // in the released assets and 3, 1 and 5 times in the checkout.
    const char* platformTopics[] = {
        "help_quick_start.md", "help_cpm22.md", "help_file_transfer.md",
    };
    for (const char* name : platformTopics) {
        std::string blob;
        if (!help_assets::bundledTopic(name, blob)) continue;
        checkTrue(contains(blob, "Windows"),
                  std::string(name) + ": names Windows, not only the Apple ports");
    }

    std::string transfer;
    if (help_assets::bundledTopic("help_file_transfer.md", transfer)) {
        checkTrue(contains(transfer, "### Windows"),
                  "the File Transfer topic has a Windows section of its own");
        checkTrue(contains(transfer, "%LOCALAPPDATA%\\z80cpmw\\data"),
                  "and names this port's data folder in it");
    }

    // 7. The resolve order with a bundled copy in hand, which is what the
    //    wiring is for: the same three arguments fetchTopic builds, with no
    //    download and nothing cached, so the third step is the one that
    //    answers. The cache is pointed at an empty scratch directory first -
    //    on the default root this would read whatever the person running the
    //    suite has cached under their own profile and could resolve to Cached.
    help_assets::setCacheRoot(scratchDir() + "\\bundled");
    std::string blob;
    if (help_assets::bundledTopic("help_qpm.md", blob)) {
        help_assets::ResolvedTopic r =
            help_assets::resolveTopic("help_qpm.md", std::string(), blob);
        checkTrue(r.source == help_assets::TopicSource::Bundled,
                  "a topic with no download and no cache file resolves to Bundled");
        checkStr(help_assets::sourceLabel(r.source), "bundled with the app",
                 "and the status line says so");
        checkStr(r.content, blob, "with the compiled-in bytes as its content");
    }
    // Put the root back so nothing after this reads a directory this section
    // chose. removeScratch() below takes the tree away either way - it is keyed
    // on scratchDir(), not on the cache root.
    help_assets::setCacheRoot("");
#endif  // HELP_BUNDLED_ASSETS
}

//=============================================================================

int main() {
    printf("Help renderer and asset suite\n");
    printf("=============================\n");

    test_bundled_file_transfer();
    test_bundled_render_invariants();
    test_bundled_configuration_matches_docs();
    test_render_headers();
    test_render_bullets();
    test_render_inline();
    test_render_tables();
    test_render_fences();
    test_safe_asset_name();
    test_to_wide();
    test_parse_index();
    test_cache_paths();
    test_cache_creates_its_directory();
    test_cache_round_trip();
    test_cache_refuses_unsafe_names();
    test_cache_resolve_order();
    test_cache_half_written_file();
    test_download_completeness();
    test_download_truncation_leaves_the_cache_alone();

    // Last, because it is the only one that can be compiled out and because it
    // borrows the scratch root the cache sections established.
    test_bundled_remote_assets();

    // Everything the cache sections wrote lives under %TEMP%; take it away
    // whether they passed or failed, so a failing run does not leave the next
    // one reading its litter.
    removeScratch();

    printf("\n=============================\n");
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
