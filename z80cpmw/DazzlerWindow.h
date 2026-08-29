/*
 * DazzlerWindow.h - Dazzler Graphics Display Window
 *
 * A Win32 window that displays the Dazzler framebuffer output.
 * Supports scaling and real-time updates.
 */

#pragma once

#include <windows.h>
#include <memory>
#include <vector>

class Dazzler;

// Posted to the owner window when the user closes the Dazzler window, so the
// owner can take the card down and clear its View menu check mark instead of
// leaving both standing over a window that is no longer on screen.
//
// WM_APP + 1 and WM_APP + 2 are taken by MainWindow.cpp (WM_APP_SHOW_WELCOME
// and WM_APP_RUN_ON_UI); this one lives here because both sides of it - the
// DazzlerWindow that posts it and the MainWindow that handles it - have to
// agree on the number, and MainWindow.cpp already includes this header.
static const UINT WM_APP_DAZZLER_CLOSED = WM_APP + 3;

class DazzlerWindow {
public:
    DazzlerWindow();
    ~DazzlerWindow();

    // Create and show the window
    bool create(HWND parent, int x, int y, int scale = 2);
    void destroy();

    // Set the Dazzler instance to display
    void setDazzler(Dazzler* dazzler);

    // Window handle
    HWND getHwnd() const { return m_hwnd; }

    // Scaling. Resizes the window in place through updateSize(), so a scale
    // change costs neither the window's position nor its HWND - which is why
    // MainWindow::applyDazzlerState() is one call rather than a rebuild.
    void setScale(int scale);
    int getScale() const { return m_scale; }

    // Force redraw
    void invalidate();
    void repaint();

    // Show/hide window
    void show(bool visible = true);
    bool isVisible() const;

    // Size the window to Dazzler::MAX_WIDTH * scale - the card's LARGEST mode,
    // the same fixed size create() uses, and not whatever mode the card is in
    // now. Needs no Dazzler attached, for the same reason.
    void updateSize();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void paint(HDC hdc);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;

    Dazzler* m_dazzler = nullptr;
    int m_scale = 2;

    // Cached bitmap for double buffering
    HBITMAP m_bitmap = nullptr;
    std::vector<uint8_t> m_pixelBuffer;
    int m_bitmapWidth = 0;
    int m_bitmapHeight = 0;
};
