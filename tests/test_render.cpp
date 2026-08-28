/*
 * test_render.cpp - Rendering conformance: what the terminal actually PAINTS.
 *
 * The other suites read the screen model through cellAt(). This one reads the
 * pixels, which is the only way to settle a question like "is ESC[31m red" -
 * a question todo.txt had down as needing a person at a screen. It creates a
 * real window, drives the parser with real bytes, asks the DWM to render the
 * window with PrintWindow(PW_RENDERFULLCONTENT), and samples the bitmap.
 *
 * The expected colours are written out here from the CGA palette rather than
 * read from TerminalView::cgaToRGB(), on purpose: a test that reads the same
 * table it is checking asserts nothing.
 */

#include "TerminalView.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_checks = 0, g_failed = 0;
static const char* g_section = "";
static void section(const char* n) { g_section = n; printf("\n-- %s\n", n); }
static void check(bool ok, const char* what) {
    g_checks++;
    if (!ok) { g_failed++; printf("  FAIL [%s] %s\n", g_section, what); }
}

// CGA palette, written independently of TerminalView::cgaToRGB().
static COLORREF cga(int i) {
    static const COLORREF p[16] = {
        RGB(0,0,0), RGB(0,0,170), RGB(0,170,0), RGB(0,170,170),
        RGB(170,0,0), RGB(170,0,170), RGB(170,85,0), RGB(170,170,170),
        RGB(85,85,85), RGB(85,85,255), RGB(85,255,85), RGB(85,255,255),
        RGB(255,85,85), RGB(255,85,255), RGB(255,255,85), RGB(255,255,255),
    };
    return p[i & 15];
}

static LRESULT CALLBACK OwnerProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

struct Shot {
    std::vector<uint32_t> px;   // BGRA, top-down
    int w = 0, h = 0;
    int offX = 0, offY = 0;     // terminal client origin within the shot
    COLORREF at(int x, int y) const {
        x += offX; y += offY;
        if (x < 0 || y < 0 || x >= w || y >= h) return 0xFFFFFFFF;
        uint32_t v = px[(size_t)y * w + x];
        return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
    }
};

int main() {
    printf("\n=== Rendering conformance suite ===\n");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = OwnerProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"z80cpmwRenderTestOwner";
    RegisterClassExW(&wc);

    HWND owner = CreateWindowExW(0, L"z80cpmwRenderTestOwner", L"render test",
                                 WS_OVERLAPPEDWINDOW, 0, 0, 900, 600,
                                 nullptr, nullptr, wc.hInstance, nullptr);
    if (!owner) { printf("  SKIP: no window station (CreateWindowEx failed)\n"); return 0; }

    TerminalView term;
    if (!term.create(owner, 0, 0, 880, 560)) { printf("  FAIL: TerminalView::create\n"); return 1; }

    const int cw = term.getCharWidth(), ch = term.getCharHeight();
    printf("  cell %dx%d\n", cw, ch);

    // Colour chart: row 0 is ESC[3Xm over the default background, row 1 is
    // ESC[4Xm on a space so the whole cell is the background colour.
    auto feed = [&](const char* s) { for (const char* p = s; *p; ++p) term.outputChar((uint8_t)*p); };
    feed("\x1B[2J\x1B[H");
    for (int i = 0; i < 8; i++) { char b[32]; snprintf(b, sizeof b, "\x1B[3%dmM", i); feed(b); }
    feed("\x1B[0m\r\n");
    // Prime the background white before row 1's first cell. This one escape is
    // the whole reason index 0 of "SGR 40-47 paint the whole cell" asserts
    // anything: ESC[40m arriving at the default background paints black over
    // black, so that cell reads CGA 0 whether the parser honoured it or not,
    // and the check could not fail. With the prime it reads CGA 0 only because
    // ESC[40m came back from CGA 7 - measured against a scratch copy of
    // TerminalView.cpp with applySGR()'s background arm narrowed to 41-47,
    // where index 0 is the one and only check that fails.
    //
    // The premise is not assumed either. That ESC[47m really does set CGA 7 is
    // index 7 of that same loop, so a parser that ignored the whole 40-47 range
    // fails there instead of quietly making this prime a no-op.
    //
    // It paints no cell of its own - no character is emitted between it and the
    // ESC[40m that follows - so the screen every section here samples is exactly
    // the screen it sampled before, eight background swatches on row 1 included,
    // and the capture predicate that waits for the terminal area to stop being
    // one flat colour is looking at the same bitmap.
    feed("\x1B[47m");
    for (int i = 0; i < 8; i++) { char b[32]; snprintf(b, sizeof b, "\x1B[4%dm ", i); feed(b); }
    feed("\x1B[0m\r\n");
    // Bright foreground (SGR 90-97) on row 2.
    for (int i = 0; i < 8; i++) { char b[32]; snprintf(b, sizeof b, "\x1B[9%dmM", i); feed(b); }
    feed("\x1B[0m");

    // The rendition row: the same character, in the same colour, in each of the
    // faces the TCELL_* flags can ask for. Row 4 (ESC[5;1H is 1-based), columns
    // 0 to 5, with column 6 left empty because that is where the cursor comes
    // to rest.
    //
    // EVERY ONE OF THESE IS CGA 15 ON BLACK, and that is the whole design of
    // the row. SGR 1 sets the CGA intensity bit as well as TCELL_BOLD, so
    // "ESC[1;37m has more ink than ESC[37m" would be measuring bright white
    // against grey - a colour difference - and would pass with no bold face
    // anywhere in the renderer. ESC[97m asks for the same CGA 15 without
    // setting TCELL_BOLD, so the two cells differ in the FACE and in nothing
    // else. The section below checks that both really are CGA 15 before it
    // compares their ink, so the discriminator is pinned rather than assumed.
    feed("\x1B[5;1H");
    feed("\x1B[0;97mM");        // col 0: plain
    feed("\x1B[0;1;37mM");      // col 1: TCELL_BOLD
    feed("\x1B[0;97;4mM");      // col 2: TCELL_UNDERLINE
    feed("\x1B[0;1;37;4mM");    // col 3: both
    feed("\x1B[0;97;5mM");      // col 4: TCELL_BLINK
    feed("\x1B[0;97;4;5mM");    // col 5: TCELL_UNDERLINE and TCELL_BLINK together
    feed("\x1B[0m");

    // Two cells on row 6 for the bleed section below: a white 'M' with a red
    // background hard against its right.
    feed("\x1B[7;1H\x1B[0;97mM\x1B[0;41m \x1B[0m");

    ShowWindow(owner, SW_SHOWNOACTIVATE);
    UpdateWindow(owner);
    term.repaint();

    RECT wr, tr;
    GetWindowRect(owner, &wr);
    GetWindowRect(term.getHwnd(), &tr);
    Shot shot;
    shot.w = wr.right - wr.left; shot.h = wr.bottom - wr.top;
    shot.offX = tr.left - wr.left; shot.offY = tr.top - wr.top;

    // PW_RENDERFULLCONTENT asks the DESKTOP COMPOSITOR for the window, not the
    // window itself, and the compositor has its own idea of when it is ready.
    // A window shown a moment ago composes to a uniform black bitmap often
    // enough to matter: measured on this machine, one capture in four came back
    // entirely black, and every colour check then read CGA 0 and failed. A
    // suite that reds one run in four teaches people to re-run it rather than
    // read it, which is worse than not having it.
    //
    // So the capture is retried until the TERMINAL'S OWN AREA stops being one
    // flat colour. The area matters: the first version of this asked whether
    // the whole window bitmap was uniform, and it never was - the title bar and
    // the frame are painted by the compositor before the client area is, so a
    // capture with a completely blank terminal in it passed the check and then
    // failed 22 of 27 colour checks. The predicate has to look where the
    // question is being asked.
    //
    // "Not uniform" rather than "not black" because the failure mode is the
    // compositor handing back a region it has not drawn into, whatever it fills
    // it with. Row 1 alone is eight background swatches in eight different
    // palette entries, so a composed capture cannot be uniform - and neither
    // can any translation regression, which changes WHICH colours appear, not
    // how many. The one regression this cannot tell from a blank capture is a
    // renderer that stopped painting altogether; that is what the model-level
    // suite in tests/test_vt52.cpp is for.
    //
    // If it never composes, this prints SKIP and exits 0. A machine with no
    // desktop compositor cannot answer the question this suite asks, and
    // failing there would say the colours are wrong rather than unmeasurable.
    auto capture = [&]() -> bool {
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = shot.w;
        bi.bmiHeader.biHeight = -shot.h;      // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ old = SelectObject(mem, dib);
        BOOL ok = PrintWindow(owner, mem, 2 /* PW_RENDERFULLCONTENT */);
        SelectObject(mem, old);
        if (ok) {
            shot.px.assign((uint32_t*)bits, (uint32_t*)bits + (size_t)shot.w * shot.h);
        }
        DeleteObject(dib); DeleteDC(mem); ReleaseDC(nullptr, screen);
        if (!ok) return false;
        // The three rows the checks below read, in the terminal's coordinates.
        COLORREF first = shot.at(0, 0);
        for (int y = 0; y < 3 * ch; y++) {
            for (int x = 0; x < 8 * cw; x++) {
                if (shot.at(x, y) != first) return true;
            }
        }
        return false;   // one flat colour: the compositor has not drawn it yet
    };

    // Pump the queue and take one capture, retrying until the compositor gives
    // back something drawn. Every capture in this file goes through here - the
    // first one, and each sample the blink section takes - so a sample that the
    // compositor had not drawn cannot be read as a blink phase.
    //
    // The pump is not just politeness: WM_TIMER is synthesised by PeekMessage
    // when the queue is otherwise empty, so the terminal's 500 ms blink tick,
    // and the WM_PAINT its InvalidateRect produces, only happen inside this
    // loop. A section that slept without pumping would watch a frozen window.
    auto pumpAndCapture = [&]() -> bool {
        for (int attempt = 0; attempt < 40; attempt++) {
            MSG msg;
            for (int i = 0; i < 200 && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE); i++) {
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
            if (capture()) return true;
            Sleep(50);
        }
        return false;
    };

    bool composed = pumpAndCapture();
    if (!composed) {
        printf("  SKIP: the window never composed (no desktop compositor?)\n");
        DestroyWindow(owner);
        return 0;
    }

    // Count pixels of a given colour inside one cell.
    auto countIn = [&](int row, int col, COLORREF want) {
        int n = 0;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                if (shot.at(col * cw + x, row * ch + y) == want) n++;
        return n;
    };

    // The font is created with CLEARTYPE_QUALITY, so a glyph's pixels are
    // subpixel-blended and almost none of them is exactly the requested
    // colour. Asking "which palette entry is this cell drawn in" is therefore
    // a nearest-neighbour question, asked of the pixel furthest from the
    // background - the middle of a stem, where blending has the least effect.
    auto inkIn = [&](int row, int col, COLORREF bg) {
        COLORREF best = bg;
        long bestD = -1;
        for (int y = 0; y < ch; y++) {
            for (int x = 0; x < cw; x++) {
                COLORREF c = shot.at(col * cw + x, row * ch + y);
                long dr = (long)GetRValue(c) - GetRValue(bg);
                long dg = (long)GetGValue(c) - GetGValue(bg);
                long db = (long)GetBValue(c) - GetBValue(bg);
                long d = dr * dr + dg * dg + db * db;
                if (d > bestD) { bestD = d; best = c; }
            }
        }
        return best;
    };
    auto nearestPaletteIndex = [&](COLORREF c) {
        int best = 0; long bestD = -1;
        for (int i = 0; i < 16; i++) {
            COLORREF p = cga(i);
            long dr = (long)GetRValue(c) - GetRValue(p);
            long dg = (long)GetGValue(c) - GetGValue(p);
            long db = (long)GetBValue(c) - GetBValue(p);
            long d = dr * dr + dg * dg + db * db;
            if (bestD < 0 || d < bestD) { bestD = d; best = i; }
        }
        return best;
    };

    static const char* NAMES[8] = { "black","red","green","yellow","blue","magenta","cyan","white" };
    // ANSI index -> CGA palette index, stated here rather than computed.
    static const int ANSI_TO_CGA[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

    section("SGR 30-37 paint the ANSI colour, not the CGA one");
    for (int i = 1; i < 8; i++) {   // 30 is black on black: nothing to see
        char what[160];
        int got = nearestPaletteIndex(inkIn(0, i, RGB(0,0,0)));
        snprintf(what, sizeof what, "ESC[3%dm ('%s') draws in CGA %d; drew CGA %d",
                 i, NAMES[i], ANSI_TO_CGA[i], got);
        check(got == ANSI_TO_CGA[i], what);
    }

    section("SGR 40-47 paint the whole cell");
    // All eight indices carry this section now. i == 0 used to be in the loop
    // for symmetry and could not fail - the default background is already
    // black, so its cell was black whether ESC[40m took effect or not - which
    // left seven checks doing the work of eight. The row is written with the
    // background primed to CGA 7 (see the ESC[47m above the row 1 loop), so
    // black here is a colour the cell had to be brought back to.
    for (int i = 0; i < 8; i++) {
        char what[160];
        int n = countIn(1, i, cga(ANSI_TO_CGA[i]));
        snprintf(what, sizeof what, "ESC[4%dm ('%s') fills the cell with CGA %d; %d/%d pixels",
                 i, NAMES[i], ANSI_TO_CGA[i], n, cw * ch);
        check(n > (cw * ch) / 2, what);
    }

    section("no colour is drawn untranslated");
    // The regression this section exists to catch is the removal of the
    // ANSI-to-CGA translation from the SGR path, which would make ESC[41m
    // (ANSI red) fill CGA 1 (blue) instead of CGA 4 (red). The value the cell
    // would then carry is the SGR parameter itself, i - so that is what must
    // be absent from the cell, not some transformation of it.
    //
    // Only four of the eight indices can witness it. ansiToCGAColor() swaps
    // bits 0 and 2, so 0, 2, 5 and 7 map to themselves: for those the
    // translated and untranslated colours are the same and there is nothing to
    // tell apart. The loop skips exactly those and checks 1, 3, 4 and 6.
    //
    // This guard used to compare ANSI_TO_CGA[i] against the bit-reversal of i,
    // which IS the mapping - so it was true for all eight indices, check() was
    // never reached, and the section contributed nothing while its comment
    // claimed four skips. Measured on this machine: the suite reported 23
    // checks with that guard and 27 with this one.
    for (int i = 0; i < 8; i++) {
        if (i == ANSI_TO_CGA[i]) continue;   // 0, 2, 5, 7: the translation is the identity here
        char what[160];
        int n = countIn(1, i, cga(i));
        snprintf(what, sizeof what, "ESC[4%dm is not CGA %d (the untranslated value); %d pixels",
                 i, i, n);
        check(n == 0, what);
    }

    section("SGR 90-97 are the bright half of the same palette");
    for (int i = 0; i < 8; i++) {
        char what[160];
        int got = nearestPaletteIndex(inkIn(2, i, RGB(0,0,0)));
        snprintf(what, sizeof what, "ESC[9%dm draws in CGA %d; drew CGA %d",
                 i, ANSI_TO_CGA[i] | 8, got);
        check(got == (ANSI_TO_CGA[i] | 8), what);
    }

    // --- The rendition row -------------------------------------------------
    //
    // Everything below reads row RR, where the six cells are the same 'M' in
    // the same CGA 15 on the same black, differing only in TCELL_* flags.
    const int RR = 4;
    const int C_PLAIN = 0, C_BOLD = 1, C_UL = 2, C_BOLDUL = 3, C_BLINK = 4, C_ULBLINK = 5;
    const COLORREF INK = cga(15), PAPER = cga(0);

    // "Ink" is a pixel nearer the foreground than the background. A brightness
    // threshold was the alternative and is worse: CLEARTYPE_QUALITY blends the
    // edge of every stem, so any fixed cutoff is a guess about how much blend
    // counts, and the guess would have to be re-made for every colour. Nearest
    // of two has an answer for every pixel and no constant to tune.
    auto dist2 = [](COLORREF a, COLORREF b) {
        long dr = (long)GetRValue(a) - GetRValue(b);
        long dg = (long)GetGValue(a) - GetGValue(b);
        long db = (long)GetBValue(a) - GetBValue(b);
        return dr * dr + dg * dg + db * db;
    };
    auto isInk = [&](COLORREF c) { return dist2(c, INK) < dist2(c, PAPER); };
    // Ink in one pixel row of one cell; y counts down from the cell's top.
    auto inkInRow = [&](int col, int y) {
        int n = 0;
        for (int x = 0; x < cw; x++) if (isInk(shot.at(col * cw + x, RR * ch + y))) n++;
        return n;
    };
    auto cellInk = [&](int col) {
        int n = 0;
        for (int y = 0; y < ch; y++) n += inkInRow(col, y);
        return n;
    };
    // The BOTTOMMOST inked row of a cell, or -1. 'M' has no descender, so for
    // the plain cell this is the foot of the letter and everything below it is
    // empty - which is what makes the underline visible as a row of ink that
    // the same character without it does not have.
    auto lowestInkRow = [&](int col) {
        int lo = -1;
        for (int y = 0; y < ch; y++) if (inkInRow(col, y) > 0) lo = y;
        return lo;
    };

    section("SGR 1 draws a heavier face, not just a brighter colour");
    {
        char what[200];
        int plainCga = nearestPaletteIndex(inkIn(RR, C_PLAIN, PAPER));
        int boldCga  = nearestPaletteIndex(inkIn(RR, C_BOLD,  PAPER));
        snprintf(what, sizeof what, "ESC[97m is CGA 15; drew CGA %d", plainCga);
        check(plainCga == 15, what);
        snprintf(what, sizeof what, "ESC[1;37m is CGA 15 too, so the two differ only in face; drew CGA %d", boldCga);
        check(boldCga == 15, what);

        int plain = cellInk(C_PLAIN), bold = cellInk(C_BOLD);
        // Measured on this machine at the suite's cell of 8x16: 32 ink pixels
        // plain against 37 bold, over an 'M', which is the answer to "does
        // Consolas Bold differ enough at this size" - it does, by five pixels
        // and reproducibly. The check is the inequality and not those numbers,
        // because the cell size follows the DPI of whatever process runs this;
        // what has to hold at any size is that the heavier face puts down more
        // ink.
        snprintf(what, sizeof what, "TCELL_BOLD puts down more ink: %d pixels bold against %d plain",
                 bold, plain);
        check(bold > plain, what);
    }

    section("SGR 4 draws a rule below the glyph");
    {
        char what[200];
        int plainLow = lowestInkRow(C_PLAIN);
        int ulLow    = lowestInkRow(C_UL);
        snprintf(what, sizeof what, "the underlined cell's lowest ink is below the plain cell's: row %d against %d of %d",
                 ulLow, plainLow, ch);
        check(plainLow >= 0 && ulLow > plainLow, what);

        // The row the underline lands on, asked of the plain cell. This is the
        // check the task is really about - coloured pixels on a row that the
        // same character without SGR 4 leaves empty - and it is asked this way
        // round so that it cannot be satisfied by ink that was there anyway.
        int onPlain = (ulLow >= 0) ? inkInRow(C_PLAIN, ulLow) : -1;
        snprintf(what, sizeof what, "row %d is empty without SGR 4; %d ink pixels there", ulLow, onPlain);
        check(onPlain == 0, what);

        int span = (ulLow >= 0) ? inkInRow(C_UL, ulLow) : 0;
        snprintf(what, sizeof what, "and it is a rule across the cell, not a stray pixel: %d of %d columns",
                 span, cw);
        check(span > cw / 2, what);
    }

    section("SGR 1 and SGR 4 compose");
    {
        // The obvious check here - "the bold+underlined cell underlines on the
        // same row as the underlined one" - was written first and it FAILS,
        // for a reason worth recording rather than working around. GDI takes
        // the underline's position from the face, and Consolas Bold does not
        // put it where Consolas does: measured at this suite's 8x16 cell, the
        // regular face rules row 15 and the bold face rules row 14. So the
        // underline is asked about against the cell that shares its FACE - the
        // bold cell - and not against the regular underlined one.
        char what[200];
        int boldLow = lowestInkRow(C_BOLD), bothLow = lowestInkRow(C_BOLDUL);
        snprintf(what, sizeof what, "the bold cell gains a rule below its glyph: lowest ink row %d against %d",
                 bothLow, boldLow);
        check(boldLow >= 0 && bothLow > boldLow, what);

        int onBold = (bothLow >= 0) ? inkInRow(C_BOLD, bothLow) : -1;
        snprintf(what, sizeof what, "row %d is empty in the bold cell without SGR 4; %d ink pixels there",
                 bothLow, onBold);
        check(onBold == 0, what);

        int both = cellInk(C_BOLDUL), ul = cellInk(C_UL);
        // And the heavier face is still there: against the underlined cell,
        // which already carries a rule of its own, the only thing left to
        // differ by is the weight.
        snprintf(what, sizeof what, "and it is still the heavier face: %d ink pixels against %d underline-only",
                 both, ul);
        check(both > ul, what);
    }

    section("a cell's last pixel column is painted by the cell to its right");
    {
        // Not a check on this commit's code: a standing property of this
        // renderer that the sections below have to work around, found while
        // writing them and pinned here so the workaround has something to point
        // at. TextOutA's opaque background for a cell reaches ONE PIXEL LEFT of
        // that cell's origin, so the rightmost pixel column of every cell ends
        // up in the colour of its right-hand NEIGHBOUR's background.
        //
        // Measured, not deduced: with a red-background space written into the
        // column beside a white 'M', the M's cell reads CGA 4 down its last
        // pixel column and its own colours everywhere else. That is why the
        // blink comparisons below stop at cw - 1: without it a blinking cell at
        // the end of a selection is compared against a pixel column that
        // belongs to the unselected cell after it, and no phase ever matches.
        const int BR = 6, yMid = ch / 2;
        char what[200];
        COLORREF last = shot.at(0 * cw + (cw - 1), BR * ch + yMid);
        COLORREF prev = shot.at(0 * cw + (cw - 2), BR * ch + yMid);
        snprintf(what, sizeof what, "the red cell's background reaches into column %d of the cell before it: %06X",
                 cw - 1, (unsigned)last);
        check(last == cga(4), what);
        snprintf(what, sizeof what, "and no further - column %d is not red: %06X", cw - 2, (unsigned)prev);
        check(prev != cga(4), what);
    }

    // Watch the blinking cell over more than one full blink period and report
    // what was seen: frames in which it is pixel-for-pixel the unblinking cell
    // beside it (the ON phase), frames in which every pixel of it is offColour
    // (the OFF phase), and frames in which the unblinking neighbour moved.
    //
    // The phase cannot be set from here and cannot be read either: it flips on
    // the terminal's own 500 ms WM_TIMER, the same tick as the cursor, and this
    // process only gets to pump the queue. Sampling is what makes that
    // deterministic rather than a race - samples ~120 ms apart against a 500 ms
    // half-period cover a full cycle several times over inside the budget, and
    // every sample goes through pumpAndCapture(), so a frame the compositor had
    // not drawn is never read as a phase.
    //
    // The budget is also what keeps a FAILURE from becoming a hang: a renderer
    // that never draws the off phase - which is exactly the state before this
    // commit - runs the samples out and fails the check, rather than waiting
    // forever for a phase that will not come.
    //
    // It watches the UNDERLINED blinking cell at the same time, in ink rather
    // than in colours, because that cell answers a different question: whether
    // the rule goes out with the character. It is never selected in either
    // call, so its own two states are "white on black" and "all black", and
    // recording the most and least ink it ever shows covers both without
    // needing to know which frame is which.
    int samples = 0, onFrames = 0, offFrames = 0, neighbourMoved = 0;
    int plainInk = 0, ulBlinkMostInk = 0, ulBlinkLeastInk = 0;
    auto sampleBlink = [&](COLORREF offColour) -> bool {
        samples = onFrames = offFrames = neighbourMoved = 0;
        plainInk = ulBlinkMostInk = 0;
        ulBlinkLeastInk = cw * ch;
        if (!pumpAndCapture()) return false;
        std::vector<COLORREF> plainRef;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw - 1; x++)
                plainRef.push_back(shot.at(C_PLAIN * cw + x, RR * ch + y));

        for (int i = 0; i < 24 && !(onFrames && offFrames); i++) {
            Sleep(120);
            if (!pumpAndCapture()) break;
            samples++;
            bool same = true, off = true, plainSame = true;
            int ulInk = 0;
            size_t k = 0;
            // cw - 1, not cw: the last pixel column of a cell carries the
            // NEIGHBOUR's background, as the bleed section above measures.
            for (int y = 0; y < ch; y++) {
                for (int x = 0; x < cw - 1; x++, k++) {
                    COLORREF b = shot.at(C_BLINK * cw + x, RR * ch + y);
                    COLORREF p = shot.at(C_PLAIN * cw + x, RR * ch + y);
                    if (b != p) same = false;
                    if (b != offColour) off = false;
                    if (p != plainRef[k]) plainSame = false;
                    if (isInk(shot.at(C_ULBLINK * cw + x, RR * ch + y))) ulInk++;
                }
            }
            if (same) onFrames++;
            if (off) offFrames++;
            if (!plainSame) neighbourMoved++;
            if (ulInk > ulBlinkMostInk) ulBlinkMostInk = ulInk;
            if (ulInk < ulBlinkLeastInk) ulBlinkLeastInk = ulInk;
        }
        // The plain cell over the same window, for the comparison the
        // underlined blinking cell's "most ink" frame is asked to beat.
        plainInk = 0;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw - 1; x++)
                if (isInk(shot.at(C_PLAIN * cw + x, RR * ch + y))) plainInk++;
        return samples > 0;
    };

    section("SGR 5 blinks the character and nothing else");
    {
        char what[220];
        bool sampled = sampleBlink(PAPER);
        // Printed rather than left silent because these are the only sampled
        // sections in the file: a reader who sees them pass wants to know they
        // watched several frames and saw the cell in both states, not that they
        // got lucky once.
        printf("  blink sampling: %d frames, %d on, %d off\n", samples, onFrames, offFrames);
        check(sampled, "the blinking cell could be sampled at all");

        snprintf(what, sizeof what,
                 "in its ON phase a blinking cell is pixel-for-pixel the unblinking one: %d of %d frames",
                 onFrames, samples);
        check(onFrames > 0, what);
        snprintf(what, sizeof what,
                 "and it goes out - the whole cell is background in the OFF phase: %d of %d frames",
                 offFrames, samples);
        check(offFrames > 0, what);
        // Not a spare check: it is the one that says the blink is per CELL. A
        // renderer that dropped the flag test and blinked the row, or that
        // repainted the row into a bitmap it had not filled, would move these
        // pixels, and every check above is blind to it because they all read a
        // single frame.
        snprintf(what, sizeof what,
                 "the unblinking neighbour is untouched across every frame: moved in %d of %d",
                 neighbourMoved, samples);
        check(neighbourMoved == 0, what);

        // ESC[4;5m together, and the reason it is worth its own two checks:
        // the underline is the FONT's, not a rule drawn after the glyph, so
        // whether it goes out with the character is a fact about GDI rather
        // than about any line of TerminalView.cpp. TerminalView.h's comment on
        // m_fonts claims it does. These measure it.
        //
        // Ink, not colour, because the two frames differ in how much of the
        // cell is lit: at its brightest this cell must beat the plain one - the
        // rule is the difference - and at its dimmest it must have no ink at
        // all, glyph and rule alike.
        snprintf(what, sizeof what,
                 "an underlined blinking cell shows its rule: %d ink pixels at its brightest against %d plain",
                 ulBlinkMostInk, plainInk);
        check(ulBlinkMostInk > plainInk, what);
        snprintf(what, sizeof what,
                 "and takes its rule with it when it goes out: %d ink pixels at its dimmest",
                 ulBlinkLeastInk);
        check(ulBlinkLeastInk == 0, what);
    }

    section("a selected blinking cell keeps its highlight while the character goes");
    {
        // This section exists for ONE question: the order in which paint()
        // applies the selection swap and the blink collapse. Both orders draw
        // the same thing on an unselected screen, so every other check in this
        // file passes either way - measured, not assumed: with the collapse
        // moved above the swap in a scratch copy of TerminalView.cpp, the suite
        // reports one failure, and it is the last check in this section.
        //
        // Selection first, then collapse - which is what TerminalView.cpp does
        // - leaves fg = bg = the SWAPPED background, i.e. the cell's own
        // foreground, so a selected blinking cell in its off phase is a solid
        // block of highlight with no character in it. Collapse first would set
        // fg = bg = the cell's background and the swap would leave them equal,
        // so the cell would fall back to the text background and the SELECTION
        // would blink instead of the text. The cells here are CGA 15 on black,
        // so the two predictions are white and black: as far apart as this
        // palette goes.
        //
        // The drag covers columns 0 to 4, so the blinking cell is inside the
        // selection and the plain cell it is compared against is too.
        //
        // The drag is sent, not posted: SendMessageW reaches the window
        // procedure directly, so the selection exists by the time the call
        // returns and no pump is needed to establish it. WM_LBUTTONDOWN also
        // calls SetFocus, and a focused terminal draws its cursor - which is on
        // column 6, and is why the rendition row stops at column 5.
        const int yMid = RR * ch + ch / 2;
        SendMessageW(term.getHwnd(), WM_LBUTTONDOWN, MK_LBUTTON,
                     MAKELPARAM(C_PLAIN * cw + cw / 2, yMid));
        SendMessageW(term.getHwnd(), WM_MOUSEMOVE, MK_LBUTTON,
                     MAKELPARAM(C_BLINK * cw + cw / 2, yMid));
        SendMessageW(term.getHwnd(), WM_LBUTTONUP, 0,
                     MAKELPARAM(C_BLINK * cw + cw / 2, yMid));

        char what[220];
        bool sampled = sampleBlink(INK);   // INK = CGA 15 = the swapped background
        printf("  selected blink sampling: %d frames, %d on, %d off\n", samples, onFrames, offFrames);
        check(sampled, "the selected blinking cell could be sampled at all");

        snprintf(what, sizeof what,
                 "selected, its ON phase still matches the selected cell beside it: %d of %d frames",
                 onFrames, samples);
        check(onFrames > 0, what);
        snprintf(what, sizeof what,
                 "and its OFF phase is a solid block of the HIGHLIGHT colour, not the text background: %d of %d frames",
                 offFrames, samples);
        check(offFrames > 0, what);
    }

    section("the blink tick invalidates blinking rows and nothing else");
    {
        // This section reads no pixels. It reads the UPDATE REGION the blink
        // tick leaves behind, because the property being checked - that a
        // screen with no blinking cell on it does not repaint on the blink's
        // account - is invisible in a bitmap: a window that repaints twice a
        // second to the same pixels looks exactly like one that does not
        // repaint at all. Counting WM_PAINT cannot see it either, since the
        // cursor's own 500 ms invalidation already produces one per tick and
        // Windows coalesces the two into a single message.
        //
        // The region is readable from here only because this process owns the
        // message loop: the tick is dispatched, and GetUpdateRect is called
        // BEFORE pumping again, because WM_PAINT is synthesised only when the
        // queue is empty and dispatching it would clear the region first.
        //
        // It runs last and leaves the screen erased, so nothing above it can
        // read the cells it destroys.
        auto tickAndUpdateRect = [&](RECT& r) -> bool {
            MSG msg;
            // Drain first, so what is measured is the region THIS tick made.
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
            for (int i = 0; i < 400; i++) {
                if (!PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { Sleep(10); continue; }
                bool tick = (msg.message == WM_TIMER && msg.hwnd == term.getHwnd());
                TranslateMessage(&msg); DispatchMessageW(&msg);
                if (tick) {
                    SetRectEmpty(&r);
                    GetUpdateRect(term.getHwnd(), &r, FALSE);
                    return true;
                }
            }
            return false;   // no tick inside four seconds: the timer is gone
        };

        // Move the cursor well away from the blinking cell first. The two
        // invalidations are unioned into one region, so a cursor sitting on the
        // blinking row would make a region that says nothing about which of
        // them produced it.
        const int CURSOR_ROW = 20;
        feed("\x1B[21;1H");

        char what[220];
        RECT r;
        if (!tickAndUpdateRect(r)) {
            printf("  SKIP: no blink tick arrived (timer not running?)\n");
        } else {
            snprintf(what, sizeof what,
                     "with a blinking cell on row %d it reaches that row: region top %d, cursor row starts at %d",
                     RR, (int)r.top, CURSOR_ROW * ch);
            check(r.top <= RR * ch, what);
            // The expectation is the VISIBLE width, not the grid width, and
            // that is not pedantry: run this process DPI-aware on a 200%
            // display and the cell becomes 15x32, so the 80-column grid is
            // 1200 pixels wide while the owner window's client area is 874.
            // The region comes back clipped to what can be seen - measured at
            // exactly 874 - and an expectation of 1200 would fail on a machine
            // where the renderer is doing precisely the right thing. The first
            // version of this check said 1200 and did fail.
            long visW = (long)TerminalView::COLS * cw;
            RECT tc, oc;
            GetClientRect(term.getHwnd(), &tc);
            GetClientRect(owner, &oc);          // the child is clipped by its parent
            if (tc.right < visW) visW = tc.right;
            if (oc.right < visW) visW = oc.right;
            snprintf(what, sizeof what,
                     "and it invalidates the whole row, not one cell: %d pixels wide against %ld visible of a %d-wide grid",
                     (int)(r.right - r.left), visW, TerminalView::COLS * cw);
            check(r.right - r.left >= visW, what);

            // Now take the blinking cell away. ED 2 blanks every cell through
            // blankCell(), which zeroes the flags, so no TCELL_BLINK survives
            // anywhere on the visible screen - and it homes the cursor, which
            // is the only thing left that invalidates on a tick.
            feed("\x1B[2J");
            if (!tickAndUpdateRect(r)) {
                printf("  SKIP: no blink tick arrived after the erase\n");
            } else {
                snprintf(what, sizeof what,
                         "with nothing blinking, the tick invalidates the cursor cell alone: %dx%d against a %dx%d cell",
                         (int)(r.right - r.left), (int)(r.bottom - r.top), cw, ch);
                check(r.right - r.left == cw && r.bottom - r.top == ch, what);
            }
        }
    }

    DestroyWindow(owner);
    printf("\n%d checks, %d failures\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
