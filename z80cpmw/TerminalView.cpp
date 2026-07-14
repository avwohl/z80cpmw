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

    // Create window (no border - parent window provides framing)
    m_hwnd = CreateWindowExW(
        0,
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

    // Scale font height by current monitor DPI. The app is per-monitor DPI v2
    // aware, so CreateFontW's height parameter is in raw device pixels — without
    // this multiplier, "size 20" is only 20 physical pixels on a 4K @ 200% screen.
    UINT dpi = m_hwnd ? GetDpiForWindow(m_hwnd) : 96;
    int scaledHeight = MulDiv(m_fontSize, dpi, 96);

    m_font = CreateFontW(
        scaledHeight,            // Height
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
    if (lines > ROWS) lines = ROWS;

    // Save the rows about to scroll off the top into the scrollback history.
    // This is the single choke point for content leaving the top of the screen
    // (line feed, printable wrap, ESC D / ESC E), so one hook captures it all.
    if (m_scrollbackLines > 0) {
        for (int row = 0; row < lines; row++) {
            std::array<TerminalCell, COLS> saved;
            for (int col = 0; col < COLS; col++) saved[col] = m_cells[row][col];
            m_scrollback.push_back(saved);
        }
        while ((int)m_scrollback.size() > m_scrollbackLines) {
            m_scrollback.pop_front();
        }
        // If the user is viewing history, follow the new lines so the viewport
        // stays anchored on the same content instead of drifting.
        if (m_scrollOffset > 0) {
            m_scrollOffset = std::min(m_scrollOffset + lines, (int)m_scrollback.size());
        }
    }

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

// Map a viewport row/col (0..ROWS-1, 0..COLS-1) to the cell that should be drawn
// there, accounting for how far the view is scrolled back. The virtual buffer is
// [scrollback...] followed by the live grid; offset 0 shows the live grid.
const TerminalCell& TerminalView::visibleCell(int row, int col) const {
    int sb = (int)m_scrollback.size();
    int virtualRow = (sb - m_scrollOffset) + row;
    if (virtualRow >= 0 && virtualRow < sb) {
        return m_scrollback[virtualRow][col];
    }
    int liveRow = virtualRow - sb;
    if (liveRow < 0) liveRow = 0;
    if (liveRow >= ROWS) liveRow = ROWS - 1;
    return m_cells[liveRow][col];
}

void TerminalView::scrollByLines(int deltaLines) {
    int maxOff = (int)m_scrollback.size();
    int newOff = std::max(0, std::min(m_scrollOffset + deltaLines, maxOff));
    if (newOff != m_scrollOffset) {
        m_scrollOffset = newOff;
        invalidate();
    }
}

void TerminalView::scrollToBottom() {
    if (m_scrollOffset != 0) {
        m_scrollOffset = 0;
        invalidate();
    }
}

void TerminalView::resetScrollback() {
    m_scrollback.clear();
    m_scrollOffset = 0;
    invalidate();
}

void TerminalView::setScrollbackLines(int lines) {
    if (lines < 0) lines = 0;
    if (lines > 100000) lines = 100000;
    m_scrollbackLines = lines;
    while ((int)m_scrollback.size() > m_scrollbackLines) {
        m_scrollback.pop_front();
    }
    if (m_scrollOffset > (int)m_scrollback.size()) {
        m_scrollOffset = (int)m_scrollback.size();
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

    case WM_SYSKEYDOWN:
        // F10 normally activates the menu bar (it arrives as a system key, not
        // WM_KEYDOWN). When it is bound in the keymap, deliver it to CP/M like
        // the other function keys instead; otherwise let the menu handle it.
        if (wParam == VK_F10 && m_keymap.find(VK_F10)) {
            handleKeyDown(wParam);
            return 0;
        }
        break;  // fall through to DefWindowProc for normal Alt/menu handling

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
            if (m_scrollOffset != 0) return 0;  // cursor hidden/frozen while viewing history
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
        // Scroll the history view. Wheel up (positive delta) goes back into
        // history; wheel down returns toward the live screen. 3 lines per notch.
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta != 0) {
            scrollByLines((delta / WHEEL_DELTA) * 3);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
        SetFocus(m_hwnd);
        handleLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        if (m_selecting) {
            handleMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

    case WM_LBUTTONUP:
        handleLButtonUp();
        return 0;

    case WM_CAPTURECHANGED:
        // Capture was stolen (e.g. by the context menu); stop dragging but keep
        // any committed selection so Copy still works.
        m_selecting = false;
        return 0;

    case WM_CONTEXTMENU: {
        // lParam is screen coords; (-1,-1) means the keyboard menu key.
        int sx = GET_X_LPARAM(lParam);
        int sy = GET_Y_LPARAM(lParam);
        if (sx == -1 && sy == -1) {
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            POINT p = { rc.left + 4, rc.top + 4 };
            ClientToScreen(m_hwnd, &p);
            sx = p.x;
            sy = p.y;
        }
        showContextMenu(sx, sy);
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
            const TerminalCell& cell = visibleCell(row, col);

            int x = col * m_charWidth;
            int y = row * m_charHeight;

            // Set colors; selected cells are drawn with fg/bg swapped (inverted),
            // which fully paints the highlight because SetBkMode is OPAQUE.
            COLORREF fg = cgaToRGB(cell.foreground);
            COLORREF bg = cgaToRGB(cell.background);
            if (isCellSelected(row, col)) {
                std::swap(fg, bg);
            }
            SetTextColor(memDC, fg);
            SetBkColor(memDC, bg);

            // Draw character
            char ch = cell.character;
            if (ch < 32) ch = ' ';
            TextOutA(memDC, x, y, &ch, 1);
        }
    }

    // Draw cursor (only on the live screen; it has no meaning while viewing history)
    if (m_scrollOffset == 0 && m_cursorVisible && GetFocus() == m_hwnd) {
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
    // Scrollback navigation is handled locally and never sent to CP/M. It uses
    // Shift+PageUp/PageDown (plain PageUp/Down still reach CP/M via the keymap)
    // and Ctrl+Home/End to jump to the oldest history / live screen.
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (shift && wParam == VK_PRIOR) { scrollByLines(ROWS - 1); return; }
    if (shift && wParam == VK_NEXT)  { scrollByLines(-(ROWS - 1)); return; }
    if (ctrl  && wParam == VK_HOME)  { scrollByLines((int)m_scrollback.size()); return; }
    if (ctrl  && wParam == VK_END)   { scrollToBottom(); return; }

    // Special keys (arrows, Home/End, Insert/Delete, PageUp/Down, F1-F12) are
    // resolved through the configurable keymap and sent to CP/M as a byte
    // sequence. Printable keys arrive separately via WM_CHAR (handleChar).
    const std::string* seq = m_keymap.find(static_cast<UINT>(wParam));
    if (seq && m_keyCallback) {
        scrollToBottom();   // a key sent to CP/M returns to the live screen
        for (char c : *seq) {
            m_keyCallback(c);
        }
        // Typing dismisses any mouse selection highlight.
        if (m_hasSelection) {
            clearSelection();
        }
    }
}

void TerminalView::handleChar(WPARAM wParam) {
    if (wParam >= 1 && wParam <= 127) {
        char ch = (char)wParam;
        if (m_keyCallback) {
            scrollToBottom();   // typing returns to the live screen
            m_keyCallback(ch);
        }
        // Typing dismisses any mouse selection highlight.
        if (m_hasSelection) {
            clearSelection();
        }
    }
}

//=============================================================================
// Mouse selection and clipboard (right-click Copy/Paste)
//=============================================================================

// Context-menu command ids (local to this window's popup menu).
static constexpr UINT IDM_COPY = 1;
static constexpr UINT IDM_PASTE = 2;

bool TerminalView::pixelToCell(int x, int y, int& row, int& col) const {
    if (m_charWidth <= 0 || m_charHeight <= 0) return false;
    col = x / m_charWidth;
    row = y / m_charHeight;
    col = std::max(0, std::min(col, COLS - 1));
    row = std::max(0, std::min(row, ROWS - 1));
    return true;
}

bool TerminalView::isCellSelected(int row, int col) const {
    if (!(m_selecting || m_hasSelection)) return false;
    int a = m_selAnchorRow * COLS + m_selAnchorCol;
    int b = m_selActiveRow * COLS + m_selActiveCol;
    int lo = std::min(a, b);
    int hi = std::max(a, b);
    int idx = row * COLS + col;
    return idx >= lo && idx <= hi;
}

void TerminalView::clearSelection() {
    if (!(m_selecting || m_hasSelection)) return;
    m_selecting = false;
    m_hasSelection = false;
    invalidate();
}

void TerminalView::handleLButtonDown(int x, int y) {
    int r, c;
    if (!pixelToCell(x, y, r, c)) return;
    m_selAnchorRow = m_selActiveRow = r;
    m_selAnchorCol = m_selActiveCol = c;
    m_selecting = true;
    m_hasSelection = false;
    SetCapture(m_hwnd);
    invalidate();
}

void TerminalView::handleMouseMove(int x, int y) {
    int r, c;
    if (!pixelToCell(x, y, r, c)) return;
    if (r == m_selActiveRow && c == m_selActiveCol) return;  // no change
    m_selActiveRow = r;
    m_selActiveCol = c;
    m_hasSelection = true;
    invalidate();
}

void TerminalView::handleLButtonUp() {
    if (!m_selecting) return;
    ReleaseCapture();
    m_selecting = false;
    // A real selection exists only if the drag actually moved off the anchor.
    m_hasSelection = (m_selAnchorRow != m_selActiveRow) ||
                     (m_selAnchorCol != m_selActiveCol);
    invalidate();
}

void TerminalView::showContextMenu(int screenX, int screenY) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    bool clipHasText = IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
    bool canPaste = clipHasText &&
                    (!m_inputReadyCallback || m_inputReadyCallback());

    AppendMenuW(menu, MF_STRING | (m_hasSelection ? 0 : MF_GRAYED), IDM_COPY, L"Copy");
    AppendMenuW(menu, MF_STRING | (canPaste ? 0 : MF_GRAYED), IDM_PASTE, L"Paste");

    UINT cmd = TrackPopupMenu(menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        screenX, screenY, 0, m_hwnd, nullptr);
    DestroyMenu(menu);

    if (cmd == IDM_COPY) {
        copySelectionToClipboard();
    } else if (cmd == IDM_PASTE) {
        pasteFromClipboard();
    }
}

void TerminalView::copySelectionToClipboard() {
    if (!m_hasSelection) return;

    int a = m_selAnchorRow * COLS + m_selAnchorCol;
    int b = m_selActiveRow * COLS + m_selActiveCol;
    int lo = std::min(a, b);
    int hi = std::max(a, b);
    int r0 = lo / COLS, c0 = lo % COLS;
    int r1 = hi / COLS, c1 = hi % COLS;

    std::wstring out;
    for (int row = r0; row <= r1; ++row) {
        int cs = (row == r0) ? c0 : 0;
        int ce = (row == r1) ? c1 : COLS - 1;
        std::wstring line;
        for (int col = cs; col <= ce; ++col) {
            char ch = visibleCell(row, col).character;
            line += (ch >= 32 && (unsigned char)ch < 127) ? (wchar_t)ch : L' ';
        }
        while (!line.empty() && line.back() == L' ') {
            line.pop_back();  // trim trailing spaces
        }
        out += line;
        if (row != r1) out += L"\r\n";
    }

    if (!OpenClipboard(m_hwnd)) return;

    size_t bytes = (out.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* p = GlobalLock(hMem);
        if (p) {
            memcpy(p, out.c_str(), bytes);
            GlobalUnlock(hMem);
            EmptyClipboard();
            if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
                GlobalFree(hMem);  // ownership not transferred on failure
            }
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
}

void TerminalView::pasteFromClipboard() {
    if (!m_keyCallback) return;
    if (m_inputReadyCallback && !m_inputReadyCallback()) return;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return;
    if (!OpenClipboard(m_hwnd)) return;

    scrollToBottom();   // delivering pasted input returns to the live screen

    HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
    if (hMem) {
        const wchar_t* w = (const wchar_t*)GlobalLock(hMem);
        if (w) {
            for (const wchar_t* s = w; *s; ++s) {
                wchar_t c = *s;
                if (c == L'\n') {
                    if (s != w && *(s - 1) == L'\r') continue;  // CRLF already sent CR
                    c = L'\r';
                }
                if (c == L'\r') { m_keyCallback('\r'); continue; }  // CP/M wants CR
                if (c == L'\t') { m_keyCallback('\t'); continue; }
                if (c >= 32 && c < 127) m_keyCallback((char)c);     // ASCII only
            }
            GlobalUnlock(hMem);
        }
    }
    CloseClipboard();
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

// CSI parameters come from untrusted guest output: clamp digit count, value
// and parameter count so a corrupt stream can't throw (std::stoi would on
// >INT_MAX) or grow the parameter vector without bound.
static constexpr size_t MAX_CSI_PARAM_DIGITS = 6;
static constexpr size_t MAX_CSI_PARAMS = 16;

static int parseCSIParam(const std::string& s) {
    if (s.empty()) return 0;
    long v = strtol(s.c_str(), nullptr, 10);
    return (int)std::min(v, 9999L);
}

void TerminalView::processCSIChar(uint8_t ch) {
    if (ch >= '0' && ch <= '9') {
        if (m_escapeCurrentParam.size() < MAX_CSI_PARAM_DIGITS) {
            m_escapeCurrentParam += (char)ch;
        }
        m_escapeState = EscapeState::CSIParam;
        return;
    }

    if (ch == ';') {
        if (m_escapeParams.size() < MAX_CSI_PARAMS) {
            m_escapeParams.push_back(parseCSIParam(m_escapeCurrentParam));
        }
        m_escapeCurrentParam.clear();
        m_escapeState = EscapeState::CSIParam;
        return;
    }

    // Final character
    if (!m_escapeCurrentParam.empty() && m_escapeParams.size() < MAX_CSI_PARAMS) {
        m_escapeParams.push_back(parseCSIParam(m_escapeCurrentParam));
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
