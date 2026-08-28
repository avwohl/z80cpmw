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
#include <wx/notebook.h>
#include <string>
#include <vector>
#include <functional>

class DiskCatalog;
struct DiskEntry;

// Settings structure
struct WxEmulatorSettings {
    std::string romFile;
    std::string diskFiles[4];
    bool debugMode = false;
    bool warnManifestWrites = true;         // Warn when writing to downloaded catalog disks
    bool clearBootConfigRequested = false;  // Set when user clicks "Clear Boot Config"
    int scrollbackLines = 1000;             // Terminal scrollback history capacity (lines)

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
    // One builder per notebook page: each creates the controls it owns, parents
    // them to its own panel and gives that panel its sizer. Creation and layout
    // are not split across two passes here the way createControls() and
    // layoutControls() split them, because a control's page IS its parent -
    // separating the two just means naming the same panel twice.
    void buildMachinePage();
    void buildTerminalPage();
    void buildDiskImagesPage();
    void populateROMList();
    void populateDiskLists();
    void populateCatalog();
    void loadSettings();
    void saveSettings();

    // Event handlers
    void onBrowseDisk(wxCommandEvent& event);
    void onNewDisk(wxCommandEvent& event);
    void onDazzlerEnabledChanged(wxCommandEvent& event);
    void onClearBootConfig(wxCommandEvent& event);
    void onRefreshCatalog(wxCommandEvent& event);
    void onDownloadDisk(wxCommandEvent& event);
    void onDeleteDisk(wxCommandEvent& event);
    void onOpenDataFolder(wxCommandEvent& event);
    void onCatalogLoaded(wxCommandEvent& event);
    void onDownloadProgress(wxCommandEvent& event);
    void onDownloadComplete(wxCommandEvent& event);
    void onOK(wxCommandEvent& event);
    void onCancel(wxCommandEvent& event);

    DiskCatalog* m_catalog;
    WxEmulatorSettings m_settings;

    // Notebook and its pages. Every control below except m_statusText and the
    // OK/Cancel buttons is parented to one of these panels, not to the dialog.
    wxNotebook* m_notebook;
    wxPanel* m_machinePage;
    wxPanel* m_terminalPage;
    wxPanel* m_diskImagesPage;

    // Controls
    wxChoice* m_romChoice;
    wxChoice* m_diskChoices[4];
    wxButton* m_browseButtons[4];
    wxButton* m_newButtons[4];
    wxButton* m_clearBootBtn;
    wxCheckBox* m_debugCheck;
    wxCheckBox* m_warnManifestCheck;
    wxSpinCtrl* m_scrollbackSpin;

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
    // Deliberately parented to the dialog, below the notebook, and NOT to any
    // page: six handlers write it and they do not all live on one page - see
    // createControls().
    wxStaticText* m_statusText;
    wxStaticText* m_diskDirLabel;
    wxTextCtrl* m_diskDirText;
    wxButton* m_openFolderBtn;
    std::string m_dataFolderPath;

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
        ID_DAZZLER_ENABLED,
        ID_CLEAR_BOOT_CONFIG,
        ID_REFRESH_CATALOG,
        ID_DOWNLOAD_DISK,
        ID_DELETE_DISK,
        ID_CATALOG_LOADED,
        ID_DOWNLOAD_PROGRESS,
        ID_DOWNLOAD_COMPLETE,
        ID_OPEN_DATA_FOLDER
    };

    wxDECLARE_EVENT_TABLE();
};

// Helper function to show the dialog from Win32 code
bool ShowWxSettingsDialog(void* parentHwnd, DiskCatalog* catalog, WxEmulatorSettings& settings);
