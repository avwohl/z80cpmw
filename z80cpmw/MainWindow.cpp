/*
 * MainWindow.cpp - Main Application Window Implementation
 */

#include "pch.h"
#include "MainWindow.h"
#include "TerminalView.h"
#include "EmulatorEngine.h"
#include "DiskCatalog.h"
#include "DazzlerWindow.h"
#include "Dazzler.h"
#include "SettingsDialogWx.h"
#include "HelpWindow.h"
// help_assets::setCacheRoot() only, called once from onCreate(). HelpAssets.h
// asks for exactly that one call from here and nothing else on the help side is
// this file's business.
#include "HelpAssets.h"
// config::renderBlock() for the configuration report. Already reachable through
// MainWindow.h -> Config.h; named here because this file calls it.
#include "ConfigReport.h"
// diskv0:: for the interface-v0 storage migration and the two places this file
// has to agree with it about a name. Already reachable through DiskCatalog.h;
// named here for the same reason.
#include "DiskMigrationV0.h"
#include "resource.h"
#include "Version.h"
#include "CrashHandler.h"
#include "emu_io.h"
// The RomWBW release the shared core emulates, for the About box. Single source
// of truth in romwbw_emu; DOWNSTREAM asks every port with a version display to
// show it.
// emu_romwbw_supported_list() for the About box, and
// emu_romwbw_release_loaded() / emu_romwbw_release_str() for
// loadedRomwbwRelease(), which is what tells the Settings dialog whether the
// RomWBW release the user has selected for the disk catalog is one the ROM in
// the banks can boot.
#include "emu_init.h"

// External function to set main window for host file dialogs
extern "C" void emu_io_set_main_window(HWND hwnd);

// Real on-disk location of the data folder, resolving MSIX/Store redirection.
// Defined in emu_io_windows.cpp; the Settings dialog uses the same function.
extern "C" const char* emu_io_get_data_folder_display();

static const wchar_t* WINDOW_CLASS = L"Z80CPM_MainWindow";
static const wchar_t* WINDOW_TITLE = L"z80cpmw - Z80 CP/M Emulator";
static bool g_mainClassRegistered = false;

// Posted to ourselves to auto-open the Getting Started help on first run, after
// the main window is shown and the message loop is running.
static const UINT WM_APP_SHOW_WELCOME = WM_APP + 1;

// Posted by postToUiThread(): lParam owns a heap-allocated std::function to run.
static const UINT WM_APP_RUN_ON_UI = WM_APP + 2;

// WM_APP + 3 is spoken for too: DazzlerWindow.h declares WM_APP_DAZZLER_CLOSED,
// which it has to, being the side that posts it.

MainWindow::MainWindow()
    : m_terminal(std::make_unique<TerminalView>())
    , m_emulator(std::make_unique<EmulatorEngine>())
    , m_diskCatalog(std::make_shared<DiskCatalog>())
    , m_uiPostGate(std::make_shared<WorkerPostGate>())
{
}

MainWindow::~MainWindow() {
    if (m_emulatorTimer) {
        KillTimer(m_hwnd, m_emulatorTimer);
    }
    // applyConfig() can build these before run() is ever entered, so the
    // message loop is not the only owner.
    destroyAccelerators();
}

bool MainWindow::create() {
    // Register window class
    if (!g_mainClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        HINSTANCE hInst = GetModuleHandle(nullptr);
        wc.hInstance = hInst;
        wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
        wc.lpszClassName = WINDOW_CLASS;
        wc.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        g_mainClassRegistered = true;
    }

    // Calculate window size based on terminal dimensions
    int charWidth = 10;  // Approximate for 20pt font
    int charHeight = 20;
    int termWidth = TerminalView::COLS * charWidth + 20;
    int termHeight = TerminalView::ROWS * charHeight + 50;

    // Adjust for window frame, menu, and status bar
    RECT rect = { 0, 0, termWidth, termHeight };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, TRUE);

    m_hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,  // WS_CLIPCHILDREN prevents drawing over child windows
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    return m_hwnd != nullptr;
}

void MainWindow::show(int cmdShow) {
    // Reopen at the saved position/size if we have one; otherwise size the
    // window to fit the terminal at the current font/DPI.
    if (!restoreWindowPlacement()) {
        resizeWindowToTerminal();
        ShowWindow(m_hwnd, cmdShow);
    }
    UpdateWindow(m_hwnd);
}

void MainWindow::resizeWindowToTerminal() {
    if (!m_hwnd || !m_terminal) return;
    if (IsZoomed(m_hwnd)) return;  // don't fight a maximized window

    int cw = m_terminal->getCharWidth();
    int ch = m_terminal->getCharHeight();
    if (cw <= 0 || ch <= 0) return;

    // Height of the status bar, which sits below the terminal.
    int statusHeight = 0;
    if (m_statusBar) {
        RECT sr = {};
        GetWindowRect(m_statusBar, &sr);
        statusHeight = sr.bottom - sr.top;
    }

    // Desired client area = the full 80x25 grid plus the status bar.
    RECT rc = { 0, 0,
                TerminalView::COLS * cw,
                TerminalView::ROWS * ch + statusHeight };

    // Expand to the full window rectangle (frame + menu), DPI-aware.
    UINT dpi = GetDpiForWindow(m_hwnd);
    DWORD style = (DWORD)GetWindowLongPtrW(m_hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    AdjustWindowRectExForDpi(&rc, style, TRUE /* has menu */, exStyle, dpi);

    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    // Keep the current top-left, but keep the window on its monitor's work area.
    RECT cur = {};
    GetWindowRect(m_hwnd, &cur);
    int x = cur.left;
    int y = cur.top;

    HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(hMon, &mi)) {
        int waW = mi.rcWork.right - mi.rcWork.left;
        int waH = mi.rcWork.bottom - mi.rcWork.top;
        if (winW > waW) winW = waW;   // never larger than the work area
        if (winH > waH) winH = waH;
        if (x + winW > mi.rcWork.right)  x = mi.rcWork.right - winW;
        if (y + winH > mi.rcWork.bottom) y = mi.rcWork.bottom - winH;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y < mi.rcWork.top)  y = mi.rcWork.top;
    }

    SetWindowPos(m_hwnd, nullptr, x, y, winW, winH, SWP_NOZORDER | SWP_NOACTIVATE);
}

bool MainWindow::restoreWindowPlacement() {
    const auto& w = config::ConfigManager::instance().get().window;
    if (w.width <= 0 || w.height <= 0) {
        return false;  // nothing saved yet
    }

    RECT r = { w.x, w.y, w.x + w.width, w.y + w.height };

    // The saved rectangle must still land on a monitor (it would be off-screen,
    // e.g. after a display was unplugged, if it does not).
    HMONITOR hMon = MonitorFromRect(&r, MONITOR_DEFAULTTONULL);
    if (hMon == nullptr) {
        return false;
    }

    // And that monitor must have the same bounds it had when we saved. If the
    // layout changed (different monitor, moved, or a resolution change), fall
    // back to the default placement instead of reopening somewhere unexpected.
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(hMon, &mi) ||
        mi.rcMonitor.left   != w.monLeft  ||
        mi.rcMonitor.top    != w.monTop   ||
        mi.rcMonitor.right  != w.monRight ||
        mi.rcMonitor.bottom != w.monBottom) {
        return false;
    }

    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(wp);
    wp.rcNormalPosition = r;
    wp.showCmd = w.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    return SetWindowPlacement(m_hwnd, &wp) != 0;
}

void MainWindow::saveWindowPlacement() {
    if (!m_hwnd) return;

    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(m_hwnd, &wp)) return;

    // rcNormalPosition is the restored (non-maximized) rectangle, which is what
    // we want to reopen at even if the window is currently maximized.
    auto& w = config::ConfigManager::instance().get().window;
    w.x = wp.rcNormalPosition.left;
    w.y = wp.rcNormalPosition.top;
    w.width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    w.height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    w.maximized = (wp.showCmd == SW_SHOWMAXIMIZED) ||
                  ((wp.flags & WPF_RESTORETOMAXIMIZED) != 0);

    // Record the monitor the window is on, so restore can detect layout changes.
    HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (hMon && GetMonitorInfo(hMon, &mi)) {
        w.monLeft   = mi.rcMonitor.left;
        w.monTop    = mi.rcMonitor.top;
        w.monRight  = mi.rcMonitor.right;
        w.monBottom = mi.rcMonitor.bottom;
    }

    config::ConfigManager::instance().save();
}

void MainWindow::destroyAccelerators() {
    if (m_hAccel) {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
    if (m_hAccelRetired) {
        DestroyAcceleratorTable(m_hAccelRetired);
        m_hAccelRetired = nullptr;
    }
}

void MainWindow::rebuildAccelerators() {
    // The table is built at runtime, not loaded from the .rc, so the keys that
    // double as app shortcuts can be released to CP/M from the config. A
    // released key is simply omitted, and then falls through to the terminal
    // and the keymap: TranslateAccelerator swallows the whole keystroke when it
    // matches, so the WM_CHAR the terminal needs is never generated.
    const auto& kb = config::ConfigManager::instance().get().keyboard;
    std::vector<ACCEL> accels;
    if (!kb.f1ToCpm) {
        accels.push_back({ FVIRTKEY, VK_F1, (WORD)ID_HELP_TOPICS });
    }
    if (!kb.f5ToCpm) {
        accels.push_back({ FVIRTKEY, VK_F5, (WORD)ID_EMU_START });
        accels.push_back({ (BYTE)(FVIRTKEY | FSHIFT), VK_F5, (WORD)ID_EMU_STOP });
    }
    if (!kb.ctrlRToCpm) {
        accels.push_back({ (BYTE)(FVIRTKEY | FCONTROL), (WORD)'R', (WORD)ID_EMU_RESET });
    }

    // TranslateAccelerator SENDs WM_COMMAND from inside the message loop, so a
    // command handler that reloads the config can land here while the table it
    // was called through is still on the stack. Retire the old handle for one
    // generation rather than freeing it under the caller's feet.
    if (m_hAccelRetired) {
        DestroyAcceleratorTable(m_hAccelRetired);
    }
    m_hAccelRetired = m_hAccel;

    // Every shortcut can be released at once, and CreateAcceleratorTable
    // rejects an empty table; a null handle is fine, the loop below skips it.
    m_hAccel = accels.empty()
                   ? nullptr
                   : CreateAcceleratorTable(accels.data(), (int)accels.size());
}

int MainWindow::run() {
    // applyConfig() has already built this during onCreate; rebuild anyway so
    // run() is correct even if it is ever entered without a config load.
    rebuildAccelerators();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!m_hAccel || !TranslateAccelerator(m_hwnd, m_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    destroyAccelerators();
    return (int)msg.wParam;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->m_hwnd = hwnd;
    } else {
        window = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        onCreate();
        return 0;

    case WM_DESTROY:
        onDestroy();
        return 0;

    case WM_SIZE:
        onSize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_COMMAND:
        onCommand(LOWORD(wParam));
        return 0;

    case WM_TIMER:
        if (wParam == IDT_EMULATOR) {
            onTimer();
        }
        return 0;

    case WM_SETFOCUS:
        if (m_terminal && m_terminal->getHwnd()) {
            SetFocus(m_terminal->getHwnd());
        }
        return 0;

    case WM_CLOSE:
        saveWindowPlacement();  // remember where/how big the window is
        // Flush even when stopped (retries a dirty-disk save that failed at
        // the last stop), but never while crashing - a corrupted machine
        // must not overwrite good images (same rule as onTimer).
        if (m_emulator && !IsCrashing()) {
            if (m_emulator->isRunning()) {
                m_emulator->stop();
            } else {
                m_emulator->flushAllDisks();
            }
        }
        DestroyWindow(m_hwnd);
        return 0;

    case WM_QUERYENDSESSION:
        return TRUE;  // nothing blocks logoff/shutdown; cleanup is below

    case WM_ENDSESSION:
        // Logoff/shutdown terminates the process after this message: WM_CLOSE
        // never arrives and destructors are not guaranteed to run, so flush
        // dirty disks and save window state here or lose them. Same
        // stopped-retry and IsCrashing rules as WM_CLOSE.
        if (wParam) {
            saveWindowPlacement();
            if (m_emulator && !IsCrashing()) {
                if (m_emulator->isRunning()) {
                    m_emulator->stop();
                } else {
                    m_emulator->flushAllDisks();
                }
            }
        }
        return 0;

    case WM_APP_SHOW_WELCOME: {
        // First-run welcome: open the scrollable Getting Started help once.
        auto& cfg = config::ConfigManager::instance().get();
        if (!cfg.welcomeShown) {
            cfg.welcomeShown = true;
            config::ConfigManager::instance().save();
            ShowHelpWindow(m_hwnd, help_topics::GettingStarted);
        }
        return 0;
    }

    case WM_APP_RUN_ON_UI: {
        auto* fn = reinterpret_cast<std::function<void()>*>(lParam);
        if (fn) {
            if (!IsCrashing()) {
                (*fn)();
            }
            delete fn;
        }
        return 0;
    }

    case WM_APP_DAZZLER_CLOSED:
        // The user closed the Dazzler window. That is the same gesture as
        // unticking View > Dazzler, so it takes the same path: the card goes
        // down, the check mark comes off, and the choice is saved. Hiding the
        // window was all that used to happen, and it left the other two saying
        // the Dazzler was on.
        //
        // Guarded rather than called flat, because onViewDazzler() TOGGLES. The
        // post sits at the back of the queue, so a View > Dazzler click already
        // queued ahead of it can turn the card off first - and an unguarded
        // toggle would then turn it straight back on and reopen the window the
        // user had just closed.
        if (m_dazzlerEnabled) {
            onViewDazzler();
        }
        return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void MainWindow::onCreate() {
    // Initialize common controls
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // Set main window handle for R8/W8 file dialogs
    emu_io_set_main_window(m_hwnd);

    // Point the help cache at the same root everything else user-writable uses.
    // HelpAssets.h asks for this exact call and had no caller, so cacheDir()
    // was falling back to %LOCALAPPDATA% read from the environment - a fourth
    // way of naming the directory that getUserDataDirectory(), DiskCatalog's
    // constructor and emu_io_windows.cpp's getDataFolder all reach through
    // SHGetKnownFolderPath instead.
    //
    // Here, and not somewhere later, because setCacheRoot has to run before any
    // help window can exist: cacheDir() is read on the thread fetchTopic()
    // detaches and there is no lock, so a later call would be a data race and
    // not a reconfiguration. Both routes to ShowHelpWindow are dispatched from
    // run()'s message loop - ID_HELP_TOPICS arrives as WM_COMMAND, whether from
    // the menu or from the F1 accelerator TranslateAccelerator turns into one,
    // and the first-run welcome is the WM_APP_SHOW_WELCOME this function posts
    // to itself at the bottom - while onCreate runs inside the CreateWindowExW
    // in MainWindow::create(), which wWinMain calls before run(). Every help
    // window is therefore opened after this line, including the posted one.
    help_assets::setCacheRoot(EmulatorEngine::getUserDataDirectory() + "\\help");

    // Create status bar
    m_statusBar = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd,
        (HMENU)IDC_STATUSBAR,
        GetModuleHandle(nullptr),
        nullptr
    );

    // Get client area dimensions
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    // Get status bar height
    RECT statusRect;
    GetWindowRect(m_statusBar, &statusRect);
    int statusHeight = statusRect.bottom - statusRect.top;

    // Create terminal view (full height minus status bar)
    m_terminal->create(
        m_hwnd,
        0, 0,
        clientRect.right,
        clientRect.bottom - statusHeight
    );

    m_terminal->setFontSize(20);  // Default, will be updated by loadSettings()

    // Set up terminal input callback
    m_terminal->setKeyInputCallback([this](char ch) {
        if (m_emulator && m_emulator->isRunning()) {
            m_emulator->sendChar(ch);
        }
    });

    // Paste/keys are only deliverable while the emulator is running.
    m_terminal->setInputReadyCallback([this]() {
        return m_emulator && m_emulator->isRunning();
    });

    // Set up emulator callbacks
    m_emulator->setOutputCallback([this](uint8_t ch) {
        onOutputChar(ch);
    });

    m_emulator->setStatusCallback([this](const std::string& status) {
        onStatusChanged(status);
    });

    // The menu handle has to exist before anything that sets a check mark:
    // loadDefaultROM() and loadSettings() -> applyConfig() both do, and both
    // used to write their check marks into a null HMENU and then have them
    // overwritten by the unconditional default below.
    m_menu = GetMenu(m_hwnd);
    checkFontMenuItem(20);  // Default, applyConfig will update

    // Load ROM and saved settings (disks, boot string, font size)
    loadDefaultROM();
    loadSettings();  // Loads saved disk paths and other settings

    // Start emulator timer
    m_emulatorTimer = SetTimer(m_hwnd, IDT_EMULATOR, TIMER_INTERVAL_MS, nullptr);

    // Update status. updateMenuState() has to run here too: until it does, the
    // menu keeps the enabled state compiled into the .rc, which offers Start on
    // a machine whose ROM failed to load.
    updateStatusBar();
    updateMenuState();

    // Show startup instructions in terminal
    showStartupInstructions();

    // On first run, open the scrollable Getting Started help once. Posted (not
    // called directly) so it happens after the main window is shown.
    if (!config::ConfigManager::instance().get().welcomeShown) {
        PostMessage(m_hwnd, WM_APP_SHOW_WELCOME, 0, 0);
    }
}

void MainWindow::onDestroy() {
    // Settle with the download workers here rather than in ~MainWindow, because
    // this is where m_hwnd stops being a window, and because it buys them the
    // whole message-loop drain - measured at about 40ms between this and the
    // process exiting - to act on the cancel instead of the microseconds a
    // destructor would leave.
    //
    // The gate first: after close() returns, no worker can PostMessage to the
    // handle this window used to be, so nothing can be delivered to whatever
    // window that handle is recycled into. Nothing else has to close it - the
    // only path that destroys a MainWindow without getting here is create()
    // failing, and no download exists then.
    //
    // Then the cancel, which is about the disk rather than about lifetime: the
    // transfer cannot finish, so the read loop noticing is what closes the file
    // and removes the partial .img before ExitProcess kills the worker
    // mid-fwrite. See DiskCatalog::cancelDownload.
    m_uiPostGate->close();
    m_diskCatalog->cancelDownload();

    if (m_emulatorTimer) {
        KillTimer(m_hwnd, m_emulatorTimer);
        m_emulatorTimer = 0;
    }

    // Clean up Dazzler window
    if (m_dazzlerWindow) {
        m_dazzlerWindow->destroy();
        m_dazzlerWindow.reset();
    }

    PostQuitMessage(0);
}

void MainWindow::onSize(int width, int height) {
    // Resize status bar
    SendMessage(m_statusBar, WM_SIZE, 0, 0);

    // Get status bar height
    RECT statusRect = {};
    if (m_statusBar) {
        GetWindowRect(m_statusBar, &statusRect);
    }
    int statusHeight = statusRect.bottom - statusRect.top;

    // When called via synthetic WM_SIZE (e.g. from setFontSize), lParam is 0,0.
    // Fall back to the actual client area rather than a hardcoded 800x500,
    // otherwise the terminal gets clamped and larger fonts clip off-screen.
    if (width <= 0 || height <= 0) {
        RECT client;
        GetClientRect(m_hwnd, &client);
        width = client.right;
        height = client.bottom;
    }

    int termWidth = width > 0 ? width : 800;
    int termHeight = (height - statusHeight) > 0 ? (height - statusHeight) : 500;

    if (m_terminal && m_terminal->getHwnd()) {
        SetWindowPos(
            m_terminal->getHwnd(),
            nullptr,
            0, 0,
            termWidth,
            termHeight,
            SWP_NOZORDER
        );
    }
}

void MainWindow::onCommand(int id) {
    switch (id) {
    case ID_FILE_LOADDISK0:
        onFileLoadDisk(0);
        break;
    case ID_FILE_LOADDISK1:
        onFileLoadDisk(1);
        break;
    case ID_FILE_SAVEDISK0:
        onFileSaveDisk(0);
        break;
    case ID_FILE_SAVEDISK1:
        onFileSaveDisk(1);
        break;
    case ID_FILE_SAVEDISKS:
        onFileSaveAllDisks();
        break;
    case ID_FILE_LOADPROFILE:
        onLoadProfile();
        break;
    case ID_FILE_SAVEPROFILE:
        onSaveProfileAs();
        break;
    case ID_FILE_EXIT:
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
        break;

    case ID_ROM_EMU_AVW:
    case ID_ROM_EMU_ROMWBW:
        onSelectROM(id);
        break;

    case ID_EMU_START:
        onEmulatorStart();
        break;
    case ID_EMU_STOP:
        onEmulatorStop();
        break;
    case ID_EMU_RESET:
        onEmulatorReset();
        break;
    case ID_EMU_SETTINGS:
        onEmulatorSettings();
        break;

    case ID_VIEW_FONT14:
        onViewFontSize(14);
        break;
    case ID_VIEW_FONT16:
        onViewFontSize(16);
        break;
    case ID_VIEW_FONT18:
        onViewFontSize(18);
        break;
    case ID_VIEW_FONT20:
        onViewFontSize(20);
        break;
    case ID_VIEW_FONT24:
        onViewFontSize(24);
        break;
    case ID_VIEW_FONT28:
        onViewFontSize(28);
        break;

    case ID_VIEW_DAZZLER:
        onViewDazzler();
        break;

    case ID_HELP_TOPICS:
        onHelpTopics();
        break;

    case ID_HELP_ABOUT:
        onHelpAbout();
        break;
    }
}

void MainWindow::onTimer() {
    // While a crash report is up, its modal loop still dispatches WM_TIMER;
    // never keep executing a corrupted machine (or flushing its disks).
    if (IsCrashing()) return;

    if (m_emulator && m_emulator->isRunning()) {
        // Idle power management: when the guest is polling console status with
        // no input available (typical CP/M prompt idle loop), skip most timer
        // ticks to reduce CPU usage. The timer still fires every 10ms so we
        // respond immediately when input arrives via emu_console_has_input().
        if (m_emulator->isIdle()) {
            if (++m_idleSkipCount < IDLE_SKIP_TICKS) {
                // Still check if input became available so we wake up promptly
                if (!emu_console_has_input()) return;
            }
            m_idleSkipCount = 0;
        } else {
            m_idleSkipCount = 0;
        }

        m_emulator->runBatch();
        m_emulator->flushOutput();

        // Force terminal to repaint after batch processing
        if (m_terminal) {
            m_terminal->repaint();
        }

        // Update Dazzler window if enabled
        if (m_dazzlerWindow && m_dazzlerEnabled) {
            m_dazzlerWindow->repaint();
        }

        // Update status bar with instruction count every ~500ms
        static int timerCount = 0;
        if (++timerCount >= 50) {  // 50 * 10ms = 500ms
            timerCount = 0;
            char buf[128];
            sprintf(buf, "Running - PC: 0x%04X  Instructions: %llu",
                    m_emulator->getProgramCounter(),
                    m_emulator->getInstructionCount());
            m_statusText = buf;
            updateStatusBar();

            // Check for NVRAM changes (user configured via ROM's SYSCONF utility)
            if (m_emulator->hasNvramChange()) {
                std::string setting = m_emulator->getNvramSetting();
                config::ConfigManager::instance().get().bootString = setting;
                saveSettings();
            }

            // Check for manifest disk write warning
            if (m_emulator->pollManifestWriteWarning()) {
                MessageBoxW(m_hwnd,
                    L"You are writing to a downloaded disk image.\n\n"
                    L"Changes may be lost if the app downloads a new version of this disk.\n"
                    L"To preserve your changes, use File -> Save Disk to save a copy.\n\n"
                    L"This warning can be disabled in Settings.",
                    L"Disk Write Warning", MB_OK | MB_ICONWARNING);
            }
        }

        // Periodic disk auto-save: upstream tracks whether writes occurred
        // and handles 20-second timing internally
        m_emulator->checkPeriodicFlush();
    }
}

void MainWindow::onFileLoadDisk(int unit) {
    wchar_t filename[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Disk Images (*.img)\0*.img\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = unit == 0 ? L"Load Disk 0" : L"Load Disk 1";

    if (GetOpenFileNameW(&ofn)) {
        char path[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, path, MAX_PATH, nullptr, nullptr);

        if (m_emulator->loadDisk(unit, path)) {
            // Update config with new disk path
            auto& cfg = config::ConfigManager::instance().get();
            config::DiskConfig diskCfg;
            diskCfg.path = path;
            cfg.disks[unit] = diskCfg;
            saveSettings();
            m_statusText = "Loaded disk " + std::to_string(unit);
            updateStatusBar();
        } else {
            MessageBoxW(m_hwnd, L"Failed to load disk image", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void MainWindow::onFileSaveDisk(int unit) {
    wchar_t filename[MAX_PATH] = {};
    swprintf_s(filename, L"disk%d.img", unit);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Disk Images (*.img)\0*.img\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = unit == 0 ? L"Save Disk 0" : L"Save Disk 1";
    ofn.lpstrDefExt = L"img";

    if (GetSaveFileNameW(&ofn)) {
        char path[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, path, MAX_PATH, nullptr, nullptr);

        if (m_emulator->saveDisk(unit, path)) {
            m_statusText = "Saved disk " + std::to_string(unit);
            updateStatusBar();
        } else {
            MessageBoxW(m_hwnd, L"Failed to save disk image", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void MainWindow::onFileSaveAllDisks() {
    // Each disk is saved back to the path it was loaded from, which is why
    // there is no app-directory lookup here: a disk lives wherever the user
    // mounted it from, and for a downloaded one that is the data folder.
    for (int unit = 0; unit < 2; unit++) {
        if (m_emulator->isDiskLoaded(unit)) {
            std::string path = m_emulator->getDiskPath(unit);
            if (!path.empty()) {
                m_emulator->saveDisk(unit, path);
            }
        }
    }

    m_statusText = "All disks saved";
    updateStatusBar();
}

void MainWindow::onSelectROM(int romId) {
    std::string romFile;

    switch (romId) {
    case ID_ROM_EMU_AVW:
        romFile = "emu_avw.rom";
        break;
    case ID_ROM_EMU_ROMWBW:
        romFile = "emu_romwbw.rom";
        break;
    default:
        return;
    }

    std::string path = findResourceFile(romFile);
    if (path.empty()) {
        MessageBoxW(m_hwnd, L"ROM file not found", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    if (m_emulator->loadROM(path)) {
        m_emulator->setROMName(romFile);
        checkROMMenuItem(romId);
        m_currentRomId = romId;
        // Choosing a ROM is the answer to every ROM notice - the banks now hold
        // what the user asked for, whatever the file said. Leaving one raised
        // would reprint a settled complaint at the next Start.
        clearNotice(Notice::DefaultRom);
        clearNotice(Notice::SavedRom);
        m_statusText = "Loaded ROM: " + romFile;
        updateStatusBar();
    } else {
        std::string msg = "Failed to load ROM: " + romFile + "\n\n" +
                          m_emulator->getROMError();
        MessageBoxA(m_hwnd, msg.c_str(), "Error", MB_OK | MB_ICONERROR);
    }
}

void MainWindow::onEmulatorStart() {
    // If downloads are in progress, wait for them
    if (m_downloadingDisks) {
        // Output message to terminal
        if (m_terminal) {
            const char* msg = "\r\nPlease wait for disk downloads to complete...\r\n";
            for (const char* p = msg; *p; ++p) {
                m_terminal->outputChar(*p);
            }
        }
        return;
    }

    // Check if any disks are loaded - if not, download and load defaults first
    bool anyDiskLoaded = false;
    for (int i = 0; i < 4; i++) {
        if (m_emulator->isDiskLoaded(i)) {
            anyDiskLoaded = true;
            break;
        }
    }

    if (!anyDiskLoaded) {
        // No disks loaded - download defaults before starting
        downloadAndStartWithDefaults();
        return;
    }

    // Normal start with disks already loaded
    startEmulator();
}

// A cached download is only trusted if it looks complete: older builds could
// cache a truncated file after a failed download, and a bad cache breaks
// every later boot. All default images are >= 8 MB.
static bool diskFileLooksComplete(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) {
        return false;
    }
    ULONGLONG size = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    return size >= (1ull << 20);
}

// Hop a piece of work onto the UI thread from a DiskCatalog worker.
//
// A free function taking the window and the gate BY VALUE, not a MainWindow
// method, and that is the fix rather than an arrangement of it: a method would
// have to read this->m_hwnd on the worker thread, and by the time a download
// callback runs, MainWindow - a stack object in wWinMain - may already be gone.
// Traced on the shipping code with the shutdown widened: the worker reached
// MainWindow::runOnUiThread - the method this replaced - while the UI thread
// was inside ~MainWindow, printing this=000000B1EDB4F450
// m_hwnd=00000000010400E6 isWindow=0. It got
// away with it there only because the destructor had not yet returned; a few
// tens of milliseconds later that read is of dead stack.
//
// The gate closes the rest of it. PostMessage on a destroyed window merely
// fails, but window handles are recycled - IsWindow's documentation says so -
// so a stale handle can name somebody else's window, which would then be
// handed a WM_APP_RUN_ON_UI whose lParam is a pointer into our heap. After
// onDestroy()'s close() no post happens at all.
//
// The captured 'this' inside fn is a different question and is safe: fn can
// only run from a WM_APP_RUN_ON_UI dispatch, only the message loop dispatches,
// and run() returns before ~MainWindow. A post that misses the loop is dropped
// with the window's queue and fn is never called.
static void postToUiThread(HWND hwnd, const std::shared_ptr<WorkerPostGate>& gate,
                           std::function<void()> fn) {
    auto* heapFn = new std::function<void()>(std::move(fn));
    bool posted = false;
    gate->postIfOpen([&] {
        posted = PostMessage(hwnd, WM_APP_RUN_ON_UI, 0,
                             reinterpret_cast<LPARAM>(heapFn)) != FALSE;
    });
    if (!posted) {
        delete heapFn;  // gate shut, or window gone; drop the work
    }
}

void MainWindow::startEmulator() {
    // Refuse before clearing the terminal, which would erase the load error
    // that explains why there is no ROM. Starting anyway runs a CPU over an
    // erased ROM bank: no output, no boot menu, and the status bar claiming
    // "Running" - the exact silent failure the ROM reporting was added to end.
    if (!m_emulator->hasROM()) {
        std::string reason = m_emulator->getROMError();
        std::string msg = "No ROM is loaded, so there is nothing to run.\n\n";
        if (!reason.empty()) {
            msg += "The last ROM failed to load: " + reason + "\n\n";
        }
        msg += "Use Emulator > ROM to choose a ROM file.";
        MessageBoxA(m_hwnd, msg.c_str(), "Cannot start", MB_OK | MB_ICONERROR);
        return;
    }

    if (m_terminal) {
        m_terminal->clear();
        m_terminal->resetScrollback();
        // Put the host-side notices back. The guard above cannot stand in for
        // this: it fires only when there is NO ROM, and the notices that matter
        // here are the ones raised when a good ROM is loaded but not the one
        // the configuration named - hasROM() is true for both, so the clear
        // above used to erase the only explanation on screen.
        printNotices();
    }

    m_emulator->start();
    updateMenuState();

    // Focus terminal
    if (m_terminal && m_terminal->getHwnd()) {
        SetFocus(m_terminal->getHwnd());
    }
}

// The two disks a machine with nothing configured is given, BY CATALOG id.
//
// By id and not by filename, because the filename is the part that moves:
// hd1k_combo.img became hd1k_combo-v0-3.5.1.img when the catalog moved to
// romwbw_disks, and it becomes hd1k_combo-v0-3.6.0.img the moment the user
// selects another RomWBW release. CATALOG_SCHEMA.md 6.1 asks a client to key on
// `id` for exactly this reason, and both of these ids exist under both published
// releases - verified in catalog-v0-3.5.1.json and catalog-v0-3.6.0.json.
static const char* const DEFAULT_DISK_IDS[2] = { "hd1k_combo", "hd1k_games" };

std::string MainWindow::cachedDefaultDisk(const char* diskId) const {
    // Three names, oldest last, and each one is a state a real installation can
    // be in:
    //
    //  1. what the catalog in hand calls it. Only available once a catalog has
    //     been fetched, and it is the only one of the three that is right when
    //     the user has selected a RomWBW release other than the bundled one.
    //  2. the interface-v0 name for the release this build bundles, which is
    //     where the storage migration put the file. Compiled in through
    //     diskv0::v0NameFor, and note what it is used for here: a CACHE PROBE
    //     and never a URL. Constructing a name to look for locally is safe in a
    //     way that constructing one to fetch from is not.
    //  3. the pre-v0 name, because a rename that FAILED leaves the file under
    //     it, and finding 49 MB that is already here beats fetching it again.
    //
    // Names 2 and 3 both name the BUNDLED release, so a hit on either mounts an
    // image built for the ROM this build boots - which is the property that
    // matters. A user who has selected another release in Settings but
    // downloaded none of its images still gets a machine that boots, rather than
    // being sent to the network for disks whose ROM this build does not ship.
    //
    // The whole point of the list is that F5 must not re-download what the
    // machine already has. onEmulatorStart lands here whenever no unit is
    // mounted, which is the ordinary state of a user whose config named no disk,
    // and the version of this function that built one hardcoded path would have
    // sent every migrated user back to the network for 57 MB.
    std::vector<std::string> names;

    DiskEntry entry;
    if (m_diskCatalog->findDiskById(diskId, entry) && !entry.filename.empty()) {
        names.push_back(entry.filename);
    }

    const std::string legacy = std::string(diskId) + ".img";
    std::string bundled;
    if (diskv0::v0NameFor(legacy, bundled)) names.push_back(bundled);
    names.push_back(legacy);

    const std::string dir = m_diskCatalog->getDownloadDirectory();
    for (const auto& name : names) {
        const std::string path = dir + "\\" + name;
        if (diskFileLooksComplete(path)) return path;
    }
    return "";
}

void MainWindow::downloadAndStartWithDefaults() {
    auto termOutput = [this](const std::string& msg) { terminalPrint(msg); };

    const std::string combo = cachedDefaultDisk(DEFAULT_DISK_IDS[0]);
    const std::string games = cachedDefaultDisk(DEFAULT_DISK_IDS[1]);

    if (!combo.empty() && !games.empty()) {
        // Nothing to fetch, so nothing to ask the network about. This is the
        // path a user with no connection is on, and it has to keep working:
        // making the catalog a precondition of STARTING - rather than only of
        // downloading - would have turned an offline launch into a failure.
        auto& cfg = config::ConfigManager::instance().get();
        config::DiskConfig disk0, disk1;
        disk0.path = combo;
        disk1.path = games;
        cfg.disks[0] = disk0;
        cfg.disks[1] = disk1;
        m_emulator->loadDisk(0, combo);
        m_emulator->loadDisk(1, games);
        termOutput("Loaded default disks.\r\n");
        saveSettings();
        startEmulator();
        return;
    }

    // Something has to be downloaded, and after the interface-v0 switch a
    // download URL exists only inside a fetched catalog: there is no release tag
    // to interpolate and no compile-time base to append a filename to. So the
    // catalog comes first, always - which also closes the hole that made this
    // the one path in the application where disk images were written with no
    // checksum check and no ledger record, because it never fetched one and
    // DiskCatalog::downloadDisk looked the expected hash up in a list that was
    // therefore empty.
    termOutput("\r\nLooking up the disk catalog...\r\n");

    m_downloadingDisks = true;   // F5 says "please wait" from here until we finish

    const HWND uiWindow = m_hwnd;
    const std::shared_ptr<WorkerPostGate> uiGate = m_uiPostGate;

    m_diskCatalog->fetchCatalog(
        [this, uiWindow, uiGate](bool ok, const std::vector<DiskEntry>&, const std::string& error) {
            postToUiThread(uiWindow, uiGate, [this, ok, error]() {
                startWithDefaultsAfterCatalog(ok, error);
            });
        });
}

void MainWindow::startWithDefaultsAfterCatalog(bool ok, const std::string& error) {
    auto termOutput = [this](const std::string& msg) { terminalPrint(msg); };

    // When the catalog cannot answer, boot on what the machine already has.
    // Refusing to start would be the wrong trade in every case that reaches
    // here: the network is flat, or the catalog does not carry one of the two
    // ids, and a machine with even one of the default disks still boots. This is
    // the ONLY thing either failure path does, and it is written once so the two
    // cannot end differently.
    auto startOnWhatIsCached = [this, &termOutput]() {
        m_downloadingDisks = false;

        bool mountedAnything = false;
        for (int unit = 0; unit < 2; unit++) {
            const std::string cached = cachedDefaultDisk(DEFAULT_DISK_IDS[unit]);
            if (cached.empty()) continue;
            auto& cfg = config::ConfigManager::instance().get();
            config::DiskConfig disk;
            disk.path = cached;
            cfg.disks[unit] = disk;
            m_emulator->loadDisk(unit, cached);
            termOutput("Disk " + std::to_string(unit) + ": " +
                       diskv0::basenameOf(cached) + " loaded\r\n");
            mountedAnything = true;
        }
        if (!mountedAnything) {
            termOutput("No disk images are available. Check your network connection, "
                       "or use Settings > Disk Images to download one.\r\n");
            return;
        }
        saveSettings();
        startEmulator();
    };

    if (!ok) {
        termOutput("  " + error + "\r\n");
        startOnWhatIsCached();
        return;
    }

    // Resolved against the catalog that has just landed, so these are the
    // filenames of the RomWBW release the user actually has selected.
    DiskEntry entries[2];
    for (int unit = 0; unit < 2; unit++) {
        if (m_diskCatalog->findDiskById(DEFAULT_DISK_IDS[unit], entries[unit])) continue;
        // A catalog that does not carry one of them. Both exist under both
        // published releases, but an id can be absent from a version - hd1k_ws4
        // is in 3.5.1 and not in 3.6.0 - so this is a real answer and not an
        // impossible one.
        termOutput(std::string("  The catalog for RomWBW ") +
                   m_diskCatalog->getSelectedRomwbwVersion() + " does not carry " +
                   DEFAULT_DISK_IDS[unit] + ".\r\n");
        startOnWhatIsCached();
        return;
    }

    // Where each will be once it is here, under the name the catalog gives it.
    const std::string combo = m_diskCatalog->getDiskPath(entries[0].filename);
    const std::string games = m_diskCatalog->getDiskPath(entries[1].filename);

    // Re-probed rather than reusing what downloadAndStartWithDefaults found: the
    // catalog may name a release whose images are not the ones already cached.
    const bool comboExists = diskFileLooksComplete(combo);
    const bool gamesExists = diskFileLooksComplete(games);

    auto setConfigDisk = [](int unit, const std::string& path) {
        auto& cfg = config::ConfigManager::instance().get();
        config::DiskConfig disk;
        disk.path = path;
        cfg.disks[unit] = disk;
    };

    if (comboExists) {
        setConfigDisk(0, combo);
        m_emulator->loadDisk(0, combo);
        termOutput("Disk 0: " + diskv0::basenameOf(combo) + " loaded\r\n");
    }
    if (gamesExists) {
        setConfigDisk(1, games);
        m_emulator->loadDisk(1, games);
        termOutput("Disk 1: " + diskv0::basenameOf(games) + " loaded\r\n");
    }

    if (comboExists && gamesExists) {
        m_downloadingDisks = false;
        saveSettings();
        startEmulator();
        return;
    }

    termOutput("\r\nDownloading default disk images...\r\n");

    const bool needComboDownload = !comboExists;
    const bool needGamesDownload = !gamesExists;

    // Download missing disks then start. DiskCatalog invokes completion
    // callbacks on a detached worker thread; hop back to the UI thread with
    // postToUiThread before touching the terminal/emulator/config (none of
    // which are thread-safe).
    //
    // The two values below are what the worker-side callbacks are allowed to
    // know about this window, and they are copies on purpose. Nothing stops the
    // user quitting mid-transfer - hd1k_combo is 49MB and takes about eight
    // seconds here, and the File menu and the close box stay live throughout -
    // so a callback that read this->m_hwnd would be reading a MainWindow that
    // wWinMain has already taken off its stack. See postToUiThread.
    const HWND uiWindow = m_hwnd;
    const std::shared_ptr<WorkerPostGate> uiGate = m_uiPostGate;

    // The names on the WIRE, which are the catalog's own filenames.
    // DiskCatalog::downloadDisk looks each of them up in the catalog it has just
    // fetched and takes BOTH the URL and the expected sha256 out of that one
    // entry, so there is no longer any way to download one of these without
    // checking it.
    const std::string comboName = entries[0].filename;
    const std::string gamesName = entries[1].filename;

    // Runs on the UI thread after the last download finishes (or fails).
    auto finishAndStart = [this]() {
        m_downloadingDisks = false;
        saveSettings();
        startEmulator();
    };

    auto downloadGames = [this, games, gamesName, finishAndStart, uiWindow, uiGate]() {
        m_diskCatalog->downloadDisk(gamesName,
            nullptr,
            [this, games, finishAndStart, uiWindow, uiGate](bool success, const std::string& error) {
                postToUiThread(uiWindow, uiGate, [this, games, finishAndStart, success, error]() {
                    if (success) {
                        auto& cfg = config::ConfigManager::instance().get();
                        config::DiskConfig disk;
                        disk.path = games;
                        cfg.disks[1] = disk;
                        m_emulator->loadDisk(1, games);
                        terminalPrint("  Disk 1: " + diskv0::basenameOf(games) +
                                      " downloaded and loaded\r\n");
                    } else {
                        terminalPrint("  Disk 1: download failed - " + error + "\r\n");
                    }
                    finishAndStart();
                });
            });
    };

    if (needComboDownload) {
        m_diskCatalog->downloadDisk(comboName,
            nullptr,
            [this, combo, needGamesDownload, downloadGames, finishAndStart, uiWindow, uiGate](bool success, const std::string& error) {
                postToUiThread(uiWindow, uiGate, [this, combo, needGamesDownload, downloadGames, finishAndStart, success, error]() {
                    if (success) {
                        auto& cfg = config::ConfigManager::instance().get();
                        config::DiskConfig disk;
                        disk.path = combo;
                        cfg.disks[0] = disk;
                        m_emulator->loadDisk(0, combo);
                        terminalPrint("  Disk 0: " + diskv0::basenameOf(combo) +
                                      " downloaded and loaded\r\n");
                    } else {
                        terminalPrint("  Disk 0: download failed - " + error + "\r\n");
                    }

                    if (needGamesDownload) {
                        downloadGames();
                    } else {
                        finishAndStart();
                    }
                });
            });
    } else if (needGamesDownload) {
        downloadGames();
    }
}

void MainWindow::onEmulatorStop() {
    m_emulator->stop();
    updateMenuState();
}

void MainWindow::onEmulatorReset() {
    // Reset is a cold boot with nothing between the keystroke and the machine.
    // EmulatorEngine::reset() stops, zeroes PC/SP/IFF, reselects bank 0, clears
    // the HBIOS state and the console queue, and starts it again if it was
    // running, so whatever CP/M held in memory is gone. Nothing asked first,
    // and both live entry points reach it unguarded: the Emulator > Reset item,
    // which is enabled at all times (the .rc leaves it enabled and
    // updateMenuState() grays only Start, Stop and the two ROM items, never
    // Reset), and the Ctrl+R accelerator, which
    // rebuildAccelerators() registers whenever "ctrlRToCpm" is false. There is
    // no toolbar; those two are the whole list.
    //
    // Ask only while the machine is running, which is where the two mobile
    // ports differ: cpmdroid and ioscpm ask unconditionally. Reset on a stopped
    // machine reads wasRunning == false inside reset() and leaves it stopped,
    // so there is no session to lose - and onEmulatorStart() cold-boots with no
    // confirmation at all, so a dialog in front of a Reset that is
    // indistinguishable from a Start would be asking about the one of the two
    // that happens not to have the word Reset on it. Nothing in this repository
    // can settle that: MainWindow.cpp is in no test suite, and which of the two
    // is right is a judgement about users rather than a fact about the code.
    //
    // MB_DEFBUTTON2 so Enter pressed at a dialog the user did not expect - the
    // mistyped Ctrl+R this exists for - cancels rather than reboots. The modal
    // loop still dispatches WM_TIMER, so the machine keeps running while the
    // question is up and No leaves the session exactly as it was.
    if (m_emulator && m_emulator->isRunning()) {
        int answer = MessageBoxW(m_hwnd,
            L"Reset restarts CP/M immediately.\n\n"
            L"Anything a running program has not yet written to a disk is lost.\n\n"
            L"Reset now?",
            L"Reset", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (answer != IDYES) {
            return;
        }
    }

    if (m_terminal) {
        m_terminal->clear();
        m_terminal->resetScrollback();
        // Same reason as startEmulator(): a reset is the other place the screen
        // is emptied, and the notices are as true after it as before it.
        printNotices();
    }
    m_emulator->reset();
    updateMenuState();
}

void MainWindow::onEmulatorSettings() {
    // Stop emulator while settings dialog is open
    bool wasRunning = m_emulator && m_emulator->isRunning();
    if (wasRunning) {
        m_emulator->stop();
    }

    // Use wxWidgets-based settings dialog for proper layout
    WxEmulatorSettings settings;

    // Pass currently loaded disk filenames to settings dialog from config
    const auto& cfg = config::ConfigManager::instance().get();

    // Debug mode from the config, which is the only durable record of it:
    // EmulatorEngine keeps m_debug private and declares no getter, and the only
    // two callers of setDebug() are applyConfig() - which feeds it cfg.debug at
    // startup and after a profile load - and the OK path below, which now
    // writes cfg.debug as well so this seed keeps reading the truth. Seeded
    // false with a "TODO: get from emulator", the box opened unticked however
    // the machine was actually running, and the unconditional setDebug() below
    // then turned debug OFF for anyone who pressed OK without touching it.
    settings.debugMode = cfg.debug;

    // The Dazzler group, which had neither a seed nor a write-back: the
    // checkbox opened unchecked on a machine running a Dazzler, and anything
    // set in it was discarded at OK.
    //
    // WHICH OF THE TWO IS AUTHORITATIVE. At startup the config is, because it
    // is the only side that exists: m_dazzlerEnabled starts false and
    // EmulatorEngine::getDazzler() starts null, and applyConfig() manufactures
    // both from cfg.dazzlers[0] - there is no path that writes the other way
    // before the user acts, so at startup they cannot disagree. Afterwards the
    // live card is authoritative, because updateConfigFromState() - which every
    // saveSettings() runs, including the one at the bottom of this function -
    // rewrites cfg.dazzlers[0].enabled from m_dazzlerEnabled and, whenever that
    // is true, the port and scale from the live Dazzler's
    // getBasePort()/getScale(). A profile load used to be able to drive the two
    // apart - applyConfig() set m_dazzlerEnabled true for an enabled profile
    // but never set it false for a disabled one, so the next save wrote the
    // live side back over the profile - and no longer can: it goes through
    // applyDazzlerState() in both directions.
    //
    // So the seed follows exactly the rule updateConfigFromState() writes back
    // by - the live card where there is one, cfg.dazzlers[0] as the record of
    // the last chosen port and scale where there is not. With neither (a config
    // that has never had a Dazzler), WxEmulatorSettings' own defaults stand,
    // and they are the same 0x0E/4 onViewDazzler falls back to.
    if (const Dazzler* liveDazzler = m_emulator->getDazzler()) {
        settings.dazzlerEnabled = m_dazzlerEnabled;
        settings.dazzlerPort = liveDazzler->getBasePort();
        settings.dazzlerScale = liveDazzler->getScale();
    } else if (!cfg.dazzlers.empty()) {
        settings.dazzlerEnabled = false;
        settings.dazzlerPort = cfg.dazzlers[0].port;
        settings.dazzlerScale = cfg.dazzlers[0].scale;
    }

    settings.warnManifestWrites = cfg.warnManifestWrites;
    settings.scrollbackLines = cfg.scrollbackLines;
    settings.bellEnabled = cfg.bellEnabled;

    // The whole "keyboard.keys" object, not a filtered view of it. The dialog
    // does a read-modify-write and hands back everything it was given,
    // including names it could not resolve, so what is passed in is what
    // decides whether an entry survives.
    settings.keyBindings = cfg.keyboard.keys;
    settings.f1ToCpm = cfg.keyboard.f1ToCpm;
    settings.f5ToCpm = cfg.keyboard.f5ToCpm;
    settings.ctrlRToCpm = cfg.keyboard.ctrlRToCpm;

    // Seed the ROM the emulator is actually running. Left empty, the dialog
    // falls back to its first entry and OK then "changes" the ROM to
    // emu_avw.rom every time, whatever the user had selected.
    settings.romFile = m_emulator->getROMName();

    // The disk catalog's RomWBW release, both halves of it: what the user has
    // chosen (or has not - empty means "the index's default"), and what the ROM
    // in the banks actually is. The dialog needs both because they can disagree,
    // and a user who selects 3.6.0 while booting a 3.5.1 ROM has to be told what
    // that means before they download 200 MB of disks that will print
    // "*** WARNING: HBIOS/CBIOS Version Mismatch ***" at them.
    settings.romwbwVersion = cfg.romwbwVersion;
    settings.loadedRomwbwRelease = loadedRomwbwRelease();

    // A bare name for a disk in the data folder, the WHOLE path for one
    // anywhere else, because that is the distinction the write-back below makes
    // when it reads these values again: it treats a bare name as "in the data
    // folder" and resolves it through DiskCatalog::getDiskPath. Handing it the
    // basename of C:\mine\volume.img therefore turned the user's own image into
    // <dataDir>\volume.img, which is not a file, so the slot was left behind by
    // the write-back and - with the dropdown carrying no such entry either -
    // erased by the "(None)" branch instead.
    const std::string dataDir = m_diskCatalog->getDownloadDirectory();
    for (int i = 0; i < 4; i++) {
        if (!cfg.disks[i].has_value() || cfg.disks[i]->path.empty()) continue;
        const std::string& diskPath = cfg.disks[i]->path;
        settings.diskFiles[i] = diskv0::isDirectlyIn(diskPath, dataDir)
                                    ? diskv0::basenameOf(diskPath)
                                    : diskPath;
    }

    // .get() rather than the shared_ptr, and that is not a hole: the dialog is
    // modal and lives entirely inside this call, on the UI thread, so this
    // MainWindow and its reference to the catalog outlive it by construction.
    // What the dialog's own callbacks need is the gate they already carry, not
    // a share of the catalog - the workers take their own.
    if (ShowWxSettingsDialog(m_hwnd, m_diskCatalog.get(), settings)) {
        // Hoisted from the disk loop below, which is where this reference used
        // to be declared, because the debug and Dazzler write-backs added here
        // need it too and one alias for the singleton is enough.
        auto& cfgMut = config::ConfigManager::instance().get();

        // Handle clear boot config request
        if (settings.clearBootConfigRequested) {
            m_emulator->clearNvramSetting();
            cfgMut.bootString.clear();
        }

        // Apply debug mode - to the config as well as to the emulator. cfg.debug
        // is what the seed above reads and what survives a restart, and nothing
        // else writes it: leaving it alone here would end a debug session at the
        // next applyConfig(), which feeds the stale value straight back to
        // setDebug(), and would show the same stale value in the box.
        cfgMut.debug = settings.debugMode;
        m_emulator->setDebug(settings.debugMode);

        // The catalog's RomWBW release. Written to the config and handed to the
        // catalog together, so the two cannot disagree about which release the
        // next fetch is for. Nothing is downloaded, deleted or unmounted here:
        // the disks in the four slots are absolute paths to files that keep
        // existing, and the images of both releases live side by side because
        // every v0 filename carries its own.
        //
        // UNCONDITIONAL, and the "if it changed" version of this was wrong. The
        // dialog tells the catalog its preference the moment the control moves,
        // because the list underneath has to be refetched to match - so by the
        // time OK is pressed the catalog may already be holding a preference the
        // dialog has since backed away from. That is not hypothetical: a switch
        // to 3.6.0 whose fetch FAILS puts the control back on the release still
        // in hand, which is usually the one already in the config, so the
        // comparison found no change, skipped the call, and left the catalog
        // preferring 3.6.0 while every stored and displayed value said 3.5.1 -
        // and the next fetch, from F5 or the next Settings open, quietly used
        // 3.6.0. Setting a preference costs nothing and starts nothing, so
        // there is no reason to make it conditional.
        cfgMut.romwbwVersion = settings.romwbwVersion;
        m_diskCatalog->setPreferredRomwbwVersion(settings.romwbwVersion);

        // Load the ROM only if it actually changed, and record the change
        // everywhere the rest of the app reads it. Reloading unconditionally
        // restarted the guest on identical firmware, and the success path used
        // to update none of the ROM name, the menu check mark or m_currentRomId
        // - so saveSettings() below then wrote the *previous* ROM back to the
        // config, and the next launch ran something else again.
        if (!settings.romFile.empty() &&
            settings.romFile != m_emulator->getROMName()) {
            std::string romPath = findResourceFile(settings.romFile);
            if (romPath.empty()) {
                std::string msg = "ROM file not found: " + settings.romFile;
                MessageBoxA(m_hwnd, msg.c_str(), "Error", MB_OK | MB_ICONERROR);
            } else if (!m_emulator->loadROM(romPath)) {
                std::string msg = "Failed to load ROM: " + settings.romFile + "\n\n" +
                                  m_emulator->getROMError();
                MessageBoxA(m_hwnd, msg.c_str(), "Error", MB_OK | MB_ICONERROR);
            } else {
                m_emulator->setROMName(settings.romFile);
                m_currentRomId = (settings.romFile == "emu_romwbw.rom")
                                     ? ID_ROM_EMU_ROMWBW
                                     : ID_ROM_EMU_AVW;
                checkROMMenuItem(m_currentRomId);
                // A ROM chosen here retires the ROM notices for the same reason
                // onSelectROM's does: the banks now hold what the user asked
                // for. saveSettings() below then writes that choice out, so the
                // next launch has nothing to complain about either.
                clearNotice(Notice::DefaultRom);
                clearNotice(Notice::SavedRom);
            }
        }

        // Load disks and update config
        for (int i = 0; i < 4; i++) {
            if (!settings.diskFiles[i].empty()) {
                std::string diskPath;
                // Check if this is already an absolute path (from Browse dialog)
                // or just a filename (from catalog/download directory)
                bool isAbsolute = (settings.diskFiles[i].length() >= 2 &&
                                   settings.diskFiles[i][1] == ':') ||
                                  (settings.diskFiles[i].length() >= 1 &&
                                   (settings.diskFiles[i][0] == '\\' || settings.diskFiles[i][0] == '/'));
                bool isManifestDisk = false;
                if (isAbsolute) {
                    diskPath = settings.diskFiles[i];
                } else {
                    diskPath = m_diskCatalog->getDiskPath(settings.diskFiles[i]);
                    isManifestDisk = true;  // From catalog = manifest disk
                }
                if (GetFileAttributesA(diskPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    m_emulator->loadDisk(i, diskPath);
                    m_emulator->setDiskIsManifest(i, isManifestDisk);
                    // Update config
                    config::DiskConfig disk;
                    disk.path = diskPath;
                    disk.isManifest = isManifestDisk;
                    cfgMut.disks[i] = disk;
                }
            } else {
                // User selected "(None)" - close the disk if one was loaded
                if (m_emulator->isDiskLoaded(i)) {
                    m_emulator->closeDisk(i);
                }
                cfgMut.disks[i] = std::nullopt;
            }
        }

        // Apply manifest write warning setting
        cfgMut.warnManifestWrites = settings.warnManifestWrites;
        for (int i = 0; i < 4; i++) {
            m_emulator->setDiskWarningSuppressed(i, !settings.warnManifestWrites);
        }

        // Apply terminal scrollback size
        cfgMut.scrollbackLines = settings.scrollbackLines;
        if (m_terminal) {
            m_terminal->setScrollbackLines(settings.scrollbackLines);
        }

        // The bell. TerminalView holds this itself and clear() deliberately
        // does not reset it, so setting it once here is enough - a guest's
        // ESC c will not put it back.
        cfgMut.bellEnabled = settings.bellEnabled;
        if (m_terminal) {
            m_terminal->setBellEnabled(settings.bellEnabled);
        }

        // The key bindings, gated on the dialog saying a row actually changed.
        // The terminal is rebuilt from cfgMut.keyboard.keys and not from
        // settings.keyBindings, because only the config is guaranteed current:
        // the dialog assigns settings.keyBindings only when it sets the dirty
        // flag, so when nothing was edited that member still holds the copy
        // seeded above. Reading the config means this line cannot depend on
        // which of the two it is.
        if (settings.keyBindingsDirty) {
            cfgMut.keyboard.keys = settings.keyBindings;
        }
        if (m_terminal) {
            m_terminal->setKeyBindings(cfgMut.keyboard.keys);
        }

        // The three shortcut switches, and the two things that have to be
        // redone for them to take effect without a restart. Both are called
        // unconditionally: they are the same pair applyConfig() runs after a
        // profile load, they read the config rather than a delta, and neither
        // is expensive enough to be worth a comparison that could get the
        // condition wrong.
        cfgMut.keyboard.f1ToCpm = settings.f1ToCpm;
        cfgMut.keyboard.f5ToCpm = settings.f5ToCpm;
        cfgMut.keyboard.ctrlRToCpm = settings.ctrlRToCpm;
        rebuildAccelerators();
        updateMenuAccelHints();

        // The Dazzler, applied to the machine and not merely to the config. A
        // config-only write-back would be discarded a second time: saveSettings()
        // below runs updateConfigFromState(), which rewrites cfg.dazzlers[0]
        // from the live card, so the live card has to be changed first or the
        // group stays as inert as it was.
        //
        // The config write above it is still needed, for the one case
        // updateConfigFromState() cannot express: with the box unticked it sets
        // enabled=false and leaves port and scale at whatever they already
        // were, so a port chosen and then switched off would not survive to
        // seed the next open.
        //
        // Written unconditionally rather than only when there is something to
        // remember, and the cost is one inert entry - enabled false, the
        // default port and scale - in the config of someone who pressed OK
        // having never touched the Dazzler. Neither of the two places that act
        // on the array can tell it from the empty one it replaces:
        // applyConfig() reads 0x0E and 4 out of an empty array and then acts
        // only on enabled or on a live card, of which an inert entry says
        // neither, and onViewDazzler reads the same 0x0E and 4 out of it.
        if (cfgMut.dazzlers.empty()) {
            cfgMut.dazzlers.push_back(config::DazzlerConfig{});
        }
        cfgMut.dazzlers[0].enabled = settings.dazzlerEnabled;
        cfgMut.dazzlers[0].port = (uint8_t)settings.dazzlerPort;
        cfgMut.dazzlers[0].scale = settings.dazzlerScale;
        applyDazzlerState(settings.dazzlerEnabled,
                          (uint8_t)settings.dazzlerPort,
                          settings.dazzlerScale);

        // Save settings to disk
        saveSettings();

        m_statusText = "Settings applied";
        updateStatusBar();
    }

    if (wasRunning) {
        m_emulator->start();
    }

    updateMenuState();
}

void MainWindow::onViewFontSize(int size) {
    if (m_terminal) {
        m_terminal->setFontSize(size);
        config::ConfigManager::instance().get().fontSize = size;
        checkFontMenuItem(size);
        resizeWindowToTerminal();  // grow/shrink the window to fit the new font
        saveSettings();  // Persist font size
    }
}

void MainWindow::applyDazzlerState(bool enabled, uint8_t port, int scale) {
    // Whether the card was ALREADY on decides whether the window is put back on
    // screen below; see the show() at the end of the enable arm.
    const bool wasEnabled = m_dazzlerEnabled;
    m_dazzlerEnabled = enabled;

    // Update menu checkmark
    CheckMenuItem(m_menu, ID_VIEW_DAZZLER, enabled ? MF_CHECKED : MF_UNCHECKED);

    if (enabled) {
        // A port change on a card that is already running needs the card torn
        // down first: EmulatorEngine::enableDazzler returns immediately when
        // m_dazzler is non-null, and Dazzler exposes getBasePort() but no
        // setBasePort() - the port is a constructor argument. Scale is the other
        // way round: Dazzler::setScale() exists, so a scale change is applied to
        // the live card just below instead of costing it its life. Dropping the
        // window's pointer first matters: disableDazzler() destroys the object
        // it points at.
        Dazzler* live = m_emulator->getDazzler();
        if (live && live->getBasePort() != port) {
            if (m_dazzlerWindow) {
                m_dazzlerWindow->setDazzler(nullptr);
            }
            m_emulator->disableDazzler();
        }
        m_emulator->enableDazzler(port, scale);

        // The scale onto the card, by hand, because nothing else does it
        // reliably. enableDazzler() sets it on the card it CONSTRUCTS and
        // returns at once ("Already enabled") for one that exists. The other
        // writer, DazzlerWindow::setScale() below, writes it through the card
        // the window is attached to - and neither its being attached nor its
        // scale having changed is guaranteed here: a port change detaches the
        // window and builds a new card, and a Settings OK that touched nothing
        // reaches this with the same scale the window already has.
        // Dazzler::m_scale is read nowhere in Dazzler.cpp - it is purely the
        // record of the choice - but
        // updateConfigFromState() reads cfg.dazzlers[0].scale back out of
        // getScale(), so leaving it stale writes the OLD scale to z80cpmw.json
        // one statement after the user changed it in the dialog.
        if (Dazzler* card = m_emulator->getDazzler()) {
            card->setScale(scale);
        }

        // WHETHER THE WINDOW ENDS UP ON SCREEN.
        //
        // Not an unconditional show(true), which is what onEmulatorSettings' OK
        // path turned into a bug the moment it began calling this on EVERY OK
        // rather than only when the Dazzler group changed: an OK for the bell or
        // a disk reopened a Dazzler window the user had closed. So it is shown
        // when this call is what ENABLES the card (View > Dazzler toggling it
        // on, the Settings checkbox being ticked, a profile arriving with it on)
        // or when there is no window yet, and is otherwise left exactly as the
        // user left it.
        //
        // Closing the window is now one of the ways the card gets DISABLED -
        // DazzlerWindow's WM_CLOSE posts WM_APP_DAZZLER_CLOSED and this window's
        // handler routes it through onViewDazzler() - so a window the user
        // closed arrives here with wasEnabled false and comes back on one click.
        const bool hadWindow = (m_dazzlerWindow != nullptr);
        const bool showIt = !hadWindow || !wasEnabled;

        // A scale change RESIZES the window; only a first enable builds one.
        // DazzlerWindow::setScale() now sizes from Dazzler::MAX_WIDTH the way
        // create() does, so the two agree, and resizing in place keeps the HWND
        // and - through SetWindowPos(SWP_NOMOVE) - keeps the window where the
        // user dragged it, which the rebuild this replaces had to reconstruct by
        // hand from GetWindowRect.
        if (!m_dazzlerWindow) {
            RECT mainRect = {};
            GetWindowRect(m_hwnd, &mainRect);
            m_dazzlerWindow = std::make_unique<DazzlerWindow>();
            m_dazzlerWindow->create(m_hwnd,
                                    mainRect.right + 10,  // next to the main window
                                    mainRect.top,
                                    scale);
        } else {
            m_dazzlerWindow->setScale(scale);   // no-op when it has not changed
        }

        // Connect to emulator's Dazzler
        if (m_dazzlerWindow && m_emulator->getDazzler()) {
            m_dazzlerWindow->setDazzler(m_emulator->getDazzler());
            // Only ever show(true), and only on the transition decided above.
            // Nothing here hides a window: the two gestures that take one off
            // the screen - View > Dazzler off and closing it - both come back
            // through the disable arm below, which does the hiding.
            if (showIt) {
                m_dazzlerWindow->show(true);
            }
        }

        // "0x" and then the port in hex, which it was not: std::to_string on a
        // uint8_t promotes to int and printed the default port 0x0E as "0x14".
        // Settings > Dazzler reads and writes base 16 behind its "Port (hex):"
        // label and DazzlerConfig::port's default is written 0x0E, so this line
        // now agrees with both.
        char portText[8];
        snprintf(portText, sizeof(portText), "0x%02X", (unsigned)port);
        m_statusText = std::string("Dazzler enabled (port ") + portText + ")";
    } else {
        // Hide and disconnect Dazzler window
        if (m_dazzlerWindow) {
            m_dazzlerWindow->show(false);
            m_dazzlerWindow->setDazzler(nullptr);
        }

        // Disable Dazzler in emulator
        m_emulator->disableDazzler();

        m_statusText = "Dazzler disabled";
    }
}

void MainWindow::onViewDazzler() {
    // Get Dazzler config (use first one or create default)
    const auto& cfg = config::ConfigManager::instance().get();
    uint8_t port = 0x0E;
    int scale = 4;
    if (!cfg.dazzlers.empty()) {
        port = cfg.dazzlers[0].port;
        scale = cfg.dazzlers[0].scale;
    }

    // The config is still the source of the port and scale here, as it always
    // was: this path only toggles, and when it is toggling ON there is no live
    // card to read them from. Every call to enableDazzler() is now
    // applyDazzlerState's, which sets m_dazzlerEnabled true in the same breath,
    // so a false m_dazzlerEnabled means no card exists.
    applyDazzlerState(!m_dazzlerEnabled, port, scale);

    // Save Dazzler state to config
    saveSettings();

    updateStatusBar();
}

void MainWindow::onHelpTopics() {
    ShowHelpWindow(m_hwnd);
}

void MainWindow::onHelpAbout() {
    // Show the real on-disk data folder (including \data, and resolving the
    // MSIX/Store redirection) so it matches where R8/W8 actually read and write
    // and what the Settings dialog shows. getUserDataDirectory() returns the
    // un-redirected base path, which does not exist for a packaged install.
    const char* realDir = emu_io_get_data_folder_display();
    std::string dataDir = realDir ? realDir : "";
    std::wstring dataDirW(dataDir.begin(), dataDir.end());

    // Convert version string to wide
    std::string verStr = VERSION_STRING;
    std::wstring verStrW(verStr.begin(), verStr.end());

    // The RomWBW releases this build can run.  Not a compile-time constant any
    // more: the core reads the version out of whichever ROM it loads, so there
    // is a list rather than a pin.  ASCII digits and dots, so the same
    // byte-wise widen the two strings above use is correct here too.
    std::string romwbwRel = emu_romwbw_supported_list();
    std::wstring romwbwRelW(romwbwRel.begin(), romwbwRel.end());

    std::wstring aboutText =
        L"z80cpmw - Z80 CP/M Emulator\n"
        L"Version " + verStrW + L"\n\n"
        L"A RomWBW/HBIOS emulator for Windows.\n"
        // The RomWBW releases the core can run. A user who hits the
        // "HBIOS/CBIOS Version Mismatch" banner is being told their disk images
        // were built by a different release than the ROM they loaded, and the
        // app displayed nothing they could compare against before this.
        L"Emulates RomWBW " + romwbwRelW + L" (from the loaded ROM).\n\n"
        L"Data Folder (disks and R8/W8 transfers):\n" + dataDirW + L"\n\n"
        L"License: GPL v3\n"
        L"CP/M OS licensed by Lineo for non-commercial use.\n\n"
        L"github.com/avwohl/z80cpmw\n"
        L"github.com/avwohl/romwbw_emu\n"
        L"github.com/avwohl/cpmemu\n"
        L"github.com/wwarthen/RomWBW";

    MessageBoxW(m_hwnd, aboutText.c_str(), L"About z80cpmw", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::updateMenuState() {
    bool running = m_emulator && m_emulator->isRunning();
    bool canStart = !running && m_emulator && m_emulator->hasROM();

    EnableMenuItem(m_menu, ID_EMU_START, canStart ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(m_menu, ID_EMU_STOP, running ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(m_menu, ID_ROM_EMU_AVW, running ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(m_menu, ID_ROM_EMU_ROMWBW, running ? MF_GRAYED : MF_ENABLED);
}

void MainWindow::updateMenuAccelHints() {
    // The .rc gives each item the hint for the default config. Whenever a key is
    // released to CP/M the hint becomes a lie, so rewrite the text to match the
    // accelerator table rebuildAccelerators() actually builds.
    if (!m_menu) return;

    const auto& kb = config::ConfigManager::instance().get().keyboard;
    struct { UINT id; const wchar_t* text; } items[] = {
        { ID_HELP_TOPICS, kb.f1ToCpm    ? L"&Help Topics" : L"&Help Topics\tF1" },
        { ID_EMU_START,   kb.f5ToCpm    ? L"&Start"       : L"&Start\tF5" },
        { ID_EMU_STOP,    kb.f5ToCpm    ? L"S&top"        : L"S&top\tShift+F5" },
        { ID_EMU_RESET,   kb.ctrlRToCpm ? L"&Reset"       : L"&Reset\tCtrl+R" },
    };
    for (const auto& item : items) {
        // SetMenuItemInfoW, not ModifyMenuW: ModifyMenu replaces the item's flag
        // word outright, and MF_ENABLED and MF_UNCHECKED are both 0, so writing
        // just the string there would quietly un-gray whatever updateMenuState()
        // had disabled. MIIM_STRING touches the text and nothing else.
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING;
        mii.dwTypeData = const_cast<LPWSTR>(item.text);
        SetMenuItemInfoW(m_menu, item.id, FALSE, &mii);   // FALSE = by command
    }
    if (m_hwnd) DrawMenuBar(m_hwnd);
}

void MainWindow::updateStatusBar() {
    if (m_statusBar) {
        std::wstring wstatus(m_statusText.begin(), m_statusText.end());
        SendMessageW(m_statusBar, SB_SETTEXTW, 0, (LPARAM)wstatus.c_str());
    }
}

void MainWindow::checkROMMenuItem(int romId) {
    CheckMenuItem(m_menu, ID_ROM_EMU_AVW, romId == ID_ROM_EMU_AVW ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(m_menu, ID_ROM_EMU_ROMWBW, romId == ID_ROM_EMU_ROMWBW ? MF_CHECKED : MF_UNCHECKED);
}

void MainWindow::checkFontMenuItem(int size) {
    CheckMenuItem(m_menu, ID_VIEW_FONT14, size == 14 ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(m_menu, ID_VIEW_FONT16, size == 16 ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(m_menu, ID_VIEW_FONT18, size == 18 ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(m_menu, ID_VIEW_FONT20, size == 20 ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(m_menu, ID_VIEW_FONT24, size == 24 ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(m_menu, ID_VIEW_FONT28, size == 28 ? MF_CHECKED : MF_UNCHECKED);
}

void MainWindow::onOutputChar(uint8_t ch) {
    // This is called from the emulator - update terminal
    if (m_terminal) {
        m_terminal->outputChar(ch);
    }
}

void MainWindow::onStatusChanged(const std::string& status) {
    m_statusText = status;
    updateStatusBar();
    updateMenuState();
}

std::string MainWindow::findResourceFile(const std::string& filename) {
    std::string appDir = EmulatorEngine::getAppDirectory();

    // Try different locations
    std::vector<std::string> paths = {
        appDir + "\\roms\\" + filename,
        appDir + "\\" + filename,
        appDir + "\\..\\roms\\" + filename,
    };

    for (const auto& path : paths) {
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return path;
        }
    }

    return "";
}

void MainWindow::terminalPrint(const std::string& text) {
    if (!m_terminal) return;
    for (const char* p = text.c_str(); *p; ++p) {
        m_terminal->outputChar(*p);
    }
}

void MainWindow::setNotice(Notice which, const std::string& text) {
    m_notices[which] = text;
    terminalPrint(text);
}

void MainWindow::clearNotice(Notice which) {
    m_notices.erase(which);
}

void MainWindow::printNotices() {
    for (const auto& kv : m_notices) {
        terminalPrint(kv.second);
    }
}

void MainWindow::reportConfigDiagnostics() {
    const auto& diags = config::ConfigManager::instance().diagnostics();

    // One notice per config::Problem kind rather than one for the whole report,
    // because the kinds do not stop being true at the same moment: saveSettings()
    // retracts one of the five outright, a second only in one sub-case, and
    // leaves the other three standing. Each notice
    // is renderBlock() called on the diagnostics of that one kind. renderBlock
    // picks its closing sentence from the kinds it is handed, so a subset of one
    // kind gets exactly the one sentence that applies to it, and nothing here
    // rewords what it says about any of them.
    //
    // The cost is that a file with problems of several kinds prints
    // renderBlock's "Configuration report:" header once per kind. That is the
    // price of being able to take one kind off the screen without taking the
    // rest with it, and each block stays self-describing when printNotices()
    // puts it back after a clear.
    // Remember, for saveSettings(), whether the file behind an UnreadableFile
    // diagnostic is still at the path the next save writes.
    //
    // Config.cpp's quarantineUnreadable() RENAMES z80cpmw.json to
    // z80cpmw.json.bad and records the new name in Diagnostic::backup, or
    // leaves backup empty when the rename could not be done at all. Only the
    // empty case leaves the user's text where ConfigManager::save() will land
    // on it, and only then does saving make renderBlock's closing "nothing has
    // been written over what you typed" false.
    //
    // The path test is the other half. A profile that could not be read is
    // quarantined by the same code and produces the same kind of diagnostic,
    // but saveSettings() writes z80cpmw.json and never touches a file under
    // profiles\, so a failed profile quarantine is not falsified by any save
    // here. Both strings come from ConfigManager (getConfigPath() for the main
    // config, getProfilePath() for a profile) and loadFromFile copies its
    // argument into Diagnostic::path verbatim, so comparing them is exact
    // rather than a guess at path normalisation.
    m_unreadableConfigStillInPlace = false;
    const std::string configPath = config::ConfigManager::instance().getConfigPath();
    for (const auto& d : diags) {
        if (d.problem == config::Problem::UnreadableFile &&
            d.backup.empty() && d.path == configPath) {
            m_unreadableConfigStillInPlace = true;
        }
    }

    static const struct { config::Problem problem; Notice notice; } kinds[] = {
        { config::Problem::UnknownMember,  Notice::ConfigUnknownMember  },
        { config::Problem::TypeMismatch,   Notice::ConfigTypeMismatch   },
        { config::Problem::ReservedKey,    Notice::ConfigReservedKey    },
        { config::Problem::UnknownKeyName, Notice::ConfigUnknownKeyName },
        { config::Problem::UnreadableFile, Notice::ConfigUnreadableFile },
    };

    for (const auto& k : kinds) {
        config::Diagnostics ofKind;
        for (const auto& d : diags) {
            if (d.problem == k.problem) ofKind.push_back(d);
        }
        if (ofKind.empty()) {
            // Every kind is visited, absent ones included, because
            // diagnostics() describes the configuration NOW in force and a kind
            // that has dropped out of it has stopped being true of it. load()
            // clears the list before it reads, and a loadProfile() that succeeds
            // REPLACES it with the profile's, so a profile that loads cleanly
            // has to take the previous file's complaints down rather than leave
            // them standing over settings nobody is using any more.
            //
            // A loadProfile() that FAILS is the one path that neither clears nor
            // replaces - it puts the in-force report back and appends this
            // attempt's UnreadableFile behind it - so nothing is retracted
            // there, which is right: those settings are still the ones running.
            clearNotice(k.notice);
            continue;
        }
        // The trailing blank line separates this block from the next notice and
        // from the boot output; renderBlock ends its last sentence, not the
        // screen.
        const std::string block = config::renderBlock(ofKind) + "\r\n";

        // Print only what CHANGED. The path this is for is onLoadProfile()'s
        // FAILURE branch, now that loadProfile() keeps the report about the file
        // still in force: every kind but UnreadableFile comes back word for word
        // as it was, so an unconditional setNotice() answered "that profile
        // would not read" by reprinting the whole startup report underneath it.
        // (The success branch reaches here too, with the profile's diagnostics
        // in place of the previous file's, and the same rule is right there for
        // the same reason.) A notice already
        // holding this exact text has been printed once and printNotices() puts
        // it back after every clear(), so re-emitting it adds nothing the
        // terminal does not already say. Every kind is still visited, because a
        // block that GREW - the profile's UnreadableFile joining the config's -
        // is not equal and is printed. Done here rather than in setNotice(),
        // whose "raise and print" contract the ROM notices rely on.
        auto held = m_notices.find(k.notice);
        if (held != m_notices.end() && held->second == block) continue;
        setNotice(k.notice, block);
    }
}

std::string MainWindow::loadedRomwbwRelease() const {
    // Asked of the ROM in the banks, not of a constant. There is no compile-time
    // RomWBW pin left to read: romwbw_emu v1.39 made the version runtime state
    // derived from whichever image was loaded, which is what lets one binary
    // boot more than one release. So the honest answer to "which RomWBW is this
    // machine" is a read of bank 0, and it changes when the user loads another
    // ROM.
    if (!m_emulator || !m_emulator->hasROM()) return std::string();

    emu_romwbw_release release;
    if (!emu_romwbw_release_loaded(m_emulator->getMemory(), &release)) return std::string();

    char buffer[EMU_ROMWBW_STR_MAX] = {};
    emu_romwbw_release_str(release, buffer, sizeof(buffer));
    return std::string(buffer);
}

void MainWindow::loadDefaultROM() {
    std::string romPath = findResourceFile("emu_avw.rom");

    if (!romPath.empty()) {
        if (m_emulator->loadROM(romPath)) {
            m_emulator->setROMName("emu_avw.rom");
            m_currentRomId = ID_ROM_EMU_AVW;
            checkROMMenuItem(m_currentRomId);
            // One of the four ROM-notice retraction sites; see setNotice's
            // comment in MainWindow.h. This is the only one of the four that
            // cannot currently have a notice to retract - onCreate() calls this
            // before anything raises one - and it is written the same way as
            // the other three so the rule is "a ROM loaded, the ROM notices are
            // over" everywhere, with no exception to remember.
            clearNotice(Notice::DefaultRom);
            clearNotice(Notice::SavedRom);
        } else {
            // The default ROM existing but being unusable is the case that
            // used to end in a silent dead emulator at startup: nothing was
            // loaded and nothing said so.
            setNotice(Notice::DefaultRom,
                      "ERROR: cannot use the default ROM (emu_avw.rom)\r\n" +
                      m_emulator->getROMError() + "\r\n"
                      "Use Emulator > ROM to choose another ROM file.\r\n\r\n");
        }
    } else {
        setNotice(Notice::DefaultRom,
                  "WARNING: ROM file not found (emu_avw.rom)\r\n"
                  "Please use Emulator > ROM to load a ROM file,\r\n"
                  "or place ROM files in the 'roms' subdirectory.\r\n\r\n");
    }
}

void MainWindow::showStartupInstructions() {
    if (!m_terminal) return;

    // Build version string with compile date/time
    char versionLine[128];
    snprintf(versionLine, sizeof(versionLine),
        "  Version %s (%s %s)\r\n", VERSION_STRING, __DATE__, __TIME__);

    // Keep this short so it fits within the 25-row terminal (which has no
    // scrollback). The full, scrollable instructions live in Help (F1) -> the
    // "Getting Started" and "Configuration File" topics, which also open
    // automatically on first run.
    const char* instructions =
        "\r\n"
        "  z80cpmw - Z80 CP/M Emulator for Windows\r\n"
        "  ========================================\r\n"
        "\r\n"
        "  Press F5 (or Emulator -> Start) to boot. At the boot menu,\r\n"
        "  type 0 and Enter to start CP/M from Disk 0.\r\n"
        "\r\n"
        "  Press F1 (or Help -> Help Topics) for Getting Started and full\r\n"
        "  instructions: disk setup, file transfer, keys, and copy/paste.\r\n"
        "\r\n"
        "  File transfer: R8 <file> imports, W8 <file> exports.\r\n";

    for (const char* p = instructions; *p; ++p) {
        m_terminal->outputChar(*p);
    }

    // Show the real data folder path (resolved through any MSIX/Store
    // redirection) so bare-name R8/W8 transfers can actually be found on disk.
    const char* dataFolder = emu_io_get_data_folder_display();
    if (dataFolder && *dataFolder) {
        std::string line = std::string("  Data folder: ") + dataFolder + "\r\n";
        for (char c : line) m_terminal->outputChar((uint8_t)c);
    }

    const char* tail =
        "  (Emulator -> Settings -> Open Folder opens it in Explorer.)\r\n"
        "\r\n";
    for (const char* p = tail; *p; ++p) {
        m_terminal->outputChar(*p);
    }

    // Output build version
    for (const char* p = versionLine; *p; ++p) {
        m_terminal->outputChar(*p);
    }
}

void MainWindow::loadSettings() {
    // Set up data directory for disks and file transfers
    std::string userDir = EmulatorEngine::getUserDataDirectory();
    std::string dataDir = userDir + "\\data";
    CreateDirectoryA(dataDir.c_str(), nullptr);
    m_diskCatalog->setDownloadDirectory(dataDir);

    // Load configuration (handles migration from old INI format automatically)
    auto& cfgMgr = config::ConfigManager::instance();
    cfgMgr.load();

    // Rename the pre-v0 catalog images and everything that points at them, with
    // the same dataDir setDownloadDirectory() was just given rather than one
    // worked out again here.
    migrateStorageToInterfaceV0(dataDir);

    // Say what the file contained that nothing read, before applying what it
    // did. load() collected the diagnostics and until now nothing displayed
    // them: a mistyped setting was detected, described, and then thrown away
    // inside the ConfigManager.
    reportConfigDiagnostics();

    // Apply the loaded configuration
    applyConfig();
}

// The interface-v0 storage migration: run once, over the images, the ledger,
// the four slots and every profile, at the one moment all of them can be moved
// together. DiskMigrationV0.h holds the reasoning about the names themselves.
//
// ORDERING, and both ends of it are load-bearing. AFTER ConfigManager::load(),
// because load() may still be migrating a z80cpmw.ini whose disk0..disk3 lines
// become DiskConfig::path and need this pass too. BEFORE applyConfig(), for two
// reasons: applyConfig mounts the four slots, so a slot rewritten after it is
// not the disk on the machine, and it also OPENS those files - a rename of a
// file this process has open fails, and it would fail on exactly the images the
// user cares most about. It is also before the first fetchCatalog can happen -
// the Settings dialog's constructor starts that - which matters because
// isDiskDownloaded(), the dropdowns and the freshness column all ask about v0
// names from then on.
//
// 'dataDir' comes from the caller and is not worked out here. This application
// already builds %LOCALAPPDATA%\z80cpmw\data in three independent places -
// DiskCatalog's constructor, EmulatorEngine::getUserDataDirectory and
// getDataFolder() in emu_io_windows.cpp - and a fourth would let this rename
// files in a folder nothing else reads. It is also why the migration is here
// and not in the installer: under MSIX the OS redirects that path per package,
// so an installer step would find nothing to rename for a Store user.
void MainWindow::migrateStorageToInterfaceV0(const std::string& dataDir) {
    auto& cfgMgr = config::ConfigManager::instance();
    if (cfgMgr.get().interfaceV0Migrated) return;

    // The files first: what the config and the profiles may be rewritten
    // against is what actually landed, never what could be mapped.
    DiskCatalog::V0FileMigration files = m_diskCatalog->migrateFilesToInterfaceV0();
    config::V0MigrationReport stored =
        cfgMgr.migrateToInterfaceV0(dataDir, files.landed, files.failures.empty());

    if (files.failures.empty() && stored.profileFailures.empty()) return;

    // Only the failures are said, and they are said as a notice rather than a
    // line of terminal output, because the screen is cleared on the next Start
    // and this is exactly the sentence that explains why a disk the user
    // configured is not in the list any more.
    std::string text = "\r\nSome files could not be renamed for the new disk "
                       "catalog. The app will try again next time it starts.\r\n";
    for (const auto& f : files.failures) {
        text += "  " + f + "\r\n";
    }
    for (const auto& f : stored.profileFailures) {
        text += "  profile " + f + "\r\n";
    }
    setNotice(Notice::StorageMigration, text);
}

void MainWindow::saveSettings() {
    updateConfigFromState();
    if (!config::ConfigManager::instance().save()) {
        // Nothing reached the disk: saveToFile writes a .tmp and renames, so a
        // false return leaves z80cpmw.json holding exactly what the user typed
        // and every notice about it still true. Retracting them here would be
        // the one lie the report cannot afford.
        return;
    }

    // ONE of the five configuration notices stops being true at this save, a
    // second does only in a sub-case, and three do not. What decides it is what
    // this save can reach:
    //
    //   UnknownMember  - renderBlock says "saving settings will drop them", and
    //                    this is that save: to_json writes only the names it
    //                    knows, and nothing carries this kind past it either -
    //                    inspectDocument fills AppConfig::unreadSections from
    //                    its TypeMismatch findings alone. The member is now gone
    //                    from the file and the sentence has become a prediction
    //                    about the past.
    //   TypeMismatch   - KEPT, and this is the retraction that had to go. Its
    //                    justification was "to_json writes our defaults over"
    //                    the skipped section, which is the opposite of what
    //                    to_json now does: inspectDocument keeps the section's
    //                    own text in AppConfig::unreadSections and to_json
    //                    SPLICES IT BACK at the JSON pointer it came from, so a
    //                    save writes that section back rather than over it.
    //                    renderBlock says two things about the kind and only the
    //                    first is about this save - "Saving settings writes such
    //                    a section back rather than over it, so what you typed is
    //                    safe; until it is corrected, nothing the application
    //                    changes in that section is saved either." The second
    //                    half is a standing condition that no save ends and every
    //                    save re-enters - it is the price of the carry, and it
    //                    is the half the user has to be told, because it is why
    //                    a setting they change in that section will not stick.
    //                    Measured with "keyboard": { "keys": ["Up", "\E[A"] } on
    //                    disk and an OK in Settings: the file still held the
    //                    array exactly as typed afterwards, and with the
    //                    retraction gone the block was still on screen after the
    //                    clear Emulator > Reset does. It is not a warning the
    //                    user can dismiss by saving, and the save that used to
    //                    dismiss it need not even be theirs: onTimer()'s
    //                    hasNvramChange() branch calls saveSettings() on its own.
    //                    What ends this notice is the user correcting the
    //                    section and the next load finding it readable, which
    //                    reportConfigDiagnostics() picks up by clearing every
    //                    kind the fresh diagnostics do not name.
    //   UnreadableFile - conditional, and this is the one that used to be
    //                    wrong. Config.cpp draws the OPPOSITE conclusion here
    //                    from the one it draws for TypeMismatch: suppressing
    //                    load()'s own save "for an UnreadableFile is enough,
    //                    because the file has been renamed out from under those
    //                    saves". quarantineUnreadable() moved z80cpmw.json to
    //                    z80cpmw.json.bad, so the save below cannot fail and
    //                    cannot destroy anything - it writes a path that is now
    //                    empty. The notice stays TRUE, and it is the only place
    //                    the UI ever shows the backup's name and the parser's
    //                    line and column. Retracting it unconditionally dropped
    //                    both on the first F5: a file that will not parse leaves
    //                    cfg.disks empty, onEmulatorStart() finds no disk
    //                    loaded and calls downloadAndStartWithDefaults(), whose
    //                    both-disks-present branch runs saveSettings() one
    //                    statement before the startEmulator() whose
    //                    printNotices() exists to survive its own clear().
    //                    The retraction is right only in the sub-case where the
    //                    quarantine FAILED (all .bad names taken, or the rename
    //                    refused): the original is then still at z80cpmw.json,
    //                    saveToFile's rename really does replace it, and the
    //                    closing sentence becomes false. That way round, not the
    //                    other: the flag says the file is still IN PLACE, which
    //                    is exactly when the save can overwrite it.
    //   ReservedKey,
    //   UnknownKeyName - kept, because the save round-trips them. from_json
    //                    reads "keyboard.keys" whole into the map (names it
    //                    cannot resolve included) and to_json writes the map
    //                    back whole; nothing in the loader ever prunes it. "The
    //                    line is still in the file to be corrected" is as true
    //                    after this save as before it.
    //
    // The ROM notices are on neither list. They describe what is in the ROM
    // banks, which no save touches - and updateConfigFromState() above writes
    // cfg.rom only for the two known ids, so on the machine where the notices
    // matter most (no ROM loaded at all, m_currentRomId still 0) the save does
    // not even rewrite the ROM name it is complaining about. They are retracted
    // where a ROM is successfully loaded instead; see loadDefaultROM().
    clearNotice(Notice::ConfigUnknownMember);
    if (m_unreadableConfigStillInPlace) {
        clearNotice(Notice::ConfigUnreadableFile);
    }
}

void MainWindow::applyConfig() {
    const auto& cfg = config::ConfigManager::instance().get();

    // Which RomWBW release the disk catalog is fetched for. Here rather than in
    // loadSettings() because this function is what turns a configuration into
    // the running state, and it has the second caller that matters: loading a
    // PROFILE replaces the whole configuration, this member included, and a
    // profile that names 3.6.0 has to move the catalog with it.
    //
    // It only sets a preference and starts nothing. The first fetch comes later
    // - from the Settings dialog's constructor, or from F5 with nothing mounted
    // - and DiskCatalog falls back to the index's default if this release is one
    // it cannot boot. Nothing is deleted, unmounted or invalidated by the change.
    m_diskCatalog->setPreferredRomwbwVersion(cfg.romwbwVersion);

    // Apply ROM selection. A config naming SBC_simh_std.rom comes from a build
    // that offered it: it is a stock ROM for real hardware, it has no port 0xEF
    // HBIOS proxy, and loading it produced a machine that ran and printed
    // nothing. Keep the default ROM loadDefaultROM() already put in place.
    if (cfg.rom == "SBC_simh_std.rom") {
        emu_error("[CONFIG] Ignoring SBC_simh_std.rom: a stock hardware ROM "
                  "this emulator cannot run. Keeping the default ROM.\n");
        // This is the notice the whole Notice machinery exists for: the machine
        // has a good ROM (loadDefaultROM put it there), so hasROM() is true and
        // startEmulator() clears the screen and boots - and without a notice
        // that survives the clear, nothing on screen says the ROM running is
        // not the one the configuration asked for.
        setNotice(Notice::SavedRom,
                  "NOTE: the saved ROM (SBC_simh_std.rom) is a stock ROM for real\r\n"
                  "hardware and cannot run here. Using the default ROM instead.\r\n\r\n");
    } else if (!cfg.rom.empty()) {
        std::string romPath = findResourceFile(cfg.rom);
        if (romPath.empty()) {
            emu_error("[CONFIG] ROM from config not found: %s\n", cfg.rom.c_str());
            setNotice(Notice::SavedRom,
                      "ERROR: the saved ROM (" + cfg.rom + ") was not found.\r\n"
                      "Use Emulator > ROM to choose one.\r\n\r\n");
        } else if (!m_emulator->loadROM(romPath)) {
            // A failed load also discards whatever loadDefaultROM() had put in
            // the banks, so this leaves the machine with no ROM at all. Say so
            // where the user will see it; the log alone was the only report.
            emu_error("[CONFIG] Cannot use ROM %s: %s\n", cfg.rom.c_str(),
                      m_emulator->getROMError().c_str());
            setNotice(Notice::SavedRom,
                      "ERROR: cannot use the saved ROM (" + cfg.rom + ")\r\n" +
                      m_emulator->getROMError() + "\r\n"
                      "Use Emulator > ROM to choose another one.\r\n\r\n");
        } else {
            m_emulator->setROMName(cfg.rom);
            // Update menu checkmark based on ROM name
            if (cfg.rom == "emu_avw.rom") {
                m_currentRomId = ID_ROM_EMU_AVW;
            } else if (cfg.rom == "emu_romwbw.rom") {
                m_currentRomId = ID_ROM_EMU_ROMWBW;
            }
            checkROMMenuItem(m_currentRomId);
            // The configured ROM is now the one running, which retires both ROM
            // notices: this call replaced whatever loadDefaultROM() had loaded,
            // so its warning about the default is no longer about the machine
            // in front of the user either.
            clearNotice(Notice::DefaultRom);
            clearNotice(Notice::SavedRom);
        }
    }
    // An empty cfg.rom takes neither branch, deliberately: there is no saved ROM
    // to disagree with, and loadDefaultROM()'s notice - which is exactly the one
    // that matters when the default could not be loaded - has to survive
    // loadSettings() to reach the screen after the first clear.

    // Apply debug mode
    m_emulator->setDebug(cfg.debug);

    // Apply boot string
    if (!cfg.bootString.empty()) {
        m_emulator->setBootString(cfg.bootString);
    }

    // Apply font size
    if (cfg.fontSize > 0 && m_terminal) {
        m_terminal->setFontSize(cfg.fontSize);
        checkFontMenuItem(cfg.fontSize);
    }

    // Apply scrollback buffer size
    if (m_terminal) {
        m_terminal->setScrollbackLines(cfg.scrollbackLines);
    }

    // Apply the bell. This is the startup path and the profile-load path, and
    // the only other caller of setBellEnabled() is onEmulatorSettings - so
    // without this line a saved "bell": false would be ignored until the user
    // happened to open Settings and press OK, TerminalView having constructed
    // with the bell on.
    if (m_terminal) {
        m_terminal->setBellEnabled(cfg.bellEnabled);
    }

    // Apply keyboard bindings (function/navigation keys -> CP/M sequences)
    if (m_terminal) {
        m_terminal->setKeyBindings(cfg.keyboard.keys);
    }

    // The shortcut keys are part of the same config, so rebuild the accelerator
    // table and re-label the menu here too. Loading a profile that releases a
    // key would otherwise keep the old shortcut live until the next restart.
    rebuildAccelerators();
    updateMenuAccelHints();

    // Load disks
    std::string downloadDataDir = EmulatorEngine::getUserDataDirectory() + "\\data";
    for (int i = 0; i < 4; i++) {
        if (cfg.disks[i].has_value()) {
            const auto& disk = cfg.disks[i].value();
            if (disk.path.empty()) {
                continue;
            }
            // Config-remembered files in the download cache get the same
            // completeness check as downloadAndStartWithDefaults: skipping a
            // truncated cache here means F5 falls into the download path and
            // fetches a good copy instead of booting garbage forever.
            bool inDownloadCache =
                _strnicmp(disk.path.c_str(), downloadDataDir.c_str(),
                          downloadDataDir.size()) == 0;
            if (inDownloadCache && !diskFileLooksComplete(disk.path)) {
                continue;
            }
            if (GetFileAttributesA(disk.path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                m_emulator->loadDisk(i, disk.path);
                m_emulator->setDiskIsManifest(i, disk.isManifest);
            }
        } else if (m_emulator->isDiskLoaded(i)) {
            // The config omits this unit (e.g. a loaded profile without it):
            // close it so the previously mounted image stops collecting
            // guest writes under the new profile. No-op at startup.
            m_emulator->closeDisk(i);
        }
    }

    // Apply manifest write warning suppression setting
    if (!cfg.warnManifestWrites) {
        for (int i = 0; i < 4; i++) {
            m_emulator->setDiskWarningSuppressed(i, true);
        }
    }

    // Apply Dazzler settings, through applyDazzlerState() rather than a second
    // copy of the create-and-show code. The copy that stood here got none of
    // that function's fixes: it left an existing card at its old port
    // (enableDazzler() returns at once when one exists), it left an existing
    // window at its old scale, and it show(true)'d unconditionally - so loading
    // a profile reopened a Dazzler window the user had closed.
    //
    // 0x0E and 4 where the array is empty, the same pair onViewDazzler() falls
    // back to, so an empty array and an inert entry (enabled false, the
    // defaults) still cannot be told apart here - which is what lets
    // onEmulatorSettings write that entry unconditionally.
    bool dazzlerOn = false;
    uint8_t dazzlerPort = 0x0E;
    int dazzlerScale = 4;
    if (!cfg.dazzlers.empty()) {
        dazzlerOn = cfg.dazzlers[0].enabled;
        dazzlerPort = cfg.dazzlers[0].port;
        dazzlerScale = cfg.dazzlers[0].scale;
    }

    // "or there is a card to take down" is what makes this safe at STARTUP,
    // which is the other thing this function is for. m_dazzlerEnabled starts
    // false and getDazzler() starts null, so a config with no Dazzler leaves
    // the disable arm - which writes the status text and unticks the View menu
    // - unreached, exactly as the old code left it. On a profile LOAD the same
    // clause is a fix: a profile with the Dazzler off used to leave a running
    // card running, its window on screen and its menu item ticked, and the next
    // save then wrote that live state back over the profile.
    if (dazzlerOn || m_dazzlerEnabled) {
        // applyDazzlerState() writes the status bar, which is right when the
        // user just asked for the Dazzler and wrong from here. At startup
        // onCreate()'s updateStatusBar() would publish "Dazzler enabled (port
        // 0x0E)" where "Ready" belongs; on a profile load onLoadProfile()
        // overwrites it with "Loaded profile: ..." in the next statement
        // anyway. So the line this function found is the line it leaves.
        const std::string statusBefore = m_statusText;
        applyDazzlerState(dazzlerOn, dazzlerPort, dazzlerScale);
        m_statusText = statusBefore;
    }
}

void MainWindow::updateConfigFromState() {
    auto& cfg = config::ConfigManager::instance().get();

    // Capture current ROM
    // (ROM name is stored in emulator, but we track via m_currentRomId)
    switch (m_currentRomId) {
    case ID_ROM_EMU_AVW:
        cfg.rom = "emu_avw.rom";
        break;
    case ID_ROM_EMU_ROMWBW:
        cfg.rom = "emu_romwbw.rom";
        break;
    }

    // Debug mode is deliberately NOT captured here: EmulatorEngine keeps
    // m_debug private and declares no getter, so there is nothing to read it
    // back from. cfg.debug is written where it CHANGES instead - the only two
    // callers of setDebug() are applyConfig(), which reads cfg.debug rather
    // than writing it, and onEmulatorSettings' OK path, which now writes both -
    // so the value this function's save carries is already the current one.

    // Note: bootString is saved automatically when NVRAM changes (in onTimer)

    // Capture font size
    if (m_terminal) {
        cfg.fontSize = m_terminal->getFontSize();
    }

    // Capture disk paths (already updated when disks are loaded)

    // Capture Dazzler state
    if (m_dazzlerEnabled) {
        if (cfg.dazzlers.empty()) {
            cfg.dazzlers.push_back(config::DazzlerConfig{});
        }
        cfg.dazzlers[0].enabled = true;
        if (m_emulator->getDazzler()) {
            cfg.dazzlers[0].port = m_emulator->getDazzler()->getBasePort();
            cfg.dazzlers[0].scale = m_emulator->getDazzler()->getScale();
        }
    } else if (!cfg.dazzlers.empty()) {
        cfg.dazzlers[0].enabled = false;
    }
}

void MainWindow::onLoadProfile() {
    auto profiles = config::ConfigManager::instance().listProfiles();
    if (profiles.empty()) {
        MessageBoxW(m_hwnd, L"No saved profiles found.", L"Load Profile", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Simple file dialog to select profile
    wchar_t filename[MAX_PATH] = {};
    // Keep the returned-by-value string alive in a named local: building the
    // wstring from two separate getProfilesDir() calls mixed iterators of two
    // different temporaries (undefined behavior, intermittent crash).
    const std::string profilesDirA = config::ConfigManager::instance().getProfilesDir();
    std::wstring profilesDir(profilesDirA.begin(), profilesDirA.end());

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Profile Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = profilesDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"Load Profile";

    if (GetOpenFileNameW(&ofn)) {
        // Extract profile name from path
        wchar_t* lastSlash = wcsrchr(filename, L'\\');
        std::wstring nameW = lastSlash ? lastSlash + 1 : filename;
        // Remove .json extension
        size_t dotPos = nameW.rfind(L'.');
        if (dotPos != std::wstring::npos) {
            nameW = nameW.substr(0, dotPos);
        }

        char nameA[MAX_PATH];
        if (!WideCharToMultiByte(CP_UTF8, 0, nameW.c_str(), -1, nameA, MAX_PATH, nullptr, nullptr)) {
            MessageBoxW(m_hwnd, L"Failed to load profile.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        if (config::ConfigManager::instance().loadProfile(nameA)) {
            // Stop before swapping ROM/disks (which also flushes dirty disk
            // writes), then resume — same pattern as onEmulatorSettings.
            // Without this, applyConfig's loadDisk calls would discard guest
            // writes made since the last flush and hot-swap the ROM under a
            // running machine.
            bool wasRunning = m_emulator && m_emulator->isRunning();
            if (wasRunning) {
                m_emulator->stop();
            }
            // After the stop, so the report does not land in the middle of the
            // guest's own output, and before applyConfig() so the file is
            // described before it is applied - the same order loadSettings()
            // uses. A loadProfile() that succeeds REPLACES the diagnostics with
            // the profile's own (it holds the previous ones only long enough to
            // decide, and drops them here), so a profile that loads cleanly
            // takes the previous file's notices down with it in this call.
            reportConfigDiagnostics();
            applyConfig();
            if (wasRunning) {
                m_emulator->start();
            }
            m_statusText = "Loaded profile: " + std::string(nameA);
            updateStatusBar();
        } else {
            // A profile that could not be READ is the case that needs the
            // report most, and it was the one case that did not get it.
            // ConfigManager::loadFromFile applies the same quarantine to a
            // profile as to z80cpmw.json, so by the time we are here the file
            // has been RENAMED and has dropped out of the Load Profile list,
            // and the diagnostic holding its new name and the parser's line and
            // column was rendered nowhere. "Failed to load profile." was the
            // whole of what the user was told.
            //
            // Only when the diagnostics are about THIS profile. loadProfile()
            // returns false for two different things, and both now leave the
            // report about the configuration actually in force standing: a file
            // that would not read (its UnreadableFile is APPENDED behind that
            // report, carrying the parser's line and column and the name the
            // file was quarantined to), and a name with no file at all, which
            // returns before loadFromFile is reached and adds nothing. Only the
            // first has anything new to say. Diagnostic::path carries a FILE
            // path only for UnreadableFile; every other kind carries a member
            // path like "display.fontsize", so matching it against
            // getProfilePath() separates the two exactly.
            //
            // The limit this used to record - loadProfile() clearing
            // m_diagnostics before it failed, so a notice about z80cpmw.json
            // went down with a profile load that changed no setting - is fixed
            // in ConfigManager, which is where it was. What is left of it here
            // is that reportConfigDiagnostics() now sees the in-force report as
            // well as the profile's; it prints only the kinds whose text
            // changed, which on this path is the UnreadableFile block and
            // nothing else.
            //
            // No stop() first, unlike the success path above: nothing is being
            // applied, so there is no reason to interrupt a running machine.
            // The cost is that the block can land in the middle of the guest's
            // own output.
            auto& cfgMgr = config::ConfigManager::instance();
            const std::string profilePath = cfgMgr.getProfilePath(nameA);
            bool described = false;
            for (const auto& d : cfgMgr.diagnostics()) {
                if (d.path == profilePath) { described = true; break; }
            }
            if (described) {
                reportConfigDiagnostics();
            }
            // Printed before the box, so the report is already on the terminal
            // behind it and is still there when it is dismissed.
            MessageBoxW(m_hwnd,
                described
                    ? L"The profile could not be read.\n\n"
                      L"The configuration report in the terminal window gives the "
                      L"reason - including the line and column for a syntax error - "
                      L"and names the file the profile was renamed to if it could be "
                      L"moved aside.\n\n"
                      L"Your current settings are unchanged."
                    : L"Failed to load profile.",
                L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void MainWindow::onSaveProfileAs() {
    wchar_t filename[MAX_PATH] = {};
    // Named local for the same iterator-pair reason as onLoadProfile().
    const std::string profilesDirA = config::ConfigManager::instance().getProfilesDir();
    std::wstring profilesDir(profilesDirA.begin(), profilesDirA.end());

    // Ensure profiles directory exists
    CreateDirectoryW(profilesDir.c_str(), nullptr);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Profile Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = profilesDir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = L"Save Profile As";
    ofn.lpstrDefExt = L"json";

    if (GetSaveFileNameW(&ofn)) {
        // Extract profile name from path
        wchar_t* lastSlash = wcsrchr(filename, L'\\');
        std::wstring nameW = lastSlash ? lastSlash + 1 : filename;
        // Remove .json extension
        size_t dotPos = nameW.rfind(L'.');
        if (dotPos != std::wstring::npos) {
            nameW = nameW.substr(0, dotPos);
        }

        char nameA[MAX_PATH];
        if (!WideCharToMultiByte(CP_UTF8, 0, nameW.c_str(), -1, nameA, MAX_PATH, nullptr, nullptr)) {
            MessageBoxW(m_hwnd, L"Failed to save profile.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        updateConfigFromState();
        if (config::ConfigManager::instance().saveAsProfile(nameA)) {
            m_statusText = "Saved profile: " + std::string(nameA);
            updateStatusBar();
        } else {
            MessageBoxW(m_hwnd, L"Failed to save profile.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}
