/*
 * SettingsDialogWx.h - wxWidgets-based Settings Dialog
 *
 * Uses wxWidgets sizers for proper auto-layout that scales correctly.
 */

#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/listctrl.h>
#include <wx/gauge.h>
#include <string>
#include <vector>
#include <functional>

class DiskCatalog;
struct DiskEntry;

// Settings structure
struct WxEmulatorSettings {
    std::string romFile;
    std::string diskFiles[4];
    int diskSlices[4] = {4, 4, 4, 4};
    bool diskSlicesAuto[4] = {true, true, true, true};
    std::string bootString;
    bool debugMode = false;

    // Dazzler settings
    bool dazzlerEnabled = false;
    int dazzlerPort = 0x0E;
    int dazzlerScale = 4;
};

class SettingsDialogWx : public wxDialog {
public:
    SettingsDialogWx(wxWindow* parent, DiskCatalog* catalog);
    virtual ~SettingsDialogWx();

    void setSettings(const WxEmulatorSettings& settings);
    const WxEmulatorSettings& getSettings() const { return m_settings; }

private:
    void createControls();
    void layoutControls();
    void populateROMList();
    void populateDiskLists();
    void populateCatalog();
    void loadSettings();
    void saveSettings();

    // Event handlers
    void onBrowseDisk(wxCommandEvent& event);
    void onNewDisk(wxCommandEvent& event);
    void onSliceAutoChanged(wxCommandEvent& event);
    void onDazzlerEnabledChanged(wxCommandEvent& event);
    void onRefreshCatalog(wxCommandEvent& event);
    void onDownloadDisk(wxCommandEvent& event);
    void onDeleteDisk(wxCommandEvent& event);
    void onCatalogLoaded(wxCommandEvent& event);
    void onDownloadProgress(wxCommandEvent& event);
    void onDownloadComplete(wxCommandEvent& event);
    void onOK(wxCommandEvent& event);
    void onCancel(wxCommandEvent& event);

    DiskCatalog* m_catalog;
    WxEmulatorSettings m_settings;

    // Controls
    wxChoice* m_romChoice;
    wxChoice* m_diskChoices[4];
    wxCheckBox* m_sliceAutoChecks[4];
    wxSpinCtrl* m_sliceSpins[4];
    wxButton* m_browseButtons[4];
    wxButton* m_newButtons[4];
    wxTextCtrl* m_bootStringText;
    wxCheckBox* m_debugCheck;

    // Dazzler controls
    wxCheckBox* m_dazzlerEnabledCheck;
    wxSpinCtrl* m_dazzlerPortSpin;
    wxSpinCtrl* m_dazzlerScaleSpin;
    wxStaticText* m_dazzlerPortLabel;
    wxStaticText* m_dazzlerScaleLabel;
    wxListCtrl* m_catalogList;
    wxButton* m_refreshBtn;
    wxButton* m_downloadBtn;
    wxButton* m_deleteBtn;
    wxGauge* m_progressBar;
    wxStaticText* m_statusText;
    wxStaticText* m_diskDirText;

    // Custom event IDs
    enum {
        ID_BROWSE_DISK0 = wxID_HIGHEST + 1,
        ID_BROWSE_DISK1,
        ID_BROWSE_DISK2,
        ID_BROWSE_DISK3,
        ID_NEW_DISK0,
        ID_NEW_DISK1,
        ID_NEW_DISK2,
        ID_NEW_DISK3,
        ID_SLICE_AUTO0,
        ID_SLICE_AUTO1,
        ID_SLICE_AUTO2,
        ID_SLICE_AUTO3,
        ID_DAZZLER_ENABLED,
        ID_REFRESH_CATALOG,
        ID_DOWNLOAD_DISK,
        ID_DELETE_DISK,
        ID_CATALOG_LOADED,
        ID_DOWNLOAD_PROGRESS,
        ID_DOWNLOAD_COMPLETE
    };

    wxDECLARE_EVENT_TABLE();
};

// Helper function to show the dialog from Win32 code
bool ShowWxSettingsDialog(void* parentHwnd, DiskCatalog* catalog, WxEmulatorSettings& settings);
