/*
 * TerminalView.cpp - Terminal Display Component Implementation
 */

#include "pch.h"
#include "TerminalView.h"

static const wchar_t* TERMINAL_CLASS = L"Z80CPM_Terminal";
static bool g_classRegistered = false;

// Swap the foreground and background nibbles of a CGA attribute byte. This is
// how reverse video is rendered, and it is applied at the cell write only - see
// processNormalChar(). It is deliberately not applied to the stored rendition:
// the foreground is four bits and the background three, so a swap is lossy and
// cannot be undone, which is what made ESC[7m ESC[27m throw away the intensity
// bit back when SGR 7 edited m_currentAttr in place.
static inline uint8_t swapAttrNibbles(uint8_t attr) {
    uint8_t fg = attr & 0x0F;
    uint8_t bg = (attr >> 4) & 0x07;
    return (uint8_t)((fg << 4) | bg);
}

// Translate an SGR colour parameter into the CGA colour index the attribute
// byte holds. The two orderings disagree on four of the eight colours:
//
//   ANSI (SGR 30-37 / 40-47):  0 black 1 RED    2 green 3 YELLOW
//                              4 BLUE  5 magenta 6 CYAN  7 white
//   CGA  (attribute nibbles):  0 black 1 BLUE   2 green 3 CYAN
//                              4 RED   5 magenta 6 brown 7 light grey
//
//   ANSI -> CGA:  0->0  1->4  2->2  3->6  4->1  5->5  6->3  7->7
//
// which is a swap of the red and blue bits, bit 0 and bit 2. Storing the ANSI
// index raw made ESC[31m draw blue, ESC[44m fill red, ESC[33m draw cyan and
// ESC[36m draw brown.
//
// The attribute byte stays CGA-ordered on purpose, so this belongs at the SGR
// parse site and nowhere else: a guest can hand over a raw CGA attribute byte
// through the HBIOS VDA "set attribute" call (setAttr() below), and cgaToRGB()
// is a CGA palette. Translating in the renderer or in the guest path would
// break both.
//
// Only a colour index goes through here. The intensity bit (0x08) that SGR 1
// sets is not a colour and must never be passed in. The mapping happens to be
// its own inverse, which is a property of the bit swap rather than something
// any caller relies on.
static inline uint8_t ansiToCGAColor(uint8_t ansi) {
    ansi &= 0x07;
    return (uint8_t)(((ansi & 0x01) << 2) | (ansi & 0x02) | ((ansi >> 2) & 0x01));
}

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
    for (HFONT& f : m_fonts) {
        if (f) {
            DeleteObject(f);
            f = nullptr;
        }
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// One CreateFontW shape, parameterised by the two things a rendition can change
// about the face. Everything else - the charset, the precisions, the quality,
// the family and the name - is identical across the four, which is the point of
// the helper: four copies of this call would be four places for the family or
// the quality to drift apart, and two faces that disagree about hinting do not
// sit on the same baseline.
static HFONT makeTerminalFont(int height, bool bold, bool underline) {
    return CreateFontW(
        height,                  // Height
        0,                       // Width (0 = auto)
        0,                       // Escapement
        0,                       // Orientation
        bold ? FW_BOLD : FW_NORMAL,  // Weight
        FALSE,                   // Italic
        underline ? TRUE : FALSE,    // Underline
        FALSE,                   // Strikeout
        DEFAULT_CHARSET,         // Charset
        OUT_TT_PRECIS,           // Output precision
        CLIP_DEFAULT_PRECIS,     // Clip precision
        CLEARTYPE_QUALITY,       // Quality
        FIXED_PITCH | FF_MODERN, // Pitch and family
        L"Consolas"              // Font name
    );
}

int TerminalView::fontIndexFor(uint8_t flags) {
    return ((flags & TCELL_BOLD) ? 1 : 0) | ((flags & TCELL_UNDERLINE) ? 2 : 0);
}

void TerminalView::createFont() {
    for (HFONT& f : m_fonts) {
        if (f) {
            DeleteObject(f);
            f = nullptr;
        }
    }

    // Scale font height by current monitor DPI. The app is per-monitor DPI v2
    // aware, so CreateFontW's height parameter is in raw device pixels — without
    // this multiplier, "size 20" is only 20 physical pixels on a 4K @ 200% screen.
    UINT dpi = m_hwnd ? GetDpiForWindow(m_hwnd) : 96;
    int scaledHeight = MulDiv(m_fontSize, dpi, 96);

    for (int i = 0; i < 4; i++) {
        m_fonts[i] = makeTerminalFont(scaledHeight, (i & 1) != 0, (i & 2) != 0);
    }

    // The grid is measured from the NORMAL face only, and deliberately so: the
    // cell size decides where all 2000 cells are drawn, and it must not depend
    // on which faces a particular screen happens to contain.
    //
    // Measured rather than assumed, over Consolas at every integer height from
    // 8 to 96 in this DC: tmAveCharWidth is the SAME for all four faces at
    // every one of those heights, so today this choice moves nothing. What does
    // differ is tmMaxCharWidth - 15 regular against 16 bold at height 16 - so
    // the bold face is not simply the regular one at another weight, and the
    // agreement in tmAveCharWidth is a property of this font rather than a rule.
    // Taking the metrics from index 0 is what keeps that a fact about the font
    // instead of a dependency of the layout.
    if (m_hwnd && m_fonts[0]) {
        HDC hdc = GetDC(m_hwnd);
        HFONT oldFont = (HFONT)SelectObject(hdc, m_fonts[0]);

        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        m_charWidth = tm.tmAveCharWidth;
        m_charHeight = tm.tmHeight;

        SelectObject(hdc, oldFont);
        ReleaseDC(m_hwnd, hdc);
    }
}

// The rows of the visible grid that have a blinking cell in them, invalidated
// one row at a time.
//
// Whole-window invalidation was the alternative and is rejected because of what
// it costs when nothing blinks at all: this runs twice a second for as long as
// the window exists, and the ordinary screen - a CP/M prompt - has no
// TCELL_BLINK cell anywhere on it. The loop below then calls InvalidateRect
// zero times, so such a screen produces no WM_PAINT on the blink's account and
// repaints exactly as often as it did before blinking existed.
//
// That is the guarantee, and it is measured rather than asserted: the section
// "the blink tick invalidates blinking rows and nothing else" in
// tests/test_render.cpp reads the update region a tick leaves behind and fails
// if a screen with nothing blinking on it gets anything larger than the cursor
// cell. Counting repaints cannot see this - the cursor's own invalidation
// below already produces one WM_PAINT per tick either way - which is why the
// check reads the region and not the message.
//
// What an idle screen does cost is the scan: 2000 cells, the same ones paint()
// reads on every repaint, twice a second, breaking out of each row at the first
// blinking cell. A counter maintained by the parser would avoid it, but the
// parser is not this commit's to touch.
void TerminalView::invalidateBlinkingRows() {
    if (!m_hwnd) return;
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (visibleCell(row, col).flags & TCELL_BLINK) {
                RECT r;
                r.left = 0;
                r.top = row * m_charHeight;
                r.right = COLS * m_charWidth;
                r.bottom = r.top + m_charHeight;
                InvalidateRect(m_hwnd, &r, FALSE);
                break;
            }
        }
    }
}

// The cell an erase leaves behind. A space in the current rendition, resolved
// through reverse video exactly as a written glyph is.
//
// Erasing paints the current background - that is what ED and EL mean, and it
// is why a program can set a colour, clear the screen, and get a screen of that
// colour. Filling with a hardcoded fg 7 / bg 0 was survivable only while ESC[2J
// also reset the rendition to that same default; once the erase stopped
// resetting it, a cleared region and the text written into it afterwards no
// longer agreed.
//
// The colour carries; the TCELL_* flags do NOT, and that split is deliberate.
// A background colour on a space is the only way an erase can show a colour at
// all, which is the whole point of the paragraph above. Underline and blink on
// a space are not colour: ESC[4m ESC[2J would draw an underscore under all
// 2000 cells and ESC[5m ESC[2J would set the whole screen strobing, neither of
// which is what a program asking to clear its screen meant. Whether a real
// VT100 would agree is not settled here and was not measured; the choice is
// made on what the software this app runs would expect to see, and it is
// written down so that the next person meets an argument rather than a
// surprise. If a guest turns up that wants underlined blanks, this is the line
// to argue with.
TerminalCell TerminalView::blankCell() const {
    const uint8_t attr = m_reverse ? swapAttrNibbles(m_currentAttr) : m_currentAttr;
    TerminalCell c;
    c.character = ' ';
    c.foreground = attr & 0x0F;
    c.background = (attr >> 4) & 0x07;
    c.flags = 0;
    return c;
}

// Erase the screen and home the cursor. Nothing else: not the attribute, not
// the escape parser's state, not the scrolling region, not VT52 or autowrap.
//
// This is what ESC[2J and VT52 ESC E mean. Erase-in-display says what to do
// with the cells and says nothing about the terminal's modes, so a program that
// sets a colour, sets a scrolling region and then clears its screen must come
// back to all three still in force.
//
// ioscpm's clearTerminal() is the same shape and resets neither the rendition
// nor the parser state, which is what made this repository's FEATURE_PARITY
// row 13 the odd one out. It does reset the scrolling region, and that part is
// ioscpm's bug rather than a model to copy: ED is not DECSTBM.
//
// The cursor homing IS deliberate, and is the one thing here a strict VT100
// would not do - a real ED leaves the cursor alone. Both sibling ports home it,
// and CP/M software written against ANSI.SYS expects ESC[2J to home, so it
// stays. Erasing the whole screen and leaving the cursor mid-screen would be
// the more surprising reading for the software this app exists to run.
void TerminalView::eraseScreen() {
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = blankCell();
        }
    }
    m_cursorRow = 0;
    m_cursorCol = 0;
    m_pendingWrap = false;
    invalidate();
}

// Erase the screen AND put every terminal mode back to power-on state. This is
// the machine-level reset - the constructor, and Emulator > Start / Reset - and
// no guest sequence reaches it. Until the split above it was also the ESC[2J
// path, which is why a program's colours died with its screen clear.
//
// The modes below were previously left alone precisely because this function
// was shared with ESC[2J. Now that it is not, a reset resets them: a fresh boot
// has no business inheriting the scrolling region, VT52 mode or the wrap
// setting the last session left behind.
void TerminalView::clear() {
    // The rendition goes back to the default FIRST, because eraseScreen() paints
    // the current one: reset it afterwards and a reset screen would be filled
    // with whatever colour the last session happened to end on.
    m_currentAttr = 0x07;
    m_reverse = false;
    m_currentFlags = 0;

    eraseScreen();

    m_escapeState = EscapeState::Normal;
    m_escapeParams.clear();
    m_escapeCurrentParam.clear();
    m_escapePrivate = false;
    m_savedCursorRow = 0;
    m_savedCursorCol = 0;
    m_savedAttr = 0x07;
    m_savedReverse = false;
    // The saved rendition is reset for the same reason as the saved attribute
    // beside it: a DECRC (ESC 8) issued after a machine reset, with no DECSC
    // since, must restore the default face and not the one the last session
    // happened to save.
    m_savedFlags = 0;
    // m_bellEnabled is deliberately NOT reset here. It is a user preference
    // arriving through setBellEnabled(), not part of the terminal's power-on
    // state, and clear() is also the ESC c (RIS) path - a guest must not be
    // able to switch the bell back on for a user who turned it off.
    m_scrollTop = 0;
    m_scrollBottom = ROWS - 1;
    m_vt52Mode = false;
    m_autoWrap = true;
    m_cursorEnabled = true;
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
        // Not m_currentFlags: this entry point takes the complete rendition as
        // arguments and never consults m_currentAttr either, so anything the
        // caller did not pass is off. Assigning the fields individually is
        // also why the line is needed at all - without it the cell would keep
        // the underline of whatever the parser last wrote there.
        m_cells[row][col].flags = 0;
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
            m_cells[row][col] = blankCell();
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

// Replace the rendition wholesale. This is the VDA video path, not SGR: the
// caller hands over a complete CGA attribute byte rather than modifying the
// current one.
void TerminalView::setAttr(uint8_t attr) {
    m_currentAttr = attr;
    // The byte given is the rendition as it should appear, so it is already in
    // the un-reversed domain m_currentAttr holds - and it carries no notion of
    // reverse video. Leaving the flag set from an earlier ESC[7m would show the
    // caller's colours swapped.
    m_reverse = false;
    // Same argument for the TCELL_* bits: a VDA attribute byte says nothing
    // about underline or blink, so an ESC[4m still in force would underline
    // text the guest asked to be drawn in a plain attribute.
    m_currentFlags = 0;
}

void TerminalView::outputChar(uint8_t ch) {
    processChar(ch);
}

const TerminalCell& TerminalView::cellAt(int row, int col) const {
    if (row < 0) row = 0;
    if (row >= ROWS) row = ROWS - 1;
    if (col < 0) col = 0;
    if (col >= COLS) col = COLS - 1;
    return m_cells[row][col];
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

    case WM_SYSKEYDOWN: {
        // Alt+<key> and bare F10 both arrive here rather than as WM_KEYDOWN,
        // because both are how Windows reaches the menu bar. Either may be
        // taken for CP/M, but only on an exact match: findExact, not find,
        // because the falling-back find() would answer "bound" for Alt+Left on
        // the strength of plain Left's binding and swallow every Alt press.
        const unsigned mods = currentKeyMods();
        if (m_keymap.findExact(static_cast<UINT>(wParam), mods)) {
            handleKeyDown(wParam);
            return 0;
        }
        // Bare F10 - no Alt held - is the one case that may use the plain
        // binding. It is a function key the guest expects, and it only arrives
        // here at all because Windows reserves it for the menu.
        if (wParam == VK_F10 && mods == keymap::KM_MOD_NONE && m_keymap.find(VK_F10)) {
            handleKeyDown(wParam);
            return 0;
        }
        break;  // fall through to DefWindowProc for normal Alt/menu handling
    }

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
            // One tick drives two blinks. Timer 1 already existed for the
            // cursor at exactly the rate text blink wants, and a second timer
            // would have given a screen showing both a cursor and blinking text
            // two phases free to drift apart.
            //
            // The text phase is advanced ABOVE the scrolled-back early return
            // below, not under it. That return exists because the cursor is not
            // drawn at all while the view is scrolled back - paint() tests
            // m_scrollOffset == 0 before drawing it - so toggling its phase
            // would be invisible work. Text is different: a blinking cell that
            // has scrolled into history is still on the screen and still
            // carries TCELL_BLINK, and freezing it would mean text that stops
            // blinking because the user reached for the scroll wheel.
            m_textBlinkOn = !m_textBlinkOn;
            invalidateBlinkingRows();

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

    // Select the plain face to start with, and track what is selected across
    // the whole grid. TextOutA is called once per cell - 2000 times a repaint,
    // one character at a time, so there are no runs to coalesce - and the face
    // is changed only where a cell asks for a different one. Selecting per cell
    // instead would have added 2000 SelectObject calls to a repaint to change
    // the face perhaps twice.
    //
    // selectedStyle starts at 0 to match the SelectObject on the line above, so
    // an ordinary screen - no bold, no underline - makes exactly one
    // SelectObject, which is what this code did before the four faces existed.
    HFONT oldFont = (HFONT)SelectObject(memDC, m_fonts[0]);
    int selectedStyle = 0;
    SetBkMode(memDC, OPAQUE);

    // Draw characters
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            const TerminalCell& cell = visibleCell(row, col);

            int x = col * m_charWidth;
            int y = row * m_charHeight;

            // The face this cell's rendition asks for. The fallback to index 0
            // is for a CreateFontW that returned null: SelectObject(nullptr) is
            // a no-op that leaves the PREVIOUS cell's face selected, so without
            // it a missing bold face would show as bold leaking rightwards
            // along the row rather than as bold missing.
            int style = fontIndexFor(cell.flags);
            if (!m_fonts[style]) style = 0;
            if (style != selectedStyle) {
                SelectObject(memDC, m_fonts[style]);
                selectedStyle = style;
            }

            // Set colors; selected cells are drawn with fg/bg swapped (inverted),
            // which fully paints the highlight because SetBkMode is OPAQUE.
            COLORREF fg = cgaToRGB(cell.foreground);
            COLORREF bg = cgaToRGB(cell.background);
            if (isCellSelected(row, col)) {
                std::swap(fg, bg);
            }

            // THE ORDER MATTERS, and this is the way round it has to be: the
            // selection swap first, then blink collapses the foreground onto
            // whatever background survived it.
            //
            // Blinking off is drawn as foreground = background: the cell is
            // still painted, in its own background, so the glyph and the font's
            // underline both vanish while the background colour - which is not
            // part of what blinks - stays exactly as it was. Skipping the
            // TextOutA instead would NOT leave the cell alone: this is a
            // repaint into a fresh bitmap that was filled with black a few
            // lines above, so an unpainted cell shows black rather than its own
            // background, and a blinking cell on a coloured background would
            // strobe the background too.
            //
            // The other order is wrong, and it was tried rather than argued: in
            // a scratch copy with the collapse moved above the swap, collapsing
            // first makes fg == bg == the cell's background and the swap leaves
            // them equal, so a SELECTED blinking cell spends half its life
            // painted in the text background instead of the highlight - the
            // selection blinks rather than the text. The section "a selected
            // blinking cell keeps its highlight while the character goes" in
            // tests/test_render.cpp exists for exactly that mutation and is the
            // only check in the suite that fails on it.
            if ((cell.flags & TCELL_BLINK) && !m_textBlinkOn) {
                fg = bg;
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
    if (m_scrollOffset == 0 && m_cursorEnabled && m_cursorVisible && GetFocus() == m_hwnd) {
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

// The modifier keys held right now, as a keymap modifier mask. Read from the
// keyboard state rather than passed down, because WM_KEYDOWN and WM_SYSKEYDOWN
// both need it and neither carries it in its parameters.
unsigned TerminalView::currentKeyMods() {
    unsigned mods = keymap::KM_MOD_NONE;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= keymap::KM_MOD_CTRL;
    if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= keymap::KM_MOD_SHIFT;
    if (GetKeyState(VK_MENU)    & 0x8000) mods |= keymap::KM_MOD_ALT;
    return mods;
}

void TerminalView::handleKeyDown(WPARAM wParam) {
    // Read the modifiers once and use the same value for both decisions below.
    // This function used to take its own pair of GetKeyState readings for the
    // scrollback test and then call currentKeyMods() again for the map lookup.
    //
    // That was not a race, and this is not a race fix. GetKeyState - unlike
    // GetAsyncKeyState - reports the calling thread's virtual key state as of
    // the last message the thread pulled from its queue, not the live hardware,
    // and nothing between the two old readings pumped a message, so they were
    // always identical. The failure that would have needed them to differ was
    // structurally impossible in any case: each of the four scrollback tests
    // this replaced ended in its own return, so a press the app took never
    // reached the m_keymap.find() below to be looked up a second time.
    //
    // What reading once buys is one source of truth for two decisions that have
    // to agree about what is held. The old code spelled "Ctrl is held" twice
    // and differently - raw GetKeyState bit tests here, currentKeyMods()' mask
    // at the lookup - so the two could be given different answers by an edit to
    // either. There is now one place to change if the modifier source ever
    // becomes GetAsyncKeyState, or a value carried down from the WM_KEYDOWN
    // that started this instead of read back from the keyboard state at all.
    const unsigned mods = currentKeyMods();

    // Scrollback navigation is handled locally and never sent to CP/M. Which
    // combinations those are, and the words describing each, live in
    // keymap::reservedFor() so that a Settings dialog refuses to bind them for
    // the same reason and in the same terms. This test runs before the
    // m_keymap.find() below, so a config that binds one of them is ignored -
    // that is what makes them worth naming somewhere a dialog can read.
    //
    // reservedFor() matches on "at least these modifiers" rather than an exact
    // set, which reproduces the four independent shift/ctrl tests that used to
    // stand here: Ctrl+Shift+PageUp scrolls back, as it always has. Preserved
    // rather than tightened - see the comment on reservedFor().
    if (keymap::reservedFor(static_cast<int>(wParam), mods)) {
        switch (wParam) {
        case VK_PRIOR: scrollByLines(ROWS - 1); break;
        case VK_NEXT:  scrollByLines(-(ROWS - 1)); break;
        case VK_HOME:  scrollByLines((int)m_scrollback.size()); break;
        case VK_END:   scrollToBottom(); break;
        }
        return;
    }

    // Special keys (arrows, Home/End, Insert/Delete, PageUp/Down, F1-F12) are
    // resolved through the configurable keymap and sent to CP/M as a byte
    // sequence. Printable keys arrive separately via WM_CHAR (handleChar).
    //
    // The modifiers are part of the lookup, so Ctrl+Left can carry a different
    // sequence from Left. Anything with no binding of its own falls back to the
    // unmodified one, which is what every modified press used to get.
    const std::string* seq = m_keymap.find(static_cast<UINT>(wParam), mods);
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
    // 0 is admitted: Ctrl+@ / Ctrl+Space / Ctrl+2 all produce WM_CHAR 0, and NUL
    // is a real byte a CP/M program can be waiting for. The old lower bound of 1
    // dropped it before it could reach the guest. WPARAM is unsigned, so the
    // upper bound is the only one needed.
    if (wParam <= 127) {
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


// ---------------------------------------------------------------------------
// Scrolling region and the editing sequences that operate inside it
// ---------------------------------------------------------------------------

void TerminalView::scrollRegionUp(int lines) {
    if (lines <= 0) return;
    if (m_scrollTop == 0 && m_scrollBottom == ROWS - 1) {
        // Whole screen: reuse the existing path so the lines leaving the top
        // still reach the scrollback.
        scrollUp(lines);
        return;
    }
    const int height = m_scrollBottom - m_scrollTop + 1;
    if (lines > height) lines = height;
    // A partial region is a program's own construction (a status line, a text
    // window). What falls out of the top of it was never on screen above the
    // region, so it is not history and must not enter the scrollback.
    for (int row = m_scrollTop; row <= m_scrollBottom - lines; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = m_cells[row + lines][col];
        }
    }
    for (int row = m_scrollBottom - lines + 1; row <= m_scrollBottom; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = blankCell();
        }
    }
    invalidate();
}

void TerminalView::scrollRegionDown(int lines) {
    if (lines <= 0) return;
    const int height = m_scrollBottom - m_scrollTop + 1;
    if (lines > height) lines = height;
    for (int row = m_scrollBottom; row >= m_scrollTop + lines; row--) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = m_cells[row - lines][col];
        }
    }
    for (int row = m_scrollTop; row < m_scrollTop + lines; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = blankCell();
        }
    }
    invalidate();
}

// LF and IND: down one line, scrolling the region when already at its bottom.
void TerminalView::lineFeed() {
    m_pendingWrap = false;
    if (m_cursorRow < m_scrollTop) {
        if (m_cursorRow < ROWS - 1) m_cursorRow++;
    } else if (m_cursorRow >= m_scrollBottom) {
        if (m_cursorRow == m_scrollBottom) scrollRegionUp(1);
        else if (m_cursorRow < ROWS - 1) m_cursorRow++;
    } else {
        m_cursorRow++;
    }
    invalidate();
}

void TerminalView::insertLines(int n) {
    if (m_cursorRow < m_scrollTop || m_cursorRow > m_scrollBottom) return;
    const int count = std::min(n, m_scrollBottom - m_cursorRow + 1);
    for (int row = m_scrollBottom; row >= m_cursorRow + count; row--) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = m_cells[row - count][col];
        }
    }
    for (int row = m_cursorRow; row < m_cursorRow + count; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = blankCell();
        }
    }
    m_cursorCol = 0;
    m_pendingWrap = false;
    invalidate();
}

void TerminalView::deleteLines(int n) {
    if (m_cursorRow < m_scrollTop || m_cursorRow > m_scrollBottom) return;
    const int count = std::min(n, m_scrollBottom - m_cursorRow + 1);
    for (int row = m_cursorRow; row <= m_scrollBottom - count; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = m_cells[row + count][col];
        }
    }
    for (int row = m_scrollBottom - count + 1; row <= m_scrollBottom; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = blankCell();
        }
    }
    m_cursorCol = 0;
    m_pendingWrap = false;
    invalidate();
}

void TerminalView::insertChars(int n) {
    const int count = std::min(n, COLS - m_cursorCol);
    if (count <= 0) return;
    for (int col = COLS - 1; col >= m_cursorCol + count; col--) {
        m_cells[m_cursorRow][col] = m_cells[m_cursorRow][col - count];
    }
    for (int col = m_cursorCol; col < m_cursorCol + count; col++) {
        m_cells[m_cursorRow][col] = blankCell();
    }
    m_pendingWrap = false;
    invalidate();
}

void TerminalView::deleteChars(int n) {
    const int count = std::min(n, COLS - m_cursorCol);
    if (count <= 0) return;
    for (int col = m_cursorCol; col < COLS - count; col++) {
        m_cells[m_cursorRow][col] = m_cells[m_cursorRow][col + count];
    }
    for (int col = COLS - count; col < COLS; col++) {
        m_cells[m_cursorRow][col] = blankCell();
    }
    m_pendingWrap = false;
    invalidate();
}

void TerminalView::eraseChars(int n) {
    const int last = std::min(m_cursorCol + std::max(n, 1) - 1, COLS - 1);
    for (int col = m_cursorCol; col <= last; col++) {
        m_cells[m_cursorRow][col] = blankCell();
    }
    m_pendingWrap = false;
    invalidate();
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
    case EscapeState::Vt52Row:
        // VT52 direct cursor address: the row is the byte value biased by 0x20.
        m_vt52CursorRow = std::max(0, std::min((int)ch - 0x20, ROWS - 1));
        m_escapeState = EscapeState::Vt52Col;
        break;
    case EscapeState::ConsumeOne:
        // The parameter byte of a character-set or line-size designator. Not
        // acted on, but it has to be eaten or it prints as a stray glyph -
        // ESC ( B used to leave a "B" on screen.
        m_escapeState = EscapeState::Normal;
        break;
    case EscapeState::Vt52Col:
        m_cursorRow = m_vt52CursorRow;
        m_cursorCol = std::max(0, std::min((int)ch - 0x20, COLS - 1));
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;
    }
}

void TerminalView::processNormalChar(uint8_t ch) {
    switch (ch) {
    case 0x07:  // Bell
        ringBell();
        break;

    case 0x08:  // Backspace
        m_pendingWrap = false;
        if (m_cursorCol > 0) {
            m_cursorCol--;
        }
        invalidate();
        break;

    case 0x09:  // Tab
        m_pendingWrap = false;
        m_cursorCol = std::min((m_cursorCol + 8) & ~7, COLS - 1);
        invalidate();
        break;

    case 0x0A:  // Line feed, with an implicit carriage return.
        // Both mobile ports do this, and without it a Unix-format file TYPEd
        // with bare LFs stair-steps down the screen.
        m_cursorCol = 0;
        lineFeed();
        break;

    case 0x0D:  // Carriage return
        m_pendingWrap = false;
        m_cursorCol = 0;
        invalidate();
        break;

    case 0x1B:  // ESC
        m_escapeState = EscapeState::Escape;
        m_escapeParams.clear();
        m_escapeCurrentParam.clear();
        m_escapePrivate = false;
        break;

    default:
        // Printable character
        if (ch >= 0x20 && ch <= 0x7E) {
            // Resolve a wrap armed by the previous last-column write.
            if (m_pendingWrap) {
                m_cursorCol = 0;
                lineFeed();
                m_pendingWrap = false;
            }
            // Reverse video is resolved here, at the write, and nowhere else.
            // m_currentAttr always holds the un-reversed rendition.
            const uint8_t attr = m_reverse ? swapAttrNibbles(m_currentAttr)
                                           : m_currentAttr;
            m_cells[m_cursorRow][m_cursorCol].character = (char)ch;
            m_cells[m_cursorRow][m_cursorCol].foreground = attr & 0x0F;
            m_cells[m_cursorRow][m_cursorCol].background = (attr >> 4) & 0x07;
            // The rest of the rendition - underline, blink, and the bold flag
            // that survives the swap above. This site and writeChar() are the
            // only two in this file that assign a cell's fields one at a time;
            // everywhere else copies a whole TerminalCell or assigns
            // blankCell(), so the new field travels for free.
            m_cells[m_cursorRow][m_cursorCol].flags = m_currentFlags;

            if (m_cursorCol >= COLS - 1) {
                // At the rightmost column: arm the wrap rather than taking it,
                // as a real VT100 does. Writing the bottom-right cell used to
                // scroll the screen there and then, which corrupts every
                // full-screen layout that draws into the corner.
                if (m_autoWrap) {
                    m_pendingWrap = true;
                }
            } else {
                m_cursorCol++;
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

    case '7':  // DECSC - save cursor AND rendition
        // A real VT100 saves the graphic rendition with the position, which is
        // what lets a program park the cursor, draw a status line in its own
        // colours, and restore both with ESC 8. Saving only the position sent
        // the caller back to the right cell wearing the status line's colours.
        // (CSI s / CSI u below are the ANSI.SYS pair and save position alone.)
        m_savedCursorRow = m_cursorRow;
        m_savedCursorCol = m_cursorCol;
        m_savedAttr = m_currentAttr;
        m_savedReverse = m_reverse;
        m_savedFlags = m_currentFlags;
        m_escapeState = EscapeState::Normal;
        break;

    case '8':  // DECRC - restore cursor AND rendition
        m_pendingWrap = false;
        m_cursorRow = m_savedCursorRow;
        m_cursorCol = m_savedCursorCol;
        m_currentAttr = m_savedAttr;
        m_reverse = m_savedReverse;
        m_currentFlags = m_savedFlags;
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'D':
        if (m_vt52Mode) {
            // VT52 cursor left
            m_pendingWrap = false;
            if (m_cursorCol > 0) m_cursorCol--;
        } else {
            lineFeed();   // VT100 index: down one line, honouring the region
        }
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'M':  // Reverse index: up one line, scrolling the region at its top
        m_pendingWrap = false;
        if (m_cursorRow == m_scrollTop) {
            scrollRegionDown(1);
        } else if (m_cursorRow > 0) {
            m_cursorRow--;
        }
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'E':
        if (m_vt52Mode) {
            // Heath/Zenith VT52: clear the screen and home the cursor. The
            // screen only - eraseScreen(), not clear(), or the guest loses the
            // attribute it set along with the text.
            eraseScreen();
        } else {
            // VT100 next line
            m_cursorCol = 0;
            lineFeed();
        }
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    // ---- VT52 -----------------------------------------------------------
    // These are VT52-exclusive, so receiving one is itself the signal that the
    // guest is driving a VT52 and switches the mode on. ANSI/VT100 output is
    // unaffected until one arrives, so nothing that worked before changes.

    case 'A':  // VT52 cursor up
        m_vt52Mode = true;
        m_pendingWrap = false;
        if (m_cursorRow > 0) m_cursorRow--;
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'B':  // VT52 cursor down
        m_vt52Mode = true;
        m_pendingWrap = false;
        m_cursorRow = std::min(m_cursorRow + 1, ROWS - 1);
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'C':  // VT52 cursor right
        m_vt52Mode = true;
        m_pendingWrap = false;
        m_cursorCol = std::min(m_cursorCol + 1, COLS - 1);
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'H':  // VT52 cursor home. In ANSI this is HTS, which is unsupported,
               // so act only in VT52 mode rather than guessing.
        if (m_vt52Mode) {
            m_pendingWrap = false;
            m_cursorRow = 0;
            m_cursorCol = 0;
            invalidate();
        }
        m_escapeState = EscapeState::Normal;
        break;

    case 'I':  // VT52 reverse line feed
        m_vt52Mode = true;
        m_pendingWrap = false;
        if (m_cursorRow > 0) {
            m_cursorRow--;
        }
        // At the top row there is no downward-scroll helper here, so clamp
        // rather than scrolling the wrong way.
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'J':  // VT52 erase to end of screen
        m_vt52Mode = true;
        clearFromCursor();
        m_escapeState = EscapeState::Normal;
        break;

    case 'K':  // VT52 erase to end of line
        m_vt52Mode = true;
        for (int col = m_cursorCol; col < COLS; col++) {
            m_cells[m_cursorRow][col] = blankCell();
        }
        m_escapeState = EscapeState::Normal;
        invalidate();
        break;

    case 'Y':  // VT52 direct cursor address; two bytes follow
        m_vt52Mode = true;
        m_escapeState = EscapeState::Vt52Row;
        break;

    case 'F':  // VT52 enter graphics mode
    case 'G':  // VT52 exit graphics mode
        // The glyphs are not remapped, but the sequence must be consumed and
        // it identifies the guest as a VT52.
        m_vt52Mode = true;
        m_escapeState = EscapeState::Normal;
        break;

    case 'Z':  // Identify
        sendAnswerback(m_vt52Mode ? "\033/Z" : "\033[?1;0c");
        m_escapeState = EscapeState::Normal;
        break;

    case '<':  // Leave VT52, return to ANSI
        m_vt52Mode = false;
        m_escapeState = EscapeState::Normal;
        break;

    case 'c':  // RIS - reset to initial state
        // clear() is exactly this: erase the screen and put every mode back to
        // power-on. It was already written for the machine reset and ESC c was
        // being swallowed, so the sequence a program sends to get a known-good
        // terminal did nothing at all. The scrollback is deliberately kept -
        // the history above the screen is the user's, not the guest's, and
        // MainWindow clears it separately when the machine itself restarts.
        clear();
        m_escapeState = EscapeState::Normal;
        break;

    case '=':  // Keypad application mode
    case '>':  // Keypad numeric mode
        // Accepted and ignored; the keypad is not emulated separately.
        m_escapeState = EscapeState::Normal;
        break;

    case '(':  // Designate G0 character set
    case ')':  // Designate G1
    case '*':  // Designate G2
    case '+':  // Designate G3
    case '#':  // Line size (DECDHL/DECSWL/DECDWL)
    case ' ':  // Select 7/8-bit controls
        // Each takes one further byte. The glyphs are not remapped, but the
        // parameter must be consumed - ESC ( B used to print its "B".
        m_escapeState = EscapeState::ConsumeOne;
        break;

    default:
        m_escapeState = EscapeState::Normal;
        break;
    }
}

void TerminalView::sendAnswerback(const char* s) {
    if (!m_keyCallback) return;
    for (const char* p = s; *p; ++p) {
        m_keyCallback(*p);
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
    // Private-parameter markers. '?' introduces the DEC private modes; '<', '='
    // and '>' introduce the secondary and tertiary device-attribute forms.
    // These are not final characters, and treating them as one is what made
    // ESC[?25l terminate early and print its own tail as text.
    if (ch == '?' || ch == '<' || ch == '=' || ch == '>') {
        m_escapePrivate = true;
        m_escapeState = EscapeState::CSIParam;
        return;
    }

    // Intermediate bytes (space through '/') belong to the sequence and are
    // not acted on here, but they must not end it either.
    if (ch >= 0x20 && ch <= 0x2F) {
        m_escapeState = EscapeState::CSIParam;
        return;
    }

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
        m_pendingWrap = false;
        m_cursorRow = std::max(m_cursorRow - std::max(p1, 1), 0);
        invalidate();
        break;

    case 'B':  // Cursor down
        m_pendingWrap = false;
        m_cursorRow = std::min(m_cursorRow + std::max(p1, 1), ROWS - 1);
        invalidate();
        break;

    case 'C':  // Cursor forward
        m_pendingWrap = false;
        m_cursorCol = std::min(m_cursorCol + std::max(p1, 1), COLS - 1);
        invalidate();
        break;

    case 'D':  // Cursor back
        m_pendingWrap = false;
        m_cursorCol = std::max(m_cursorCol - std::max(p1, 1), 0);
        invalidate();
        break;

    case 'H':
    case 'f':  // Cursor position
        m_pendingWrap = false;
        m_cursorRow = std::max(0, std::min(std::max(p1, 1) - 1, ROWS - 1));
        m_cursorCol = std::max(0, std::min(std::max(p2, 1) - 1, COLS - 1));
        invalidate();
        break;

    case 'G':   // Cursor horizontal absolute
    case '`':   // ... and its HPA alias
        m_pendingWrap = false;
        m_cursorCol = std::max(0, std::min(std::max(p1, 1) - 1, COLS - 1));
        invalidate();
        break;

    case 'd':  // Vertical position absolute
        m_pendingWrap = false;
        m_cursorRow = std::max(0, std::min(std::max(p1, 1) - 1, ROWS - 1));
        invalidate();
        break;

    case 'L':  // Insert lines
        insertLines(std::max(p1, 1));
        break;

    case 'M':  // Delete lines
        deleteLines(std::max(p1, 1));
        break;

    case '@':  // Insert characters
        insertChars(std::max(p1, 1));
        break;

    case 'P':  // Delete characters
        deleteChars(std::max(p1, 1));
        break;

    case 'X':  // Erase characters
        eraseChars(std::max(p1, 1));
        break;

    case 'S':  // Scroll up within the region
        // Clamped to the region height: past that a further scroll is a no-op,
        // and the parameter can be six digits long.
        for (int i = 0, n = std::min(std::max(p1, 1), ROWS); i < n; i++) {
            scrollRegionUp(1);
        }
        break;

    case 'T':  // Scroll down within the region
        for (int i = 0, n = std::min(std::max(p1, 1), ROWS); i < n; i++) {
            scrollRegionDown(1);
        }
        break;

    case 'r': {  // DECSTBM - set the scrolling region (1-based); ESC[r resets
        m_pendingWrap = false;
        int top = (m_escapeParams.size() > 0 && m_escapeParams[0] > 0)
                      ? m_escapeParams[0] - 1 : 0;
        // Clamp rather than reject: a program written for a 24-line console
        // sends ESC[1;24r on this 25-line screen, and dropping the sequence
        // outright would leave the previous region in force.
        int bottom = (m_escapeParams.size() > 1 && m_escapeParams[1] > 0)
                         ? m_escapeParams[1] - 1 : ROWS - 1;
        if (bottom > ROWS - 1) bottom = ROWS - 1;
        if (top < bottom) {
            m_scrollTop = top;
            m_scrollBottom = bottom;
            // The cursor homes after the region is set.
            m_cursorRow = 0;
            m_cursorCol = 0;
            invalidate();
        }
        break;
    }

    case 'J':  // Erase in display
        switch (p1) {
        case 0: clearFromCursor(); break;
        case 1: clearToCursor(); break;
        // eraseScreen(), not clear(): ED 2 erases cells and says nothing about
        // the terminal's modes. See the comments on both functions.
        case 2: eraseScreen(); break;
        }
        break;

    case 'K':  // Erase in line
        switch (p1) {
        case 0:  // Clear to end of line
            for (int col = m_cursorCol; col < COLS; col++) {
                m_cells[m_cursorRow][col] = blankCell();
            }
            break;
        case 1:  // Clear to beginning
            for (int col = 0; col <= m_cursorCol; col++) {
                m_cells[m_cursorRow][col] = blankCell();
            }
            break;
        case 2:  // Clear entire line
            for (int col = 0; col < COLS; col++) {
                m_cells[m_cursorRow][col] = blankCell();
            }
            break;
        }
        invalidate();
        break;

    case 'm':  // SGR (Select Graphic Rendition)
        // Only the non-private form is a rendition. ESC[>4;2m and ESC[>m are
        // xterm's modifyOtherKeys and say nothing about colour; without this
        // guard the bare one was read as ESC[m and reset the whole rendition.
        if (m_escapePrivate) break;
        if (m_escapeParams.empty()) {
            // ESC[m is ESC[0m, which means reverse video is cleared too.
            // Assigning the default attribute directly left m_reverse set, so a
            // later ESC[27m swapped a byte that had never been swapped and put
            // the whole terminal into reverse.
            applySGR(0);
        } else {
            for (size_t i = 0; i < m_escapeParams.size(); i++) {
                const int param = m_escapeParams[i];
                // Extended colour: ESC[38;5;<n>m and ESC[38;2;<r>;<g>;<b>m, and
                // 48 for the background. This terminal is CGA - sixteen
                // foregrounds, eight backgrounds - so there is nothing to apply,
                // but the subparameters still have to be stepped over. Read as
                // parameters in their own right they land as colours: the "44"
                // of ESC[38;5;44m set a red background.
                if (param == 38 || param == 48) {
                    if (i + 1 < m_escapeParams.size()) {
                        const int form = m_escapeParams[i + 1];
                        if (form == 5)      i += 2;   // ;5;<index>
                        else if (form == 2) i += 4;   // ;2;<r>;<g>;<b>
                        else                i += 1;
                    }
                    continue;
                }
                applySGR(param);
            }
        }
        break;

    case 's':  // Save cursor
        m_savedCursorRow = m_cursorRow;
        m_savedCursorCol = m_cursorCol;
        break;

    case 'u':  // Restore cursor
        m_pendingWrap = false;
        m_cursorRow = m_savedCursorRow;
        m_cursorCol = m_savedCursorCol;
        invalidate();
        break;

    case 'h':  // Set mode
        if (m_escapePrivate) {
            for (int p : m_escapeParams) {
                if (p == 2) m_vt52Mode = false;      // DECANM: select ANSI
                else if (p == 7) m_autoWrap = true;  // DECAWM on
                else if (p == 25) {                  // DECTCEM: show cursor
                    m_cursorEnabled = true;
                    invalidate();
                }
            }
        }
        break;

    case 'l':  // Reset mode
        if (m_escapePrivate) {
            for (int p : m_escapeParams) {
                if (p == 2) m_vt52Mode = true;       // DECANM: select VT52
                else if (p == 7) {                   // DECAWM off
                    m_autoWrap = false;
                    // Drop a wrap armed while it was on, or the next character
                    // would still wrap after autowrap was switched off.
                    m_pendingWrap = false;
                }
                else if (p == 25) {                  // DECTCEM: hide cursor
                    m_cursorEnabled = false;
                    invalidate();
                }
            }
        }
        break;

    case 'n':  // Device status report
        if (!m_escapePrivate) {
            if (p1 == 6) {
                // Cursor position report, 1-based. Built with std::to_string
                // rather than a format string: no buffer to size, and nothing
                // guest-controlled reaches a printf.
                std::string reply = "\033[" + std::to_string(m_cursorRow + 1)
                                  + ";" + std::to_string(m_cursorCol + 1) + "R";
                sendAnswerback(reply.c_str());
            } else if (p1 == 5) {
                sendAnswerback("\033[0n");   // terminal OK
            }
        }
        break;

    case 'c':  // Device attributes
        // Only the primary form answers; the '>' and '=' variants ask for
        // something this terminal does not claim to be.
        if (!m_escapePrivate && p1 == 0) {
            sendAnswerback("\033[?1;0c");   // a VT100 with no options
        }
        break;
    }
}

void TerminalView::applySGR(int param) {
    switch (param) {
    case 0:  // Reset
        m_currentAttr = 0x07;
        m_reverse = false;
        m_currentFlags = 0;
        break;
    case 1:  // Bold
        // Both halves, and the 0x08 is not optional. The CGA intensity bit is
        // the only thing that makes bold visible today, and five checks in
        // tests/test_vt52.cpp pin it: "SGR 1 sets the bold (intensity) bit",
        // "SGR 22 clears the bold bit", "ESC[1;37m is bright white",
        // "ESC[37;1m is bright white too" and "a colour after bold keeps the
        // intensity bit". TCELL_BOLD is the record a renderer can draw a
        // heavier face from, and the only one that survives the reverse-video
        // swap.
        m_currentAttr |= 0x08;
        m_currentFlags |= TCELL_BOLD;
        break;
    case 22:  // Bold off
        m_currentAttr &= (uint8_t)~0x08;
        m_currentFlags &= (uint8_t)~TCELL_BOLD;
        break;
    case 4:  // Underline
        m_currentFlags |= TCELL_UNDERLINE;
        break;
    case 24:  // Underline off
        m_currentFlags &= (uint8_t)~TCELL_UNDERLINE;
        break;
    case 5:  // Slow blink
    case 6:  // Rapid blink
        // One flag for both rates. Nothing here can tell them apart: the only
        // blink this terminal owns is the cursor timer, and a renderer reading
        // TCELL_BLINK would have exactly one phase to work with. ECMA-48
        // separates them; a single bit is the honest thing to store until
        // something can draw two speeds.
        m_currentFlags |= TCELL_BLINK;
        break;
    case 25:  // Blink off - both rates
        m_currentFlags &= (uint8_t)~TCELL_BLINK;
        break;
    case 7:  // Reverse on
        // A flag, not an edit. Setting it twice is naturally idempotent, and
        // nothing about the stored rendition changes, so 27 can always undo it
        // exactly.
        m_reverse = true;
        break;
    case 27:  // Reverse off
        m_reverse = false;
        break;
    default:
        // SGR 21 falls through here on purpose and does nothing. ECMA-48 calls
        // it double-underline; several terminals treat it as bold-off, and the
        // two readings disagree about the bit this file would have to touch.
        // Nothing available here can settle which a CP/M guest meant, and
        // guessing wrong is worse than ignoring it, so it stays a documented
        // no-op rather than becoming a plausible-looking case above.
        //
        // 0xF8 keeps the background nibble AND bit 3, which is the intensity
        // bit SGR 1 sets. Masking with 0xF0 instead - which is what this did -
        // cleared bold every time a colour arrived, so ESC[1;37m came out dim
        // while ESC[37;1m came out bright.
        //
        // The parameter is an ANSI colour index and the attribute byte is
        // CGA-ordered; see ansiToCGAColor() at the top of this file. Stored
        // raw, a program asking for red got blue.
        if (param >= 30 && param <= 37) {
            m_currentAttr = (uint8_t)((m_currentAttr & 0xF8)
                                      | ansiToCGAColor((uint8_t)(param - 30)));
        } else if (param >= 40 && param <= 47) {
            m_currentAttr = (uint8_t)((m_currentAttr & 0x0F)
                                      | (ansiToCGAColor((uint8_t)(param - 40)) << 4));
        } else if (param >= 90 && param <= 97) {
            // The bright half. These were dropped entirely until now, so
            // ESC[91m came out in whatever colour was already set - measured
            // as CGA 7 from a fresh reset, which is to say indistinguishable
            // from no colour at all. cpmdroid's TerminalView.kt has had the
            // branch since its ANSI fix; this is the same rule.
            //
            // 0xF0 and not 0xF8: the bright bit IS bit 3, so a bright colour
            // sets the intensity SGR 1 would have set, exactly as SGR 1
            // followed by SGR 3x does. Preserving bit 3 here would make
            // ESC[22m unable to dim a colour that was asked for bright.
            m_currentAttr = (uint8_t)((m_currentAttr & 0xF0)
                                      | ansiToCGAColor((uint8_t)(param - 90)) | 0x08);
        } else if (param >= 100 && param <= 107) {
            // The background nibble is three bits wide - bit 7 is blink on
            // real CGA hardware, and cgaToRGB() masks with 0x0F, so a bright
            // background can only be stored by borrowing it. It is not, and
            // these are folded onto the normal background instead: a wrong
            // shade beats a cell that starts blinking. ECMA-48 leaves both
            // legal; the packed byte is what decides.
            m_currentAttr = (uint8_t)((m_currentAttr & 0x0F)
                                      | (ansiToCGAColor((uint8_t)(param - 100)) << 4));
        }
        break;
    }
}

// Sound the bell. The 0x07 case used to call MessageBeep(MB_OK) straight out
// with nothing to consult, so a guest that BELs in a loop could not be shut up;
// cpmdroid made the bell a setting and this is the same rule. The preference
// itself arrives through setBellEnabled(); see the note there about the config
// key, which exists but is not yet wired to it.
//
// The hook replaces MessageBeep() rather than running alongside it, and is
// reached only when the bell is enabled - so a caller counting hook calls
// counts exactly the bells the user would have heard. That is what
// tests/test_vt52.cpp relies on, and it is also what keeps the suite silent.
void TerminalView::ringBell() {
    if (!m_bellEnabled) return;
    if (m_bellHook) {
        m_bellHook();
        return;
    }
    MessageBeep(MB_OK);
}

void TerminalView::clearFromCursor() {
    // Erasing resolves an armed wrap, the same way ED 2 does through
    // eraseScreen(). Leaving it armed meant the next glyph after an ED 0 still
    // took a wrap that the erase had already made meaningless.
    m_pendingWrap = false;
    for (int col = m_cursorCol; col < COLS; col++) {
        m_cells[m_cursorRow][col] = blankCell();
    }
    for (int row = m_cursorRow + 1; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = blankCell();
        }
    }
    invalidate();
}

void TerminalView::clearToCursor() {
    m_pendingWrap = false;   // as clearFromCursor()
    for (int row = 0; row < m_cursorRow; row++) {
        for (int col = 0; col < COLS; col++) {
            m_cells[row][col] = blankCell();
        }
    }
    for (int col = 0; col <= m_cursorCol; col++) {
        m_cells[m_cursorRow][col] = blankCell();
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
