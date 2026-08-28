/*
 * test_help.cpp - Help renderer, index parser and asset-name suite.
 *
 * todo.txt: "the seven remote help topics have no bundled copy and no on-disk
 * cache." This is the step before that one. Bundling renders markdown that has
 * never been through markdownToText, and a cache turns a filename that arrived
 * over the network into a path on this machine, so both halves of the job are
 * built out of z80cpmw\HelpAssets.cpp - and every defect the renderer already
 * had would have been baked into a binary and taken offline with it. They are
 * fixed and pinned here first.
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
 * Those assets are NOT read here. This suite needs no sibling checkout and no
 * window station, which is what lets it sit second in tests\run_tests.bat: the
 * host-file and HBIOS blocks below it exit /b 1 when ..\romwbw_emu or ..\cpmemu
 * are missing, so a block appended after them is unreachable on a machine that
 * has only this repository. The index JSON below is a copy of the published one
 * rather than a read of it, for the same reason.
 *
 * Build and run: tests\run_tests.bat
 */

#include "HelpAssets.h"
#include "HelpWindow.h"

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

int main() {
    printf("Help renderer and asset suite\n");
    printf("=============================\n");

    test_bundled_file_transfer();
    test_bundled_render_invariants();
    test_render_headers();
    test_render_bullets();
    test_render_inline();
    test_render_tables();
    test_render_fences();
    test_safe_asset_name();
    test_to_wide();
    test_parse_index();

    printf("\n=============================\n");
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
