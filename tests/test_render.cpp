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
    for (int i = 0; i < 8; i++) { char b[32]; snprintf(b, sizeof b, "\x1B[4%dm ", i); feed(b); }
    feed("\x1B[0m\r\n");
    // Bright foreground (SGR 90-97) on row 2.
    for (int i = 0; i < 8; i++) { char b[32]; snprintf(b, sizeof b, "\x1B[9%dmM", i); feed(b); }
    feed("\x1B[0m");

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

    bool composed = false;
    for (int attempt = 0; attempt < 40 && !composed; attempt++) {
        MSG msg;
        for (int i = 0; i < 200 && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE); i++) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        composed = capture();
        if (!composed) Sleep(50);
    }
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
    // i == 0 is in the loop for symmetry but cannot fail: the default
    // background is already black, so the cell is black whether ESC[40m took
    // effect or not. The other seven carry the section.
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

    DestroyWindow(owner);
    printf("\n%d checks, %d failures\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
