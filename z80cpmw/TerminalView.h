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

// Terminal cell structure
struct TerminalCell {
    char character = ' ';
    uint8_t foreground = 7;  // White
    uint8_t background = 0;  // Black
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
    HFONT m_font = nullptr;

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

    uint8_t m_currentAttr = 0x07;  // Default: white on black

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

    // Whether SGR 7 (reverse video) is currently in effect. m_currentAttr holds
    // the already-swapped byte, so this is what lets 27 undo the swap and lets
    // a colour set while reversed land in the right nibble.
    bool m_reverse = false;

    KeyInputCallback m_keyCallback;
    std::function<bool()> m_inputReadyCallback;

    // Configurable special-key bindings (function/navigation keys -> CP/M bytes)
    keymap::KeyMap m_keymap;

    bool m_cursorVisible = true;
    UINT_PTR m_cursorTimer = 0;

    // --- Mouse selection / clipboard ---
    bool m_selecting = false;       // left button held, drag in progress
    bool m_hasSelection = false;    // a committed, non-empty selection exists
    int m_selAnchorRow = 0, m_selAnchorCol = 0;  // where the drag started
    int m_selActiveRow = 0, m_selActiveCol = 0;  // current end of the drag
};
