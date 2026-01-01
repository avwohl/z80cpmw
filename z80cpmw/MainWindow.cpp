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
#include "resource.h"

// External function to set main window for host file dialogs
extern "C" void emu_io_set_main_window(HWND hwnd);

static const wchar_t* WINDOW_CLASS = L"Z80CPM_MainWindow";
static const wchar_t* WINDOW_TITLE = L"z80cpmw - Z80 CP/M Emulator";
static bool g_mainClassRegistered = false;

MainWindow::MainWindow()
    : m_terminal(std::make_unique<TerminalView>())
    , m_emulator(std::make_unique<EmulatorEngine>())
    , m_diskCatalog(std::make_unique<DiskCatalog>())
{
}

MainWindow::~MainWindow() {
    if (m_emulatorTimer) {
        KillTimer(m_hwnd, m_emulatorTimer);
    }
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
    ShowWindow(m_hwnd, cmdShow);
    UpdateWindow(m_hwnd);
}

int MainWindow::run() {
    // Load keyboard accelerators (F5, Shift+F5, Ctrl+R)
    HACCEL hAccel = LoadAccelerators(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_ACCELERATORS));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(m_hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
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
        if (m_emulator && m_emulator->isRunning()) {
            m_emulator->stop();
        }
        DestroyWindow(m_hwnd);
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

    m_terminal->setFontSize(m_currentFontSize);

    // Set up terminal input callback
    m_terminal->setKeyInputCallback([this](char ch) {
        if (m_emulator && m_emulator->isRunning()) {
            m_emulator->sendChar(ch);
        }
    });

    // Set up emulator callbacks
    m_emulator->setOutputCallback([this](uint8_t ch) {
        onOutputChar(ch);
    });

    m_emulator->setStatusCallback([this](const std::string& status) {
        onStatusChanged(status);
    });

    // Load ROM and saved settings (disks, boot string, font size)
    loadDefaultROM();
    loadSettings();  // Loads saved disk paths and other settings

    // Set menu items
    m_menu = GetMenu(m_hwnd);
    checkROMMenuItem(ID_ROM_EMU_AVW);
    checkFontMenuItem(m_currentFontSize);

    // Start emulator timer
    m_emulatorTimer = SetTimer(m_hwnd, IDT_EMULATOR, TIMER_INTERVAL_MS, nullptr);

    // Update status
    updateStatusBar();

    // Show startup instructions in terminal
    showStartupInstructions();
}

void MainWindow::onDestroy() {
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

    // Resize terminal - ensure positive dimensions (fallback for WM_SIZE with 0,0)
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
    case ID_FILE_EXIT:
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
        break;

    case ID_ROM_EMU_AVW:
    case ID_ROM_EMU_ROMWBW:
    case ID_ROM_SBC_SIMH:
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
    if (m_emulator && m_emulator->isRunning()) {
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
        }
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
            m_diskPaths[unit] = path;  // Save for persistence
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
    std::string appDir = EmulatorEngine::getAppDirectory();

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
    case ID_ROM_SBC_SIMH:
        romFile = "SBC_simh_std.rom";
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
        m_statusText = "Loaded ROM: " + romFile;
        updateStatusBar();
    } else {
        MessageBoxW(m_hwnd, L"Failed to load ROM", L"Error", MB_OK | MB_ICONERROR);
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

void MainWindow::startEmulator() {
    if (m_terminal) {
        m_terminal->clear();
    }

    // Apply auto slice count based on loaded disk count (1: 8, 2: 4, 3+: 2)
    int diskCount = 0;
    for (int i = 0; i < 4; i++) {
        if (m_emulator->isDiskLoaded(i)) diskCount++;
    }
    int autoSlices = (diskCount <= 1) ? 8 : (diskCount == 2) ? 4 : 2;
    for (int i = 0; i < 4; i++) {
        if (m_emulator->isDiskLoaded(i)) {
            m_emulator->setDiskSliceCount(i, autoSlices);
        }
    }

    m_emulator->start();
    updateMenuState();

    // Focus terminal
    if (m_terminal && m_terminal->getHwnd()) {
        SetFocus(m_terminal->getHwnd());
    }
}

void MainWindow::downloadAndStartWithDefaults() {
    std::string userDir = EmulatorEngine::getUserDataDirectory();
    std::string dataDir = userDir + "\\data";
    std::string combo = dataDir + "\\hd1k_combo.img";
    std::string games = dataDir + "\\hd1k_games.img";

    // Helper to output string to terminal
    auto termOutput = [this](const char* msg) {
        if (m_terminal) {
            for (const char* p = msg; *p; ++p) {
                m_terminal->outputChar(*p);
            }
        }
    };

    // Check if combo disk already exists
    bool comboExists = (GetFileAttributesA(combo.c_str()) != INVALID_FILE_ATTRIBUTES);
    bool gamesExists = (GetFileAttributesA(games.c_str()) != INVALID_FILE_ATTRIBUTES);

    if (comboExists && gamesExists) {
        // Both exist, just load them
        m_diskPaths[0] = combo;
        m_emulator->loadDisk(0, combo);
        m_diskPaths[1] = games;
        m_emulator->loadDisk(1, games);
        termOutput("Loaded default disks.\r\n");
        saveSettings();
        startEmulator();
        return;
    }

    // Need to download at least one disk
    termOutput("\r\nDownloading default disk images...\r\n");

    bool needComboDownload = !comboExists;
    bool needGamesDownload = !gamesExists;

    // Load existing disk if present
    if (comboExists) {
        m_diskPaths[0] = combo;
        m_emulator->loadDisk(0, combo);
        termOutput("Disk 0: hd1k_combo.img loaded\r\n");
    }
    if (gamesExists) {
        m_diskPaths[1] = games;
        m_emulator->loadDisk(1, games);
        termOutput("Disk 1: hd1k_games.img loaded\r\n");
    }

    // Download missing disks then start
    if (needComboDownload) {
        m_diskCatalog->downloadDisk("hd1k_combo.img",
            nullptr,
            [this, combo, games, needGamesDownload](bool success, const std::string& error) {
                auto termOut = [this](const char* msg) {
                    if (m_terminal) {
                        for (const char* p = msg; *p; ++p) {
                            m_terminal->outputChar(*p);
                        }
                    }
                };

                if (success) {
                    m_diskPaths[0] = combo;
                    m_emulator->loadDisk(0, combo);
                    termOut("  Disk 0: hd1k_combo.img downloaded and loaded\r\n");
                } else {
                    termOut("  Disk 0: download failed - ");
                    termOut(error.c_str());
                    termOut("\r\n");
                }

                if (needGamesDownload) {
                    m_diskCatalog->downloadDisk("hd1k_games.img",
                        nullptr,
                        [this, games](bool success2, const std::string& error2) {
                            auto termOut2 = [this](const char* msg) {
                                if (m_terminal) {
                                    for (const char* p = msg; *p; ++p) {
                                        m_terminal->outputChar(*p);
                                    }
                                }
                            };

                            if (success2) {
                                m_diskPaths[1] = games;
                                m_emulator->loadDisk(1, games);
                                termOut2("  Disk 1: hd1k_games.img downloaded and loaded\r\n");
                            } else {
                                termOut2("  Disk 1: download failed - ");
                                termOut2(error2.c_str());
                                termOut2("\r\n");
                            }
                            saveSettings();
                            startEmulator();
                        });
                } else {
                    saveSettings();
                    startEmulator();
                }
            });
    } else if (needGamesDownload) {
        m_diskCatalog->downloadDisk("hd1k_games.img",
            nullptr,
            [this, games](bool success, const std::string& error) {
                auto termOut = [this](const char* msg) {
                    if (m_terminal) {
                        for (const char* p = msg; *p; ++p) {
                            m_terminal->outputChar(*p);
                        }
                    }
                };

                if (success) {
                    m_diskPaths[1] = games;
                    m_emulator->loadDisk(1, games);
                    termOut("  Disk 1: hd1k_games.img downloaded and loaded\r\n");
                } else {
                    termOut("  Disk 1: download failed - ");
                    termOut(error.c_str());
                    termOut("\r\n");
                }
                saveSettings();
                startEmulator();
            });
    }
}

void MainWindow::onEmulatorStop() {
    m_emulator->stop();
    updateMenuState();
}

void MainWindow::onEmulatorReset() {
    if (m_terminal) {
        m_terminal->clear();
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
    settings.bootString = m_emulator->getBootString();
    settings.debugMode = false;  // TODO: get from emulator

    // Pass currently loaded disk filenames to settings dialog
    for (int i = 0; i < 4; i++) {
        if (!m_diskPaths[i].empty()) {
            // Extract filename from full path
            size_t lastSlash = m_diskPaths[i].find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                settings.diskFiles[i] = m_diskPaths[i].substr(lastSlash + 1);
            } else {
                settings.diskFiles[i] = m_diskPaths[i];
            }
        }
    }

    if (ShowWxSettingsDialog(m_hwnd, m_diskCatalog.get(), settings)) {
        // Apply boot string
        m_emulator->setBootString(settings.bootString);

        // Apply debug mode
        m_emulator->setDebug(settings.debugMode);

        // Load ROM if changed
        if (!settings.romFile.empty()) {
            std::string romPath = findResourceFile(settings.romFile);
            if (!romPath.empty()) {
                m_emulator->loadROM(romPath);
            }
        }

        // Load disks and save paths for persistence
        int diskCount = 0;
        for (int i = 0; i < 4; i++) {
            if (!settings.diskFiles[i].empty()) {
                std::string diskPath;
                // Check if this is already an absolute path (from Browse dialog)
                // or just a filename (from catalog/download directory)
                bool isAbsolute = (settings.diskFiles[i].length() >= 2 &&
                                   settings.diskFiles[i][1] == ':') ||
                                  (settings.diskFiles[i].length() >= 1 &&
                                   (settings.diskFiles[i][0] == '\\' || settings.diskFiles[i][0] == '/'));
                if (isAbsolute) {
                    diskPath = settings.diskFiles[i];
                } else {
                    diskPath = m_diskCatalog->getDiskPath(settings.diskFiles[i]);
                }
                if (GetFileAttributesA(diskPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    m_emulator->loadDisk(i, diskPath);
                    m_diskPaths[i] = diskPath;  // Save for persistence
                    diskCount++;
                }
            } else {
                // User selected "(None)" - close the disk if one was loaded
                if (m_emulator->isDiskLoaded(i)) {
                    m_emulator->closeDisk(i);
                }
                m_diskPaths[i].clear();
            }
        }

        // Calculate auto slice count based on disk count (matching CBIOS logic):
        // 1 disk: 8 slices, 2 disks: 4 slices, 3+ disks: 2 slices
        int autoSlices = (diskCount <= 1) ? 8 : (diskCount == 2) ? 4 : 2;

        // Apply slice counts to loaded disks
        for (int i = 0; i < 4; i++) {
            if (m_emulator->isDiskLoaded(i)) {
                m_emulator->setDiskSliceCount(i, autoSlices);
            }
        }

        // Save boot string
        m_bootString = settings.bootString;

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
        m_currentFontSize = size;
        checkFontMenuItem(size);
        saveSettings();  // Persist font size
    }
}

void MainWindow::onViewDazzler() {
    m_dazzlerEnabled = !m_dazzlerEnabled;

    // Update menu checkmark
    CheckMenuItem(m_menu, ID_VIEW_DAZZLER, m_dazzlerEnabled ? MF_CHECKED : MF_UNCHECKED);

    if (m_dazzlerEnabled) {
        // Enable Dazzler in emulator
        m_emulator->enableDazzler(m_dazzlerPort, m_dazzlerScale);

        // Create Dazzler window
        if (!m_dazzlerWindow) {
            m_dazzlerWindow = std::make_unique<DazzlerWindow>();

            // Position next to main window
            RECT mainRect;
            GetWindowRect(m_hwnd, &mainRect);
            m_dazzlerWindow->create(m_hwnd, mainRect.right + 10, mainRect.top, m_dazzlerScale);
        }

        // Connect to emulator's Dazzler
        if (m_dazzlerWindow && m_emulator->getDazzler()) {
            m_dazzlerWindow->setDazzler(m_emulator->getDazzler());
            m_dazzlerWindow->show(true);
        }

        m_statusText = "Dazzler enabled (port 0x" +
            std::to_string(m_dazzlerPort) + ")";
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

    updateStatusBar();
}

void MainWindow::onHelpTopics() {
    ShowHelpWindow(m_hwnd);
}

void MainWindow::onHelpAbout() {
    // Build about text with dynamic data directory path
    std::string dataDir = EmulatorEngine::getUserDataDirectory();
    std::wstring dataDirW(dataDir.begin(), dataDir.end());

    std::wstring aboutText =
        L"z80cpmw - Z80 CP/M Emulator\n"
        L"Version 1.0\n\n"
        L"A RomWBW/HBIOS emulator for Windows.\n\n"
        L"Data Directory:\n" + dataDirW + L"\n\n"
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

    EnableMenuItem(m_menu, ID_EMU_START, running ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(m_menu, ID_EMU_STOP, running ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(m_menu, ID_ROM_EMU_AVW, running ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(m_menu, ID_ROM_EMU_ROMWBW, running ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(m_menu, ID_ROM_SBC_SIMH, running ? MF_GRAYED : MF_ENABLED);
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
    CheckMenuItem(m_menu, ID_ROM_SBC_SIMH, romId == ID_ROM_SBC_SIMH ? MF_CHECKED : MF_UNCHECKED);
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

void MainWindow::loadDefaultROM() {
    std::string romPath = findResourceFile("emu_avw.rom");

    if (!romPath.empty()) {
        if (m_emulator->loadROM(romPath)) {
            m_emulator->setROMName("emu_avw.rom");
            m_currentRomId = ID_ROM_EMU_AVW;
        }
    } else {
        // ROM not found - show message in terminal
        if (m_terminal) {
            const char* msg = "WARNING: ROM file not found (emu_avw.rom)\r\n"
                              "Please use Emulator > Select ROM to load a ROM file,\r\n"
                              "or place ROM files in the 'roms' subdirectory.\r\n\r\n";
            for (const char* p = msg; *p; ++p) {
                m_terminal->outputChar(*p);
            }
        }
    }
}

void MainWindow::loadDefaultDisks() {
    // Try to find and load default disk images
    std::vector<std::string> defaultDisks = {
        "cpm_wbw.img",
        "zsys_wbw.img"
    };

    std::string appDir = EmulatorEngine::getAppDirectory();
    std::string disksDir = appDir + "\\disks";

    int diskCount = 0;
    for (int unit = 0; unit < 2 && unit < (int)defaultDisks.size(); unit++) {
        std::string diskPath = disksDir + "\\" + defaultDisks[unit];
        if (GetFileAttributesA(diskPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            m_emulator->loadDisk(unit, diskPath);
            m_emulator->setDiskPath(unit, diskPath);
            diskCount++;
        }
    }

    // Apply auto slice count (1 disk: 8, 2 disks: 4, 3+: 2)
    int autoSlices = (diskCount <= 1) ? 8 : (diskCount == 2) ? 4 : 2;
    for (int i = 0; i < 4; i++) {
        if (m_emulator->isDiskLoaded(i)) {
            m_emulator->setDiskSliceCount(i, autoSlices);
        }
    }
}

void MainWindow::showStartupInstructions() {
    if (!m_terminal) return;

    // Build version string with compile date/time
    char versionLine[128];
    snprintf(versionLine, sizeof(versionLine),
        "  Build: %s %s\r\n", __DATE__, __TIME__);

    const char* instructions =
        "\r\n"
        "  z80cpmw - Z80 CP/M Emulator for Windows\r\n"
        "  ========================================\r\n"
        "\r\n"
        "  Getting Started:\r\n"
        "\r\n"
        "  1. Download disk images:\r\n"
        "     Emulator -> Settings -> select disk -> Download\r\n"
        "\r\n"
        "  2. Assign disks to units:\r\n"
        "     In Settings, select downloaded disks for Disk 0, 1, etc.\r\n"
        "\r\n"
        "  3. Start the emulator:\r\n"
        "     Press F5 or Emulator -> Start\r\n"
        "\r\n"
        "  4. At the RomWBW boot menu:\r\n"
        "     Type 0 and press Enter to boot CP/M from Disk 0\r\n"
        "     (Or set Boot String to \"0\" in Settings to auto-boot)\r\n"
        "\r\n"
        "  File Transfer (R8/W8):\r\n"
        "     R8 filename - Import file from host (opens file picker)\r\n"
        "     W8 filename - Export file to host (opens save dialog)\r\n"
        "\r\n"
        "  Keyboard Shortcuts:\r\n"
        "     F5        - Start emulator\r\n"
        "     Shift+F5  - Stop emulator\r\n"
        "     Ctrl+R    - Reset emulator\r\n"
        "\r\n";

    for (const char* p = instructions; *p; ++p) {
        m_terminal->outputChar(*p);
    }

    // Output build version
    for (const char* p = versionLine; *p; ++p) {
        m_terminal->outputChar(*p);
    }
}

std::string MainWindow::getSettingsPath() {
    std::string userDir = EmulatorEngine::getUserDataDirectory();
    return userDir + "\\z80cpmw.ini";
}

void MainWindow::loadSettings() {
    std::string path = getSettingsPath();

    // Set up data directory for disks and file transfers (use user data dir for Store app compatibility)
    std::string userDir = EmulatorEngine::getUserDataDirectory();
    std::string dataDir = userDir + "\\data";
    CreateDirectoryA(dataDir.c_str(), nullptr);
    m_diskCatalog->setDownloadDirectory(dataDir);

    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        // First run - no settings file

        // Helper to output string to terminal
        auto termOutput = [this](const char* msg) {
            if (m_terminal) {
                for (const char* p = msg; *p; ++p) {
                    m_terminal->outputChar(*p);
                }
            }
        };

        // Check for default disks and load them if present
        std::string combo = dataDir + "\\hd1k_combo.img";
        std::string games = dataDir + "\\hd1k_games.img";

        bool comboLoaded = false;
        bool gamesLoaded = false;
        bool needComboDownload = false;
        bool needGamesDownload = false;

        // Check if combo disk exists
        if (GetFileAttributesA(combo.c_str()) != INVALID_FILE_ATTRIBUTES) {
            m_diskPaths[0] = combo;
            m_emulator->loadDisk(0, combo);
            comboLoaded = true;
            termOutput("Disk 0: hd1k_combo.img loaded\r\n");
        } else {
            needComboDownload = true;
        }

        // Check if games disk exists
        if (GetFileAttributesA(games.c_str()) != INVALID_FILE_ATTRIBUTES) {
            m_diskPaths[1] = games;
            m_emulator->loadDisk(1, games);
            gamesLoaded = true;
            termOutput("Disk 1: hd1k_games.img loaded\r\n");
        } else {
            needGamesDownload = true;
        }

        // Download missing disks (DiskCatalog can only do one at a time, so chain them)
        if (needComboDownload || needGamesDownload) {
            m_downloadingDisks = true;
            termOutput("\r\nDownloading default disk images (please wait)...\r\n");
        }

        if (needComboDownload) {
            m_diskCatalog->downloadDisk("hd1k_combo.img",
                nullptr,
                [this, combo, games, needGamesDownload](bool success, const std::string& error) {
                    // Helper for terminal output in callback
                    auto termOut = [this](const char* msg) {
                        if (m_terminal) {
                            for (const char* p = msg; *p; ++p) {
                                m_terminal->outputChar(*p);
                            }
                        }
                    };

                    if (success) {
                        m_diskPaths[0] = combo;
                        m_emulator->loadDisk(0, combo);
                        termOut("  Disk 0: hd1k_combo.img downloaded and loaded\r\n");
                    } else {
                        termOut("  Disk 0: download failed - ");
                        termOut(error.c_str());
                        termOut("\r\n");
                    }

                    // Chain games download after combo completes
                    if (needGamesDownload) {
                        m_diskCatalog->downloadDisk("hd1k_games.img",
                            nullptr,
                            [this, games](bool success2, const std::string& error2) {
                                auto termOut2 = [this](const char* msg) {
                                    if (m_terminal) {
                                        for (const char* p = msg; *p; ++p) {
                                            m_terminal->outputChar(*p);
                                        }
                                    }
                                };

                                if (success2) {
                                    m_diskPaths[1] = games;
                                    m_emulator->loadDisk(1, games);
                                    termOut2("  Disk 1: hd1k_games.img downloaded and loaded\r\n");
                                } else {
                                    termOut2("  Disk 1: download failed - ");
                                    termOut2(error2.c_str());
                                    termOut2("\r\n");
                                }
                                m_downloadingDisks = false;
                                // Only show "Press F5" if emulator not already running
                                if (!m_emulator->isRunning()) {
                                    termOut2("\r\nPress F5 to start emulator.\r\n");
                                }
                                saveSettings();
                            });
                    } else {
                        m_downloadingDisks = false;
                        if (!m_emulator->isRunning()) {
                            termOut("\r\nPress F5 to start emulator.\r\n");
                        }
                        saveSettings();
                    }
                });
        } else if (needGamesDownload) {
            // Only games needs download
            m_diskCatalog->downloadDisk("hd1k_games.img",
                nullptr,
                [this, games](bool success, const std::string& error) {
                    auto termOut = [this](const char* msg) {
                        if (m_terminal) {
                            for (const char* p = msg; *p; ++p) {
                                m_terminal->outputChar(*p);
                            }
                        }
                    };

                    if (success) {
                        m_diskPaths[1] = games;
                        m_emulator->loadDisk(1, games);
                        termOut("  Disk 1: hd1k_games.img downloaded and loaded\r\n");
                    } else {
                        termOut("  Disk 1: download failed - ");
                        termOut(error.c_str());
                        termOut("\r\n");
                    }
                    m_downloadingDisks = false;
                    if (!m_emulator->isRunning()) {
                        termOut("\r\nPress F5 to start emulator.\r\n");
                    }
                    saveSettings();
                });
        }

        // Save settings if no downloads pending (downloads will save when done)
        if (!needComboDownload && !needGamesDownload) {
            saveSettings();
        }
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // Remove trailing newline
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char* cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        // Parse key=value
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* value = eq + 1;

        if (strcmp(key, "disk0") == 0) {
            m_diskPaths[0] = value;
        } else if (strcmp(key, "disk1") == 0) {
            m_diskPaths[1] = value;
        } else if (strcmp(key, "disk2") == 0) {
            m_diskPaths[2] = value;
        } else if (strcmp(key, "disk3") == 0) {
            m_diskPaths[3] = value;
        } else if (strcmp(key, "bootString") == 0) {
            m_bootString = value;
        } else if (strcmp(key, "fontSize") == 0) {
            m_currentFontSize = atoi(value);
        }
    }
    fclose(f);

    // Apply loaded settings
    int diskCount = 0;
    for (int i = 0; i < 4; i++) {
        if (!m_diskPaths[i].empty()) {
            if (GetFileAttributesA(m_diskPaths[i].c_str()) != INVALID_FILE_ATTRIBUTES) {
                m_emulator->loadDisk(i, m_diskPaths[i]);
                diskCount++;
            }
        }
    }

    // Apply auto slice count (1 disk: 8, 2 disks: 4, 3+: 2)
    int autoSlices = (diskCount <= 1) ? 8 : (diskCount == 2) ? 4 : 2;
    for (int i = 0; i < 4; i++) {
        if (m_emulator->isDiskLoaded(i)) {
            m_emulator->setDiskSliceCount(i, autoSlices);
        }
    }

    if (!m_bootString.empty()) {
        m_emulator->setBootString(m_bootString);
    }

    if (m_currentFontSize > 0 && m_terminal) {
        m_terminal->setFontSize(m_currentFontSize);
        checkFontMenuItem(m_currentFontSize);
    }
}

void MainWindow::saveSettings() {
    std::string path = getSettingsPath();
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;

    for (int i = 0; i < 4; i++) {
        if (!m_diskPaths[i].empty()) {
            fprintf(f, "disk%d=%s\n", i, m_diskPaths[i].c_str());
        }
    }

    if (!m_bootString.empty()) {
        fprintf(f, "bootString=%s\n", m_bootString.c_str());
    }

    fprintf(f, "fontSize=%d\n", m_currentFontSize);

    fclose(f);
}
