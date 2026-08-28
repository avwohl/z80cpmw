/*
 * test_vt52.cpp - Headless terminal conformance suite for TerminalView.
 *
 * CHANGELOG [1.0.20] announced a suite of 73 checks and said, in the same
 * bullet, that it was not committed to this repository. It never was: the only
 * trace it left was the reference to this filename in TerminalView.h, and
 * todo.txt asked for the gap to be closed because the suite "is the only thing
 * that would catch a regression in any of this". This file is that suite,
 * rebuilt from the [1.0.20] feature list and from the parser as it stands.
 *
 * It needs no window. TerminalView::create() is never called, so m_hwnd stays
 * null and every paint path short-circuits; the parser and the screen buffer do
 * not care. The suite therefore drives the class the way a guest does - bytes
 * in through outputChar() - and reads back only through the public interface:
 *
 *   - screen content through cellAt(), and
 *   - the cursor through ESC [ 6 n, which puts the answerback path under test
 *     rather than assuming it works.
 *
 * Build and run: tests\run_tests.bat
 */

#include "TerminalView.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

// Render control bytes readably so a failure message is legible.
static std::string escape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == 0x1B) {
            out += "\\E";
        } else if (c >= 0x20 && c < 0x7F) {
            out += (char)c;
        } else {
            char buf[8];
            snprintf(buf, sizeof buf, "\\%03o", c);
            out += buf;
        }
    }
    return out;
}

// Report one check. A function rather than a macro body so the failure message
// can carry both values; the macros below only supply the description.
static void check(bool ok, const char* what, const std::string& got,
                  const std::string& want) {
    g_checks++;
    if (ok) return;
    g_failed++;
    printf("  FAIL [%s] %s\n        got:  %s\n        want: %s\n",
           g_section, what, got.c_str(), want.c_str());
}

#define CHECK_INT(actual, expected, what) \
    check((actual) == (expected), (what), std::to_string(actual), \
          std::to_string(expected))

#define CHECK_STR(actual, expected, what) \
    check((actual) == std::string(expected), (what), \
          "\"" + escape(actual) + "\"", "\"" + escape(expected) + "\"")

#define CHECK_TRUE(cond, what) check((cond), (what), "false", "true")

//=============================================================================
// Terminal fixture
//=============================================================================

// A TerminalView plus the guest end of its answerback channel. Everything the
// terminal sends back - ESC[6n replies, device attributes - lands in `replies`.
struct Term {
    TerminalView tv;
    std::string replies;
    int bells = 0;

    Term() {
        tv.setKeyInputCallback([this](char ch) { replies += ch; });
        // Count bells instead of ringing them. This is not only how test_bell()
        // asks its questions: without it the suite is audible, because
        // test_control_chars()'s "BEL does not move the cursor" sends a real
        // 0x07 and reached MessageBeep() on every run.
        tv.setBellHook([this]() { bells++; });
    }

    void send(const std::string& s) {
        for (unsigned char c : s) tv.outputChar(c);
    }

    // Ask the terminal where the cursor is. This is the suite's only cursor
    // read, which is deliberate: a broken ESC[6n fails loudly in its own
    // section instead of quietly weakening every other check.
    void cursor(int& row, int& col) {
        replies.clear();
        send("\033[6n");
        row = col = -1;
        // Expect ESC [ <row> ; <col> R
        if (replies.size() < 6 || replies[0] != '\033' || replies[1] != '[') return;
        if (replies[replies.size() - 1] != 'R') return;
        size_t semi = replies.find(';');
        if (semi == std::string::npos) return;
        row = atoi(replies.substr(2, semi - 2).c_str());
        col = atoi(replies.substr(semi + 1, replies.size() - semi - 2).c_str());
    }

    int row() { int r, c; cursor(r, c); return r; }
    int col() { int r, c; cursor(r, c); return c; }

    char at(int r, int c) { return tv.cellAt(r, c).character; }
    uint8_t fg(int r, int c) { return tv.cellAt(r, c).foreground; }
    uint8_t bg(int r, int c) { return tv.cellAt(r, c).background; }
    uint8_t flags(int r, int c) { return tv.cellAt(r, c).flags; }

    // A whole row as text, with trailing blanks trimmed.
    std::string line(int r) {
        std::string s;
        for (int c = 0; c < TerminalView::COLS; c++) s += tv.cellAt(r, c).character;
        while (!s.empty() && s[s.size() - 1] == ' ') s.erase(s.size() - 1);
        return s;
    }

    // Put the cursor somewhere, in the 1-based coordinates CSI uses.
    void home(int r = 1, int c = 1) {
        send("\033[" + std::to_string(r) + ";" + std::to_string(c) + "H");
    }
};

//=============================================================================
// 1. Control characters
//=============================================================================

static void test_control_chars() {
    section("control characters");

    { Term t; t.send("AB\bC");
      CHECK_STR(t.line(0), "AC", "BS moves back one column"); }

    { Term t; t.send("\b");
      CHECK_INT(t.col(), 1, "BS at column 1 clamps"); }

    { Term t; t.send("A\tB");
      CHECK_INT(t.col(), 10, "TAB advances to the next 8-column stop"); }

    { Term t; t.send("\t");
      CHECK_INT(t.col(), 9, "TAB from column 1 lands on column 9"); }

    { Term t; t.home(1, 79); t.send("\t");
      CHECK_INT(t.col(), 80, "TAB clamps at the last column"); }

    { Term t; t.send("AB\nC");
      CHECK_STR(t.line(1), "C", "LF carries an implicit CR");
      CHECK_INT(t.row(), 2, "LF moves down one row"); }

    { Term t; t.send("ABC\rD");
      CHECK_STR(t.line(0), "DBC", "CR returns to column 1"); }

    { Term t; t.home(1, 5); t.send("\007");
      CHECK_INT(t.col(), 5, "BEL does not move the cursor"); }

    { Term t; t.send("A\001\002B");
      CHECK_STR(t.line(0), "AB", "unhandled C0 bytes print nothing"); }

    { Term t; t.send("A\177B");
      CHECK_STR(t.line(0), "AB", "DEL (0x7F) is not printable"); }
}

//=============================================================================
// The bell (BEL, 0x07)
//
// todo.txt: "the terminal bell is unconditional: TerminalView.cpp's 0x07 case
// calls MessageBeep(MB_OK) with nothing to consult. cpmdroid made it a
// setting." It now consults setBellEnabled(), and every Term in this file
// installs a counting hook in place of MessageBeep() - which is what makes the
// suite silent as well as testable.
//=============================================================================

static void test_bell() {
    section("bell");

    { Term t;
      CHECK_TRUE(t.tv.isBellEnabled(), "the bell is on by default"); }

    { Term t; t.send("\007");
      CHECK_INT(t.bells, 1, "one BEL rings once"); }

    { Term t; t.send("\007\007\007");
      CHECK_INT(t.bells, 3, "three BELs ring three times"); }

    { Term t; t.tv.setBellEnabled(false); t.send("\007\007\007");
      CHECK_INT(t.bells, 0, "a disabled bell rings not at all");
      CHECK_TRUE(!t.tv.isBellEnabled(), "and reads back as disabled"); }

    { Term t; t.tv.setBellEnabled(false); t.send("\007");
      t.tv.setBellEnabled(true); t.send("\007");
      CHECK_INT(t.bells, 1, "switching the bell back on rings again"); }

    // Suppressing the bell suppresses the bell and nothing else: BEL has never
    // moved the cursor or printed anything, and must not start doing either.
    { Term t; t.tv.setBellEnabled(false); t.home(1, 5); t.send("\007");
      CHECK_INT(t.col(), 5, "a suppressed BEL leaves the cursor column alone");
      CHECK_INT(t.row(), 1, "a suppressed BEL leaves the cursor row alone");
      CHECK_STR(t.line(0), "", "a suppressed BEL prints nothing"); }

    // The bell setting is the user's, not the guest's. clear() is the machine
    // reset AND the ESC c path, so if it reset m_bellEnabled a program could
    // turn the bell back on for someone who had switched it off.
    { Term t; t.tv.setBellEnabled(false); t.tv.clear(); t.send("\007");
      CHECK_TRUE(!t.tv.isBellEnabled(), "clear() does not re-enable the bell");
      CHECK_INT(t.bells, 0, "and no bell rings after it"); }

    { Term t; t.tv.setBellEnabled(false); t.send("\033c\007");
      CHECK_TRUE(!t.tv.isBellEnabled(), "ESC c does not re-enable the bell");
      CHECK_INT(t.bells, 0, "and no bell rings after it either"); }

    // What a BEL arriving mid-CSI actually does, read off processCSIChar()
    // rather than assumed: 0x07 is none of the private markers, not an
    // intermediate byte (those are 0x20-0x2F), not a digit and not ';', so it
    // falls through to the final-character branch and ends the sequence.
    // executeCSI() has no case for it, so ESC[3 BEL is swallowed whole - no
    // bell, no output - and the parser is left in Normal state.
    { Term t; t.send("\033[3\007");
      CHECK_INT(t.bells, 0, "a BEL inside a CSI is eaten as the final byte, not rung");
      t.send("A");
      CHECK_STR(t.line(0), "A", "and the parser is back in Normal state after it");
      CHECK_INT(t.col(), 2, "with the '3' never applied as a movement"); }
}

//=============================================================================
// 2. Cursor movement - CSI A B C D H f G ` d, ESC 7 / ESC 8, CSI s / CSI u
//=============================================================================

static void test_cursor_movement() {
    section("cursor movement");

    { Term t; t.home(10, 10); t.send("\033[3A");
      CHECK_INT(t.row(), 7, "CUU moves up by the parameter"); }

    { Term t; t.home(10, 10); t.send("\033[A");
      CHECK_INT(t.row(), 9, "CUU with no parameter moves one"); }

    { Term t; t.home(1, 1); t.send("\033[5A");
      CHECK_INT(t.row(), 1, "CUU clamps at the top"); }

    { Term t; t.home(10, 10); t.send("\033[3B");
      CHECK_INT(t.row(), 13, "CUD moves down by the parameter"); }

    { Term t; t.home(25, 1); t.send("\033[5B");
      CHECK_INT(t.row(), 25, "CUD clamps at the bottom"); }

    { Term t; t.home(10, 10); t.send("\033[4C");
      CHECK_INT(t.col(), 14, "CUF moves right by the parameter"); }

    { Term t; t.home(1, 80); t.send("\033[5C");
      CHECK_INT(t.col(), 80, "CUF clamps at the right margin"); }

    { Term t; t.home(10, 10); t.send("\033[4D");
      CHECK_INT(t.col(), 6, "CUB moves left by the parameter"); }

    { Term t; t.home(1, 1); t.send("\033[5D");
      CHECK_INT(t.col(), 1, "CUB clamps at the left margin"); }

    { Term t; t.send("\033[7;13H"); int r, c; t.cursor(r, c);
      CHECK_INT(r, 7, "CUP sets the row (1-based)");
      CHECK_INT(c, 13, "CUP sets the column (1-based)"); }

    { Term t; t.home(5, 5); t.send("\033[H"); int r, c; t.cursor(r, c);
      CHECK_INT(r, 1, "bare CUP homes the row");
      CHECK_INT(c, 1, "bare CUP homes the column"); }

    { Term t; t.send("\033[9;9f"); int r, c; t.cursor(r, c);
      CHECK_INT(r, 9, "HVP (f) is an alias for CUP");
      CHECK_INT(c, 9, "HVP (f) sets the column too"); }

    { Term t; t.send("\033[99;99H"); int r, c; t.cursor(r, c);
      CHECK_INT(r, 25, "CUP clamps the row to the screen");
      CHECK_INT(c, 80, "CUP clamps the column to the screen"); }

    { Term t; t.home(5, 5); t.send("\033[20G");
      CHECK_INT(t.col(), 20, "CHA (G) sets the column absolutely");
      CHECK_INT(t.row(), 5, "CHA (G) leaves the row alone"); }

    { Term t; t.home(5, 5); t.send("\033[20`");
      CHECK_INT(t.col(), 20, "HPA (`) is an alias for CHA"); }

    { Term t; t.home(5, 5); t.send("\033[12d");
      CHECK_INT(t.row(), 12, "VPA (d) sets the row absolutely");
      CHECK_INT(t.col(), 5, "VPA (d) leaves the column alone"); }

    { Term t; t.home(4, 4); t.send("\0337"); t.home(20, 20); t.send("\0338");
      int r, c; t.cursor(r, c);
      CHECK_INT(r, 4, "ESC 7 / ESC 8 restore the saved row");
      CHECK_INT(c, 4, "ESC 7 / ESC 8 restore the saved column"); }

    { Term t; t.home(6, 6); t.send("\033[s"); t.home(20, 20); t.send("\033[u");
      int r, c; t.cursor(r, c);
      CHECK_INT(r, 6, "CSI s / CSI u restore the saved row");
      CHECK_INT(c, 6, "CSI s / CSI u restore the saved column"); }
}

//=============================================================================
// 3. Erase - CSI J, CSI K, CSI X
//=============================================================================

static void test_erase() {
    section("erase");

    { Term t; t.send("hello\033[2;1Hworld"); t.home(1, 1); t.send("\033[2J");
      CHECK_STR(t.line(0), "", "ED 2 clears the first row");
      CHECK_STR(t.line(1), "", "ED 2 clears the second row"); }

    { Term t; t.send("abcdef"); t.home(1, 4); t.send("\033[0J");
      CHECK_STR(t.line(0), "abc", "ED 0 erases from the cursor on"); }

    { Term t; t.send("abcdef"); t.home(1, 3); t.send("\033[1J");
      CHECK_STR(t.line(0), "   def", "ED 1 erases up to the cursor"); }

    { Term t; t.send("line1\nline2"); t.home(1, 1); t.send("\033[0J");
      CHECK_STR(t.line(1), "", "ED 0 also clears the rows below"); }

    { Term t; t.send("abcdef"); t.home(1, 4); t.send("\033[0K");
      CHECK_STR(t.line(0), "abc", "EL 0 erases to the end of the line"); }

    { Term t; t.send("abcdef"); t.home(1, 3); t.send("\033[1K");
      CHECK_STR(t.line(0), "   def", "EL 1 erases to the cursor"); }

    { Term t; t.send("abcdef"); t.home(1, 3); t.send("\033[2K");
      CHECK_STR(t.line(0), "", "EL 2 erases the whole line"); }

    { Term t; t.send("abcdef"); t.home(1, 2); t.send("\033[3X");
      CHECK_STR(t.line(0), "a   ef", "ECH (X) erases N characters in place"); }

    { Term t; t.send("abcdef"); t.home(1, 2); t.send("\033[X");
      CHECK_STR(t.line(0), "a cdef", "ECH with no parameter erases one"); }

    { Term t; t.send("abc"); t.home(1, 2); t.send("\033[999X");
      CHECK_STR(t.line(0), "a", "ECH clamps to the end of the line"); }
}

//=============================================================================
// 4. Insert and delete - CSI L M @ P
//=============================================================================

static void test_insert_delete() {
    section("insert and delete");

    { Term t; t.send("row1\nrow2\nrow3"); t.home(2, 1); t.send("\033[L");
      CHECK_STR(t.line(1), "", "IL opens a blank line at the cursor");
      CHECK_STR(t.line(2), "row2", "IL pushes the old line down"); }

    { Term t; t.send("row1\nrow2\nrow3"); t.home(2, 1); t.send("\033[2L");
      CHECK_STR(t.line(3), "row2", "IL N pushes down by N"); }

    { Term t; t.send("row1\nrow2\nrow3"); t.home(2, 1); t.send("\033[M");
      CHECK_STR(t.line(1), "row3", "DL closes the line at the cursor"); }

    { Term t; t.send("row1\nrow2\nrow3"); t.home(1, 1); t.send("\033[2M");
      CHECK_STR(t.line(0), "row3", "DL N closes N lines"); }

    { Term t; t.send("abcdef"); t.home(1, 3); t.send("\033[2@");
      CHECK_STR(t.line(0), "ab  cdef", "ICH opens N columns at the cursor"); }

    { Term t; t.send("abcdef"); t.home(1, 3); t.send("\033[2P");
      CHECK_STR(t.line(0), "abef", "DCH closes N columns at the cursor"); }

    { Term t; t.send("abcdef"); t.home(1, 1); t.send("\033[999P");
      CHECK_STR(t.line(0), "", "DCH clamps to the line width"); }

    { Term t; t.home(2, 5); t.send("\033[L");
      CHECK_INT(t.col(), 1, "IL returns the cursor to column 1"); }

    { Term t; t.home(2, 5); t.send("\033[M");
      CHECK_INT(t.col(), 1, "DL returns the cursor to column 1"); }
}

//=============================================================================
// 5. Scrolling region (DECSTBM), index and reverse index
//=============================================================================

static void test_scroll_region() {
    section("scrolling region");

    { Term t; t.home(10, 10); t.send("\033[5;15r"); int r, c; t.cursor(r, c);
      CHECK_INT(r, 1, "DECSTBM homes the cursor row");
      CHECK_INT(c, 1, "DECSTBM homes the cursor column"); }

    // Inside a three-line region: indexing off its bottom must move only those
    // rows, and must leave everything outside the region untouched.
    { Term t;
      t.send("\033[2;4r");
      t.home(2, 1); t.send("A");
      t.home(3, 1); t.send("B");
      t.home(4, 1); t.send("C");
      t.home(1, 1); t.send("TOP");
      t.home(5, 1); t.send("BOTTOM");
      t.home(4, 1); t.send("\n");
      CHECK_STR(t.line(1), "B", "LF at the region bottom scrolls the region");
      CHECK_STR(t.line(2), "C", "the region's rows move up by one");
      CHECK_STR(t.line(3), "", "a blank line enters at the region bottom");
      CHECK_STR(t.line(0), "TOP", "rows above the region are untouched");
      CHECK_STR(t.line(4), "BOTTOM", "rows below the region are untouched"); }

    { Term t;
      t.send("\033[2;4r");
      t.home(2, 1); t.send("A"); t.home(3, 1); t.send("B"); t.home(4, 1); t.send("C");
      t.home(4, 1); t.send("\033D");
      CHECK_STR(t.line(1), "B", "IND (ESC D) honours the region"); }

    { Term t;
      t.send("\033[2;4r");
      t.home(2, 1); t.send("A"); t.home(3, 1); t.send("B"); t.home(4, 1); t.send("C");
      t.home(2, 1); t.send("\033M");
      CHECK_STR(t.line(1), "", "RI at the region top opens a blank line");
      CHECK_STR(t.line(2), "A", "RI pushes the region down"); }

    { Term t; t.home(10, 1); t.send("\033M");
      CHECK_INT(t.row(), 9, "RI away from the region top just moves up"); }

    { Term t; t.send("\033[1;99r"); t.home(25, 1); t.send("\n");
      CHECK_INT(t.row(), 25, "DECSTBM clamps the bottom to the screen"); }

    { Term t;
      t.send("\033[5;5r");           // top == bottom: rejected, region unchanged
      t.home(25, 1); t.send("Z");
      CHECK_STR(t.line(24), "Z", "a degenerate region is rejected"); }

    { Term t;
      t.send("\033[2;4r");
      t.home(2, 1); t.send("A"); t.home(3, 1); t.send("B"); t.home(4, 1); t.send("C");
      t.send("\033[2S");
      CHECK_STR(t.line(1), "C", "SU (S) scrolls the region up by N"); }

    { Term t;
      t.send("\033[2;4r");
      t.home(2, 1); t.send("A"); t.home(3, 1); t.send("B"); t.home(4, 1); t.send("C");
      t.send("\033[T");
      CHECK_STR(t.line(2), "A", "SD (T) scrolls the region down by N"); }
}

//=============================================================================
// 6. Deferred autowrap and DECAWM
//=============================================================================

static void test_autowrap() {
    section("autowrap");

    { Term t; t.home(1, 80); t.send("X");
      CHECK_INT(t.col(), 80, "a glyph in the last column leaves the cursor there");
      CHECK_INT(t.row(), 1, "and does not move to the next row yet"); }

    { Term t; t.home(1, 80); t.send("XY"); int r, c; t.cursor(r, c);
      CHECK_INT(r, 2, "the next glyph takes the deferred wrap");
      CHECK_INT(c, 2, "landing in column 1, leaving the cursor on column 2");
      CHECK_STR(t.line(1), "Y", "the wrapped glyph is on the new row"); }

    { Term t; t.home(1, 80); t.send("X\r");
      CHECK_INT(t.row(), 1, "CR cancels an armed wrap"); }

    { Term t; t.home(1, 80); t.send("X\033[D");
      CHECK_INT(t.row(), 1, "a cursor move cancels an armed wrap"); }

    { Term t; t.send("\033[?7l"); t.home(1, 80); t.send("XY");
      CHECK_INT(t.row(), 1, "DECAWM off keeps the cursor on the row");
      CHECK_STR(t.line(1), "", "DECAWM off writes nothing to the next row"); }

    { Term t; t.send("\033[?7l"); t.home(1, 80); t.send("X"); t.send("\033[?7h");
      t.send("Y");
      CHECK_INT(t.row(), 1, "re-enabling DECAWM does not revive a dropped wrap"); }

    { Term t; t.send("\033[?7l\033[?7h"); t.home(1, 80); t.send("XY");
      CHECK_INT(t.row(), 2, "DECAWM back on wraps again"); }

    // The bottom-right corner used to scroll the screen the moment the glyph
    // landed, which corrupts every full-screen layout that draws into it.
    { Term t; t.home(1, 1); t.send("TOPROW"); t.home(25, 80); t.send("X");
      CHECK_STR(t.line(0), "TOPROW",
                "writing the bottom-right cell does not scroll the screen"); }
}

//=============================================================================
// 7. SGR
//=============================================================================

static void test_sgr() {
    section("SGR");

    { Term t; t.send("\033[31mA");
      CHECK_INT(t.fg(0, 0), 4, "SGR 31 sets the foreground"); }

    { Term t; t.send("\033[44mA");
      CHECK_INT(t.bg(0, 0), 1, "SGR 44 sets the background"); }

    // The whole colour table, one index at a time. An SGR parameter is an ANSI
    // colour index and the attribute byte is CGA-ordered; the two orderings
    // agree on black, green, magenta and white and disagree on the other four,
    // because red and blue trade places (bit 0 and bit 2 swap):
    //
    //   ANSI 0 black 1 RED  2 green 3 YELLOW 4 BLUE 5 magenta 6 CYAN 7 white
    //   CGA  0 black 1 BLUE 2 green 3 CYAN   4 RED  5 magenta 6 brown 7 lt grey
    //
    // Storing the parameter raw - which is what this did - drew red as blue,
    // yellow as cyan, blue as red and cyan as brown. These sixteen checks are
    // the whole mapping, so it cannot come back quietly.
    { Term t; t.send("\033[30mA");
      CHECK_INT(t.fg(0, 0), 0, "SGR 30 (ANSI black) is CGA 0 (black)"); }
    { Term t; t.send("\033[31mA");
      CHECK_INT(t.fg(0, 0), 4, "SGR 31 (ANSI red) is CGA 4 (red)"); }
    { Term t; t.send("\033[32mA");
      CHECK_INT(t.fg(0, 0), 2, "SGR 32 (ANSI green) is CGA 2 (green)"); }
    { Term t; t.send("\033[33mA");
      CHECK_INT(t.fg(0, 0), 6, "SGR 33 (ANSI yellow) is CGA 6 (brown)"); }
    { Term t; t.send("\033[34mA");
      CHECK_INT(t.fg(0, 0), 1, "SGR 34 (ANSI blue) is CGA 1 (blue)"); }
    { Term t; t.send("\033[35mA");
      CHECK_INT(t.fg(0, 0), 5, "SGR 35 (ANSI magenta) is CGA 5 (magenta)"); }
    { Term t; t.send("\033[36mA");
      CHECK_INT(t.fg(0, 0), 3, "SGR 36 (ANSI cyan) is CGA 3 (cyan)"); }
    { Term t; t.send("\033[37mA");
      CHECK_INT(t.fg(0, 0), 7, "SGR 37 (ANSI white) is CGA 7 (light grey)"); }

    { Term t; t.send("\033[40mA");
      CHECK_INT(t.bg(0, 0), 0, "SGR 40 (ANSI black) is CGA 0 (black)"); }
    { Term t; t.send("\033[41mA");
      CHECK_INT(t.bg(0, 0), 4, "SGR 41 (ANSI red) is CGA 4 (red)"); }
    { Term t; t.send("\033[42mA");
      CHECK_INT(t.bg(0, 0), 2, "SGR 42 (ANSI green) is CGA 2 (green)"); }
    { Term t; t.send("\033[43mA");
      CHECK_INT(t.bg(0, 0), 6, "SGR 43 (ANSI yellow) is CGA 6 (brown)"); }
    { Term t; t.send("\033[44mA");
      CHECK_INT(t.bg(0, 0), 1, "SGR 44 (ANSI blue) is CGA 1 (blue)"); }
    { Term t; t.send("\033[45mA");
      CHECK_INT(t.bg(0, 0), 5, "SGR 45 (ANSI magenta) is CGA 5 (magenta)"); }
    { Term t; t.send("\033[46mA");
      CHECK_INT(t.bg(0, 0), 3, "SGR 46 (ANSI cyan) is CGA 3 (cyan)"); }
    { Term t; t.send("\033[47mA");
      CHECK_INT(t.bg(0, 0), 7, "SGR 47 (ANSI white) is CGA 7 (light grey)"); }

    { Term t; t.send("\033[1mA");
      CHECK_INT(t.fg(0, 0), 0x0F, "SGR 1 sets the bold (intensity) bit"); }

    // The bright half, SGR 90-97 and 100-107. These were not handled at all
    // until now: they fell through applySGR()'s default and left the byte
    // alone, so ESC[91m from a fresh reset drew in CGA 7 - measured on the
    // painted pixels by tests/test_render.cpp, not inferred. Bright is the
    // same ANSI index with the intensity bit set, which is why each
    // expectation here is its 3x counterpart above plus 8.
    { Term t; t.send("\033[90mA");
      CHECK_INT(t.fg(0, 0), 8, "SGR 90 (bright black) is CGA 8 (dark grey)"); }
    { Term t; t.send("\033[91mA");
      CHECK_INT(t.fg(0, 0), 12, "SGR 91 (bright red) is CGA 12 (light red)"); }
    { Term t; t.send("\033[92mA");
      CHECK_INT(t.fg(0, 0), 10, "SGR 92 (bright green) is CGA 10 (light green)"); }
    { Term t; t.send("\033[93mA");
      CHECK_INT(t.fg(0, 0), 14, "SGR 93 (bright yellow) is CGA 14 (yellow)"); }
    { Term t; t.send("\033[94mA");
      CHECK_INT(t.fg(0, 0), 9, "SGR 94 (bright blue) is CGA 9 (light blue)"); }
    { Term t; t.send("\033[95mA");
      CHECK_INT(t.fg(0, 0), 13, "SGR 95 (bright magenta) is CGA 13 (light magenta)"); }
    { Term t; t.send("\033[96mA");
      CHECK_INT(t.fg(0, 0), 11, "SGR 96 (bright cyan) is CGA 11 (light cyan)"); }
    { Term t; t.send("\033[97mA");
      CHECK_INT(t.fg(0, 0), 15, "SGR 97 (bright white) is CGA 15 (white)"); }

    // A bright colour sets the same bit SGR 1 does, so SGR 22 must be able to
    // dim it. Preserving bit 3 across a 9x - which is what the 3x branch does,
    // and the easy way to write this - would make ESC[91m ESC[22m a no-op.
    { Term t; t.send("\033[91m\033[22mA");
      CHECK_INT(t.fg(0, 0), 4, "SGR 22 dims a bright colour to its plain twin"); }

    // ...and a plain colour after a bright one is NOT plain. The 3x branch
    // masks with 0xF8 on purpose, so that ESC[1;37m stays bright; the
    // intensity bit that 9x sets is the same bit, so 3x preserves that too.
    // The way back to plain red is ESC[22m or ESC[0m, not ESC[31m. Recorded
    // because it is a consequence of the earlier fix rather than a choice
    // made here, and the next person to touch either branch should see it.
    { Term t; t.send("\033[91m\033[31mA");
      CHECK_INT(t.fg(0, 0), 12, "a plain colour after a bright one keeps the intensity bit"); }

    { Term t; t.send("\033[91m\033[0m\033[31mA");
      CHECK_INT(t.fg(0, 0), 4, "and a reset between them gets plain red back"); }

    // The background nibble is three bits: bit 7 is blink on CGA hardware and
    // cgaToRGB() masks it off, so 100-107 fold onto the plain background
    // rather than borrowing it.
    { Term t; t.send("\033[101mA");
      CHECK_INT(t.bg(0, 0), 4, "SGR 101 (bright red bg) folds onto CGA 4 (red)"); }

    { Term t; t.send("\033[91;101mA");
      CHECK_INT(t.fg(0, 0), 12, "a bright foreground and background do not collide");
      CHECK_INT(t.bg(0, 0), 4, "and the background keeps its own nibble"); }

    { Term t; t.send("\033[1m\033[22mA");
      CHECK_INT(t.fg(0, 0), 0x07, "SGR 22 clears the bold bit"); }

    { Term t; t.send("\033[31;44m\033[0mA");
      CHECK_INT(t.fg(0, 0), 7, "SGR 0 restores the default foreground");
      CHECK_INT(t.bg(0, 0), 0, "SGR 0 restores the default background"); }

    { Term t; t.send("\033[31;44mA");
      CHECK_INT(t.fg(0, 0), 4, "a multi-parameter SGR applies the foreground");
      CHECK_INT(t.bg(0, 0), 1, "a multi-parameter SGR applies the background"); }

    { Term t; t.send("\033[31;44m\033[7mA");
      CHECK_INT(t.fg(0, 0), 1, "SGR 7 swaps the background into the foreground");
      CHECK_INT(t.bg(0, 0), 4, "SGR 7 swaps the foreground into the background"); }

    { Term t; t.send("\033[31;44m\033[7m\033[7mA");
      CHECK_INT(t.fg(0, 0), 1, "a second SGR 7 does not swap back"); }

    // The [1.0.20] fix: ESC[27m used to reset the whole attribute byte.
    { Term t; t.send("\033[31;44m\033[7m\033[27mA");
      CHECK_INT(t.fg(0, 0), 4, "SGR 27 restores the foreground, not the default");
      CHECK_INT(t.bg(0, 0), 1, "SGR 27 restores the background, not the default"); }

    { Term t; t.send("\033[7m\033[32mA");
      CHECK_INT(t.bg(0, 0), 2, "a colour set while reversed lands in the swapped nibble"); }

    { Term t; t.send("\033[7m\033[32m\033[27mA");
      CHECK_INT(t.fg(0, 0), 2, "and reads back as the foreground once un-reversed"); }

    { Term t; t.send("\033[mA");
      CHECK_INT(t.fg(0, 0), 7, "a bare ESC[m is a reset"); }

    // Reverse video must be a flag, not a destructive edit of the attribute.
    // Swapping the nibbles in place cannot round-trip: the foreground is four
    // bits and the background three, so the intensity bit falls off the end.
    { Term t; t.send("\033[1;31m\033[7m\033[27mA");
      CHECK_INT(t.fg(0, 0), 12, "SGR 7 then 27 preserves a bold foreground");
      CHECK_INT(t.bg(0, 0), 0, "SGR 7 then 27 preserves the background"); }

    { Term t; t.send("\033[7m\033[1m\033[27mA");
      CHECK_INT(t.fg(0, 0), 0x0F, "bold set while reversed survives un-reversing");
      CHECK_INT(t.bg(0, 0), 0, "and does not leak into the background"); }

    // ESC[m is ESC[0m, which includes clearing reverse.
    { Term t; t.send("\033[7m\033[m\033[27mA");
      CHECK_INT(t.fg(0, 0), 7, "ESC[m clears reverse, so a later ESC[27m is a no-op");
      CHECK_INT(t.bg(0, 0), 0, "ESC[m leaves the background at the default"); }

    // Bold and colour are independent, so the order they arrive in cannot matter.
    { Term t; t.send("\033[1;37mA");
      CHECK_INT(t.fg(0, 0), 0x0F, "ESC[1;37m is bright white"); }

    { Term t; t.send("\033[37;1mA");
      CHECK_INT(t.fg(0, 0), 0x0F, "ESC[37;1m is bright white too"); }

    { Term t; t.send("\033[1m\033[31mA");
      CHECK_INT(t.fg(0, 0), 12, "a colour after bold keeps the intensity bit"); }

    { Term t; t.send("\033[7mA");
      CHECK_INT(t.fg(0, 0), 0, "a cell written while reversed shows the background as its foreground");
      CHECK_INT(t.bg(0, 0), 7, "and the foreground as its background"); }

    // The private forms are not renditions. ESC[>4;2m and ESC[>m are xterm's
    // modifyOtherKeys; the bare one used to be read as ESC[m and reset the lot.
    { Term t; t.send("\033[31;44m\033[>m"); t.send("A");
      CHECK_INT(t.fg(0, 0), 4, "ESC[>m does not reset the foreground");
      CHECK_INT(t.bg(0, 0), 1, "ESC[>m does not reset the background"); }

    { Term t; t.send("\033[31m\033[>4;2m"); t.send("A");
      CHECK_INT(t.fg(0, 0), 4, "ESC[>4;2m does not touch the rendition"); }

    // Extended-colour subparameters must be stepped over, not read as colours.
    // This terminal is CGA and has nothing to apply them to.
    { Term t; t.send("\033[38;5;44m"); t.send("A");
      CHECK_INT(t.bg(0, 0), 0, "ESC[38;5;44m does not set a background from the index");
      CHECK_INT(t.fg(0, 0), 7, "and leaves the foreground alone"); }

    { Term t; t.send("\033[38;2;1;2;3m"); t.send("A");
      CHECK_INT(t.fg(0, 0), 7, "ESC[38;2;r;g;b m is consumed whole"); }

    { Term t; t.send("\033[31;38;5;44;1m"); t.send("A");
      CHECK_INT(t.fg(0, 0), 12, "parameters around an extended colour still apply"); }

    // DECSC/DECRC save the rendition with the position; CSI s / CSI u do not.
    { Term t; t.send("\033[31;44m\0337\033[32;46m\0338"); t.send("A");
      CHECK_INT(t.fg(0, 0), 4, "ESC 8 restores the saved foreground");
      CHECK_INT(t.bg(0, 0), 1, "ESC 8 restores the saved background"); }

    { Term t; t.send("\033[7m\0337\033[27m\0338"); t.send("A");
      CHECK_INT(t.fg(0, 0), 0, "ESC 8 restores the saved reverse-video flag"); }

    { Term t; t.send("\033[31m\033[s\033[32m\033[u"); t.send("A");
      CHECK_INT(t.fg(0, 0), 2, "CSI u restores the cursor only, not the rendition"); }
}

//=============================================================================
// Per-cell attributes - TerminalCell::flags
//
// todo.txt: "there is no per-cell attribute beyond the packed CGA byte...
// bold / underline / blink / reverse have nowhere to live and nothing to
// render them." That item splits into a parser commit and a rendering commit
// by its own account; this section is the parser half - where the bits are
// stored and what moves them. Nothing paints them yet, so every check here
// reads cellAt().flags.
//
// The item's list of four is out of date on one entry, and two things are NOT
// in the flags byte:
//
//   - Reverse video, which was done before this and is not new here. It is
//     resolved into the colour nibbles at the write, and that is what makes
//     SGR 7 and 27 exact inverses (see test_sgr above); a reversed cell
//     therefore carries flags == 0.
//   - Bold's visibility. SGR 1 still sets the CGA intensity bit as well as
//     TCELL_BOLD, because the intensity bit is the only thing that shows
//     today. The five bold checks in test_sgr() are the tripwire for that: if
//     a change here moves any of them, bold has stopped setting 0x08.
//=============================================================================

static void test_attributes() {
    section("per-cell attributes");

    // Bold in both places at once.
    { Term t; t.send("\033[1mA");
      CHECK_TRUE((t.flags(0, 0) & TCELL_BOLD) != 0, "SGR 1 sets TCELL_BOLD");
      CHECK_INT(t.fg(0, 0), 0x0F, "and still sets the CGA intensity bit"); }

    { Term t; t.send("\033[1m\033[22mA");
      CHECK_INT((int)t.flags(0, 0), 0, "SGR 22 clears TCELL_BOLD");
      CHECK_INT(t.fg(0, 0), 0x07, "and still clears the intensity bit"); }

    { Term t; t.send("\033[4mA");
      CHECK_INT((int)t.flags(0, 0), (int)TCELL_UNDERLINE, "SGR 4 sets TCELL_UNDERLINE"); }

    { Term t; t.send("\033[4m\033[24mA");
      CHECK_INT((int)t.flags(0, 0), 0, "SGR 24 clears it"); }

    { Term t; t.send("\033[5mA");
      CHECK_INT((int)t.flags(0, 0), (int)TCELL_BLINK, "SGR 5 sets TCELL_BLINK"); }

    // One bit for both blink rates: nothing here can draw two speeds, so
    // storing two flags would be storing a distinction that cannot be honoured.
    { Term t; t.send("\033[6mA");
      CHECK_INT((int)t.flags(0, 0), (int)TCELL_BLINK, "SGR 6 (rapid blink) sets the same bit"); }

    { Term t; t.send("\033[5m\033[25mA");
      CHECK_INT((int)t.flags(0, 0), 0, "SGR 25 clears blink"); }

    { Term t; t.send("\033[6m\033[25mA");
      CHECK_INT((int)t.flags(0, 0), 0, "SGR 25 clears rapid blink too"); }

    { Term t; t.send("\033[1;4;5mA");
      CHECK_INT((int)t.flags(0, 0), (int)(TCELL_BOLD | TCELL_UNDERLINE | TCELL_BLINK),
                "the three accumulate in one byte"); }

    { Term t; t.send("\033[1;4;5m\033[0mA");
      CHECK_INT((int)t.flags(0, 0), 0, "SGR 0 clears all of them"); }

    { Term t; t.send("\033[1;4;5m\033[mA");
      CHECK_INT((int)t.flags(0, 0), 0, "and so does a bare ESC[m"); }

    // SGR 21 is left unhandled on purpose. ECMA-48 calls it double-underline
    // and several terminals treat it as bold-off; nothing available here can
    // settle which a CP/M guest meant, so it does nothing at all rather than
    // doing one of the two and looking deliberate.
    { Term t; t.send("\033[1m\033[21mA");
      CHECK_TRUE((t.flags(0, 0) & TCELL_BOLD) != 0, "SGR 21 is a no-op: bold is not cleared");
      CHECK_INT(t.fg(0, 0), 0x0F, "SGR 21 is a no-op: the intensity bit is not cleared");
      CHECK_INT((int)t.flags(0, 0), (int)TCELL_BOLD, "SGR 21 is a no-op: no underline appears"); }

    // Reverse video is not a flag. It is already in the colours by the time the
    // cell is written.
    { Term t; t.send("\033[7mA");
      CHECK_INT((int)t.flags(0, 0), 0, "a reversed cell carries no flag bit");
      CHECK_INT(t.fg(0, 0), 0, "the swap is in the colours instead"); }

    // ...which is exactly why bold needs the flag. The swap moves the
    // foreground into a three-bit background nibble and the intensity bit falls
    // off the end; TCELL_BOLD is the only surviving record that this cell was
    // written bold.
    { Term t; t.send("\033[1;31m\033[7mA");
      CHECK_TRUE((t.flags(0, 0) & TCELL_BOLD) != 0, "bold survives the reverse-video swap as a flag");
      CHECK_INT(t.bg(0, 0), 4, "though the intensity bit is lost to the swap");
      CHECK_INT(t.fg(0, 0), 0, "and the background becomes the foreground"); }

    // An erase carries the colour and drops the flags. A background colour on a
    // space is the only way an erase can show a colour at all; underline and
    // blink on a space are not colour, and carrying them would underline all
    // 2000 cells or set the whole screen strobing. See blankCell().
    { Term t; t.send("\033[4;5m\033[2J");
      CHECK_INT((int)t.flags(0, 0), 0, "ESC[2J does not underline or blink the blanks"); }

    { Term t; t.send("\033[44m\033[2J");
      CHECK_INT(t.bg(0, 0), 1, "ESC[2J still paints the current background"); }

    { Term t; t.send("\033[4;44mAB\033[2K");
      CHECK_INT((int)t.flags(0, 0), 0, "ESC[2K drops the flags as well");
      CHECK_INT(t.bg(0, 0), 1, "and keeps the background, like every other erase"); }

    // Extended-colour subparameters must be stepped over, or their digits land
    // as renditions in their own right. This is a sharper test of the skip than
    // the colour one in test_sgr(): the "5" of ESC[38;5;n is the same number as
    // SGR 5, so a broken skip sets the whole run blinking.
    { Term t; t.send("\033[38;5;5m"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), 0, "the ;5 of ESC[38;5;5m is an index, not SGR 5 blink"); }

    { Term t; t.send("\033[48;5;4m"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), 0, "and the ;4 of ESC[48;5;4m is not SGR 4 underline"); }

    { Term t; t.send("\033[38;2;1;4;5m"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), 0, "the r;g;b form is consumed whole too"); }

    { Term t; t.send("\033[4;38;5;5;1m"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), (int)(TCELL_UNDERLINE | TCELL_BOLD),
                "parameters around an extended colour still apply"); }

    // The private forms are not renditions: ESC[>4;2m is xterm's
    // modifyOtherKeys and its "4" is a key-modifier resource, not underline.
    { Term t; t.send("\033[>4;2m"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), 0, "ESC[>4;2m is not SGR 4"); }

    { Term t; t.send("\033[4m\033[>4;2m"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), (int)TCELL_UNDERLINE,
                "and it leaves an underline already in force alone"); }

    // Scrolling moves whole cells, so the flags travel with them. The blank
    // line scrolled in underneath comes from blankCell() and does not.
    { Term t; t.home(2, 1); t.send("\033[4mA"); t.home(25, 1); t.send("\n");
      CHECK_INT((int)t.at(0, 0), (int)'A', "the cell moved up a row");
      CHECK_INT((int)t.flags(0, 0), (int)TCELL_UNDERLINE, "and kept its underline across the scroll");
      CHECK_INT((int)t.flags(24, 0), 0, "while the line scrolled in underneath is plain"); }

    // Editing sequences copy whole cells for the same reason.
    { Term t; t.send("\033[4mA\033[H\033[@");
      CHECK_INT((int)t.at(0, 1), (int)'A', "ICH shifted the cell right");
      CHECK_INT((int)t.flags(0, 1), (int)TCELL_UNDERLINE, "and its flags went with it");
      CHECK_INT((int)t.flags(0, 0), 0, "and the cell it inserts is plain"); }

    // DECSC/DECRC save and restore the flags with the rest of the rendition,
    // as they already do for the attribute byte and the reverse flag.
    { Term t; t.send("\033[4m\0337\033[24m\0338"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), (int)TCELL_UNDERLINE, "ESC 8 restores the saved flags"); }

    { Term t; t.send("\033[1;4m\0337\033[0m\0338"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), (int)(TCELL_BOLD | TCELL_UNDERLINE),
                "ESC 8 restores all of them at once");
      CHECK_INT(t.fg(0, 0), 0x0F, "and the attribute byte with them"); }

    // A machine reset clears the SAVED flags too, or an ESC 8 with no ESC 7
    // since would restore a face from before the reset.
    { Term t; t.send("\033[4m\0337"); t.tv.clear(); t.send("\0338"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), 0, "clear() resets the saved flags, not just the current ones"); }

    { Term t; t.send("\033[1;4;5m"); t.tv.clear(); t.send("A");
      CHECK_INT((int)t.flags(0, 0), 0, "clear() resets the current flags"); }

    { Term t; t.send("\033[1;4;5m\033c"); t.send("A");
      CHECK_INT((int)t.flags(0, 0), 0, "and so does ESC c, which is the same path"); }

    // setAttr() is the VDA path: the caller hands over a whole CGA attribute
    // byte, which says nothing about underline or blink, so an ESC[4m still in
    // force must not underline what it draws.
    { Term t; t.send("\033[4m"); t.tv.setAttr(0x07); t.send("A");
      CHECK_INT((int)t.flags(0, 0), 0, "setAttr() clears the flags"); }

    // writeChar() likewise takes its whole rendition as arguments. It is one of
    // only two places in TerminalView.cpp that assign a cell's fields one at a
    // time, so without an explicit flags line it would inherit whatever the
    // parser last left in that cell.
    { Term t; t.send("\033[4mA"); t.tv.writeChar(0, 0, 'B', 7, 0);
      CHECK_INT((int)t.at(0, 0), (int)'B', "writeChar() replaced the character");
      CHECK_INT((int)t.flags(0, 0), 0, "and did not inherit the underline that was there"); }

    // A fresh cell has nothing set.
    { Term t;
      CHECK_INT((int)t.flags(0, 0), 0, "an untouched cell carries no flags"); }
}

//=============================================================================
// 8. Private-parameter markers and DEC modes
//=============================================================================

static void test_private_modes() {
    section("private modes");

    // The [1.0.20] fix: '?' was treated as a final byte, so ESC[?25l ended at
    // the '?' and "25l" appeared on the screen.
    { Term t; t.send("\033[?25l");
      CHECK_STR(t.line(0), "", "ESC[?25l prints nothing"); }

    { Term t; t.send("\033[?25h");
      CHECK_STR(t.line(0), "", "ESC[?25h prints nothing"); }

    { Term t; t.send("\033[?1049h");
      CHECK_STR(t.line(0), "", "an unhandled private mode prints nothing"); }

    { Term t; t.send("\033[?7l");
      CHECK_STR(t.line(0), "", "ESC[?7l prints nothing"); }

    // Intermediate bytes (0x20-0x2F) belong to the sequence and must not end it.
    { Term t; t.send("\033[4 qA");
      CHECK_STR(t.line(0), "A", "an intermediate byte does not end a CSI"); }

    { Term t; t.send("\033[>c");
      CHECK_STR(t.replies, "", "the secondary DA form stays silent"); }

    { Term t; t.send("\033[=c");
      CHECK_STR(t.replies, "", "the tertiary DA form stays silent"); }

    // Guest output is untrusted: digit count, value and parameter count are all
    // capped, and none of it may throw or escape the screen.
    { Term t; t.send("\033[99999999999999H");
      CHECK_TRUE(t.row() >= 1 && t.row() <= 25,
                 "an over-long parameter cannot escape the screen"); }

    { Term t; t.send("\033[1;2;3;4;5;6;7;8;9;10;11;12;13;14;15;16;17;18;19;20H");
      CHECK_TRUE(t.row() >= 1 && t.row() <= 25,
                 "an over-long parameter list is survivable"); }
}

//=============================================================================
// 9. Character-set and line-size designators
//=============================================================================

static void test_designators() {
    section("designators");

    // ESC ( B used to leave its "B" on the screen.
    { Term t; t.send("\033(BA");
      CHECK_STR(t.line(0), "A", "ESC ( B is consumed with its parameter"); }

    { Term t; t.send("\033)0A");
      CHECK_STR(t.line(0), "A", "ESC ) 0 is consumed with its parameter"); }

    { Term t; t.send("\033*AA");
      CHECK_STR(t.line(0), "A", "ESC * is consumed with its parameter"); }

    { Term t; t.send("\033+AA");
      CHECK_STR(t.line(0), "A", "ESC + is consumed with its parameter"); }

    { Term t; t.send("\033#6A");
      CHECK_STR(t.line(0), "A", "ESC # 6 is consumed with its parameter"); }

    { Term t; t.send("\033 FA");
      CHECK_STR(t.line(0), "A", "ESC SP F is consumed with its parameter"); }

    { Term t; t.send("\033=A\033>B");
      CHECK_STR(t.line(0), "AB", "the keypad-mode sequences are consumed"); }
}

//=============================================================================
// 10. Answerback
//=============================================================================

static void test_answerback() {
    section("answerback");

    { Term t; t.home(7, 13); t.replies.clear(); t.send("\033[6n");
      CHECK_STR(t.replies, "\033[7;13R", "DSR 6 reports the cursor 1-based"); }

    { Term t; t.replies.clear(); t.send("\033[6n");
      CHECK_STR(t.replies, "\033[1;1R", "DSR 6 reports home as 1;1"); }

    { Term t; t.replies.clear(); t.send("\033[5n");
      CHECK_STR(t.replies, "\033[0n", "DSR 5 reports the terminal is OK"); }

    { Term t; t.replies.clear(); t.send("\033[c");
      CHECK_STR(t.replies, "\033[?1;0c", "DA reports a VT100 with no options"); }

    { Term t; t.replies.clear(); t.send("\033[0c");
      CHECK_STR(t.replies, "\033[?1;0c", "DA 0 is the same as a bare DA"); }

    { Term t; t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033[?1;0c", "ESC Z identifies as a VT100 in ANSI mode"); }

    { Term t; t.send("\033A"); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033/Z", "ESC Z identifies as a VT52 in VT52 mode"); }

    { Term t; t.replies.clear(); t.send("\033[?6n");
      CHECK_STR(t.replies, "", "a private DSR form stays silent"); }
}

//=============================================================================
// 11. VT52
//=============================================================================

static void test_vt52() {
    section("VT52");

    { Term t; t.home(10, 10); t.send("\033A");
      CHECK_INT(t.row(), 9, "ESC A moves the cursor up"); }

    { Term t; t.home(10, 10); t.send("\033B");
      CHECK_INT(t.row(), 11, "ESC B moves the cursor down"); }

    { Term t; t.home(10, 10); t.send("\033C");
      CHECK_INT(t.col(), 11, "ESC C moves the cursor right"); }

    // ESC D is IND in ANSI and cursor-left in VT52, so the mode has to be
    // entered before the VT52 reading applies.
    { Term t; t.send("\033A"); t.home(10, 10); t.send("\033D");
      CHECK_INT(t.col(), 9, "ESC D moves the cursor left in VT52 mode"); }

    { Term t; t.home(10, 10); t.send("\033D");
      CHECK_INT(t.row(), 11, "ESC D is IND in ANSI mode"); }

    { Term t; t.send("\033A"); t.home(10, 10); t.send("\033H"); int r, c; t.cursor(r, c);
      CHECK_INT(r, 1, "ESC H homes the row in VT52 mode");
      CHECK_INT(c, 1, "ESC H homes the column in VT52 mode"); }

    { Term t; t.home(10, 10); t.send("\033H");
      CHECK_INT(t.row(), 10, "ESC H does nothing in ANSI mode - it is HTS, unsupported"); }

    { Term t; t.home(10, 10); t.send("\033I");
      CHECK_INT(t.row(), 9, "ESC I is a reverse line feed"); }

    { Term t; t.send("abcdef"); t.home(1, 4); t.send("\033J");
      CHECK_STR(t.line(0), "abc", "ESC J erases to the end of the screen"); }

    { Term t; t.send("abcdef"); t.home(1, 4); t.send("\033K");
      CHECK_STR(t.line(0), "abc", "ESC K erases to the end of the line"); }

    // ESC Y takes two bytes, each biased by 0x20.
    { Term t; t.send("\033Y");
      t.send(std::string(1, (char)(0x20 + 6)));
      t.send(std::string(1, (char)(0x20 + 11)));
      int r, c; t.cursor(r, c);
      CHECK_INT(r, 7, "ESC Y sets the row from the biased byte");
      CHECK_INT(c, 12, "ESC Y sets the column from the biased byte"); }

    { Term t; t.send("hello"); t.send("\033A"); t.send("\033E");
      CHECK_STR(t.line(0), "", "ESC E clears the screen in VT52 mode"); }

    { Term t; t.send("hello\033E");
      CHECK_STR(t.line(0), "hello", "ESC E is NEL in ANSI mode and does not clear");
      CHECK_STR(t.line(1), "", "ESC E in ANSI mode moves to the next line"); }

    // Any VT52-exclusive sequence is itself the signal to enter VT52 mode.
    { Term t; t.send("\033A"); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033/Z", "ESC A auto-detects VT52"); }

    { Term t; t.send("\033F"); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033/Z", "ESC F (graphics on) auto-detects VT52"); }

    { Term t; t.send("\033FA\033G");
      CHECK_STR(t.line(0), "A", "the graphics-mode sequences are consumed"); }

    { Term t; t.send("\033A\033<"); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033[?1;0c", "ESC < returns to ANSI"); }

    { Term t; t.send("\033[?2l"); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033/Z", "ESC[?2l selects VT52 (DECANM)"); }

    { Term t; t.send("\033[?2l\033[?2h"); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033[?1;0c", "ESC[?2h selects ANSI (DECANM)"); }
}

//=============================================================================
// 12. What an erase must NOT do
//
// FEATURE_PARITY.md item 13 holds this repository's terminal row at partial
// partly because clear() resets the attribute and the escape state, which
// erase-in-display alone has no business doing. ioscpm's clearTerminal() resets
// neither. These are the checks for that.
//=============================================================================

static void test_erase_preserves_state() {
    section("erase does not reset terminal state");

    { Term t; t.send("\033[31;44m"); t.send("\033[2J"); t.send("A");
      CHECK_INT(t.fg(0, 0), 4, "ED 2 preserves the foreground attribute");
      CHECK_INT(t.bg(0, 0), 1, "ED 2 preserves the background attribute"); }

    { Term t; t.send("\033[31;44m\033[7m"); t.send("\033[2J"); t.send("\033[27mA");
      CHECK_INT(t.fg(0, 0), 4, "ED 2 preserves the reverse-video flag"); }

    { Term t; t.send("\033[2;4r"); t.send("\033[2J");
      t.home(2, 1); t.send("A"); t.home(3, 1); t.send("B"); t.home(4, 1); t.send("C");
      t.home(4, 1); t.send("\n");
      CHECK_STR(t.line(1), "B", "ED 2 preserves the scrolling region"); }

    { Term t; t.send("\033A"); t.send("\033[2J"); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033/Z", "ED 2 preserves VT52 mode"); }

    { Term t; t.send("\033[?7l"); t.send("\033[2J"); t.home(1, 80); t.send("XY");
      CHECK_INT(t.row(), 1, "ED 2 preserves the DECAWM setting"); }

    { Term t; t.send("\033[31m"); t.send("\033[0J"); t.send("A");
      CHECK_INT(t.fg(0, 0), 4, "ED 0 preserves the attribute"); }

    { Term t; t.send("\033[31m"); t.send("\033[2K"); t.send("A");
      CHECK_INT(t.fg(0, 0), 4, "EL 2 preserves the attribute"); }

    { Term t; t.send("\033A\033[31m\033E"); t.send("A");
      CHECK_INT(t.fg(0, 0), 4, "VT52 ESC E preserves the attribute"); }

    // An erase paints the current background. Once ED stopped resetting the
    // rendition, filling with a hardcoded fg 7 / bg 0 meant the cleared area and
    // the text written into it afterwards no longer agreed.
    { Term t; t.send("\033[44m\033[2J");
      CHECK_INT(t.bg(0, 0), 1, "ED 2 fills with the current background");
      CHECK_INT(t.bg(24, 79), 1, "ED 2 fills the whole screen with it"); }

    { Term t; t.send("abcdef"); t.send("\033[41m"); t.home(1, 4); t.send("\033[0K");
      CHECK_INT(t.bg(0, 4), 4, "EL 0 fills with the current background");
      CHECK_INT(t.bg(0, 0), 0, "and leaves the cells it did not erase alone"); }

    { Term t; t.send("\033[42m"); t.home(1, 2); t.send("\033[3X");
      CHECK_INT(t.bg(0, 2), 2, "ECH fills with the current background"); }

    { Term t; t.send("\033[43m"); t.send("\033[L");
      CHECK_INT(t.bg(0, 0), 6, "IL opens a line in the current background"); }

    { Term t; t.send("\033[45m\033[7m\033[2J");
      CHECK_INT(t.fg(0, 0), 5, "an erase while reversed paints the swapped pair");
      CHECK_INT(t.bg(0, 0), 7, "with the foreground as the background"); }

    // A machine reset must not inherit the colour the last session ended on.
    { Term t; t.send("\033[44m"); t.tv.clear();
      CHECK_INT(t.bg(0, 0), 0, "clear() erases to the default background, not the current one"); }

    // An erase resolves an armed wrap, consistently across all three forms.
    { Term t; t.home(1, 80); t.send("X\033[0J"); t.send("Y");
      CHECK_INT(t.row(), 1, "ED 0 cancels an armed wrap"); }

    { Term t; t.home(1, 80); t.send("X\033[1J"); t.send("Y");
      CHECK_INT(t.row(), 1, "ED 1 cancels an armed wrap"); }

    { Term t; t.home(1, 80); t.send("X\033[2J"); t.send("Y");
      CHECK_INT(t.row(), 1, "ED 2 cancels an armed wrap"); }
}

//=============================================================================
// RIS (ESC c)
//=============================================================================

static void test_ris() {
    section("RIS (ESC c)");

    { Term t; t.send("hello\033c"); t.send("A");
      CHECK_STR(t.line(0), "A", "ESC c erases the screen"); }

    { Term t; t.send("\033[31;44m\033c"); t.send("A");
      CHECK_INT(t.fg(0, 0), 7, "ESC c restores the default foreground");
      CHECK_INT(t.bg(0, 0), 0, "ESC c restores the default background"); }

    { Term t; t.send("\033A\033c"); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033[?1;0c", "ESC c returns the terminal to ANSI"); }

    { Term t; t.send("\033[?7l\033c"); t.home(1, 80); t.send("XY");
      CHECK_INT(t.row(), 2, "ESC c puts autowrap back on"); }

    { Term t; t.send("\033[2;4r\033c"); t.home(25, 1); t.send("Z");
      CHECK_STR(t.line(24), "Z", "ESC c restores the full-screen scrolling region"); }

    { Term t; t.home(10, 10); t.send("\033c"); int r, c; t.cursor(r, c);
      CHECK_INT(r, 1, "ESC c homes the cursor row");
      CHECK_INT(c, 1, "ESC c homes the cursor column"); }
}

//=============================================================================
// 13. clear() is still the full reset
//
// The other half of the split: what ESC[2J must not touch, a machine reset
// must. clear() is what the constructor and Emulator > Start / Reset call, and
// no guest sequence reaches it.
//=============================================================================

static void test_clear_is_a_full_reset() {
    section("clear() is a full reset");

    { Term t; t.send("\033[31;44mhello"); t.tv.clear(); t.send("A");
      CHECK_STR(t.line(0), "A", "clear() erases the screen");
      CHECK_INT(t.fg(0, 0), 7, "clear() restores the default foreground");
      CHECK_INT(t.bg(0, 0), 0, "clear() restores the default background"); }

    { Term t; t.send("\033[7m"); t.tv.clear(); t.send("\033[31mA");
      CHECK_INT(t.fg(0, 0), 4, "clear() drops the reverse-video flag"); }

    { Term t; t.send("\033A"); t.tv.clear(); t.replies.clear(); t.send("\033Z");
      CHECK_STR(t.replies, "\033[?1;0c", "clear() returns the terminal to ANSI"); }

    { Term t; t.send("\033[?7l"); t.tv.clear(); t.home(1, 80); t.send("XY");
      CHECK_INT(t.row(), 2, "clear() puts autowrap back on"); }

    { Term t; t.send("\033[2;4r"); t.tv.clear();
      t.home(25, 1); t.send("Z");
      CHECK_STR(t.line(24), "Z", "clear() restores the full-screen scrolling region"); }

    { Term t; t.home(10, 10); t.tv.clear(); int r, c; t.cursor(r, c);
      CHECK_INT(r, 1, "clear() homes the cursor row");
      CHECK_INT(c, 1, "clear() homes the cursor column"); }

    // A reset arriving mid-sequence must not leave the parser stranded.
    { Term t; t.send("\033[3"); t.tv.clear(); t.send("A");
      CHECK_STR(t.line(0), "A", "clear() resets the escape parser state"); }
}

//=============================================================================
// 14. Key map
//
// Keymap.h is header-only and comes in with TerminalView.h, so it can be
// checked in the same binary. The modifier support is the part worth pinning:
// before it, every modified press fell through to the unmodified sequence and
// Ctrl+Left was indistinguishable from Left.
//=============================================================================

static void test_keymap() {
    section("key map");

    // The termcap-style decoder.
    CHECK_STR(keymap::decode("\\E[A"), "\033[A", "\\E decodes to ESC");
    CHECK_STR(keymap::decode("^A"), "\001", "^A decodes to a control byte");
    CHECK_STR(keymap::decode("^?"), "\177", "^? decodes to DEL");
    CHECK_STR(keymap::decode("\\101"), "A", "an octal escape decodes to its byte");
    CHECK_STR(keymap::decode("\\n"), "\n", "\\n decodes to LF, and is not rewritten to CR");

    // Name parsing, with and without modifier prefixes.
    CHECK_INT(keymap::keyIdForName("Left"),
              (long)keymap::keyId(VK_LEFT, keymap::KM_MOD_NONE), "a bare name has no modifiers");
    CHECK_INT(keymap::keyIdForName("Ctrl+Left"),
              (long)keymap::keyId(VK_LEFT, keymap::KM_MOD_CTRL), "Ctrl+ sets the ctrl bit");
    CHECK_INT(keymap::keyIdForName("ctrl+left"),
              (long)keymap::keyId(VK_LEFT, keymap::KM_MOD_CTRL), "prefixes are case-insensitive");
    CHECK_INT(keymap::keyIdForName("Control+Left"),
              (long)keymap::keyId(VK_LEFT, keymap::KM_MOD_CTRL), "Control+ is accepted for Ctrl+");
    CHECK_INT(keymap::keyIdForName("Ctrl+Shift+F3"),
              (long)keymap::keyId(VK_F3, keymap::KM_MOD_CTRL | keymap::KM_MOD_SHIFT),
              "prefixes stack");
    CHECK_INT(keymap::keyIdForName("Nonsense"), -1L, "an unbindable name is rejected");
    CHECK_INT(keymap::keyIdForName("Ctrl+Nonsense"), -1L,
              "a modifier on an unbindable name is still rejected");

    // Defaults, and the modified/unmodified split.
    {
        keymap::KeyMap km;
        const std::string* plain = km.find(VK_LEFT, keymap::KM_MOD_NONE);
        const std::string* ctrl  = km.find(VK_LEFT, keymap::KM_MOD_CTRL);
        CHECK_TRUE(plain != nullptr, "Left is bound by default");
        CHECK_TRUE(ctrl != nullptr, "Ctrl+Left is bound by default");
        CHECK_STR(*plain, "\033[D", "Left sends the plain VT100 sequence");
        CHECK_STR(*ctrl, "\033[1;5D", "Ctrl+Left sends the xterm modified form");
        CHECK_TRUE(*plain != *ctrl, "Ctrl+Left is distinguishable from Left");

        // A modified press with no binding of its own falls back, which is what
        // every modified press used to get.
        const std::string* shiftLeft = km.find(VK_LEFT, keymap::KM_MOD_SHIFT);
        CHECK_TRUE(shiftLeft != nullptr, "Shift+Left falls back to Left");
        CHECK_STR(*shiftLeft, "\033[D", "and sends the unmodified sequence");

        CHECK_TRUE(km.find(VK_F13, keymap::KM_MOD_NONE) == nullptr,
                   "an unbound key returns nullptr");
    }

    // Overrides layer on top of the defaults, and an empty value unbinds.
    {
        keymap::KeyMap km;
        km.build({ {"Ctrl+Left", "^A"}, {"Ctrl+Right", "^F"} });
        const std::string* ctrlLeft = km.find(VK_LEFT, keymap::KM_MOD_CTRL);
        CHECK_TRUE(ctrlLeft != nullptr, "an override binds");
        CHECK_STR(*ctrlLeft, "\001", "Ctrl+Left can be rebound to WordStar word-left");

        const std::string* plain = km.find(VK_LEFT, keymap::KM_MOD_NONE);
        CHECK_TRUE(plain != nullptr, "overriding the modified key leaves the plain one");
        CHECK_STR(*plain, "\033[D", "and the plain one is unchanged");
    }

    {
        keymap::KeyMap km;
        km.build({ {"Up", ""} });
        CHECK_TRUE(km.find(VK_UP, keymap::KM_MOD_NONE) == nullptr,
                   "an empty override unbinds the key");
    }

    // Unbinding a MODIFIED key must not fall back to the plain one. Merging by
    // name left the modified entry merely absent, and find() then answered a
    // config that said "send nothing" with plain Left's sequence.
    {
        keymap::KeyMap km;
        km.build({ {"Ctrl+Left", ""} });
        CHECK_TRUE(km.find(VK_LEFT, keymap::KM_MOD_CTRL) == nullptr,
                   "an unbound Ctrl+Left sends nothing, not Left");
        const std::string* plain = km.find(VK_LEFT, keymap::KM_MOD_NONE);
        CHECK_TRUE(plain != nullptr, "and the plain binding is untouched");
        CHECK_STR(*plain, "\033[D", "still sending its own sequence");
    }

    // An override in a different case from the default is the same binding.
    // Merging by name kept both and let ASCII ordering decide: "CTRL+Left"
    // sorts before "Ctrl+Left", so that spelling lost to the default.
    {
        keymap::KeyMap km;
        km.build({ {"CTRL+Left", "^A"} });
        const std::string* s = km.find(VK_LEFT, keymap::KM_MOD_CTRL);
        CHECK_TRUE(s != nullptr, "an upper-case modifier prefix binds");
        CHECK_STR(*s, "\001", "and beats the default whatever its spelling"); }

    {
        keymap::KeyMap km;
        km.build({ {"up", "^K"} });
        const std::string* s = km.find(VK_UP, keymap::KM_MOD_NONE);
        CHECK_TRUE(s != nullptr, "a lower-case key name binds");
        CHECK_STR(*s, "\013", "and overrides the default"); }

    // A name with a typo should be rejected, not bound to something near it.
    CHECK_INT(keymap::keyIdForName("F1x"), -1L, "\"F1x\" is not F1");
    CHECK_INT(keymap::keyIdForName("F13"), -1L, "F13 is out of range");
    CHECK_INT(keymap::keyIdForName("F0"), -1L, "F0 is out of range");
    CHECK_INT(keymap::keyIdForName("F12"),
              (long)keymap::keyId(VK_F12, keymap::KM_MOD_NONE), "F12 is still accepted");

    // findExact does not fall back. The Alt path depends on this: Alt is the
    // menu key, so an Alt press may only be diverted from the menu when that
    // exact combination is bound - never on the strength of the plain binding.
    {
        keymap::KeyMap km;
        CHECK_TRUE(km.findExact(VK_LEFT, keymap::KM_MOD_NONE) != nullptr,
                   "findExact finds an exactly-matching binding");
        CHECK_TRUE(km.findExact(VK_LEFT, keymap::KM_MOD_ALT) == nullptr,
                   "findExact does not fall back to the unmodified binding");
        CHECK_TRUE(km.find(VK_LEFT, keymap::KM_MOD_ALT) != nullptr,
                   "but find() still does");

        km.build({ {"Alt+Left", "\\E[1;3D"} });
        const std::string* altLeft = km.findExact(VK_LEFT, keymap::KM_MOD_ALT);
        CHECK_TRUE(altLeft != nullptr, "an explicit Alt binding is found exactly");
        CHECK_STR(*altLeft, "\033[1;3D", "and carries its own sequence");
    }

    // The combinations the application answers itself. handleKeyDown consults
    // keymap::reservedFor() before the map, so a binding on one of these can
    // never send a byte; the table is checked here rather than the view's
    // behaviour because the view needs a window and this suite has none.
    //
    // The rows are walked out of keymap::reservedKeys() itself. This block used
    // to iterate a hard-coded copy of the same four rows, which made it a test
    // that agreed with itself: a row added to, removed from or edited in
    // reservedKeys() broke no assertion here, because the suite never read the
    // table it was meant to be checking. kExpected is now the assertion rather
    // than the iteration source, and it is spelled out a second time on purpose
    // - changing reservedKeys() is supposed to fail this suite until someone
    // states here that the change was intended.
    {
        static const struct { const char* name; int vk; unsigned mods; } kExpected[] = {
            { "Shift+PageUp",   VK_PRIOR, keymap::KM_MOD_SHIFT },
            { "Shift+PageDown", VK_NEXT,  keymap::KM_MOD_SHIFT },
            { "Ctrl+Home",      VK_HOME,  keymap::KM_MOD_CTRL  },
            { "Ctrl+End",       VK_END,   keymap::KM_MOD_CTRL  },
        };
        const size_t expectedCount = sizeof kExpected / sizeof kExpected[0];

        size_t count = 0;
        const keymap::ReservedKey* table = keymap::reservedKeys(&count);
        CHECK_TRUE(table != nullptr, "reservedKeys() hands back a table");
        CHECK_INT((long)count, (long)expectedCount,
                  "reservedKeys() holds exactly the rows this suite expects");

        for (size_t i = 0; i < count && i < expectedCount; ++i) {
            const std::string name = kExpected[i].name;
            CHECK_INT(table[i].vk, kExpected[i].vk,
                      (name + " is the reserved key at this position").c_str());
            CHECK_INT((long)table[i].mods, (long)kExpected[i].mods,
                      (name + " reserves the modifiers it is meant to").c_str());

            // Asked again through the public predicates, because those - not the
            // struct fields - are what handleKeyDown and classifyName() call.
            const unsigned id = keymap::keyId(table[i].vk, table[i].mods);
            CHECK_TRUE(keymap::isReservedForApp(id),
                       (name + " is reserved by the app").c_str());
            // A Settings dialog shows this string, so an empty one is a bug
            // even though the combination is correctly refused.
            const char* purpose = table[i].purpose ? table[i].purpose : "";
            CHECK_TRUE(*purpose != '\0',
                       (name + " says what it is reserved for").c_str());
            const char* why = keymap::reservedPurpose(id);
            CHECK_STR(std::string(why ? why : ""), purpose,
                      (name + " reports that same text through reservedPurpose").c_str());
            CHECK_INT(keymap::keyIdForName(kExpected[i].name), (long)id,
                      (name + " is spelled the way the config file spells it").c_str());
        }
    }

    // The modifier test is a mask, not an equality. handleKeyDown read "shift
    // held" and "ctrl held" independently before the table existed, so a press
    // carrying an extra modifier was swallowed too; that is preserved.
    CHECK_TRUE(keymap::reservedFor(VK_PRIOR, keymap::KM_MOD_SHIFT) != nullptr,
               "Shift+PageUp is reserved");
    CHECK_TRUE(keymap::reservedFor(VK_PRIOR,
                                   keymap::KM_MOD_CTRL | keymap::KM_MOD_SHIFT) != nullptr,
               "and so is Ctrl+Shift+PageUp - the match is a mask, not an exact set");
    CHECK_TRUE(keymap::reservedFor(VK_PRIOR, keymap::KM_MOD_NONE) == nullptr,
               "plain PageUp is the guest's and still reaches CP/M");
    CHECK_TRUE(keymap::reservedFor(VK_HOME, keymap::KM_MOD_SHIFT) == nullptr,
               "Shift+Home is not the reserved Ctrl+Home");
    CHECK_TRUE(keymap::reservedFor(VK_LEFT, keymap::KM_MOD_CTRL) == nullptr,
               "a modifier that is reserved on one key is not reserved on every key");
    CHECK_TRUE(keymap::reservedPurpose(keymap::keyId(VK_NEXT, keymap::KM_MOD_NONE)) == nullptr,
               "reservedPurpose reports nothing for an unreserved id");

    // No default binding may sit on a reserved combination: it would be a
    // sequence the app guarantees never to send, and a row a Settings dialog
    // would have to grey out on the strength of its own defaults.
    for (const auto& kv : keymap::defaultBindings()) {
        long id = keymap::keyIdForName(kv.first);
        CHECK_TRUE(id >= 0 && !keymap::isReservedForApp((unsigned)id),
                   (kv.first + " is a bindable default and is not reserved").c_str());
    }

    // Being reserved is a fact about the user interface, not about the map.
    // build() keeps whatever the file said, because a Settings dialog reads the
    // whole map back out and writes it whole - dropping the entry at load would
    // silently delete a line the user typed by hand. The stored sequence simply
    // never fires, and classifyName() is how a caller refuses it out loud.
    {
        keymap::KeyMap km;
        km.build({ {"Shift+PageUp", "X"} });
        const std::string* s = km.find(VK_PRIOR, keymap::KM_MOD_SHIFT);
        CHECK_TRUE(s != nullptr, "build() still stores a binding on a reserved combination");
        CHECK_STR(*s, "X", "with exactly the sequence the config gave it");
    }

    // classifyName is what a config walker uses to tell a typo apart from a
    // combination that resolves but is not the user's to have.
    {
        unsigned id = 0;
        const char* why = "not cleared";
        CHECK_TRUE(keymap::classifyName("Ctrl+Left", &id, &why) == keymap::NameStatus::Ok,
                   "a bindable name classifies as Ok");
        CHECK_INT((long)id, (long)keymap::keyId(VK_LEFT, keymap::KM_MOD_CTRL),
                  "and reports the id it resolved to");
        CHECK_TRUE(why == nullptr, "with no reason attached");

        id = 0; why = "not cleared";
        CHECK_TRUE(keymap::classifyName("Nonsense+Q", &id, &why) == keymap::NameStatus::Unknown,
                   "a name that does not resolve classifies as Unknown");
        CHECK_INT((long)id, 0L, "and leaves the id untouched");
        CHECK_TRUE(why == nullptr, "and has no reason either");

        id = 0; why = nullptr;
        CHECK_TRUE(keymap::classifyName("Shift+PageUp", &id, &why) == keymap::NameStatus::Reserved,
                   "a reserved combination classifies as Reserved, not Unknown");
        CHECK_INT((long)id, (long)keymap::keyId(VK_PRIOR, keymap::KM_MOD_SHIFT),
                  "still reporting its id, so a dialog can point at the row");
        CHECK_TRUE(why != nullptr && *why != '\0', "and carrying the text that explains why");

        CHECK_TRUE(keymap::classifyName("pgup") == keymap::NameStatus::Ok,
                   "plain PageUp is bindable, under any spelling, with no out-parameters");
    }
}

//=============================================================================
// 15. cellAt() bounds
//=============================================================================

static void test_cell_at() {
    section("cellAt bounds");

    { Term t; t.send("A");
      CHECK_INT((int)t.at(0, 0), (int)'A', "cellAt reads a written cell"); }

    { Term t;
      CHECK_INT((int)t.at(-5, -5), (int)' ', "cellAt clamps negative coordinates"); }

    { Term t; t.home(25, 80); t.send("Z");
      CHECK_INT((int)t.at(999, 999), (int)'Z', "cellAt clamps out-of-range coordinates"); }
}

//=============================================================================

int main() {
    printf("TerminalView conformance suite\n");
    printf("==============================\n");

    test_control_chars();
    test_bell();
    test_cursor_movement();
    test_erase();
    test_insert_delete();
    test_scroll_region();
    test_autowrap();
    test_sgr();
    test_attributes();
    test_private_modes();
    test_designators();
    test_answerback();
    test_vt52();
    test_erase_preserves_state();
    test_ris();
    test_clear_is_a_full_reset();
    test_keymap();
    test_cell_at();

    printf("\n==============================\n");
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
