/*
 * SettingsDialogWx.cpp - wxWidgets-based Settings Dialog Implementation
 */

#include "pch.h"
#include "SettingsDialogWx.h"
#include "DiskCatalog.h"
#include <wx/statline.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

// Real on-disk location of the data folder, resolving MSIX/Store redirection.
extern "C" const char* emu_io_get_data_folder_display();

wxBEGIN_EVENT_TABLE(SettingsDialogWx, wxDialog)
    EVT_BUTTON(ID_BROWSE_DISK0, SettingsDialogWx::onBrowseDisk)
    EVT_BUTTON(ID_BROWSE_DISK1, SettingsDialogWx::onBrowseDisk)
    EVT_BUTTON(ID_BROWSE_DISK2, SettingsDialogWx::onBrowseDisk)
    EVT_BUTTON(ID_BROWSE_DISK3, SettingsDialogWx::onBrowseDisk)
    EVT_BUTTON(ID_NEW_DISK0, SettingsDialogWx::onNewDisk)
    EVT_BUTTON(ID_NEW_DISK1, SettingsDialogWx::onNewDisk)
    EVT_BUTTON(ID_NEW_DISK2, SettingsDialogWx::onNewDisk)
    EVT_BUTTON(ID_NEW_DISK3, SettingsDialogWx::onNewDisk)
    EVT_CHECKBOX(ID_DAZZLER_ENABLED, SettingsDialogWx::onDazzlerEnabledChanged)
    EVT_BUTTON(ID_CLEAR_BOOT_CONFIG, SettingsDialogWx::onClearBootConfig)
    EVT_BUTTON(ID_REFRESH_CATALOG, SettingsDialogWx::onRefreshCatalog)
    EVT_BUTTON(ID_DOWNLOAD_DISK, SettingsDialogWx::onDownloadDisk)
    EVT_BUTTON(ID_DELETE_DISK, SettingsDialogWx::onDeleteDisk)
    EVT_BUTTON(ID_OPEN_DATA_FOLDER, SettingsDialogWx::onOpenDataFolder)
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
    // Height is computed, not guessed, and that is a bug fix rather than tidying.
    // Measured on this machine at 200% display scaling by logging
    // GetSizer()->ComputeFittingWindowSize(this): the old single column wanted
    // 850x1320 but was pinned to SetSize(900, 750) - 570px short. What that cost,
    // measured with GetWindowRect on the running dialog and confirmed against a
    // PrintWindow capture: the catalog list came out 844x0, and the whole
    // download section below the Dazzler box (header, folder path, list,
    // Download/Delete, progress) did not render at all, with the Dazzler box
    // itself clipped mid-group. At 200% the downloader was unreachable. The
    // 650/750 pair is a fixed pixel count, so the more the display scales the
    // further short it falls.
    //
    // Paged, the tallest page is Machine at 762x559 (Disk Images 824x521,
    // Terminal 410x70) and the whole dialog fits in 886x819 with nothing
    // compressed - the list now measures 808x288, above the 250 it asks for.
    // 819 is more than the old 750: this is not the dialog getting shorter, it is
    // the dialog no longer claiming to need less than it does.
    //
    // Width is deliberately left at the 800/900 already in use. Fit() asks for
    // 886, within 14px of it, so there is nothing to win by moving it. That keeps
    // the DIALOG's width identical, not the content's: the notebook border plus
    // the page's 15px inset cost the list 36px of width (844 -> 808, both
    // measured). Its three columns declare 220+450+120 and have never had that
    // much, so this widens an overflow that already existed rather than creating
    // one.
    Fit();
    const int fittedHeight = GetSize().GetHeight();
    SetMinSize(wxSize(800, fittedHeight));
    SetSize(wxSize(900, fittedHeight));
    char sizeMsg[80];
    sprintf_s(sizeMsg, "[Settings] Constructor: fitted height %d\n", fittedHeight);
    OutputDebugStringA(sizeMsg);
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
    // A notebook, not one long column. Every control used to be parented to the
    // dialog and stacked in a single paddedSizer that measured 1320px tall in a
    // 750px dialog - see the sizing block in the constructor for what that cost.
    // There was also nowhere to put a new section, which is the other half of
    // why this is paged.
    m_notebook = new wxNotebook(this, wxID_ANY);
    m_machinePage = new wxPanel(m_notebook, wxID_ANY);
    m_terminalPage = new wxPanel(m_notebook, wxID_ANY);
    m_diskImagesPage = new wxPanel(m_notebook, wxID_ANY);
    m_notebook->AddPage(m_machinePage, "Machine", true);
    m_notebook->AddPage(m_terminalPage, "Terminal");
    m_notebook->AddPage(m_diskImagesPage, "Disk Images");

    buildMachinePage();
    buildTerminalPage();
    buildDiskImagesPage();

    // The status line is the one control that stays on the dialog. Six handlers
    // write it and they do not share a page: onNewDisk is a Machine control's
    // handler, while onRefreshCatalog, onCatalogLoaded, onDownloadDisk,
    // onDownloadComplete and onDeleteDisk all belong to Disk Images. Put it on
    // either page and five of its six writers would be updating a label the
    // user is not looking at.
    //
    // The disk dropdowns are also written across the page boundary -
    // populateDiskLists() runs from the catalog handlers and refills controls on
    // the Machine page - but that is persistent state the user can switch pages
    // to go and read. A status message only means anything at the moment it is
    // written, so it has to be somewhere always on screen.
    m_statusText = new wxStaticText(this, wxID_ANY, "Ready");
}

// Everything that describes the emulated machine: which ROM it boots, what is
// in its four disk slots, its boot config, debug, and the Dazzler card.
void SettingsDialogWx::buildMachinePage() {
    wxWindow* page = m_machinePage;
    wxBoxSizer* content = new wxBoxSizer(wxVERTICAL);

    // ROM selection row
    m_romChoice = new wxChoice(page, wxID_ANY);
    wxBoxSizer* romSizer = new wxBoxSizer(wxHORIZONTAL);
    romSizer->Add(new wxStaticText(page, wxID_ANY, "ROM:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    romSizer->Add(m_romChoice, 1, wxEXPAND);
    content->Add(romSizer, 0, wxEXPAND | wxBOTTOM, 10);

    // Disk rows using a flex grid for alignment
    wxFlexGridSizer* diskGrid = new wxFlexGridSizer(4, 4, 8, 10);
    diskGrid->AddGrowableCol(1);  // Disk dropdown stretches

    for (int i = 0; i < 4; i++) {
        m_diskChoices[i] = new wxChoice(page, wxID_ANY);
        m_browseButtons[i] = new wxButton(page, ID_BROWSE_DISK0 + i, "Browse...");
        m_newButtons[i] = new wxButton(page, ID_NEW_DISK0 + i, "New");

        wxString label = wxString::Format("Disk %d:", i);
        diskGrid->Add(new wxStaticText(page, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        diskGrid->Add(m_diskChoices[i], 1, wxEXPAND);
        diskGrid->Add(m_browseButtons[i], 0);
        diskGrid->Add(m_newButtons[i], 0);
    }
    content->Add(diskGrid, 0, wxEXPAND | wxBOTTOM, 15);

    // Boot config row
    m_clearBootBtn = new wxButton(page, ID_CLEAR_BOOT_CONFIG, "Clear Boot Config");
    wxBoxSizer* bootSizer = new wxBoxSizer(wxHORIZONTAL);
    bootSizer->Add(m_clearBootBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    bootSizer->Add(new wxStaticText(page, wxID_ANY, "(Use 'W' at boot menu to configure autoboot)"), 0, wxALIGN_CENTER_VERTICAL);
    content->Add(bootSizer, 0, wxEXPAND | wxBOTTOM, 10);

    // Debug checkbox
    m_debugCheck = new wxCheckBox(page, wxID_ANY, "Enable Debug Mode");
    content->Add(m_debugCheck, 0, wxBOTTOM, 15);

    // Separator before Dazzler. The one that used to sit before the catalog is
    // gone: the page boundary already separates it, and a rule across the top
    // of Disk Images would divide that page from nothing.
    content->Add(new wxStaticLine(page), 0, wxEXPAND | wxBOTTOM, 15);

    // Dazzler section. The static box and the controls inside it are siblings,
    // both parented to the page, exactly as they were both parented to the
    // dialog before - wxMSW draws that correctly and changing it is not this
    // commit's business.
    wxStaticBoxSizer* dazzlerBox = new wxStaticBoxSizer(wxVERTICAL, page, "Cromemco Dazzler Graphics Card");

    m_dazzlerEnabledCheck = new wxCheckBox(page, ID_DAZZLER_ENABLED, "Enable Dazzler Graphics Card");
    m_dazzlerPortLabel = new wxStaticText(page, wxID_ANY, "Port (hex):");
    m_dazzlerPortSpin = new wxSpinCtrl(page, wxID_ANY, "14", wxDefaultPosition,
                                        wxSize(70, -1), wxSP_ARROW_KEYS, 0, 255, 0x0E);
    m_dazzlerScaleLabel = new wxStaticText(page, wxID_ANY, "Scale:");
    m_dazzlerScaleSpin = new wxSpinCtrl(page, wxID_ANY, "4", wxDefaultPosition,
                                         wxSize(60, -1), wxSP_ARROW_KEYS, 1, 8, 4);

    // Dazzler enable checkbox
    dazzlerBox->Add(m_dazzlerEnabledCheck, 0, wxBOTTOM, 10);

    // Dazzler port and scale row
    wxBoxSizer* dazzlerRowSizer = new wxBoxSizer(wxHORIZONTAL);
    dazzlerRowSizer->Add(m_dazzlerPortLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    dazzlerRowSizer->Add(m_dazzlerPortSpin, 0, wxRIGHT, 20);
    dazzlerRowSizer->Add(m_dazzlerScaleLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    dazzlerRowSizer->Add(m_dazzlerScaleSpin, 0);
    dazzlerBox->Add(dazzlerRowSizer, 0, wxLEFT, 20);

    content->Add(dazzlerBox, 0, wxEXPAND);

    // 15px inset all round, the margin the single column used to get from
    // mainSizer's wxALL.
    wxBoxSizer* pageSizer = new wxBoxSizer(wxVERTICAL);
    pageSizer->Add(content, 1, wxEXPAND | wxALL, 15);
    page->SetSizer(pageSizer);
}

// Scrollback is the only terminal setting this dialog has today, so this page
// holds one row. It exists now because reparenting is the expensive half of the
// change and doing it once is cheaper than doing it again per new section.
void SettingsDialogWx::buildTerminalPage() {
    wxWindow* page = m_terminalPage;
    wxBoxSizer* content = new wxBoxSizer(wxVERTICAL);

    // Terminal scrollback history size (lines). 0 disables scrollback.
    m_scrollbackSpin = new wxSpinCtrl(page, wxID_ANY, "1000", wxDefaultPosition,
                                       wxSize(90, -1), wxSP_ARROW_KEYS, 0, 100000, 1000);
    wxBoxSizer* scrollbackSizer = new wxBoxSizer(wxHORIZONTAL);
    scrollbackSizer->Add(new wxStaticText(page, wxID_ANY, "Terminal scrollback (lines):"),
                         0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    scrollbackSizer->Add(m_scrollbackSpin, 0, wxALIGN_CENTER_VERTICAL);
    content->Add(scrollbackSizer, 0);

    wxBoxSizer* pageSizer = new wxBoxSizer(wxVERTICAL);
    pageSizer->Add(content, 1, wxEXPAND | wxALL, 15);
    page->SetSizer(pageSizer);
}

// The download catalog, the folder it downloads into, and the warning that
// fires when the guest writes to something downloaded from it.
void SettingsDialogWx::buildDiskImagesPage() {
    wxWindow* page = m_diskImagesPage;
    wxBoxSizer* content = new wxBoxSizer(wxVERTICAL);

    // Catalog section header
    m_refreshBtn = new wxButton(page, ID_REFRESH_CATALOG, "Refresh");
    wxBoxSizer* catalogHeaderSizer = new wxBoxSizer(wxHORIZONTAL);
    catalogHeaderSizer->Add(new wxStaticText(page, wxID_ANY, "Download Disk Images:"), 0, wxALIGN_CENTER_VERTICAL);
    catalogHeaderSizer->AddStretchSpacer();
    catalogHeaderSizer->Add(m_refreshBtn, 0);
    content->Add(catalogHeaderSizer, 0, wxEXPAND | wxBOTTOM, 4);

    // Data directory path (where disks and R8/W8 file transfers are stored).
    // Use the resolved real path so it works even for the sandboxed Store build,
    // whose %LOCALAPPDATA% writes are redirected into the package's LocalCache.
    const char* realDir = emu_io_get_data_folder_display();
    m_dataFolderPath = realDir ? realDir : "";
    if (m_dataFolderPath.empty() && m_catalog) {
        m_dataFolderPath = m_catalog->getDownloadDirectory();
    }

    m_diskDirLabel = new wxStaticText(page, wxID_ANY,
        "Data folder (disks and R8/W8 transfers):");
    // Read-only text control so the user can select and copy the path.
    m_diskDirText = new wxTextCtrl(page, wxID_ANY, wxString::FromUTF8(m_dataFolderPath),
        wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    m_openFolderBtn = new wxButton(page, ID_OPEN_DATA_FOLDER, "Open Folder");

    // Show data directory path with a copyable field and an Open Folder button
    content->Add(m_diskDirLabel, 0, wxBOTTOM, 2);
    wxBoxSizer* dataDirSizer = new wxBoxSizer(wxHORIZONTAL);
    dataDirSizer->Add(m_diskDirText, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    dataDirSizer->Add(m_openFolderBtn, 0);
    content->Add(dataDirSizer, 0, wxEXPAND | wxBOTTOM, 8);

    // Catalog list
    m_catalogList = new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, 250),
                                    wxLC_REPORT | wxLC_SINGLE_SEL);
    m_catalogList->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 220);
    m_catalogList->InsertColumn(1, "Description", wxLIST_FORMAT_LEFT, 450);
    m_catalogList->InsertColumn(2, "Status", wxLIST_FORMAT_LEFT, 120);
    content->Add(m_catalogList, 1, wxEXPAND | wxBOTTOM, 8);

    // Catalog action buttons and progress. All three are only ever touched by
    // handlers on this page, so unlike m_statusText they can live here.
    m_downloadBtn = new wxButton(page, ID_DOWNLOAD_DISK, "Download");
    m_deleteBtn = new wxButton(page, ID_DELETE_DISK, "Delete");
    m_progressBar = new wxGauge(page, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 20));
    wxBoxSizer* actionSizer = new wxBoxSizer(wxHORIZONTAL);
    actionSizer->Add(m_downloadBtn, 0, wxRIGHT, 5);
    actionSizer->Add(m_deleteBtn, 0, wxRIGHT, 15);
    actionSizer->Add(m_progressBar, 1, wxALIGN_CENTER_VERTICAL);
    content->Add(actionSizer, 0, wxEXPAND | wxBOTTOM, 10);

    // Warn on manifest writes. It is here rather than on Machine because the
    // disks it warns about are the ones downloaded on this page.
    m_warnManifestCheck = new wxCheckBox(page, wxID_ANY, "Warn on Downloaded Disk Writes");
    content->Add(m_warnManifestCheck, 0);

    wxBoxSizer* pageSizer = new wxBoxSizer(wxVERTICAL);
    pageSizer->Add(content, 1, wxEXPAND | wxALL, 15);
    page->SetSizer(pageSizer);
}

void SettingsDialogWx::layoutControls() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // The notebook takes all the vertical slack so the catalog list still grows
    // when the dialog is resized. The status line and the buttons below it are
    // fixed height and stay on screen whichever page is selected.
    mainSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 10);
    mainSizer->Add(m_statusText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

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
    } else {
        m_romChoice->SetSelection(0);
    }

    // Disk selections
    for (int i = 0; i < 4; i++) {
        if (!m_settings.diskFiles[i].empty()) {
            int idx = m_diskChoices[i]->FindString(wxString::FromUTF8(m_settings.diskFiles[i]));
            if (idx != wxNOT_FOUND) {
                m_diskChoices[i]->SetSelection(idx);
            }
        }
    }

    // Debug mode
    m_debugCheck->SetValue(m_settings.debugMode);

    // Warn on manifest writes
    m_warnManifestCheck->SetValue(m_settings.warnManifestWrites);

    // Terminal scrollback
    m_scrollbackSpin->SetValue(m_settings.scrollbackLines);

    // Dazzler settings
    m_dazzlerEnabledCheck->SetValue(m_settings.dazzlerEnabled);
    m_dazzlerPortSpin->SetValue(m_settings.dazzlerPort);
    m_dazzlerScaleSpin->SetValue(m_settings.dazzlerScale);

    // Enable/disable Dazzler controls based on enabled state
    bool dazzlerEnabled = m_settings.dazzlerEnabled;
    m_dazzlerPortLabel->Enable(dazzlerEnabled);
    m_dazzlerPortSpin->Enable(dazzlerEnabled);
    m_dazzlerScaleLabel->Enable(dazzlerEnabled);
    m_dazzlerScaleSpin->Enable(dazzlerEnabled);
}

void SettingsDialogWx::saveSettings() {
    // ROM selection
    switch (m_romChoice->GetSelection()) {
        case 1: m_settings.romFile = "emu_romwbw.rom"; break;
        default: m_settings.romFile = "emu_avw.rom"; break;
    }

    // Disk selections
    for (int i = 0; i < 4; i++) {
        int sel = m_diskChoices[i]->GetSelection();
        if (sel > 0) {
            m_settings.diskFiles[i] = m_diskChoices[i]->GetString(sel).ToStdString();
        } else {
            m_settings.diskFiles[i] = "";
        }
    }

    // Debug mode
    m_settings.debugMode = m_debugCheck->GetValue();

    // Warn on manifest writes
    m_settings.warnManifestWrites = m_warnManifestCheck->GetValue();

    // Terminal scrollback
    m_settings.scrollbackLines = m_scrollbackSpin->GetValue();

    // Dazzler settings
    m_settings.dazzlerEnabled = m_dazzlerEnabledCheck->GetValue();
    m_settings.dazzlerPort = m_dazzlerPortSpin->GetValue();
    m_settings.dazzlerScale = m_dazzlerScaleSpin->GetValue();
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

void SettingsDialogWx::onDazzlerEnabledChanged(wxCommandEvent& event) {
    bool enabled = m_dazzlerEnabledCheck->GetValue();
    m_dazzlerPortLabel->Enable(enabled);
    m_dazzlerPortSpin->Enable(enabled);
    m_dazzlerScaleLabel->Enable(enabled);
    m_dazzlerScaleSpin->Enable(enabled);
}

void SettingsDialogWx::onClearBootConfig(wxCommandEvent& event) {
    m_settings.clearBootConfigRequested = true;
    wxMessageBox("Boot configuration will be cleared when you click OK.\n\n"
                 "Use 'W' at the boot menu to configure autoboot.",
                 "Clear Boot Config", wxOK | wxICON_INFORMATION, this);
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

void SettingsDialogWx::onOpenDataFolder(wxCommandEvent& event) {
    if (m_dataFolderPath.empty()) return;
    // Open the folder in Explorer. The folder is created on demand by the
    // resolver, so it should already exist.
    wxString path = wxString::FromUTF8(m_dataFolderPath);
    ShellExecuteW(nullptr, L"open", path.wc_str(), nullptr, nullptr, SW_SHOWNORMAL);
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

// Points at the currently-open Settings dialog, or nullptr when none is open.
// UI-thread only (all Settings access goes through the Win32 menu handler), so
// a plain static needs no synchronization. Used to keep Settings a singleton.
static SettingsDialogWx* s_openSettingsDialog = nullptr;

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

    // Settings is a singleton: if a dialog is already open, bring the existing
    // one to the front instead of stacking a second instance. The wx dialog is
    // parented to nullptr, so ShowModal() only blocks other wx windows - the
    // Win32 main window's Settings menu stays live inside ShowModal()'s nested
    // message loop. Without this guard, re-selecting Settings opens duplicate
    // dialogs that each seed from / write back to config independently and
    // drift out of sync (last one closed wins).
    if (s_openSettingsDialog != nullptr) {
        OutputDebugStringA("[Settings] Already open - raising existing dialog\n");
        s_openSettingsDialog->Raise();
        s_openSettingsDialog->SetFocus();
        return false;  // no new dialog; leave the caller's settings untouched
    }

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
        s_openSettingsDialog = &dlg;
        result = (dlg.ShowModal() == wxID_OK);
        s_openSettingsDialog = nullptr;
        OutputDebugStringA("[Settings] Dialog closed\n");
        if (result) {
            settings = dlg.getSettings();
        }
    }
    catch (const std::exception& e) {
        s_openSettingsDialog = nullptr;
        OutputDebugStringA("[Settings] Exception: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        MessageBoxA(nullptr, e.what(), "Settings Exception", MB_OK | MB_ICONERROR);
    }
    catch (...) {
        s_openSettingsDialog = nullptr;
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
        // A structured exception may have escaped ShowModal() while the stack
        // dialog was still registered; clear the singleton so Settings can be
        // reopened afterward instead of being permanently "already open".
        s_openSettingsDialog = nullptr;
        DWORD code = GetExceptionCode();
        char msg[256];
        sprintf_s(msg, "Settings dialog crashed with exception code 0x%08X", code);
        OutputDebugStringA(msg);
        OutputDebugStringA("\n");
        MessageBoxA(nullptr, msg, "Settings Crash", MB_OK | MB_ICONERROR);
        return false;
    }
}
