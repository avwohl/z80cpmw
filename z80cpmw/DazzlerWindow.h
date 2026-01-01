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

    // Scaling
    void setScale(int scale);
    int getScale() const { return m_scale; }

    // Force redraw
    void invalidate();
    void repaint();

    // Show/hide window
    void show(bool visible = true);
    bool isVisible() const;

    // Update window size based on Dazzler resolution
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
