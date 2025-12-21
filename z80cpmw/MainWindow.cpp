/*
 * MainWindow.cpp - Main Application Window Implementation
 */

#include "pch.h"
#include "MainWindow.h"
#include "TerminalView.h"
#include "EmulatorEngine.h"
#include "DiskCatalog.h"
#include "SettingsDialog.h"
#include "resource.h"

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
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
        wc.lpszClassName = WINDOW_CLASS;
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

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
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
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

    // Create terminal view
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

    // Load default ROM and disks
    loadDefaultROM();
    loadDefaultDisks();

    // Set menu items
    m_menu = GetMenu(m_hwnd);
    checkROMMenuItem(ID_ROM_EMU_AVW);
    checkFontMenuItem(m_currentFontSize);

    // Start emulator timer
    m_emulatorTimer = SetTimer(m_hwnd, IDT_EMULATOR, TIMER_INTERVAL_MS, nullptr);

    // Update status
    updateStatusBar();
}

void MainWindow::onDestroy() {
    if (m_emulatorTimer) {
        KillTimer(m_hwnd, m_emulatorTimer);
        m_emulatorTimer = 0;
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

    case ID_HELP_ABOUT:
        onHelpAbout();
        break;
    }
}

void MainWindow::onTimer() {
    if (m_emulator && m_emulator->isRunning()) {
        m_emulator->runBatch();

        // Force terminal to repaint after batch processing
        if (m_terminal) {
            m_terminal->repaint();
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
    if (m_terminal) {
        m_terminal->clear();
    }
    m_emulator->start();
    updateMenuState();

    // Focus terminal
    if (m_terminal && m_terminal->getHwnd()) {
        SetFocus(m_terminal->getHwnd());
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

    SettingsDialog dialog(m_hwnd, m_diskCatalog.get());

    // Set current settings
    EmulatorSettings settings;
    settings.bootString = m_emulator->getBootString();
    settings.debugMode = false;  // TODO: get from emulator
    dialog.setSettings(settings);

    if (dialog.show()) {
        // Apply new settings
        const auto& newSettings = dialog.getSettings();

        // Apply boot string
        m_emulator->setBootString(newSettings.bootString);

        // Apply debug mode
        m_emulator->setDebug(newSettings.debugMode);

        // Load ROM if changed
        if (!newSettings.romFile.empty()) {
            std::string romPath = findResourceFile(newSettings.romFile);
            if (!romPath.empty()) {
                m_emulator->loadROM(romPath);
            }
        }

        // Load disks
        for (int i = 0; i < 4; i++) {
            if (!newSettings.diskFiles[i].empty()) {
                std::string diskPath = m_diskCatalog->getDiskPath(newSettings.diskFiles[i]);
                if (GetFileAttributesA(diskPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    m_emulator->loadDisk(i, diskPath);
                }
            }
        }

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
    }
}

void MainWindow::onHelpAbout() {
    MessageBoxW(m_hwnd,
        L"z80cpmw - Z80 CP/M Emulator\n"
        L"Version 1.0\n\n"
        L"A RomWBW/HBIOS emulator for Windows.\n\n"
        L"Based on:\n"
        L"  - qkz80 Z80 emulator core\n"
        L"  - RomWBW by Wayne Warthen\n\n"
        L"License: MIT\n"
        L"CP/M OS licensed by Lineo for non-commercial use.",
        L"About z80cpmw",
        MB_OK | MB_ICONINFORMATION
    );
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
        m_emulator->loadROM(romPath);
        m_emulator->setROMName("emu_avw.rom");
        m_currentRomId = ID_ROM_EMU_AVW;
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

    for (int unit = 0; unit < 2 && unit < (int)defaultDisks.size(); unit++) {
        std::string diskPath = disksDir + "\\" + defaultDisks[unit];
        if (GetFileAttributesA(diskPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            m_emulator->loadDisk(unit, diskPath);
            m_emulator->setDiskPath(unit, diskPath);
        }
    }
}
