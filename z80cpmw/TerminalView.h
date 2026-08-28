/*
 * TerminalView.h - Terminal Display Component
 *
 * A VT100-compatible terminal display for the emulator.
 * 25 rows x 80 columns of character cells.
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <deque>
#include <array>
#include "Keymap.h"

// Per-cell rendition bits that have nowhere to live in a packed CGA attribute
// byte, kept in TerminalCell::flags.
//
// Reverse video is deliberately NOT one of these. It is resolved into the
// colour nibbles at the write - see swapAttrNibbles() in TerminalView.cpp - so
// a reversed cell is stored as its swapped colours and carries no flag. That is
// what makes SGR 7 and SGR 27 exact inverses, and moving reverse in here would
// undo it.
//
// Bold is the one bit that lives in both places. SGR 1 keeps setting the CGA
// intensity bit (0x08 of the foreground nibble) AND sets TCELL_BOLD, because
// the intensity bit is the only way bold shows today and is what the colour
// checks in tests/test_vt52.cpp pin; the flag is what a renderer that draws a
// heavier face will read, and it is also the only record of bold that survives
// the reverse-video swap, which pushes the intensity bit off the end of the
// three-bit background nibble.
enum : uint8_t {
    TCELL_BOLD      = 0x01,
    TCELL_UNDERLINE = 0x02,
    TCELL_BLINK     = 0x04,
};

// Terminal cell structure
struct TerminalCell {
    char character = ' ';
    uint8_t foreground = 7;  // White
    uint8_t background = 0;  // Black
    uint8_t flags = 0;       // TCELL_* bits; see the enum above
};

// Input callback type
using KeyInputCallback = std::function<void(char ch)>;

class TerminalView {
public:
    static constexpr int ROWS = 25;
    static constexpr int COLS = 80;

    TerminalView();
    ~TerminalView();

    // Window management
    bool create(HWND parent, int x, int y, int width, int height);
    void destroy();
    HWND getHwnd() const { return m_hwnd; }

    // Display operations
    void clear();
    void setCursor(int row, int col);
    void writeChar(int row, int col, char ch, uint8_t fg = 7, uint8_t bg = 0);
    void scrollUp(int lines);
    void setAttr(uint8_t attr);

    // Output a character with VT100 escape sequence processing
    void outputChar(uint8_t ch);

    // Read a cell of the live screen. The counterpart to writeChar(), and what
    // test_vt52.cpp checks the editing sequences against - the alternative was
    // reaching into m_cells from a test, which pins the private layout.
    // Out-of-range coordinates clamp rather than fault.
    const TerminalCell& cellAt(int row, int col) const;

    // Font
    void setFontSize(int size);
    int getFontSize() const { return m_fontSize; }

    // Scrollback history. The terminal keeps lines that scroll off the top so
    // the user can scroll back into them (mouse wheel / Shift+PageUp/Down).
    void setScrollbackLines(int lines);          // capacity in lines; 0 disables
    int getScrollbackLines() const { return m_scrollbackLines; }
    void resetScrollback();                      // clear history (on start/reset)

    // The bell (BEL, 0x07). On by default. cpmdroid made the bell a setting and
    // todo.txt asked for the same here, because this one was unconditional: the
    // 0x07 case called MessageBeep() with nothing to consult.
    //
    // The preference travels as "display.bell" / AppConfig::bellEnabled, which
    // tests/test_config.cpp round-trips. Two places deliver it, and they are the
    // whole list: MainWindow::applyConfig on the startup and profile-load paths,
    // and MainWindow::onEmulatorSettings when the Terminal page's checkbox is
    // used. Anything that adds a third is a second source of truth whose
    // precedence would be settled by call order, so change one of those two
    // rather than adding one here.
    //
    // The state is a user preference and NOT part of the terminal's power-on
    // state, so clear() and ESC c leave it alone.
    void setBellEnabled(bool on) { m_bellEnabled = on; }
    bool isBellEnabled() const { return m_bellEnabled; }

    // Divert the bell. The hook is called INSTEAD of MessageBeep(), and only
    // when the bell is enabled, so a caller sees exactly the bells a user would
    // have heard. tests/test_vt52.cpp installs a counter here: without it the
    // suite audibly beeps on every run, because test_control_chars()'s "BEL
    // does not move the cursor" reaches the real bell.
    void setBellHook(std::function<void()> hook) { m_bellHook = hook; }

    // Input callback
    void setKeyInputCallback(KeyInputCallback cb) { m_keyCallback = cb; }

    // Apply key bindings (name -> termcap-style sequence) from the config,
    // layered over the built-in defaults. See Keymap.h.
    void setKeyBindings(const std::map<std::string, std::string>& bindings) {
        m_keymap.build(bindings);
    }

    // Predicate the terminal uses to decide whether pasted input can be
    // delivered (e.g. only while the emulator is running). Optional.
    void setInputReadyCallback(std::function<bool()> cb) { m_inputReadyCallback = cb; }

    // Get character dimensions
    int getCharWidth() const { return m_charWidth; }
    int getCharHeight() const { return m_charHeight; }

    // Force redraw
    void invalidate();

    // Force immediate repaint (bypassing WM_PAINT)
    void repaint();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void createFont();
    void paint(HDC hdc);
    // Which of the four faces in m_fonts a cell's TCELL_* bits ask for.
    // Written out as two tests rather than as (flags & 3) so that renumbering
    // the enum cannot silently re-map the table.
    static int fontIndexFor(uint8_t flags);
    // Invalidate the rows of the VISIBLE grid that contain a TCELL_BLINK cell,
    // and nothing else. Called once per blink tick; a screen with no blinking
    // cell calls InvalidateRect zero times and so does not repaint.
    void invalidateBlinkingRows();
    static unsigned currentKeyMods();   // Ctrl/Shift/Alt held now, as keymap bits
    void handleKeyDown(WPARAM wParam);
    void handleChar(WPARAM wParam);

    // Scrollback helpers
    const TerminalCell& visibleCell(int row, int col) const;  // maps viewport->history/live
    void scrollByLines(int deltaLines);   // +ve scrolls up into history, -ve toward live
    void scrollToBottom();                // return to the live screen (offset 0)

    // Mouse selection and clipboard
    bool pixelToCell(int x, int y, int& row, int& col) const;  // clamps to grid
    bool isCellSelected(int row, int col) const;
    void clearSelection();
    void handleLButtonDown(int x, int y);
    void handleMouseMove(int x, int y);
    void handleLButtonUp();
    void showContextMenu(int screenX, int screenY);
    void copySelectionToClipboard();
    void pasteFromClipboard();

    // VT100 escape sequence processing
    void processChar(uint8_t ch);
    void processNormalChar(uint8_t ch);
    void processEscapeChar(uint8_t ch);
    void processCSIChar(uint8_t ch);
    void executeCSI(uint8_t finalChar);
    // Reply to a terminal query (ESC Z, ESC[c, ESC[6n). Goes straight to the
    // guest rather than through the key path, which scrolls the view back to
    // the live screen - the terminal answering a question is not the user
    // typing.
    void sendAnswerback(const char* s);
    void applySGR(int param);
    // Sound the bell, honouring setBellEnabled() and setBellHook(). The 0x07
    // case calls this and nothing else; it used to call MessageBeep() straight
    // out with nothing to consult.
    void ringBell();
    // The cell an erase leaves behind: a space carrying the CURRENT rendition,
    // not the power-on default. Erasing paints the current background, which is
    // what lets a program set a colour, clear, and have the cleared area be that
    // colour. Every erase, and every blank line scrolled in, goes through here.
    TerminalCell blankCell() const;
    // Erase the whole screen and home the cursor, touching nothing else. The
    // ESC[2J and VT52 ESC E path; clear() is the machine-level reset built on
    // top of it. See the comments on both in TerminalView.cpp.
    void eraseScreen();
    void clearFromCursor();
    void clearToCursor();

    // Scrolling region (DECSTBM) and the editing sequences that work within it.
    void scrollRegionUp(int lines);
    void scrollRegionDown(int lines);
    void lineFeed();            // LF / IND, honouring the region
    void insertLines(int n);    // IL
    void deleteLines(int n);    // DL
    void insertChars(int n);    // ICH
    void deleteChars(int n);    // DCH
    void eraseChars(int n);     // ECH

    // CGA color to RGB conversion
    static COLORREF cgaToRGB(uint8_t cgaColor);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;

    // The four faces a cell can be drawn in, indexed by fontIndexFor(): bit 0
    // is TCELL_BOLD, bit 1 is TCELL_UNDERLINE. One CreateFontW shape built four
    // times - see createFont() - because GDI has no way to turn weight or
    // underline on for a single TextOut call; the face has to carry it.
    //
    // Underline is the font's own underline rather than a line drawn by hand,
    // and that is what makes the blink "off" phase work with no extra code.
    // GDI draws the underline in the TEXT colour - measured: the rule under an
    // ESC[4m cell reads as the cell's foreground, CGA 15, in the rendering
    // suite - so collapsing the foreground onto the background takes the rule
    // with the glyph. A hand-drawn rule would have needed its own suppression,
    // and the suite pins that it does not need one: see "takes its rule with
    // it" in tests/test_render.cpp.
    //
    // There is no italic face. Nothing in the parser sets an italic flag -
    // TCELL_* has three bits and SGR 3 is not among them - so a fifth and sixth
    // face would have no way to be asked for.
    HFONT m_fonts[4] = { nullptr, nullptr, nullptr, nullptr };

    TerminalCell m_cells[ROWS][COLS];

    // Scrollback ring buffer of full 80-column lines that have scrolled off the
    // top. m_scrollOffset is how many lines the view is scrolled up from the live
    // bottom (0 = showing the live screen). The fixed column count means history
    // never needs reflowing.
    std::deque<std::array<TerminalCell, COLS>> m_scrollback;
    int m_scrollbackLines = 1000;   // capacity; overridden from config
    int m_scrollOffset = 0;

    int m_cursorRow = 0;
    int m_cursorCol = 0;
    int m_savedCursorRow = 0;
    int m_savedCursorCol = 0;
    // DECSC/DECRC (ESC 7 / ESC 8) save the rendition alongside the position, as
    // a VT100 does. CSI s / CSI u are the ANSI.SYS pair and save position only.
    uint8_t m_savedAttr = 0x07;
    bool m_savedReverse = false;
    uint8_t m_savedFlags = 0;      // the TCELL_* half of the saved rendition

    uint8_t m_currentAttr = 0x07;  // Default: white on black
    // The rendition bits that do not fit in the attribute byte, stamped into
    // every cell a printable character writes. Not the reverse-video flag,
    // which is resolved into the colours instead; see the TCELL_* enum.
    uint8_t m_currentFlags = 0;

    int m_fontSize = 16;
    int m_charWidth = 8;
    int m_charHeight = 16;

    // Escape sequence state
    enum class EscapeState {
        Normal,
        Escape,
        CSI,
        CSIParam,
        Vt52Row,     // after ESC Y, expecting the row byte (value + 0x20)
        Vt52Col,     // after ESC Y <row>, expecting the column byte
        ConsumeOne   // swallow one byte (character-set / line-size designator)
    };
    EscapeState m_escapeState = EscapeState::Normal;
    std::vector<int> m_escapeParams;
    std::string m_escapeCurrentParam;
    // Set by '?' (and the other private-parameter markers) after ESC [. Without
    // it those bytes fell through as though they were the sequence's final
    // character, so ESC[?25l ended at the '?' and printed "25l" on screen.
    bool m_escapePrivate = false;

    // VT52 mode. ANSI/VT100 is the default; entered by ESC[?2l or by any
    // VT52-exclusive sequence, left by ESC < or ESC[?2h. Only ESC D/E/H are
    // read differently per mode, so ANSI behaviour is untouched while false.
    bool m_vt52Mode = false;
    int m_vt52CursorRow = 0;   // latched while parsing ESC Y <row> <col>

    // DECTCEM: whether the guest wants a cursor drawn at all. Distinct from
    // m_cursorVisible, which is the blink phase and belongs to the timer.
    bool m_cursorEnabled = true;

    // Scrolling region (DECSTBM), 0-based and inclusive. Defaults to the whole
    // screen, in which case scrolling still feeds the scrollback; a partial
    // region does not, because those lines never left the top of the screen.
    int m_scrollTop = 0;
    int m_scrollBottom = ROWS - 1;

    // Deferred autowrap. A glyph written to the last column leaves the cursor
    // there and arms this; the wrap happens when the next printable character
    // arrives. Writing the bottom-right cell used to scroll the screen
    // immediately, which corrupts any full-screen layout.
    bool m_pendingWrap = false;
    bool m_autoWrap = true;      // DECAWM

    // Whether SGR 7 (reverse video) is in effect. It is a pure flag: the swap
    // it describes is applied at the cell write and never to m_currentAttr,
    // which always holds the un-reversed rendition. That is what makes 7 and 27
    // exact inverses - swapping the byte in place cannot round-trip, because
    // the foreground is four bits and the background only three.
    bool m_reverse = false;

    // BEL. A user preference, not part of the terminal's power-on state: see
    // setBellEnabled(). m_bellHook, when set, stands in for MessageBeep().
    bool m_bellEnabled = true;
    std::function<void()> m_bellHook;

    KeyInputCallback m_keyCallback;
    std::function<bool()> m_inputReadyCallback;

    // Configurable special-key bindings (function/navigation keys -> CP/M bytes)
    keymap::KeyMap m_keymap;

    bool m_cursorVisible = true;
    UINT_PTR m_cursorTimer = 0;

    // The phase TCELL_BLINK cells are drawn in, flipped by the same 500 ms
    // WM_TIMER as the cursor. Sharing the tick is deliberate: a second timer
    // would give a screen carrying both a cursor and blinking text two
    // independent phases that drift against each other, and 500 ms is already
    // the rate this terminal blinks at.
    //
    // It is a SEPARATE bool from m_cursorVisible, which cannot stand in for it.
    // m_cursorVisible is forced true by WM_SETFOCUS and false by WM_KILLFOCUS
    // and is frozen while the view is scrolled back, so text blinking off it
    // would stop, or stick on, every time the window changed focus.
    //
    // true at construction so a terminal whose SetTimer failed - or one built
    // without a window at all, which is how tests/test_vt52.cpp uses this class,
    // never calling create() - shows its blinking text rather than hiding it
    // forever.
    bool m_textBlinkOn = true;

    // --- Mouse selection / clipboard ---
    bool m_selecting = false;       // left button held, drag in progress
    bool m_hasSelection = false;    // a committed, non-empty selection exists
    int m_selAnchorRow = 0, m_selAnchorCol = 0;  // where the drag started
    int m_selActiveRow = 0, m_selActiveCol = 0;  // current end of the drag
};
