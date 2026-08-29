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
    //
    // Named as Dazzler::MAX_WIDTH/MAX_HEIGHT rather than written 128, because
    // updateSize() has to arrive at the same number from the same place: a
    // window resized by setScale() and a window built by create() differ then
    // only by the scale they were given.
    int width = Dazzler::MAX_WIDTH * m_scale;
    int height = Dazzler::MAX_HEIGHT * m_scale;

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
    // No card needed, and that is not an accident: this runs from setScale(),
    // and MainWindow::applyDazzlerState() disconnects the window from its
    // Dazzler (setDazzler(nullptr)) before rebuilding the card for a port
    // change. The old "!m_dazzler return" made the scale silently not apply in
    // exactly that case.
    if (!m_hwnd) return;

    // The card's LARGEST mode, not its current one, which is what this function
    // used to size from and why it ended up with no caller at all.
    // m_dazzler->getWidth() reads m_x4Mode and m_use2K, both false from
    // Dazzler's constructor, so on a card no guest has touched it is 32: a
    // scale-4 window would have been sized to a 128x128 client where create()
    // had just given it 512x512. paint() StretchDIBits' whatever mode the card
    // is in over the whole client rect, so the fixed maximum is the contract
    // and the smaller modes are meant to be stretched into it.
    //
    // Measured on the fixed size, driving the running app: View > Dazzler at
    // scale 4 gives a 512x512 client, and a Settings change to scale 2 - which
    // reaches here - leaves a 256x256 client, the same HWND, and the window at
    // the position it had been dragged to.
    int width = Dazzler::MAX_WIDTH * m_scale;
    int height = Dazzler::MAX_HEIGHT * m_scale;

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
        // Hide instead of destroy - let main window manage lifetime - and TELL
        // the main window, which hiding alone did not: the card stayed enabled
        // and View > Dazzler stayed ticked over a window that was not on
        // screen, so getting it back cost two clicks (untick, tick) instead of
        // one.
        //
        // POSTED rather than called back into the owner, and the thread is the
        // point. This window is created by MainWindow::applyDazzlerState() and
        // MainWindow::applyConfig(), both on the UI thread, so its WindowProc
        // runs on that thread and PostMessage lands on that same thread's
        // queue: the owner's handler runs after this WM_CLOSE has returned.
        // A direct call would re-enter this object from inside handleMessage(),
        // since what the owner does with the news is show(false) and
        // setDazzler(nullptr) on this very window.
        //
        // m_parent's only reader. create() passes nullptr as the
        // CreateWindowEx parent ("No parent for independent window"), so the
        // handle it stashes is the only route back to the owner.
        show(false);
        if (m_parent) {
            PostMessageW(m_parent, WM_APP_DAZZLER_CLOSED, 0, 0);
        }
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
