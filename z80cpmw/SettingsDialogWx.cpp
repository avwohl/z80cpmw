/*
 * SettingsDialogWx.cpp - wxWidgets-based Settings Dialog Implementation
 */

#include "pch.h"
#include "SettingsDialogWx.h"
#include "DiskCatalog.h"
#include <wx/statline.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

wxBEGIN_EVENT_TABLE(SettingsDialogWx, wxDialog)
    EVT_BUTTON(ID_BROWSE_DISK0, SettingsDialogWx::onBrowseDisk)
    EVT_BUTTON(ID_BROWSE_DISK1, SettingsDialogWx::onBrowseDisk)
    EVT_BUTTON(ID_BROWSE_DISK2, SettingsDialogWx::onBrowseDisk)
    EVT_BUTTON(ID_BROWSE_DISK3, SettingsDialogWx::onBrowseDisk)
    EVT_BUTTON(ID_NEW_DISK0, SettingsDialogWx::onNewDisk)
    EVT_BUTTON(ID_NEW_DISK1, SettingsDialogWx::onNewDisk)
    EVT_BUTTON(ID_NEW_DISK2, SettingsDialogWx::onNewDisk)
    EVT_BUTTON(ID_NEW_DISK3, SettingsDialogWx::onNewDisk)
    EVT_BUTTON(ID_REFRESH_CATALOG, SettingsDialogWx::onRefreshCatalog)
    EVT_BUTTON(ID_DOWNLOAD_DISK, SettingsDialogWx::onDownloadDisk)
    EVT_BUTTON(ID_DELETE_DISK, SettingsDialogWx::onDeleteDisk)
    EVT_BUTTON(wxID_OK, SettingsDialogWx::onOK)
    EVT_BUTTON(wxID_CANCEL, SettingsDialogWx::onCancel)
    EVT_COMMAND(ID_CATALOG_LOADED, wxEVT_COMMAND_TEXT_UPDATED, SettingsDialogWx::onCatalogLoaded)
    EVT_COMMAND(ID_DOWNLOAD_PROGRESS, wxEVT_COMMAND_TEXT_UPDATED, SettingsDialogWx::onDownloadProgress)
    EVT_COMMAND(ID_DOWNLOAD_COMPLETE, wxEVT_COMMAND_TEXT_UPDATED, SettingsDialogWx::onDownloadComplete)
wxEND_EVENT_TABLE()

SettingsDialogWx::SettingsDialogWx(wxWindow* parent, DiskCatalog* catalog)
    : wxDialog(parent, wxID_ANY, "Settings", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_catalog(catalog)
{
    OutputDebugStringA("[Settings] Constructor: creating controls\n");
    createControls();
    OutputDebugStringA("[Settings] Constructor: laying out controls\n");
    layoutControls();
    OutputDebugStringA("[Settings] Constructor: populating ROM list\n");
    populateROMList();
    OutputDebugStringA("[Settings] Constructor: populating disk lists\n");
    populateDiskLists();

    OutputDebugStringA("[Settings] Constructor: setting size\n");
    SetMinSize(wxSize(650, 550));
    SetSize(wxSize(750, 650));
    Centre();

    OutputDebugStringA("[Settings] Constructor: starting catalog refresh\n");
    // Start loading catalog
    wxCommandEvent evt;
    onRefreshCatalog(evt);
    OutputDebugStringA("[Settings] Constructor: done\n");
}

SettingsDialogWx::~SettingsDialogWx() {
}

void SettingsDialogWx::createControls() {
    // ROM selection
    m_romChoice = new wxChoice(this, wxID_ANY);

    // Disk selections with slices, browse, and new buttons
    for (int i = 0; i < 4; i++) {
        m_diskChoices[i] = new wxChoice(this, wxID_ANY);
        m_sliceSpins[i] = new wxSpinCtrl(this, wxID_ANY, "4", wxDefaultPosition,
                                          wxSize(60, -1), wxSP_ARROW_KEYS, 1, 8, 4);
        m_browseButtons[i] = new wxButton(this, ID_BROWSE_DISK0 + i, "Browse...");
        m_newButtons[i] = new wxButton(this, ID_NEW_DISK0 + i, "New");
    }

    // Boot string
    m_bootStringText = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1));

    // Debug checkbox
    m_debugCheck = new wxCheckBox(this, wxID_ANY, "Enable Debug Mode");

    // Catalog list
    m_catalogList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 180),
                                    wxLC_REPORT | wxLC_SINGLE_SEL);
    m_catalogList->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 180);
    m_catalogList->InsertColumn(1, "Description", wxLIST_FORMAT_LEFT, 300);
    m_catalogList->InsertColumn(2, "Status", wxLIST_FORMAT_LEFT, 100);

    // Catalog buttons
    m_refreshBtn = new wxButton(this, ID_REFRESH_CATALOG, "Refresh");
    m_downloadBtn = new wxButton(this, ID_DOWNLOAD_DISK, "Download");
    m_deleteBtn = new wxButton(this, ID_DELETE_DISK, "Delete");

    // Progress bar
    m_progressBar = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 20));

    // Status text
    m_statusText = new wxStaticText(this, wxID_ANY, "Ready");
}

void SettingsDialogWx::layoutControls() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Add padding around everything
    wxBoxSizer* paddedSizer = new wxBoxSizer(wxVERTICAL);

    // ROM selection row
    wxBoxSizer* romSizer = new wxBoxSizer(wxHORIZONTAL);
    romSizer->Add(new wxStaticText(this, wxID_ANY, "ROM:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    romSizer->Add(m_romChoice, 1, wxEXPAND);
    paddedSizer->Add(romSizer, 0, wxEXPAND | wxBOTTOM, 10);

    // Disk rows using a flex grid for alignment
    wxFlexGridSizer* diskGrid = new wxFlexGridSizer(4, 6, 8, 10);
    diskGrid->AddGrowableCol(1);  // Disk dropdown stretches

    for (int i = 0; i < 4; i++) {
        wxString label = wxString::Format("Disk %d:", i);
        diskGrid->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        diskGrid->Add(m_diskChoices[i], 1, wxEXPAND);

        wxBoxSizer* sliceSizer = new wxBoxSizer(wxHORIZONTAL);
        sliceSizer->Add(new wxStaticText(this, wxID_ANY, "Slices:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        sliceSizer->Add(m_sliceSpins[i], 0);
        diskGrid->Add(sliceSizer, 0, wxALIGN_CENTER_VERTICAL);

        diskGrid->Add(m_browseButtons[i], 0);
        diskGrid->Add(m_newButtons[i], 0);
        diskGrid->AddSpacer(0);  // Empty cell for alignment
    }
    paddedSizer->Add(diskGrid, 0, wxEXPAND | wxBOTTOM, 15);

    // Boot string row
    wxBoxSizer* bootSizer = new wxBoxSizer(wxHORIZONTAL);
    bootSizer->Add(new wxStaticText(this, wxID_ANY, "Boot String:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    bootSizer->Add(m_bootStringText, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    bootSizer->Add(new wxStaticText(this, wxID_ANY, "(e.g., 0 to auto-boot CP/M)"), 0, wxALIGN_CENTER_VERTICAL);
    paddedSizer->Add(bootSizer, 0, wxEXPAND | wxBOTTOM, 10);

    // Debug checkbox
    paddedSizer->Add(m_debugCheck, 0, wxBOTTOM, 15);

    // Separator
    paddedSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 15);

    // Catalog section header
    wxBoxSizer* catalogHeaderSizer = new wxBoxSizer(wxHORIZONTAL);
    catalogHeaderSizer->Add(new wxStaticText(this, wxID_ANY, "Download Disk Images:"), 0, wxALIGN_CENTER_VERTICAL);
    catalogHeaderSizer->AddStretchSpacer();
    catalogHeaderSizer->Add(m_refreshBtn, 0);
    paddedSizer->Add(catalogHeaderSizer, 0, wxEXPAND | wxBOTTOM, 8);

    // Catalog list
    paddedSizer->Add(m_catalogList, 1, wxEXPAND | wxBOTTOM, 8);

    // Catalog action buttons and progress
    wxBoxSizer* actionSizer = new wxBoxSizer(wxHORIZONTAL);
    actionSizer->Add(m_downloadBtn, 0, wxRIGHT, 5);
    actionSizer->Add(m_deleteBtn, 0, wxRIGHT, 15);
    actionSizer->Add(m_progressBar, 1, wxALIGN_CENTER_VERTICAL);
    paddedSizer->Add(actionSizer, 0, wxEXPAND | wxBOTTOM, 8);

    // Status text
    paddedSizer->Add(m_statusText, 0, wxEXPAND | wxBOTTOM, 15);

    // Add padded content to main sizer
    mainSizer->Add(paddedSizer, 1, wxEXPAND | wxALL, 15);

    // OK/Cancel buttons
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxRIGHT, 10);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0);
    mainSizer->Add(buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    SetSizer(mainSizer);
}

void SettingsDialogWx::populateROMList() {
    m_romChoice->Clear();
    m_romChoice->Append("EMU AVW (Default)");
    m_romChoice->Append("EMU RomWBW");
    m_romChoice->Append("SBC SIMH");
    m_romChoice->SetSelection(0);
}

void SettingsDialogWx::populateDiskLists() {
    for (int i = 0; i < 4; i++) {
        m_diskChoices[i]->Clear();
        m_diskChoices[i]->Append("(None)");

        // Add downloaded disks from catalog
        if (m_catalog) {
            for (const auto& entry : m_catalog->getCatalogEntries()) {
                if (entry.isDownloaded) {
                    m_diskChoices[i]->Append(wxString::FromUTF8(entry.filename));
                }
            }
        }

        m_diskChoices[i]->SetSelection(0);
    }
}

void SettingsDialogWx::populateCatalog() {
    m_catalogList->DeleteAllItems();

    if (!m_catalog) return;

    const auto& entries = m_catalog->getCatalogEntries();
    for (size_t i = 0; i < entries.size(); i++) {
        long idx = m_catalogList->InsertItem(i, wxString::FromUTF8(entries[i].filename));
        m_catalogList->SetItem(idx, 1, wxString::FromUTF8(entries[i].description));
        m_catalogList->SetItem(idx, 2, entries[i].isDownloaded ? "Downloaded" : "Available");
    }
}

void SettingsDialogWx::setSettings(const WxEmulatorSettings& settings) {
    m_settings = settings;
    loadSettings();
}

void SettingsDialogWx::loadSettings() {
    // ROM selection
    if (m_settings.romFile == "emu_romwbw.rom") {
        m_romChoice->SetSelection(1);
    } else if (m_settings.romFile == "SBC_simh_std.rom") {
        m_romChoice->SetSelection(2);
    } else {
        m_romChoice->SetSelection(0);
    }

    // Disk selections and slices
    for (int i = 0; i < 4; i++) {
        m_sliceSpins[i]->SetValue(m_settings.diskSlices[i]);

        if (!m_settings.diskFiles[i].empty()) {
            int idx = m_diskChoices[i]->FindString(wxString::FromUTF8(m_settings.diskFiles[i]));
            if (idx != wxNOT_FOUND) {
                m_diskChoices[i]->SetSelection(idx);
            }
        }
    }

    // Boot string
    m_bootStringText->SetValue(wxString::FromUTF8(m_settings.bootString));

    // Debug mode
    m_debugCheck->SetValue(m_settings.debugMode);
}

void SettingsDialogWx::saveSettings() {
    // ROM selection
    switch (m_romChoice->GetSelection()) {
        case 1: m_settings.romFile = "emu_romwbw.rom"; break;
        case 2: m_settings.romFile = "SBC_simh_std.rom"; break;
        default: m_settings.romFile = "emu_avw.rom"; break;
    }

    // Disk selections and slices
    for (int i = 0; i < 4; i++) {
        int sel = m_diskChoices[i]->GetSelection();
        if (sel > 0) {
            m_settings.diskFiles[i] = m_diskChoices[i]->GetString(sel).ToStdString();
        } else {
            m_settings.diskFiles[i] = "";
        }
        m_settings.diskSlices[i] = m_sliceSpins[i]->GetValue();
    }

    // Boot string
    m_settings.bootString = m_bootStringText->GetValue().ToStdString();

    // Debug mode
    m_settings.debugMode = m_debugCheck->GetValue();
}

void SettingsDialogWx::onBrowseDisk(wxCommandEvent& event) {
    int unit = event.GetId() - ID_BROWSE_DISK0;

    wxFileDialog dlg(this, "Select Disk Image", "", "",
                     "Disk Images (*.img)|*.img|All Files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() == wxID_OK) {
        wxString path = dlg.GetPath();
        int idx = m_diskChoices[unit]->FindString(path);
        if (idx == wxNOT_FOUND) {
            idx = m_diskChoices[unit]->Append(path);
        }
        m_diskChoices[unit]->SetSelection(idx);
    }
}

void SettingsDialogWx::onNewDisk(wxCommandEvent& event) {
    int unit = event.GetId() - ID_NEW_DISK0;

    wxFileDialog dlg(this, "Create New Disk Image",
                     "", wxString::Format("newdisk%d.img", unit),
                     "Disk Images (*.img)|*.img",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() == wxID_OK) {
        wxString path = dlg.GetPath();

        // Create empty 8MB disk image filled with 0xE5
        const size_t DISK_SIZE = 8 * 1024 * 1024;
        std::vector<uint8_t> emptyDisk(DISK_SIZE, 0xE5);

        wxFile file;
        if (file.Create(path, true) && file.Write(emptyDisk.data(), emptyDisk.size()) == DISK_SIZE) {
            m_statusText->SetLabel("Created new 8MB disk image");

            int idx = m_diskChoices[unit]->Append(path);
            m_diskChoices[unit]->SetSelection(idx);
        } else {
            wxMessageBox("Failed to create disk image", "Error", wxOK | wxICON_ERROR);
        }
    }
}

void SettingsDialogWx::onRefreshCatalog(wxCommandEvent& event) {
    if (!m_catalog) return;

    m_statusText->SetLabel("Loading disk catalog...");
    m_refreshBtn->Enable(false);

    // Store dialog pointer for callback
    SettingsDialogWx* dlg = this;

    m_catalog->fetchCatalog([dlg](bool success, const std::vector<DiskEntry>& entries, const std::string& error) {
        // Post event to main thread
        wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, ID_CATALOG_LOADED);
        evt.SetInt(success ? 1 : 0);
        evt.SetString(wxString::FromUTF8(error));
        wxPostEvent(dlg, evt);
    });
}

void SettingsDialogWx::onCatalogLoaded(wxCommandEvent& event) {
    m_refreshBtn->Enable(true);

    if (event.GetInt()) {
        populateCatalog();
        populateDiskLists();
        loadSettings();  // Reapply selections after dropdowns are repopulated
        m_statusText->SetLabel("Catalog loaded");
    } else {
        m_statusText->SetLabel("Failed to load catalog: " + event.GetString());
    }
}

void SettingsDialogWx::onDownloadDisk(wxCommandEvent& event) {
    if (!m_catalog) return;

    long sel = m_catalogList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel < 0) {
        wxMessageBox("Please select a disk to download", "Info", wxOK | wxICON_INFORMATION);
        return;
    }

    wxString filename = m_catalogList->GetItemText(sel);
    std::string filenameStr = filename.ToStdString();

    if (m_catalog->isDiskDownloaded(filenameStr)) {
        wxMessageBox("This disk is already downloaded", "Info", wxOK | wxICON_INFORMATION);
        return;
    }

    m_statusText->SetLabel("Downloading " + filename + "...");
    m_downloadBtn->Enable(false);
    m_progressBar->SetValue(0);

    SettingsDialogWx* dlg = this;

    m_catalog->downloadDisk(filenameStr,
        [dlg](size_t current, size_t total) {
            wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, ID_DOWNLOAD_PROGRESS);
            evt.SetInt(total > 0 ? (int)(current * 100 / total) : 0);
            wxPostEvent(dlg, evt);
        },
        [dlg](bool success, const std::string& error) {
            wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, ID_DOWNLOAD_COMPLETE);
            evt.SetInt(success ? 1 : 0);
            evt.SetString(wxString::FromUTF8(error));
            wxPostEvent(dlg, evt);
        }
    );
}

void SettingsDialogWx::onDownloadProgress(wxCommandEvent& event) {
    m_progressBar->SetValue(event.GetInt());
}

void SettingsDialogWx::onDownloadComplete(wxCommandEvent& event) {
    m_downloadBtn->Enable(true);
    m_progressBar->SetValue(event.GetInt() ? 100 : 0);

    if (event.GetInt()) {
        m_statusText->SetLabel("Download complete");
        populateCatalog();
        populateDiskLists();
    } else {
        m_statusText->SetLabel("Download failed: " + event.GetString());
    }
}

void SettingsDialogWx::onDeleteDisk(wxCommandEvent& event) {
    if (!m_catalog) return;

    long sel = m_catalogList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel < 0) {
        wxMessageBox("Please select a disk to delete", "Info", wxOK | wxICON_INFORMATION);
        return;
    }

    wxString filename = m_catalogList->GetItemText(sel);
    std::string filenameStr = filename.ToStdString();

    if (!m_catalog->isDiskDownloaded(filenameStr)) {
        wxMessageBox("This disk is not downloaded", "Info", wxOK | wxICON_INFORMATION);
        return;
    }

    int result = wxMessageBox("Delete " + filename + "?", "Confirm Delete",
                              wxYES_NO | wxICON_QUESTION);
    if (result == wxYES) {
        if (m_catalog->deleteDownloadedDisk(filenameStr)) {
            m_statusText->SetLabel("Disk deleted");
            populateCatalog();
            populateDiskLists();
        } else {
            wxMessageBox("Failed to delete disk", "Error", wxOK | wxICON_ERROR);
        }
    }
}

void SettingsDialogWx::onOK(wxCommandEvent& event) {
    saveSettings();
    EndModal(wxID_OK);
}

void SettingsDialogWx::onCancel(wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}

// Minimal wxApp for hosting dialogs in a Win32 app
class MinimalWxApp : public wxApp {
public:
    virtual bool OnInit() override { return true; }
};

// Internal function that does the actual dialog work
static bool ShowWxSettingsDialogInternal(DiskCatalog* catalog, WxEmulatorSettings& settings) {
    OutputDebugStringA("[Settings] ShowWxSettingsDialog called\n");

    // Initialize wxWidgets if not already done
    static bool wxInitialized = false;
    static MinimalWxApp* wxAppInstance = nullptr;
    if (!wxInitialized) {
        OutputDebugStringA("[Settings] Initializing wxWidgets...\n");

        // Create and set the app instance BEFORE wxEntryStart
        wxAppInstance = new MinimalWxApp();
        wxApp::SetInstance(wxAppInstance);

        // Use wxEntryStart for more complete initialization
        int argc = 0;
        char* argv[] = { nullptr };
        if (!wxEntryStart(argc, argv)) {
            OutputDebugStringA("[Settings] wxEntryStart failed\n");
            MessageBoxA(nullptr, "wxEntryStart failed", "Settings Error", MB_OK | MB_ICONERROR);
            delete wxAppInstance;
            wxAppInstance = nullptr;
            wxApp::SetInstance(nullptr);
            return false;
        }

        // Call OnInit
        if (!wxAppInstance->OnInit()) {
            OutputDebugStringA("[Settings] wxApp::OnInit failed\n");
            MessageBoxA(nullptr, "wxApp::OnInit failed", "Settings Error", MB_OK | MB_ICONERROR);
            return false;
        }

        OutputDebugStringA("[Settings] wxWidgets initialized OK\n");
        wxInitialized = true;
    }

    OutputDebugStringA("[Settings] Creating dialog...\n");
    bool result = false;
    try {
        // Don't try to parent to Win32 window - just create as top-level
        OutputDebugStringA("[Settings] About to construct SettingsDialogWx\n");
        SettingsDialogWx dlg(nullptr, catalog);
        OutputDebugStringA("[Settings] Dialog constructed, setting settings\n");
        dlg.setSettings(settings);

        // Center on screen since we don't have a parent
        dlg.Centre();

        OutputDebugStringA("[Settings] Showing modal dialog\n");
        result = (dlg.ShowModal() == wxID_OK);
        OutputDebugStringA("[Settings] Dialog closed\n");
        if (result) {
            settings = dlg.getSettings();
        }
    }
    catch (const std::exception& e) {
        OutputDebugStringA("[Settings] Exception: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        MessageBoxA(nullptr, e.what(), "Settings Exception", MB_OK | MB_ICONERROR);
    }
    catch (...) {
        OutputDebugStringA("[Settings] Unknown exception\n");
        MessageBoxA(nullptr, "Unknown exception in settings dialog", "Settings Error", MB_OK | MB_ICONERROR);
    }

    OutputDebugStringA("[Settings] Returning\n");
    return result;
}

// Helper function to show the dialog from Win32 code
// Uses SEH to catch access violations
bool ShowWxSettingsDialog(void* parentHwnd, DiskCatalog* catalog, WxEmulatorSettings& settings) {
    (void)parentHwnd;  // Not used anymore

    __try {
        return ShowWxSettingsDialogInternal(catalog, settings);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        DWORD code = GetExceptionCode();
        char msg[256];
        sprintf_s(msg, "Settings dialog crashed with exception code 0x%08X", code);
        OutputDebugStringA(msg);
        OutputDebugStringA("\n");
        MessageBoxA(nullptr, msg, "Settings Crash", MB_OK | MB_ICONERROR);
        return false;
    }
}
