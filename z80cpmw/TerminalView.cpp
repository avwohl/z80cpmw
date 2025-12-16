/*
 * TerminalView.cpp - Terminal Display Component Implementation
 */

#include "pch.h"
#include "TerminalView.h"

static const wchar_t* TERMINAL_CLASS = L"Z80CPM_Terminal";
static bool g_classRegistered = false;

TerminalView::TerminalView() {
    clear();
}

TerminalView::~TerminalView() {
    destroy();
}

bool TerminalView::create(HWND parent, int x, int y, int width, int height) {
    m_parent = parent;

    // Register window class if not already done
    if (!g_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;  // Removed CS_OWNDC
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_IBEAM);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = TERMINAL_CLASS;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        g_classRegistered = true;
    }

    // Create window
    m_hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        TERMINAL_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, width, height,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!m_hwnd) {
        return false;
    }

    createFont();

    // Start cursor blink timer
    m_cursorTimer = SetTimer(m_hwnd, 1, 500, nullptr);

    return true;
}

void TerminalView::destroy() {
    if (m_cursorTimer) {
        KillTimer(m_hwnd, m_cursorTimer);
        m_cursorTimer = 0;
    }
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void TerminalView::createFont() {
    if (m_font) {
        DeleteObject(m_font);
    }

    m_font = CreateFontW(
        m_fontSize,              // Height
        0,                       // Width (0 = auto)
        0,                       // Escapement
        0,                       // Orientation
        FW_NORMAL,               // Weight
        FALSE,                   // Italic
        FALSE,                   // Underline
        FALSE,                   // Strikeout
        DEFAULT_CHARSET,         // Charset
        OUT_TT_PRECIS,           // Output precision
        CLIP_DEFAULT_PRECIS,     // Clip precision
        CLEARTYPE_QUALITY,       // Quality
        FIXED_PITCH | FF_MODERN, // Pitch and family
        L"Consolas"              // Font name
    );

    // Get character dimensions
    if (m_hwnd && m_font) {
        HDC hdc = GetDC(m_hwnd);
        HFONT oldFont = (HFONT)SelectObject(hdc, m_font);

        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        m_charWidth = tm.tmAveCharWidth;
        m_charHeight = tm.tmHeight;

        SelectObject(hdc, oldFont);
        ReleaseDC(m_hwnd, hdc);
    }
}

void TerminalView::clear() {
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = TerminalCell();
        }
    }
    m_cursorRow = 0;
    m_cursorCol = 0;
    m_escapeState = EscapeState::Normal;
    m_escapeParams.clear();
    m_escapeCurrentParam.clear();
    m_currentAttr = 0x07;
    invalidate();
}

void TerminalView::setCursor(int row, int col) {
    m_cursorRow = std::max(0, std::min(row, ROWS - 1));
    m_cursorCol = std::max(0, std::min(col, COLS - 1));
    invalidate();
}

void TerminalView::writeChar(int row, int col, char ch, uint8_t fg, uint8_t bg) {
    if (row >= 0 && row < ROWS && col >= 0 && col < COLS) {
        m_cells[row][col].character = ch;
        m_cells[row][col].foreground = fg;
        m_cells[row][col].background = bg;
        invalidate();
    }
}

void TerminalView::scrollUp(int lines) {
    if (lines <= 0) return;

    for (int row = 0; row < ROWS - lines; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = m_cells[row + lines][col];
        }
    }

    for (int row = ROWS - lines; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = TerminalCell();
        }
    }
    invalidate();
}

void TerminalView::setAttr(uint8_t attr) {
    m_currentAttr = attr;
}

void TerminalView::outputChar(uint8_t ch) {
    processChar(ch);
}

void TerminalView::setFontSize(int size) {
    if (size != m_fontSize && size >= 8 && size <= 48) {
        m_fontSize = size;
        createFont();
        invalidate();

        // Notify parent of size change
        if (m_parent) {
            PostMessage(m_parent, WM_SIZE, 0, 0);
        }
    }
}

void TerminalView::invalidate() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void TerminalView::repaint() {
    if (!m_hwnd) return;

    // Force repaint through WM_PAINT
    RedrawWindow(m_hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
}

LRESULT CALLBACK TerminalView::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TerminalView* view = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        view = static_cast<TerminalView*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        // Set m_hwnd early so handleMessage can use it for DefWindowProc
        view->m_hwnd = hwnd;
    } else {
        view = reinterpret_cast<TerminalView*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (view) {
        return view->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TerminalView::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        paint(hdc);
        EndPaint(m_hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN:
        handleKeyDown(wParam);
        return 0;

    case WM_CHAR:
        handleChar(wParam);
        return 0;

    case WM_SETFOCUS:
        m_cursorVisible = true;
        invalidate();
        return 0;

    case WM_KILLFOCUS:
        m_cursorVisible = false;
        invalidate();
        return 0;

    case WM_TIMER:
        if (wParam == 1) {
            m_cursorVisible = !m_cursorVisible;
            // Only redraw cursor area
            RECT cursorRect;
            cursorRect.left = m_cursorCol * m_charWidth;
            cursorRect.top = m_cursorRow * m_charHeight;
            cursorRect.right = cursorRect.left + m_charWidth;
            cursorRect.bottom = cursorRect.top + m_charHeight;
            InvalidateRect(m_hwnd, &cursorRect, FALSE);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;  // We handle background in WM_PAINT

    case WM_MOUSEWHEEL: {
        // Could implement scrollback buffer here
        return 0;
    }
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void TerminalView::paint(HDC hdc) {
    // Get client rect with caching
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    // Cache valid client rect dimensions (GetClientRect sometimes returns 0 outside WM_PAINT)
    static int cachedWidth = 0, cachedHeight = 0;
    if (clientRect.right > 0 && clientRect.bottom > 0) {
        cachedWidth = clientRect.right;
        cachedHeight = clientRect.bottom;
    } else if (cachedWidth > 0 && cachedHeight > 0) {
        clientRect.right = cachedWidth;
        clientRect.bottom = cachedHeight;
    }

    // Don't paint if dimensions are invalid
    if (clientRect.right <= 0 || clientRect.bottom <= 0) {
        return;
    }

    // Double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Select font
    HFONT oldFont = (HFONT)SelectObject(memDC, m_font);
    SetBkMode(memDC, OPAQUE);

    // Draw characters
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            const TerminalCell& cell = m_cells[row][col];

            int x = col * m_charWidth;
            int y = row * m_charHeight;

            // Set colors
            SetTextColor(memDC, cgaToRGB(cell.foreground));
            SetBkColor(memDC, cgaToRGB(cell.background));

            // Draw character
            char ch = cell.character;
            if (ch < 32) ch = ' ';
            TextOutA(memDC, x, y, &ch, 1);
        }
    }

    // Draw cursor
    if (m_cursorVisible && GetFocus() == m_hwnd) {
        int x = m_cursorCol * m_charWidth;
        int y = m_cursorRow * m_charHeight;

        RECT cursorRect = { x, y + m_charHeight - 2, x + m_charWidth, y + m_charHeight };
        HBRUSH cursorBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(memDC, &cursorRect, cursorBrush);
        DeleteObject(cursorBrush);
    }

    SelectObject(memDC, oldFont);

    // Copy to screen
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

void TerminalView::handleKeyDown(WPARAM wParam) {
    // Handle special keys
    char ch = 0;

    switch (wParam) {
    case VK_UP:    ch = 0x1B; if (m_keyCallback) { m_keyCallback(ch); m_keyCallback('['); m_keyCallback('A'); } return;
    case VK_DOWN:  ch = 0x1B; if (m_keyCallback) { m_keyCallback(ch); m_keyCallback('['); m_keyCallback('B'); } return;
    case VK_RIGHT: ch = 0x1B; if (m_keyCallback) { m_keyCallback(ch); m_keyCallback('['); m_keyCallback('C'); } return;
    case VK_LEFT:  ch = 0x1B; if (m_keyCallback) { m_keyCallback(ch); m_keyCallback('['); m_keyCallback('D'); } return;
    case VK_HOME:  ch = 0x1B; if (m_keyCallback) { m_keyCallback(ch); m_keyCallback('['); m_keyCallback('H'); } return;
    case VK_END:   ch = 0x1B; if (m_keyCallback) { m_keyCallback(ch); m_keyCallback('['); m_keyCallback('F'); } return;
    case VK_DELETE: ch = 0x7F; break;
    }

    if (ch && m_keyCallback) {
        m_keyCallback(ch);
    }
}

void TerminalView::handleChar(WPARAM wParam) {
    if (wParam >= 1 && wParam <= 127) {
        char ch = (char)wParam;
        if (m_keyCallback) {
            m_keyCallback(ch);
        }
    }
}

void TerminalView::processChar(uint8_t ch) {
    switch (m_escapeState) {
    case EscapeState::Normal:
        processNormalChar(ch);
        break;
    case EscapeState::Escape:
        processEscapeChar(ch);
        break;
    case EscapeState::CSI:
    case EscapeState::CSIParam:
        processCSIChar(ch);
        break;
    }
}

void TerminalView::processNormalChar(uint8_t ch) {
    switch (ch) {
    case 0x07:  // Bell
        MessageBeep(MB_OK);
        break;

    case 0x08:  // Backspace
        if (m_cursorCol > 0) {
            m_cursorCol--;
        }
        invalidate();
        break;

    case 0x09:  // Tab
        m_cursorCol = std::min((m_cursorCol + 8) & ~7, COLS - 1);
        invalidate();
        break;

    case 0x0A:  // Line feed
        m_cursorRow++;
        if (m_cursorRow >= ROWS) {
            scrollUp(1);
            m_cursorRow = ROWS - 1;
        }
        invalidate();
        break;

    case 0x0D:  // Carriage return
        m_cursorCol = 0;
        invalidate();
        break;

    case 0x1B:  // ESC
        m_escapeState = EscapeState::Escape;
        m_escapeParams.clear();
        m_escapeCurrentParam.clear();
        break;

    default:
        // Printable character
        if (ch >= 0x20 && ch <= 0x7E) {
            m_cells[m_cursorRow][m_cursorCol].character = (char)ch;
            m_cells[m_cursorRow][m_cursorCol].foreground = m_currentAttr & 0x0F;
            m_cells[m_cursorRow][m_cursorCol].background = (m_currentAttr >> 4) & 0x07;

            m_cursorCol++;
            if (m_cursorCol >= COLS) {
                m_cursorCol = 0;
                m_cursorRow++;
                if (m_cursorRow >= ROWS) {
                    scrollUp(1);
                    m_cursorRow = ROWS - 1;
                }
            }
            invalidate();
        }
        break;
    }
}

void TerminalView::processEscapeChar(uint8_t ch) {
    switch (ch) {
    case '[':  // CSI
        m_escapeState = EscapeState::CSI;
        break;

    case '7':  // Save cursor
        m_savedCursorRow = m_cursorRow;
        m_savedCursorCol = m_cursorCol;
        m_escapeState = EscapeState::Normal;
        break;

    case '8':  // Restore cursor
        m_cursorRow = m_savedCursorRow;
        m_cursorCol = m_savedCursorCol;
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'D':  // Index (move down)
        m_cursorRow++;
        if (m_cursorRow >= ROWS) {
            scrollUp(1);
            m_cursorRow = ROWS - 1;
        }
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'M':  // Reverse index (move up)
        if (m_cursorRow > 0) {
            m_cursorRow--;
        }
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'E':  // Next line
        m_cursorCol = 0;
        m_cursorRow++;
        if (m_cursorRow >= ROWS) {
            scrollUp(1);
            m_cursorRow = ROWS - 1;
        }
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    default:
        m_escapeState = EscapeState::Normal;
        break;
    }
}

void TerminalView::processCSIChar(uint8_t ch) {
    if (ch >= '0' && ch <= '9') {
        m_escapeCurrentParam += (char)ch;
        m_escapeState = EscapeState::CSIParam;
        return;
    }

    if (ch == ';') {
        m_escapeParams.push_back(m_escapeCurrentParam.empty() ? 0 : std::stoi(m_escapeCurrentParam));
        m_escapeCurrentParam.clear();
        m_escapeState = EscapeState::CSIParam;
        return;
    }

    // Final character
    if (!m_escapeCurrentParam.empty()) {
        m_escapeParams.push_back(std::stoi(m_escapeCurrentParam));
    }

    executeCSI(ch);
    m_escapeState = EscapeState::Normal;
}

void TerminalView::executeCSI(uint8_t finalChar) {
    int p1 = m_escapeParams.size() > 0 ? m_escapeParams[0] : 0;
    int p2 = m_escapeParams.size() > 1 ? m_escapeParams[1] : 0;

    switch (finalChar) {
    case 'A':  // Cursor up
        m_cursorRow = std::max(m_cursorRow - std::max(p1, 1), 0);
        invalidate();
        break;

    case 'B':  // Cursor down
        m_cursorRow = std::min(m_cursorRow + std::max(p1, 1), ROWS - 1);
        invalidate();
        break;

    case 'C':  // Cursor forward
        m_cursorCol = std::min(m_cursorCol + std::max(p1, 1), COLS - 1);
        invalidate();
        break;

    case 'D':  // Cursor back
        m_cursorCol = std::max(m_cursorCol - std::max(p1, 1), 0);
        invalidate();
        break;

    case 'H':
    case 'f':  // Cursor position
        m_cursorRow = std::max(0, std::min(std::max(p1, 1) - 1, ROWS - 1));
        m_cursorCol = std::max(0, std::min(std::max(p2, 1) - 1, COLS - 1));
        invalidate();
        break;

    case 'J':  // Erase in display
        switch (p1) {
        case 0: clearFromCursor(); break;
        case 1: clearToCursor(); break;
        case 2: clear(); break;
        }
        break;

    case 'K':  // Erase in line
        switch (p1) {
        case 0:  // Clear to end of line
            for (int col = m_cursorCol; col < COLS; col++) {
                m_cells[m_cursorRow][col] = TerminalCell();
            }
            break;
        case 1:  // Clear to beginning
            for (int col = 0; col <= m_cursorCol; col++) {
                m_cells[m_cursorRow][col] = TerminalCell();
            }
            break;
        case 2:  // Clear entire line
            for (int col = 0; col < COLS; col++) {
                m_cells[m_cursorRow][col] = TerminalCell();
            }
            break;
        }
        invalidate();
        break;

    case 'm':  // SGR (Select Graphic Rendition)
        if (m_escapeParams.empty()) {
            m_currentAttr = 0x07;
        } else {
            for (int param : m_escapeParams) {
                applySGR(param);
            }
        }
        break;

    case 's':  // Save cursor
        m_savedCursorRow = m_cursorRow;
        m_savedCursorCol = m_cursorCol;
        break;

    case 'u':  // Restore cursor
        m_cursorRow = m_savedCursorRow;
        m_cursorCol = m_savedCursorCol;
        invalidate();
        break;
    }
}

void TerminalView::applySGR(int param) {
    switch (param) {
    case 0:  // Reset
        m_currentAttr = 0x07;
        break;
    case 1:  // Bold
        m_currentAttr |= 0x08;
        break;
    case 7:  // Reverse
        {
            uint8_t fg = m_currentAttr & 0x0F;
            uint8_t bg = (m_currentAttr >> 4) & 0x07;
            m_currentAttr = (fg << 4) | bg;
        }
        break;
    case 27:  // Reverse off
        m_currentAttr = 0x07;
        break;
    default:
        if (param >= 30 && param <= 37) {
            m_currentAttr = (m_currentAttr & 0xF0) | (param - 30);
        } else if (param >= 40 && param <= 47) {
            m_currentAttr = (m_currentAttr & 0x0F) | ((param - 40) << 4);
        }
        break;
    }
}

void TerminalView::clearFromCursor() {
    for (int col = m_cursorCol; col < COLS; col++) {
        m_cells[m_cursorRow][col] = TerminalCell();
    }
    for (int row = m_cursorRow + 1; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = TerminalCell();
        }
    }
    invalidate();
}

void TerminalView::clearToCursor() {
    for (int row = 0; row < m_cursorRow; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = TerminalCell();
        }
    }
    for (int col = 0; col <= m_cursorCol; col++) {
        m_cells[m_cursorRow][col] = TerminalCell();
    }
    invalidate();
}

COLORREF TerminalView::cgaToRGB(uint8_t cgaColor) {
    // CGA color palette
    static const COLORREF palette[16] = {
        RGB(0, 0, 0),        // 0: Black
        RGB(0, 0, 170),      // 1: Blue
        RGB(0, 170, 0),      // 2: Green
        RGB(0, 170, 170),    // 3: Cyan
        RGB(170, 0, 0),      // 4: Red
        RGB(170, 0, 170),    // 5: Magenta
        RGB(170, 85, 0),     // 6: Brown
        RGB(170, 170, 170),  // 7: Light gray
        RGB(85, 85, 85),     // 8: Dark gray
        RGB(85, 85, 255),    // 9: Light blue
        RGB(85, 255, 85),    // 10: Light green
        RGB(85, 255, 255),   // 11: Light cyan
        RGB(255, 85, 85),    // 12: Light red
        RGB(255, 85, 255),   // 13: Light magenta
        RGB(255, 255, 85),   // 14: Yellow
        RGB(255, 255, 255),  // 15: White
    };

    return palette[cgaColor & 0x0F];
}
