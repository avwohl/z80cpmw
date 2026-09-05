/*
 * MainWindow.h - Main Application Window
 *
 * The main window containing the terminal view, menus, and status bar.
 */

#pragma once

#include <windows.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "Config.h"

class TerminalView;
class EmulatorEngine;
class DiskCatalog;
class WorkerPostGate;
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
    // F5 with nothing mounted. Two halves now, because after the URL switch
    // there is no download without a catalog: this one probes the data folder
    // and starts straight away if both default images are already there, and
    // otherwise fetches the catalog and hands over to the second.
    void downloadAndStartWithDefaults();
    // UI THREAD, from the catalog callback posted by the above. 'ok' and 'error'
    // are that fetch's verdict.
    void startWithDefaultsAfterCatalog(bool ok, const std::string& error);
    // Where a default image would be if it is already here, tried under every
    // name this application has ever given it. Empty when none of them is a
    // complete file. See the definition for the three names and why each is
    // still worth a look.
    std::string cachedDefaultDisk(const char* diskId) const;
    // The RomWBW release the ROM in the banks declares, as "3.5.1", or empty
    // when no ROM is loaded or its HCB cannot be read. Read from the loaded
    // image rather than from a constant - there is no compile-time pin any more.
    std::string loadedRomwbwRelease() const;
    void onViewFontSize(int size);
    void onViewDazzler();

    // Bring the machine, the Dazzler window and the View menu's check mark into
    // line with one requested Dazzler state. The one place that does it, and
    // it has three callers: onViewDazzler(), which toggles it;
    // onEmulatorSettings()' OK path, which can also change the port or the
    // scale of a card that is already running - the reason the port and scale
    // are parameters rather than read from the config here; and applyConfig(),
    // for startup and for a profile load.
    //
    // It does NOT save. The two callers that act on something the user just
    // asked for do that themselves and updateConfigFromState() reads back the
    // state this leaves behind; applyConfig() has nothing to write back,
    // because it is applying a configuration that was just loaded.
    //
    // It does not force the window onto the screen either: onEmulatorSettings()
    // reaches it on EVERY OK, so it shows the window only when this call is
    // what ENABLES the card, or when there is no window yet. A window the user
    // closed is a card that got disabled with it - DazzlerWindow's WM_CLOSE
    // posts WM_APP_DAZZLER_CLOSED - so it comes back on the next enable.
    void applyDazzlerState(bool enabled, uint8_t port, int scale);
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

    // Host-side notices, which are the messages that have to outlive a clear of
    // the terminal.
    //
    // startEmulator() and onEmulatorReset() both call clear() and
    // resetScrollback(), which erased everything the ROM and configuration
    // loaders had printed during onCreate(). The worst case was silent: the
    // saved ROM is unusable, loadDefaultROM() has already put a good one in the
    // banks, so hasROM() is true, startEmulator()'s no-ROM guard does not fire,
    // and the two lines explaining which ROM is actually about to boot were
    // wiped by the clear immediately below that guard.
    //
    // A notice is a claim about the state of the app, so it is held rather than
    // printed once, and it lives exactly as long as the claim is true - every
    // clearNotice() call site is a place that makes one of them false. The
    // container is std::map, not unordered_map, because the enumerator order
    // below IS the print order: what the configuration file said first, then
    // what the ROM banks ended up holding, which is the notice that belongs
    // nearest the boot output it explains. The five Config* enumerators are in
    // config::Problem's own order, which ConfigReport.h documents as the order
    // the kinds get worse in.
    enum class Notice {
        ConfigUnknownMember,
        ConfigTypeMismatch,
        ConfigReservedKey,
        ConfigUnknownKeyName,
        ConfigUnreadableFile,
        DefaultRom,   // loadDefaultROM(): no usable emu_avw.rom
        SavedRom,     // applyConfig(): the ROM named by the config is not the one running
        StorageMigration,  // migrateStorageToInterfaceV0(): a file it could not rename
    };

    // Raise a notice AND print it now. Notices are raised where nothing is
    // about to clear the screen - during onCreate(), and again when a profile
    // is loaded - so one that was only remembered would sit unread until the
    // next Start; printNotices() runs only where the screen has just been
    // emptied.
    void setNotice(Notice which, const std::string& text);
    // Retract a notice. Erasing a notice that was never raised is a no-op, so a
    // caller does not have to know which of them it is contradicting.
    void clearNotice(Notice which);
    void printNotices();

    // Turn ConfigManager::diagnostics() into notices, one per config::Problem
    // kind. Called after every load of a configuration file.
    void reportConfigDiagnostics();

    // Find and load ROM/disk files
    std::string findResourceFile(const std::string& filename);
    void loadDefaultROM();

    // Startup help
    void showStartupInstructions();

    // Settings persistence (via ConfigManager)
    void loadSettings();
    void saveSettings();
    // Renames the pre-v0 catalog images and rewrites everything that names one.
    // Called from loadSettings() only, and only between the config load and
    // applyConfig(); see the comment on the definition for why that window and
    // why it is handed the data folder rather than finding it.
    void migrateStorageToInterfaceV0(const std::string& dataDir);
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

    // shared_ptr, not unique_ptr, and that is load-bearing rather than a style
    // choice: DiskCatalog hands each of its detached workers a
    // shared_from_this(), so a fetch or a download still running when the app
    // quits keeps the catalog alive until it is done instead of reading its
    // members out of a freed block. DiskCatalog.h has the measurements. Built
    // with make_shared in the member-init list, so shared_from_this() is legal
    // from the first call onwards.
    std::shared_ptr<DiskCatalog> m_diskCatalog;

    // The permission a download worker needs before it may PostMessage to
    // m_hwnd. Closed by onDestroy(); see postToUiThread() and WorkerPostGate.
    std::shared_ptr<WorkerPostGate> m_uiPostGate;

    std::unique_ptr<DazzlerWindow> m_dazzlerWindow;

    int m_currentRomId = 0;         // For menu checkmark tracking
    std::string m_statusText = "Ready";

    // The notices currently true, keyed and ordered by the enum above.
    std::map<Notice, std::string> m_notices;

    // Whether saveSettings() is allowed to retract Notice::ConfigUnreadableFile.
    //
    // That notice is the only place in the whole UI that names the file the
    // broken configuration was renamed to and quotes the parser's line and
    // column, so it may be taken down only where the save really did falsify
    // it: when the file it describes is STILL sitting at the path this save
    // writes, because ConfigManager's quarantine rename failed. Where the
    // rename succeeded there is nothing left at that path to overwrite and the
    // notice stays true - see saveSettings().
    //
    // reportConfigDiagnostics() recomputes it from ConfigManager::diagnostics()
    // every time it runs, and that function is the only place the notice is
    // ever raised, so the flag and the notice cannot drift apart: a later clean
    // load takes down both.
    bool m_unreadableConfigStillInPlace = false;

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
