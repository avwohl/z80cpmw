/*
 * SettingsDialog.h - Settings and Disk Catalog Dialog
 *
 * Provides configuration UI and disk image download management.
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

class DiskCatalog;
struct DiskEntry;

// Settings data structure
struct EmulatorSettings {
    std::string romFile;
    std::string diskFiles[4];
    int diskSlices[4] = {4, 4, 4, 4};
    bool debugMode = false;
    bool clearBootConfigRequested = false;  // Set when user clicks "Clear Boot Config"
};

class SettingsDialog {
public:
    SettingsDialog(HWND parent, DiskCatalog* catalog);
    ~SettingsDialog();

    // Show the dialog (modal)
    // Returns true if user clicked OK, false if cancelled
    bool show();

    // Get/set settings
    const EmulatorSettings& getSettings() const { return m_settings; }
    void setSettings(const EmulatorSettings& settings) { m_settings = settings; }

    // Callback when settings are applied
    using SettingsChangedCallback = std::function<void(const EmulatorSettings&)>;
    void setSettingsChangedCallback(SettingsChangedCallback cb) { m_onSettingsChanged = cb; }

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onInitDialog(HWND hwnd);
    void onCommand(HWND hwnd, int id, int notifyCode);
    void onNotify(HWND hwnd, NMHDR* nmhdr);
    void onTimer(HWND hwnd);

    void populateROMList(HWND hwnd);
    void populateDiskLists(HWND hwnd);
    void populateDiskCatalog(HWND hwnd);
    void updateDownloadStatus(HWND hwnd);

    void onDownloadDisk(HWND hwnd);
    void onDeleteDisk(HWND hwnd);
    void onRefreshCatalog(HWND hwnd);
    void onBrowseDisk(HWND hwnd, int unit);
    void onCreateDisk(HWND hwnd, int unit);

    void saveSettings(HWND hwnd);
    void loadSettings(HWND hwnd);

    HWND m_parent;
    DiskCatalog* m_catalog;
    EmulatorSettings m_settings;
    SettingsChangedCallback m_onSettingsChanged;

    std::string m_downloadingFilename;
    bool m_catalogLoading = false;
};

// Dialog resource IDs
#define IDD_SETTINGS            200
#define IDC_ROM_COMBO           201
#define IDC_DISK0_COMBO         202
#define IDC_DISK1_COMBO         203
#define IDC_DISK2_COMBO         204
#define IDC_DISK3_COMBO         205
#define IDC_DISK0_BROWSE        206
#define IDC_DISK1_BROWSE        207
#define IDC_DISK2_BROWSE        208
#define IDC_DISK3_BROWSE        209
#define IDC_DISK0_CREATE        210
#define IDC_DISK1_CREATE        211
#define IDC_DISK2_CREATE        212
#define IDC_DISK3_CREATE        213
#define IDC_SLICE0_SPIN         214
#define IDC_SLICE1_SPIN         215
#define IDC_SLICE2_SPIN         216
#define IDC_SLICE3_SPIN         217
#define IDC_CLEAR_BOOT          218
#define IDC_DEBUG_CHECK         219
#define IDC_CATALOG_LIST        220
#define IDC_DOWNLOAD_BTN        221
#define IDC_DELETE_BTN          222
#define IDC_REFRESH_BTN         223
#define IDC_DOWNLOAD_PROGRESS   224
#define IDC_STATUS_TEXT         225
#define IDT_DOWNLOAD            226
