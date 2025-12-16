/*
 * TerminalView.h - Terminal Display Component
 *
 * A VT100-compatible terminal display for the emulator.
 * 25 rows x 80 columns of character cells.
 */

#pragma once

#include <windows.h>
#include <string>
#include <functional>

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

    // Input callback
    void setKeyInputCallback(KeyInputCallback cb) { m_keyCallback = cb; }

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

    bool m_cursorVisible = true;
    UINT_PTR m_cursorTimer = 0;
};
