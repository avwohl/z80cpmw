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
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <algorithm>

#include "DiskCatalog.h"

// The gate the DiskCatalog workers this dialog starts must pass before they
// may touch it. It used to be defined here, as SettingsDialogPostGate;
// MainWindow now needs the same thing for the same reason, so it lives beside
// the callback contract that creates the need - see WorkerPostGate in
// DiskCatalog.h for what it is holding shut, why a weak_ptr to the dialog
// would not have, and what it measured.

// Where a dialog should sit, and how small the user may make it, on a display
// with a given work area.
struct WxDialogPlacement {
    int width, height;        // what to open at
    int minWidth, minHeight;  // the floor to allow it to be dragged down to
    int x, y;                 // top-left corner
};

// Pure arithmetic, and deliberately taking the work area as an argument instead
// of asking wxDisplay for it: the case this exists for is a display nobody here
// owns. It is what the Settings dialog's fitted size is put through - see the
// sizing block in SettingsDialogWx's constructor for what each argument is
// measured from, and for what handing the fitted height straight to SetMinSize
// used to cost.
//
// Two properties, and they are the whole point:
//  - nothing it returns is taller or wider than the work area, INCLUDING the
//    floor. A floor a screen cannot satisfy is the same bug with a smaller
//    number in it;
//  - the position centres the window inside the WORK area, so that a window as
//    tall as the work area - which the clamp can now produce - keeps its caption
//    on screen where it can be dragged, and keeps its bottom edge off the
//    taskbar.
inline WxDialogPlacement placeDialogInWorkArea(int preferredWidth, int preferredMinWidth,
                                               int fittedHeight, int furniture,
                                               int minPage, const wxRect& work) {
    WxDialogPlacement p;
    p.width = std::min(preferredWidth, work.GetWidth());
    p.height = std::min(fittedHeight, work.GetHeight());
    p.minWidth = std::min(preferredMinWidth, work.GetWidth());
    p.minHeight = std::min(furniture + minPage, work.GetHeight());
    p.x = work.x + (work.GetWidth() - p.width) / 2;
    p.y = work.y + (work.GetHeight() - p.height) / 2;
    return p;
}

// Settings structure
struct WxEmulatorSettings {
    std::string romFile;
    std::string diskFiles[4];

    // Which RomWBW release the disk catalog is fetched for, e.g. "3.5.1". In
    // and back out; empty means "no preference", which is what a configuration
    // written before this release says and what makes the catalog take the
    // index's own default entry.
    //
    // It comes back UNCHANGED when the dialog could not offer a list - a
    // machine with no network never gets an index, so the choice control holds
    // one placeholder - and that is the point. A control that could not be
    // populated must not be allowed to write an emptier value than it was
    // given; the same rule the four disk dropdowns learned the hard way.
    std::string romwbwVersion;

    // The RomWBW release the ROM in the banks declares, e.g. "3.5.1", or empty
    // when there is no ROM or its HCB cannot be read. IN ONLY - the dialog
    // displays it and never writes it. It is what lets the Disk Images page say
    // that disks built for another release do not match the ROM in the banks -
    // and, since the release's own ROM is now fetched from the same catalog as
    // its disks, what the next Start will offer to do about it.
    std::string loadedRomwbwRelease;
    bool debugMode = false;
    bool warnManifestWrites = true;         // Warn when writing to downloaded catalog disks
    bool clearBootConfigRequested = false;  // Set when user clicks "Clear Boot Config"
    int scrollbackLines = 1000;             // Terminal scrollback history capacity (lines)
    bool bellEnabled = true;                // Whether BEL (0x07) sounds

    // The whole "keyboard.keys" object, carried in and back out. The dialog
    // does a read-modify-write of it, so what comes back includes every entry
    // that went in, including names the dialog could not read - see
    // SettingsDialogWx::rebuildKeyRows().
    //
    // keyBindingsDirty is the gate on writing it back at all. Without it, every
    // OK would overwrite "keyboard.keys" whether or not the user opened the
    // Keyboard page, which turns any defect in the round trip into a lost
    // config rather than a wrong dialog.
    std::map<std::string, std::string> keyBindings;
    bool keyBindingsDirty = false;

    // KeyboardConfig's three app-shortcut switches, stored in the config's own
    // sense: true means the key is released to CP/M. The checkboxes on screen
    // read the other way round - see loadSettings().
    bool f1ToCpm = false;
    bool f5ToCpm = false;
    bool ctrlRToCpm = true;

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
    void buildKeyboardPage();
    void buildDiskImagesPage();
    void populateROMList();
    // The RomWBW releases the catalog offers that this build's core can boot,
    // and the sentence underneath saying what the selected one means for the
    // ROM in the banks. Both are refilled whenever a catalog lands, because
    // until one does there is no list to show.
    void populateVersionList();
    void updateRomwbwVersionNote();
    void populateDiskLists();
    void populateCatalog();
    void loadSettings();
    // The disk dropdown selections alone. Split out of loadSettings() because
    // onCatalogLoaded has to reapply them after populateDiskLists() has emptied
    // and refilled the dropdowns, and it used to do that by calling
    // loadSettings() whole - which also reset every other control on the dialog
    // from m_settings. The catalog is fetched on a worker thread that posts
    // ID_CATALOG_LOADED back, so onCatalogLoaded runs seconds after the dialog
    // opened and anything the user had ticked in the meantime was silently put
    // back. Argued from the code rather than measured on screen: the fetch goes
    // over the network and this machine's catalog is already cached, so the
    // race is hard to stage on purpose.
    void loadDiskSelections();
    // Refill the four dropdowns and put back what was CHOSEN in them, which is
    // not the same thing as what m_settings arrived holding - the user may have
    // picked a disk before pressing Download. Every caller that repopulates the
    // dropdowns has to go through this, because "(None)" is not a neutral state
    // to be left in: saveSettings() writes "" for it and MainWindow reads "" as
    // "close this unit and forget it".
    void repopulateDiskLists();
    void saveSettings();

    // Keyboard page. m_keyRows is the model; the wxListCtrl is a view of it,
    // one row per entry in the same order.
    void rebuildKeyRows();
    void populateKeyList();
    void refreshKeyRow(long row);
    void updateKeyEditor();
    void applyKeySequenceToRow(const std::string& sequence);
    void commitPendingKeySequence();
    // The one writer of m_keySeqValid, because BECOMING VALID IS WHAT RETRACTS
    // onOK()'s refusal from the shared status line, and the two must not be able
    // to come apart. Four paths make the box valid again - a corrected sequence,
    // Default, Unbind, and every selection change, which goes through
    // updateKeyEditor() - and rebuildKeyRows() resets it with the model. When
    // the retraction lived in the first of those alone, clicking Default or
    // picking another row left the status line insisting the sequence could not
    // be used after the box holding it had been emptied.
    void setKeySeqValid(bool valid);
    struct KeyRow;
    static wxString keyRowStatus(const KeyRow& row,
                                 const std::map<unsigned, std::string>& defaults);

    // Event handlers
    void onBrowseDisk(wxCommandEvent& event);
    void onNewDisk(wxCommandEvent& event);
    void onDazzlerEnabledChanged(wxCommandEvent& event);
    void onClearBootConfig(wxCommandEvent& event);
    void onRefreshCatalog(wxCommandEvent& event);
    void onRomwbwVersionChanged(wxCommandEvent& event);
    void onDownloadDisk(wxCommandEvent& event);
    void onDeleteDisk(wxCommandEvent& event);
    void onOpenDataFolder(wxCommandEvent& event);
    void onCatalogLoaded(wxCommandEvent& event);
    void onDownloadProgress(wxCommandEvent& event);
    void onDownloadComplete(wxCommandEvent& event);
    void onKeySelected(wxListEvent& event);
    void onKeySequenceChanged(wxCommandEvent& event);
    void onKeyRestoreDefault(wxCommandEvent& event);
    void onKeyUnbind(wxCommandEvent& event);
    void onOK(wxCommandEvent& event);
    void onCancel(wxCommandEvent& event);

    DiskCatalog* m_catalog;
    WxEmulatorSettings m_settings;

    // Handed by value into every DiskCatalog callback this dialog starts, and
    // closed by ~SettingsDialogWx. Never null: it is built in the constructor's
    // member-init list, which runs before the body's catalog refresh. See
    // WorkerPostGate in DiskCatalog.h for what it is holding shut and why a
    // weak_ptr to the dialog would not have.
    std::shared_ptr<WorkerPostGate> m_postGate;

    // Notebook and its pages. Every control below except m_statusText and the
    // OK/Cancel buttons is parented to one of these panels, not to the dialog.
    wxNotebook* m_notebook;
    wxPanel* m_machinePage;
    wxPanel* m_terminalPage;
    wxPanel* m_keyboardPage;
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
    wxCheckBox* m_bellCheck;

    // Keyboard page controls.
    wxListCtrl* m_keyList;
    wxTextCtrl* m_keySeqText;
    wxStaticText* m_keyHintText;
    wxButton* m_keyDefaultBtn;
    wxButton* m_keyUnbindBtn;
    // Worded as what the APP gets, which is the opposite sense from the config
    // members they load from and save to.
    wxCheckBox* m_f1HelpCheck;
    wxCheckBox* m_f5StartStopCheck;
    wxCheckBox* m_ctrlRResetCheck;

    // One line of the Keyboard page's list, keyed by RESOLVED ID rather than by
    // name, because "Ctrl+Left", "ctrl+left" and "Control+Left" are three
    // spellings of one binding - the same rule ConfigManager::load()'s fill
    // loop matches by.
    struct KeyRow {
        unsigned id = 0;
        std::string name;        // the spelling this row is written back under
        std::string sequence;    // termcap-style, exactly as it appears in the file
        bool hasEntry = false;   // "keys" carried an entry for this id
        bool edited = false;     // the user changed it while the dialog was open
        const char* purpose = nullptr;  // reservedKeys() words, or null if bindable
    };
    std::vector<KeyRow> m_keyRows;

    // Entries of "keys" that no row owns, kept verbatim so that a
    // read-modify-write cannot delete them: names this build cannot resolve
    // (a typo, or a key a later version binds), and the losing spelling where
    // the file holds two names for one binding.
    std::map<std::string, std::string> m_keyCarry;

    // Built-in default sequence per resolved id, from keymap::defaultBindings().
    std::map<unsigned, std::string> m_keyDefaults;

    long m_keySelectedRow = -1;
    bool m_keysDirty = false;   // gates the write-back; see WxEmulatorSettings
    // False while the text field holds something validateSequence() refuses.
    // onOK() consults it, because a sequence that is never stored still has to
    // stop the dialog closing as though it had been accepted.
    //
    // Written only by setKeySeqValid(), never assigned directly: the status line
    // has to be retracted wherever this goes back to true.
    bool m_keySeqValid = true;

    // Dazzler controls
    wxCheckBox* m_dazzlerEnabledCheck;
    wxSpinCtrl* m_dazzlerPortSpin;
    wxSpinCtrl* m_dazzlerScaleSpin;
    wxStaticText* m_dazzlerPortLabel;
    wxStaticText* m_dazzlerScaleLabel;
    wxChoice* m_romwbwVersionChoice;
    // choice index -> the `romwbw_version` string that index means, e.g.
    // "3.5.1". Kept beside the control because what the control DISPLAYS is
    // catalogv0::displayLabel() - "RomWBW 3.6.0 (preview)" - and the label is
    // documentation the index may reword at any time, where the version string
    // is the key the preference is stored under. Empty for the placeholder row
    // shown before any catalog has been fetched.
    std::vector<std::string> m_romwbwVersionIds;
    wxStaticText* m_romwbwVersionNote;

    // Row -> the ROM FILENAME that row stands for, kept beside m_romChoice for
    // the same reason as the list above: what the control displays is a label
    // ("EMU AVW (Default)") and what the configuration stores is a filename.
    // The two packaged ROMs plus, when the machine is running one, the catalog
    // ROM for the RomWBW release it is set to - appended by loadSettings() so
    // that OK writes back the ROM in the banks instead of replacing it with the
    // first entry.
    std::vector<std::string> m_romFileIds;

    wxListCtrl* m_catalogList;
    // Row -> the catalog FILENAME that row is about, filled by populateCatalog.
    //
    // The two handlers used to take the filename out of the list control's
    // column 0, which quietly made a display column part of the catalog API:
    // showing anything friendlier there - the entry's `name`, say, now that a
    // v0 filename is 23 characters of hd1k_combo-v0-3.5.1.img - would have sent
    // "Combo (Recommended)" to downloadDisk(). The text on screen and the key
    // the API is given are now two different things on purpose.
    std::vector<std::string> m_catalogRowFilenames;
    wxButton* m_refreshBtn;
    wxButton* m_downloadBtn;
    wxButton* m_deleteBtn;
    wxGauge* m_progressBar;
    // Deliberately parented to the dialog, below the notebook, and NOT to any
    // page: its writers do not all live on one page - see createControls().
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
        ID_ROMWBW_VERSION,
        ID_DOWNLOAD_DISK,
        ID_DELETE_DISK,
        ID_CATALOG_LOADED,
        ID_DOWNLOAD_PROGRESS,
        ID_DOWNLOAD_COMPLETE,
        ID_OPEN_DATA_FOLDER,
        ID_KEY_LIST,
        ID_KEY_SEQUENCE,
        ID_KEY_DEFAULT,
        ID_KEY_UNBIND
    };

    wxDECLARE_EVENT_TABLE();
};

// Helper function to show the dialog from Win32 code
bool ShowWxSettingsDialog(void* parentHwnd, DiskCatalog* catalog, WxEmulatorSettings& settings);
