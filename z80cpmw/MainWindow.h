/*
 * MainWindow.h - Main Application Window
 *
 * The main window containing the terminal view, menus, and status bar.
 */

#pragma once

#include <windows.h>
#include <memory>
#include <string>

class TerminalView;
class EmulatorEngine;
class DiskCatalog;

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    // Create and show the window
    bool create();
    void show(int cmdShow);

    // Message loop
    int run();

    // Get window handle
    HWND getHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Message handlers
    void onCreate();
    void onDestroy();
    void onSize(int width, int height);
    void onCommand(int id);
    void onTimer();

    // Menu actions
    void onFileLoadDisk(int unit);
    void onFileSaveDisk(int unit);
    void onFileSaveAllDisks();
    void onSelectROM(int romId);
    void onEmulatorStart();
    void onEmulatorStop();
    void onEmulatorReset();
    void onEmulatorSettings();
    void startEmulator();
    void downloadAndStartWithDefaults();
    void onViewFontSize(int size);
    void onHelpAbout();

    // Update menu state
    void updateMenuState();
    void updateStatusBar();
    void checkROMMenuItem(int romId);
    void checkFontMenuItem(int size);

    // Callbacks from emulator
    void onOutputChar(uint8_t ch);
    void onStatusChanged(const std::string& status);

    // Find and load ROM/disk files
    std::string findResourceFile(const std::string& filename);
    void loadDefaultROM();
    void loadDefaultDisks();

    // Startup help
    void showStartupInstructions();

    // Settings persistence
    void loadSettings();
    void saveSettings();
    std::string getSettingsPath();

    HWND m_hwnd = nullptr;
    HWND m_statusBar = nullptr;
    HMENU m_menu = nullptr;

    std::unique_ptr<TerminalView> m_terminal;
    std::unique_ptr<EmulatorEngine> m_emulator;
    std::unique_ptr<DiskCatalog> m_diskCatalog;

    int m_currentRomId = 0;
    int m_currentFontSize = 20;
    std::string m_statusText = "Ready";

    // Saved disk paths
    std::string m_diskPaths[4];
    std::string m_bootString;

    UINT_PTR m_emulatorTimer = 0;
    static constexpr int TIMER_INTERVAL_MS = 10;  // 100 Hz

    // Track if initial disk downloads are in progress
    bool m_downloadingDisks = false;
};
