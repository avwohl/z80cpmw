/*
 * MainWindow.h - Main Application Window
 *
 * The main window containing the terminal view, menus, and status bar.
 */

#pragma once

#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include "Config.h"

class TerminalView;
class EmulatorEngine;
class DiskCatalog;
class DazzlerWindow;
class Dazzler;

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

    // Window placement persistence (remembers position/size across runs)
    bool restoreWindowPlacement();
    void saveWindowPlacement();

    // Resize the window so the client area exactly fits the 80x25 terminal at
    // the current font/DPI (plus the status bar). Used on font change and as the
    // default size when there is no saved placement.
    void resizeWindowToTerminal();
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
    void onViewDazzler();
    void onHelpTopics();
    void onHelpAbout();

    // (Re)build the accelerator table from the current keyboard config, so
    // toggling a shortcut into CP/M takes effect without restarting the app.
    void rebuildAccelerators();
    void destroyAccelerators();
    // Update menu state
    void updateMenuState();
    // Rewrite the shortcut hints on Help/Start/Stop/Reset so the menu shows the
    // keys that are actually registered under the current keyboard config.
    void updateMenuAccelHints();
    void updateStatusBar();
    void checkROMMenuItem(int romId);
    void checkFontMenuItem(int size);

    // Callbacks from emulator
    void onOutputChar(uint8_t ch);
    void onStatusChanged(const std::string& status);

    // Write host-side text (not guest output) into the terminal. No-op before
    // the terminal exists.
    void terminalPrint(const std::string& text);

    // Find and load ROM/disk files
    std::string findResourceFile(const std::string& filename);
    void loadDefaultROM();
    void loadDefaultDisks();

    // Startup help
    void showStartupInstructions();

    // Run fn on the UI thread. Download callbacks arrive on worker threads;
    // everything they touch (terminal, emulator, config) is single-threaded.
    void runOnUiThread(std::function<void()> fn);

    // Settings persistence (via ConfigManager)
    void loadSettings();
    void saveSettings();
    void applyConfig();           // Apply loaded config to emulator state
    void updateConfigFromState(); // Capture current state to config

    // Profile management
    void onLoadProfile();
    void onSaveProfileAs();

    HWND m_hwnd = nullptr;
    HWND m_statusBar = nullptr;
    HMENU m_menu = nullptr;
    HACCEL m_hAccel = nullptr;
    HACCEL m_hAccelRetired = nullptr;   // freed one rebuild later, see above

    std::unique_ptr<TerminalView> m_terminal;
    std::unique_ptr<EmulatorEngine> m_emulator;
    std::unique_ptr<DiskCatalog> m_diskCatalog;
    std::unique_ptr<DazzlerWindow> m_dazzlerWindow;

    int m_currentRomId = 0;         // For menu checkmark tracking
    std::string m_statusText = "Ready";

    // Runtime Dazzler state (config is source of truth for persistence)
    bool m_dazzlerEnabled = false;

    UINT_PTR m_emulatorTimer = 0;
    static constexpr int TIMER_INTERVAL_MS = 10;  // 100 Hz

    // Idle power management: skip timer ticks when guest is polling console
    // with no input available, to reduce CPU usage from ~95% to ~5%.
    int m_idleSkipCount = 0;
    static constexpr int IDLE_SKIP_TICKS = 9;  // Skip 9 of 10 ticks when idle (~100ms effective)

    // Track if initial disk downloads are in progress
    bool m_downloadingDisks = false;
};
