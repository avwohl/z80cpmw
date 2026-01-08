/*
 * SettingsDialog.cpp - Settings and Disk Catalog Dialog Implementation
 */

#include "pch.h"
#include "SettingsDialog.h"
#include "DiskCatalog.h"
#include "EmulatorEngine.h"
#include <commctrl.h>

SettingsDialog::SettingsDialog(HWND parent, DiskCatalog* catalog)
    : m_parent(parent)
    , m_catalog(catalog)
{
}

SettingsDialog::~SettingsDialog() {
}

bool SettingsDialog::show() {
    // Create dialog from template in memory
    // This avoids needing a .rc file for now

    // Dialog template structure
    #pragma pack(push, 4)
    struct {
        DLGTEMPLATE dlg;
        WORD menu;
        WORD wndClass;
        WCHAR title[32];
    } dlgTemplate = {};
    #pragma pack(pop)

    dlgTemplate.dlg.style = DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    dlgTemplate.dlg.dwExtendedStyle = 0;
    dlgTemplate.dlg.cdit = 0;  // We'll create controls manually
    dlgTemplate.dlg.x = 0;
    dlgTemplate.dlg.y = 0;
    dlgTemplate.dlg.cx = 400;
    dlgTemplate.dlg.cy = 350;
    dlgTemplate.menu = 0;
    dlgTemplate.wndClass = 0;
    wcscpy_s(dlgTemplate.title, L"Settings");

    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandle(nullptr),
        &dlgTemplate.dlg,
        m_parent,
        DialogProc,
        reinterpret_cast<LPARAM>(this)
    );

    return result == IDOK;
}

INT_PTR CALLBACK SettingsDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* dialog = nullptr;

    if (msg == WM_INITDIALOG) {
        dialog = reinterpret_cast<SettingsDialog*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
    } else {
        dialog = reinterpret_cast<SettingsDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (dialog) {
        return dialog->handleMessage(hwnd, msg, wParam, lParam);
    }

    return FALSE;
}

INT_PTR SettingsDialog::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        onInitDialog(hwnd);
        return TRUE;

    case WM_COMMAND:
        onCommand(hwnd, LOWORD(wParam), HIWORD(wParam));
        return TRUE;

    case WM_NOTIFY:
        onNotify(hwnd, reinterpret_cast<NMHDR*>(lParam));
        return TRUE;

    case WM_TIMER:
        onTimer(hwnd);
        return TRUE;

    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;

    case WM_USER + 1:  // Download complete
        {
            bool success = (wParam != 0);
            m_downloadingFilename.clear();
            KillTimer(hwnd, IDT_DOWNLOAD);
            if (success) {
                SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Download complete");
                populateDiskCatalog(hwnd);
            } else {
                SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Download failed");
            }
            EnableWindow(GetDlgItem(hwnd, IDC_DOWNLOAD_BTN), TRUE);
        }
        return TRUE;

    case WM_USER + 2:  // Catalog loaded
        {
            bool success = (wParam != 0);
            m_catalogLoading = false;
            KillTimer(hwnd, IDT_DOWNLOAD + 1);
            EnableWindow(GetDlgItem(hwnd, IDC_REFRESH_BTN), TRUE);
            if (success) {
                populateDiskCatalog(hwnd);
                SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Catalog loaded");
            } else {
                SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Failed to load catalog - check network connection");
            }
        }
        return TRUE;
    }

    return FALSE;
}

void SettingsDialog::onInitDialog(HWND hwnd) {
    // Resize dialog to proper size - make it wider and taller
    int width = 750;
    int height = 650;
    SetWindowPos(hwnd, nullptr,
                 (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
                 width, height, SWP_NOZORDER);

    // Get client area for control layout
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;

    // Create controls manually - use generous spacing
    int y = 20;
    int labelWidth = 80;
    int leftMargin = 20;
    int rightMargin = 20;
    int rowHeight = 36;  // Taller rows for better readability
    int comboWidth = 220;
    int sliceLabelWidth = 55;
    int sliceEditWidth = 50;
    int btnWidth = 80;
    int btnHeight = 28;
    int ctrlHeight = 26;

    // ROM selection
    CreateWindowW(L"STATIC", L"ROM:", WS_CHILD | WS_VISIBLE,
                  leftMargin, y + 5, labelWidth, 22, hwnd, nullptr, nullptr, nullptr);
    CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                  leftMargin + labelWidth, y, 300, 200, hwnd,
                  (HMENU)IDC_ROM_COMBO, nullptr, nullptr);
    y += rowHeight;

    // Disk selections (4 units) with slice count
    for (int i = 0; i < 4; i++) {
        wchar_t label[32];
        swprintf_s(label, L"Disk %d:", i);
        int x = leftMargin;

        // Label
        CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE,
                      x, y + 5, labelWidth, 22, hwnd, nullptr, nullptr, nullptr);
        x += labelWidth;

        // Disk combo
        CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      x, y, comboWidth, 200, hwnd,
                      (HMENU)(IDC_DISK0_COMBO + i), nullptr, nullptr);
        x += comboWidth + 15;

        // Slices label
        CreateWindowW(L"STATIC", L"Slices:", WS_CHILD | WS_VISIBLE,
                      x, y + 5, sliceLabelWidth, 22, hwnd, nullptr, nullptr, nullptr);
        x += sliceLabelWidth;

        // Slice count spin control
        HWND editSlice = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"4",
                      WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
                      x, y, sliceEditWidth, ctrlHeight, hwnd,
                      (HMENU)(IDC_SLICE0_SPIN + i), nullptr, nullptr);
        HWND spinSlice = CreateWindowW(UPDOWN_CLASSW, nullptr,
                      WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
                      0, 0, 0, 0, hwnd, (HMENU)(IDC_SLICE0_SPIN + 100 + i), nullptr, nullptr);
        SendMessage(spinSlice, UDM_SETBUDDY, (WPARAM)editSlice, 0);
        SendMessage(spinSlice, UDM_SETRANGE32, 1, 8);
        SendMessage(spinSlice, UDM_SETPOS32, 0, m_settings.diskSlices[i]);
        x += sliceEditWidth + 15;

        // Browse button
        CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      x, y - 1, btnWidth, btnHeight, hwnd,
                      (HMENU)(IDC_DISK0_BROWSE + i), nullptr, nullptr);
        x += btnWidth + 10;

        // New button
        CreateWindowW(L"BUTTON", L"New", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      x, y - 1, 60, btnHeight, hwnd,
                      (HMENU)(IDC_DISK0_CREATE + i), nullptr, nullptr);

        y += rowHeight;
    }

    y += 15;

    // Boot string
    CreateWindowW(L"STATIC", L"Boot String:", WS_CHILD | WS_VISIBLE,
                  leftMargin, y + 5, labelWidth, 22, hwnd, nullptr, nullptr, nullptr);
    CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  leftMargin + labelWidth, y, 100, ctrlHeight, hwnd,
                  (HMENU)IDC_BOOT_STRING, nullptr, nullptr);
    CreateWindowW(L"STATIC", L"(empty=menu, 0=disk0, 0.2=slice2, C=ROM app)", WS_CHILD | WS_VISIBLE,
                  leftMargin + labelWidth + 115, y + 5, 320, 22, hwnd, nullptr, nullptr, nullptr);
    y += rowHeight;

    // Debug checkbox
    CreateWindowW(L"BUTTON", L"Enable Debug Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  leftMargin + labelWidth, y, 200, 26, hwnd,
                  (HMENU)IDC_DEBUG_CHECK, nullptr, nullptr);
    y += rowHeight + 15;

    // Separator
    CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                  leftMargin, y, clientWidth - leftMargin - rightMargin, 2, hwnd, nullptr, nullptr, nullptr);
    y += 20;

    // Disk catalog section
    CreateWindowW(L"STATIC", L"Download Disk Images:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y, 220, 22, hwnd, nullptr, nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  clientWidth - rightMargin - 90, y - 3, 90, btnHeight, hwnd,
                  (HMENU)IDC_REFRESH_BTN, nullptr, nullptr);
    y += 32;

    // Catalog list (ListView)
    int listHeight = 180;
    HWND listView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
                  WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                  leftMargin, y, clientWidth - leftMargin - rightMargin, listHeight, hwnd,
                  (HMENU)IDC_CATALOG_LIST, nullptr, nullptr);

    // Set up list view columns
    ListView_SetExtendedListViewStyle(listView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    int listWidth = clientWidth - leftMargin - rightMargin;
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)L"Name";
    col.cx = 180;
    ListView_InsertColumn(listView, 0, &col);
    col.pszText = (LPWSTR)L"Description";
    col.cx = listWidth - 180 - 100 - 20;  // Remaining space minus name and status
    ListView_InsertColumn(listView, 1, &col);
    col.pszText = (LPWSTR)L"Status";
    col.cx = 100;
    ListView_InsertColumn(listView, 2, &col);

    y += listHeight + 12;

    // Download/Delete buttons
    CreateWindowW(L"BUTTON", L"Download", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  leftMargin, y, 100, btnHeight, hwnd,
                  (HMENU)IDC_DOWNLOAD_BTN, nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  leftMargin + 115, y, 80, btnHeight, hwnd,
                  (HMENU)IDC_DELETE_BTN, nullptr, nullptr);

    // Progress bar
    CreateWindowW(PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE,
                  leftMargin + 210, y + 4, clientWidth - leftMargin - rightMargin - 210, 20, hwnd,
                  (HMENU)IDC_DOWNLOAD_PROGRESS, nullptr, nullptr);
    y += 36;

    // Status text
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                  leftMargin, y, clientWidth - leftMargin - rightMargin, 24, hwnd,
                  (HMENU)IDC_STATUS_TEXT, nullptr, nullptr);

    // OK/Cancel buttons at bottom right
    // Note: Cancel closes dialog without saving changes. Downloaded disks are NOT deleted.
    RECT dlgRect;
    GetClientRect(hwnd, &dlgRect);
    int okCancelY = dlgRect.bottom - 45;
    CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  clientWidth - 200, okCancelY, 90, 32, hwnd,
                  (HMENU)IDOK, nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  clientWidth - 100, okCancelY, 90, 32, hwnd,
                  (HMENU)IDCANCEL, nullptr, nullptr);

    // Set font for all controls
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
        SendMessage(child, WM_SETFONT, lParam, TRUE);
        return TRUE;
    }, (LPARAM)hFont);

    // Populate controls
    populateROMList(hwnd);
    populateDiskLists(hwnd);
    loadSettings(hwnd);

    // Fetch catalog
    onRefreshCatalog(hwnd);
}

void SettingsDialog::onCommand(HWND hwnd, int id, int notifyCode) {
    switch (id) {
    case IDOK:
        saveSettings(hwnd);
        if (m_onSettingsChanged) {
            m_onSettingsChanged(m_settings);
        }
        EndDialog(hwnd, IDOK);
        break;

    case IDCANCEL:
        EndDialog(hwnd, IDCANCEL);
        break;

    case IDC_DOWNLOAD_BTN:
        onDownloadDisk(hwnd);
        break;

    case IDC_DELETE_BTN:
        onDeleteDisk(hwnd);
        break;

    case IDC_REFRESH_BTN:
        onRefreshCatalog(hwnd);
        break;

    case IDC_DISK0_BROWSE:
    case IDC_DISK1_BROWSE:
    case IDC_DISK2_BROWSE:
    case IDC_DISK3_BROWSE:
        onBrowseDisk(hwnd, id - IDC_DISK0_BROWSE);
        break;

    case IDC_DISK0_CREATE:
    case IDC_DISK1_CREATE:
    case IDC_DISK2_CREATE:
    case IDC_DISK3_CREATE:
        onCreateDisk(hwnd, id - IDC_DISK0_CREATE);
        break;
    }
}

void SettingsDialog::onNotify(HWND hwnd, NMHDR* nmhdr) {
    if (nmhdr->idFrom == IDC_CATALOG_LIST) {
        if (nmhdr->code == LVN_ITEMCHANGED) {
            // Update button states based on selection
            HWND listView = GetDlgItem(hwnd, IDC_CATALOG_LIST);
            int sel = ListView_GetNextItem(listView, -1, LVNI_SELECTED);

            bool hasSelection = (sel >= 0);
            bool isDownloaded = false;

            if (hasSelection && sel < (int)m_catalog->getCatalogEntries().size()) {
                isDownloaded = m_catalog->getCatalogEntries()[sel].isDownloaded;
            }

            EnableWindow(GetDlgItem(hwnd, IDC_DOWNLOAD_BTN), hasSelection && !isDownloaded);
            EnableWindow(GetDlgItem(hwnd, IDC_DELETE_BTN), hasSelection && isDownloaded);
        }
    }
}

void SettingsDialog::onTimer(HWND hwnd) {
    updateDownloadStatus(hwnd);
}

void SettingsDialog::populateROMList(HWND hwnd) {
    HWND combo = GetDlgItem(hwnd, IDC_ROM_COMBO);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);

    // Find available ROM files
    std::string appDir = EmulatorEngine::getAppDirectory();
    std::vector<std::pair<std::string, std::string>> roms = {
        {"emu_avw.rom", "EMU AVW (Default)"},
        {"emu_romwbw.rom", "EMU RomWBW"},
        {"SBC_simh_std.rom", "SBC SIMH Standard"}
    };

    for (const auto& rom : roms) {
        std::string path = appDir + "\\roms\\" + rom.first;
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::wstring wname(rom.second.begin(), rom.second.end());
            int idx = (int)SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wname.c_str());
            // Store filename as item data
            SendMessageW(combo, CB_SETITEMDATA, idx, (LPARAM)_strdup(rom.first.c_str()));
        }
    }

    if (SendMessageW(combo, CB_GETCOUNT, 0, 0) > 0) {
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
    }
}

void SettingsDialog::populateDiskLists(HWND hwnd) {
    // Get list of available disk images
    std::vector<std::string> disks;
    disks.push_back("");  // Empty option

    // Add downloaded disks from catalog
    for (const auto& entry : m_catalog->getCatalogEntries()) {
        if (entry.isDownloaded) {
            disks.push_back(entry.filename);
        }
    }

    // Populate all 4 disk combos
    for (int i = 0; i < 4; i++) {
        HWND combo = GetDlgItem(hwnd, IDC_DISK0_COMBO + i);
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);

        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"(None)");

        for (size_t j = 1; j < disks.size(); j++) {
            std::wstring wname(disks[j].begin(), disks[j].end());
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wname.c_str());
        }

        SendMessageW(combo, CB_SETCURSEL, 0, 0);
    }
}

void SettingsDialog::populateDiskCatalog(HWND hwnd) {
    HWND listView = GetDlgItem(hwnd, IDC_CATALOG_LIST);
    ListView_DeleteAllItems(listView);

    const auto& entries = m_catalog->getCatalogEntries();
    for (size_t i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];

        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = (int)i;
        item.iSubItem = 0;
        std::wstring wname(entry.name.begin(), entry.name.end());
        item.pszText = (LPWSTR)wname.c_str();
        ListView_InsertItem(listView, &item);

        std::wstring wdesc(entry.description.begin(), entry.description.end());
        ListView_SetItemText(listView, (int)i, 1, (LPWSTR)wdesc.c_str());

        const wchar_t* status = entry.isDownloaded ? L"Downloaded" : L"Available";
        ListView_SetItemText(listView, (int)i, 2, (LPWSTR)status);
    }

    // Update disk selection combos with new available disks
    populateDiskLists(hwnd);
}

void SettingsDialog::updateDownloadStatus(HWND hwnd) {
    // This is called periodically during download
    // Update progress bar and status text
}

void SettingsDialog::onDownloadDisk(HWND hwnd) {
    HWND listView = GetDlgItem(hwnd, IDC_CATALOG_LIST);
    int sel = ListView_GetNextItem(listView, -1, LVNI_SELECTED);
    if (sel < 0) return;

    const auto& entries = m_catalog->getCatalogEntries();
    if (sel >= (int)entries.size()) return;

    const auto& entry = entries[sel];
    if (entry.isDownloaded) return;

    m_downloadingFilename = entry.filename;
    SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Downloading...");
    EnableWindow(GetDlgItem(hwnd, IDC_DOWNLOAD_BTN), FALSE);

    HWND progress = GetDlgItem(hwnd, IDC_DOWNLOAD_PROGRESS);
    SendMessage(progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(progress, PBM_SETPOS, 0, 0);

    // Start download
    m_catalog->downloadDisk(
        entry.filename,
        [hwnd, progress](size_t downloaded, size_t total) {
            if (total > 0) {
                int percent = (int)((downloaded * 100) / total);
                PostMessage(progress, PBM_SETPOS, percent, 0);
            }
        },
        [this, hwnd](bool success, const std::string& error) {
            // Must post message to update UI from callback thread
            PostMessage(hwnd, WM_USER + 1, success ? 1 : 0, 0);
        }
    );

    // Set timer to check for completion
    SetTimer(hwnd, IDT_DOWNLOAD, 100, nullptr);
}

void SettingsDialog::onDeleteDisk(HWND hwnd) {
    HWND listView = GetDlgItem(hwnd, IDC_CATALOG_LIST);
    int sel = ListView_GetNextItem(listView, -1, LVNI_SELECTED);
    if (sel < 0) return;

    const auto& entries = m_catalog->getCatalogEntries();
    if (sel >= (int)entries.size()) return;

    const auto& entry = entries[sel];
    if (!entry.isDownloaded) return;

    // Confirm deletion
    std::wstring msg = L"Delete downloaded disk image:\n";
    msg += std::wstring(entry.name.begin(), entry.name.end());
    msg += L"?";

    if (MessageBoxW(hwnd, msg.c_str(), L"Confirm Delete", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        if (m_catalog->deleteDownloadedDisk(entry.filename)) {
            populateDiskCatalog(hwnd);
            SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Disk image deleted");
        } else {
            SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Failed to delete disk image");
        }
    }
}

void SettingsDialog::onRefreshCatalog(HWND hwnd) {
    if (m_catalogLoading) return;

    m_catalogLoading = true;
    SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Loading disk catalog...");
    EnableWindow(GetDlgItem(hwnd, IDC_REFRESH_BTN), FALSE);

    m_catalog->fetchCatalog([this, hwnd](bool success, const std::vector<DiskEntry>& entries, const std::string& error) {
        // Post message to update UI from callback thread
        PostMessage(hwnd, WM_USER + 2, success ? 1 : 0, 0);
    });

    // Handle completion in timer
    SetTimer(hwnd, IDT_DOWNLOAD + 1, 100, [](HWND hwnd, UINT, UINT_PTR id, DWORD) {
        SettingsDialog* dlg = reinterpret_cast<SettingsDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (dlg && !dlg->m_catalogLoading) {
            KillTimer(hwnd, id);
        }
    });
}

void SettingsDialog::onBrowseDisk(HWND hwnd, int unit) {
    wchar_t filename[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Disk Images (*.img)\0*.img\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"Select Disk Image";

    if (GetOpenFileNameW(&ofn)) {
        // Add to combo and select
        HWND combo = GetDlgItem(hwnd, IDC_DISK0_COMBO + unit);
        int idx = (int)SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)filename);
        SendMessageW(combo, CB_SETCURSEL, idx, 0);
    }
}

void SettingsDialog::onCreateDisk(HWND hwnd, int unit) {
    wchar_t filename[MAX_PATH] = {};
    swprintf_s(filename, L"newdisk%d.img", unit);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Disk Images (*.img)\0*.img\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = L"Create New Disk Image";
    ofn.lpstrDefExt = L"img";

    if (GetSaveFileNameW(&ofn)) {
        // Create 8MB disk filled with 0xE5 (CP/M empty format)
        FILE* f = _wfopen(filename, L"wb");
        if (f) {
            const size_t diskSize = 8 * 1024 * 1024;  // 8MB
            std::vector<uint8_t> buffer(65536, 0xE5);
            size_t written = 0;
            while (written < diskSize) {
                size_t toWrite = (diskSize - written < buffer.size()) ? (diskSize - written) : buffer.size();
                fwrite(buffer.data(), 1, toWrite, f);
                written += toWrite;
            }
            fclose(f);

            // Add to combo and select
            HWND combo = GetDlgItem(hwnd, IDC_DISK0_COMBO + unit);
            int idx = (int)SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)filename);
            SendMessageW(combo, CB_SETCURSEL, idx, 0);

            SetDlgItemTextW(hwnd, IDC_STATUS_TEXT, L"Created new 8MB disk image");
        } else {
            MessageBoxW(hwnd, L"Failed to create disk image", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void SettingsDialog::saveSettings(HWND hwnd) {
    // ROM
    HWND romCombo = GetDlgItem(hwnd, IDC_ROM_COMBO);
    int romSel = (int)SendMessageW(romCombo, CB_GETCURSEL, 0, 0);
    if (romSel >= 0) {
        char* romFile = (char*)SendMessageW(romCombo, CB_GETITEMDATA, romSel, 0);
        if (romFile) {
            m_settings.romFile = romFile;
        }
    }

    // Disks and slice counts
    for (int i = 0; i < 4; i++) {
        HWND combo = GetDlgItem(hwnd, IDC_DISK0_COMBO + i);
        int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
        if (sel > 0) {  // 0 is "(None)"
            wchar_t text[MAX_PATH];
            SendMessageW(combo, CB_GETLBTEXT, sel, (LPARAM)text);
            char narrow[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, text, -1, narrow, MAX_PATH, nullptr, nullptr);
            m_settings.diskFiles[i] = narrow;
        } else {
            m_settings.diskFiles[i] = "";
        }

        // Get slice count from edit control
        wchar_t sliceStr[16];
        GetDlgItemTextW(hwnd, IDC_SLICE0_SPIN + i, sliceStr, 16);
        int slices = _wtoi(sliceStr);
        if (slices < 1) slices = 1;
        if (slices > 8) slices = 8;
        m_settings.diskSlices[i] = slices;
    }

    // Boot string
    wchar_t bootStr[256];
    GetDlgItemTextW(hwnd, IDC_BOOT_STRING, bootStr, 256);
    char narrowBoot[256];
    WideCharToMultiByte(CP_UTF8, 0, bootStr, -1, narrowBoot, 256, nullptr, nullptr);
    m_settings.bootString = narrowBoot;

    // Debug mode
    m_settings.debugMode = (SendDlgItemMessage(hwnd, IDC_DEBUG_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

void SettingsDialog::loadSettings(HWND hwnd) {
    // ROM - find and select matching entry
    HWND romCombo = GetDlgItem(hwnd, IDC_ROM_COMBO);
    int count = (int)SendMessageW(romCombo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; i++) {
        char* romFile = (char*)SendMessageW(romCombo, CB_GETITEMDATA, i, 0);
        if (romFile && m_settings.romFile == romFile) {
            SendMessageW(romCombo, CB_SETCURSEL, i, 0);
            break;
        }
    }

    // Disk slice counts - update spin controls
    for (int i = 0; i < 4; i++) {
        wchar_t sliceStr[16];
        swprintf_s(sliceStr, L"%d", m_settings.diskSlices[i]);
        SetDlgItemTextW(hwnd, IDC_SLICE0_SPIN + i, sliceStr);
    }

    // Boot string
    std::wstring wboot(m_settings.bootString.begin(), m_settings.bootString.end());
    SetDlgItemTextW(hwnd, IDC_BOOT_STRING, wboot.c_str());

    // Debug mode
    SendDlgItemMessage(hwnd, IDC_DEBUG_CHECK, BM_SETCHECK,
                       m_settings.debugMode ? BST_CHECKED : BST_UNCHECKED, 0);
}
