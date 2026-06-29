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
    void applySGR(int param);
    void clearFromCursor();
    void clearToCursor();

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
        CSIParam
    };
    EscapeState m_escapeState = EscapeState::Normal;
    std::vector<int> m_escapeParams;
    std::string m_escapeCurrentParam;

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
