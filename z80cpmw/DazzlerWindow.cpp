/*
 * DazzlerWindow.cpp - Dazzler Graphics Display Window Implementation
 */

#include "pch.h"
#include "DazzlerWindow.h"
#include "Dazzler.h"

static const wchar_t* DAZZLER_CLASS = L"Z80CPM_Dazzler";
static bool g_dazzlerClassRegistered = false;

DazzlerWindow::DazzlerWindow() {
}

DazzlerWindow::~DazzlerWindow() {
    destroy();
}

bool DazzlerWindow::create(HWND parent, int x, int y, int scale) {
    m_parent = parent;
    m_scale = scale > 0 ? scale : 4;

    // Register window class if not already done
    if (!g_dazzlerClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = DAZZLER_CLASS;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        g_dazzlerClassRegistered = true;
    }

    // Fixed size for maximum resolution (128x128 in X4 2K mode)
    // Smaller modes will be scaled to fill this space
    int width = 128 * m_scale;
    int height = 128 * m_scale;

    // Adjust for window frame
    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, FALSE);

    m_hwnd = CreateWindowExW(
        0,
        DAZZLER_CLASS,
        L"Cromemco Dazzler",
        (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX) | WS_VISIBLE,
        x, y,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,  // No parent for independent window
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    return m_hwnd != nullptr;
}

void DazzlerWindow::destroy() {
    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_bitmap = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void DazzlerWindow::setDazzler(Dazzler* dazzler) {
    m_dazzler = dazzler;
    if (m_dazzler) {
        // Set up update callback
        m_dazzler->setUpdateCallback([this]() {
            invalidate();
        });
        // Don't resize window - use fixed size and scale content to fit
        invalidate();
    }
}

void DazzlerWindow::setScale(int scale) {
    if (scale > 0 && scale != m_scale) {
        m_scale = scale;
        if (m_dazzler) {
            m_dazzler->setScale(scale);
        }
        updateSize();
    }
}

void DazzlerWindow::invalidate() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void DazzlerWindow::repaint() {
    if (!m_hwnd) return;
    RedrawWindow(m_hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
}

void DazzlerWindow::show(bool visible) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

bool DazzlerWindow::isVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void DazzlerWindow::updateSize() {
    if (!m_hwnd || !m_dazzler) return;

    int width = m_dazzler->getWidth() * m_scale;
    int height = m_dazzler->getHeight() * m_scale;

    // Adjust for window frame
    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, GetWindowLong(m_hwnd, GWL_STYLE), FALSE);

    SetWindowPos(m_hwnd, nullptr, 0, 0,
                 rect.right - rect.left,
                 rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER);

    // Invalidate cached bitmap
    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_bitmap = nullptr;
    }
    m_bitmapWidth = 0;
    m_bitmapHeight = 0;

    invalidate();
}

LRESULT CALLBACK DazzlerWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DazzlerWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<DazzlerWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->m_hwnd = hwnd;
    } else {
        window = reinterpret_cast<DazzlerWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT DazzlerWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        paint(hdc);
        EndPaint(m_hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;  // We handle background in WM_PAINT

    case WM_CLOSE:
        // Hide instead of destroy - let main window manage lifetime
        show(false);
        return 0;

    case WM_SIZE:
        invalidate();
        return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void DazzlerWindow::paint(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    int windowWidth = clientRect.right;
    int windowHeight = clientRect.bottom;

    if (!m_dazzler) {
        // No dazzler - fill with black
        HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &clientRect, brush);
        DeleteObject(brush);
        return;
    }

    if (!m_dazzler->isEnabled()) {
        // Dazzler disabled - fill with dark gray (simulating CRT off)
        HBRUSH brush = CreateSolidBrush(RGB(32, 32, 32));
        FillRect(hdc, &clientRect, brush);
        DeleteObject(brush);
        return;
    }

    int srcWidth = m_dazzler->getWidth();
    int srcHeight = m_dazzler->getHeight();

    // Scale to fill window while maintaining aspect ratio (always square)
    int dstWidth = windowWidth;
    int dstHeight = windowHeight;

    // Create or resize bitmap if needed
    if (m_bitmapWidth != srcWidth || m_bitmapHeight != srcHeight) {
        if (m_bitmap) {
            DeleteObject(m_bitmap);
        }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = srcWidth;
        bmi.bmiHeader.biHeight = -srcHeight;  // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        m_bitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        m_bitmapWidth = srcWidth;
        m_bitmapHeight = srcHeight;

        // Resize pixel buffer
        m_pixelBuffer.resize(srcWidth * srcHeight * 4);
    }

    // Render Dazzler output to buffer
    m_dazzler->render(m_pixelBuffer.data());

    // Copy to bitmap
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = srcWidth;
    bmi.bmiHeader.biHeight = -srcHeight;  // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Convert RGBA to BGRA for Windows
    std::vector<uint8_t> bgraBuffer(m_pixelBuffer.size());
    for (size_t i = 0; i < m_pixelBuffer.size(); i += 4) {
        bgraBuffer[i + 0] = m_pixelBuffer[i + 2];  // B
        bgraBuffer[i + 1] = m_pixelBuffer[i + 1];  // G
        bgraBuffer[i + 2] = m_pixelBuffer[i + 0];  // R
        bgraBuffer[i + 3] = m_pixelBuffer[i + 3];  // A
    }

    // Create memory DC and select bitmap
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, dstWidth, dstHeight);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Use StretchDIBits to scale
    SetStretchBltMode(memDC, COLORONCOLOR);
    StretchDIBits(
        memDC,
        0, 0, dstWidth, dstHeight,
        0, 0, srcWidth, srcHeight,
        bgraBuffer.data(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );

    // Copy to window (clientRect already defined at top of function)
    BitBlt(hdc, 0, 0, windowWidth, windowHeight, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}
