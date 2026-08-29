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

class DiskCatalog;
struct DiskEntry;

// The permission a DiskCatalog worker thread needs before it may touch the
// Settings dialog, and the thing ~SettingsDialogWx revokes to shut those
// workers out for good.
//
// WHY THIS EXISTS. DiskCatalog::fetchCatalog and DiskCatalog::downloadDisk both
// run their completion callbacks on a DETACHED std::thread, and the dialog is a
// stack object - ShowWxSettingsDialogInternal's "SettingsDialogWx dlg(nullptr,
// catalog)" - destroyed the moment ShowModal() returns. The constructor starts
// a catalog fetch (see the "Constructor: starting catalog refresh" trace), so
// closing Settings before the download landed left the worker calling
// wxPostEvent on a freed dialog. Measured, not deduced: every dump it produced
// was 0xC0000005 at the same address, z80cpmw.exe+0x5ABF3, reading garbage
// (0xFFFFFFFFFFFFFFFF, 0x40, 0x14DBC427666D) with the faulting stack running
// thread trampoline -> fetchCatalog's worker -> the std::function call. Driven
// with WM_COMMAND 2004 and a WM_CLOSE some milliseconds later, twelve cycles
// per run, the pre-fix binary crashed at EVERY delay tried - iteration 6 at
// 150ms, iteration 3 at 400ms, and at 0ms and 800ms as well. The delay only
// moves the odds, never the bug, because the window is "a fetch is still in
// flight" and how long that lasts is the network's business: curl pulls
// disks.xml in about half a second, but the in-app fetch - which follows
// GitHub's redirect and then stats all twenty entries in
// updateDownloadedStatus - was measured still running six seconds after the
// dialog opened. Note also that a crash here need not exit the process:
// CrashHandler's report thread puts up a modal message box before it
// terminates, and two of these runs sat on it, so "still running" is not the
// test - a new .dmp is.
//
// WHY A weak_ptr TO THE DIALOG IS NOT ENOUGH, which is the whole reason this is
// a mutex and not an atomic flag. The obvious fix - hand the worker a
// weak_ptr, lock it, and post if the lock succeeds - leaves the crash in place
// with a smaller window: between "lock succeeded" and "wxPostEvent ran", the UI
// thread can return from ShowModal() and run the destructor. The shared_ptr the
// worker is holding keeps the CONTROL BLOCK alive, not the wxDialog, and it is
// the wxDialog that wxPostEvent dereferences. Nothing in that scheme ever makes
// the destructor and the post exclude each other.
//
// WHICH WINDOW THIS CLOSES, AND HOW. postIfOpen() holds m_mutex ACROSS the post
// and close() takes the same mutex, so the two can never overlap. Once close()
// has returned, every later postIfOpen() finds m_open false and does nothing;
// and a post that had already begun completed before close() could acquire the
// lock, i.e. while the dialog was still whole. There is no third state, so
// "the destructor has returned" and "a worker may still post" are now mutually
// exclusive facts rather than a race with better odds. With the gate in, the
// same driver ran 0/50/150/250/400/600/800ms with repeats - 144 open-close
// cycles - and wrote no dump at all, while the Settings dialog left open still
// reaches "Catalog loaded" with the catalog's twenty entries in the list.
//
// LIFETIME. The gate outlives the dialog because the callbacks hold shared_ptr
// copies of it and the dialog holds one more; the last callback to be destroyed
// frees it.
//
// HOW LONG close() CAN BLOCK THE UI THREAD. Only for one wxPostEvent, which
// wx/event.h defines as dest->AddPendingEvent(event) -> QueueEvent(Clone()):
// one heap clone appended to the handler's pending list, no I/O, no dialog
// code, no callback of ours. It does NOT wait for the download: a fetch with
// minutes left to run delays closing Settings by nothing. That is deliberate,
// and it is why the fix is not a cancel - see the comment on
// DiskCatalog::cancelDownload for the rest of that argument.
//
// The calls stayed wxPostEvent rather than becoming wxQueueEvent, and that was
// checked rather than assumed. wx/event.h says wxPostEvent is "not thread-safe,
// use wxQueueEvent()", and names the reason: Clone() shallow-copies wxString
// members, so a refcounted string buffer would end up shared between the worker
// that posts and the main thread that handles - and both of these events carry
// a wxString. It does not bite this build: wx/string.h typedefs wxStringImpl to
// wxStdString to std::wstring unconditionally ("All the symbols here only exist
// for compatibility"), and the MSVC std::wstring copy constructor deep-copies.
// wxQueueEvent would mean a heap event whose ownership the gate has to hand
// back and delete on the refused path - a second way to get lifetime wrong in
// the code whose whole job is to get lifetime right. If wxString ever goes back
// to copy-on-write under this toolchain, that trade flips.
//
// The callable handed to postIfOpen must therefore stay that cheap: it runs
// under the gate lock while the UI thread may be waiting in close(), so it must
// not call back into the dialog or block on the UI thread. Lock ordering is
// one-way for the same reason - a worker takes this mutex and then wx's
// pending-event locks, while the UI thread holds only this one inside close()
// and has released it long before ~wxEvtHandler goes near wx's.
class SettingsDialogPostGate {
public:
    // UI thread, from ~SettingsDialogWx and nowhere else.
    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = false;
    }

    // Worker thread. 'post' runs only while the dialog is provably alive, and
    // is not called at all once close() has returned.
    template <typename PostFn>
    void postIfOpen(PostFn&& post) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) return;
        post();
    }

private:
    std::mutex m_mutex;
    bool m_open = true;
};

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
    // SettingsDialogPostGate for what it is holding shut and why a weak_ptr to
    // the dialog would not have.
    std::shared_ptr<SettingsDialogPostGate> m_postGate;

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
    wxListCtrl* m_catalogList;
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
