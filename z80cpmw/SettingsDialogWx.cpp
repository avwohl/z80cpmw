/*
 * SettingsDialogWx.cpp - wxWidgets-based Settings Dialog Implementation
 */

#include "pch.h"
#include "SettingsDialogWx.h"
#include "DiskCatalog.h"
#include "Keymap.h"
// diskv0::idOfV0Name and diskv0::releaseOfV0Name: the release switch reads a
// slot filename when no catalog in hand can identify it.
#include "DiskMigrationV0.h"
#include <wx/statline.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/settings.h>
#include <wx/display.h>
#include <algorithm>
#include <set>

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
    EVT_CHOICE(ID_ROMWBW_VERSION, SettingsDialogWx::onRomwbwVersionChanged)
    EVT_CHECKBOX(ID_SHOW_PRERELEASE, SettingsDialogWx::onShowPrereleaseChanged)
    EVT_TEXT(ID_CATALOG_INDEX_URL, SettingsDialogWx::onCatalogIndexUrlChanged)
    EVT_BUTTON(ID_DOWNLOAD_DISK, SettingsDialogWx::onDownloadDisk)
    EVT_BUTTON(ID_UPDATE_DISK, SettingsDialogWx::onUpdateDisk)
    EVT_BUTTON(ID_DELETE_DISK, SettingsDialogWx::onDeleteDisk)
    EVT_BUTTON(ID_OPEN_DATA_FOLDER, SettingsDialogWx::onOpenDataFolder)
    EVT_LIST_ITEM_SELECTED(ID_KEY_LIST, SettingsDialogWx::onKeySelected)
    EVT_TEXT(ID_KEY_SEQUENCE, SettingsDialogWx::onKeySequenceChanged)
    EVT_BUTTON(ID_KEY_DEFAULT, SettingsDialogWx::onKeyRestoreDefault)
    EVT_BUTTON(ID_KEY_UNBIND, SettingsDialogWx::onKeyUnbind)
    EVT_BUTTON(wxID_OK, SettingsDialogWx::onOK)
    EVT_BUTTON(wxID_CANCEL, SettingsDialogWx::onCancel)
    EVT_COMMAND(ID_CATALOG_LOADED, wxEVT_COMMAND_TEXT_UPDATED, SettingsDialogWx::onCatalogLoaded)
    EVT_COMMAND(ID_DOWNLOAD_PROGRESS, wxEVT_COMMAND_TEXT_UPDATED, SettingsDialogWx::onDownloadProgress)
    EVT_COMMAND(ID_DOWNLOAD_COMPLETE, wxEVT_COMMAND_TEXT_UPDATED, SettingsDialogWx::onDownloadComplete)
wxEND_EVENT_TABLE()

// Forward-declared because the constructor reserves the release note's height
// before Fit() measures, and these three are defined further down beside the
// page builders that use them. Declaring rather than moving them: they sit with
// kNoteCols and the note text they wrap, and that is where a reader looking for
// "how wide is a note" will go.
static std::string hardWrap(const std::string& text, size_t cols);
static int wrappedLineCount(const std::string& s);
extern const size_t kNoteCols;

SettingsDialogWx::SettingsDialogWx(wxWindow* parent, DiskCatalog* catalog)
    : wxDialog(parent, wxID_ANY, "Settings", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_catalog(catalog)
    // Built here, in the init list, and kept here now that the fetch it was for
    // has moved to setSettings(). It was needed here because the body's
    // onRefreshCatalog() handed a copy to a worker thread before the constructor
    // had finished; the object is fully built before setSettings() runs, so that
    // reason has lapsed. Constructing it first is still the right shape - it is
    // what every worker-posting path assumes - and a gate created later would be
    // a null shared_ptr for anything that reached for it in between.
    , m_postGate(std::make_shared<WorkerPostGate>())
{
    OutputDebugStringA("[Settings] Constructor: creating controls\n");
    createControls();
    OutputDebugStringA("[Settings] Constructor: laying out controls\n");
    layoutControls();
    OutputDebugStringA("[Settings] Constructor: populating ROM list\n");
    // No settings yet - setSettings() has not been called - so there is no
    // preference to keep and this puts up the placeholder. loadSettings() runs
    // it again with the stored id the moment there is one.
    populateROMList(std::string());
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
    // Paged, the tallest page was Machine at 762x559 and the dialog settled at
    // 819 high. The Keyboard page is taller than Machine, so Fit() asks for more
    // again. That is precisely why the height is left to Fit() rather than
    // written down: adding a page moves it, and a constant would have had to be
    // found and corrected here instead.
    //
    // A notebook is as tall as its tallest page, so the other three gain empty
    // space below their content and lose nothing.
    //
    // Width is deliberately left at the 800/900 already in use. Fit() asks for
    // 886, within 14px of it, so there is nothing to win by moving it. That keeps
    // the DIALOG's width identical, not the content's: the notebook border plus
    // the page's 15px inset cost the list 36px of width (844 -> 808, both
    // measured). Its three columns declare 220+450+120 and have never had that
    // much, so this widens an overflow that already existed rather than creating
    // one.
    //
    // WHAT FIT() ASKS FOR IS A REQUEST, NOT A PROMISE. The fitted height used to
    // be handed to SetMinSize as well as to SetSize, which turned "taller than
    // the screen" into "taller than the screen and unshrinkable" - the status
    // line and the OK/Cancel row are the two things layoutControls() puts BELOW
    // the notebook, so they are what goes off the bottom, and a dialog that
    // cannot be made shorter cannot be made to show them again. The one display
    // this was checked on has 2160 lines; a 1366x768 laptop at 100% has about
    // 728 of work area, and nothing in the fit knows that.
    //
    // Measured on the running dialog after this change, at 200% on a 3840x2160
    // display whose work area is 3840x2064: it opens 900x1105 at 1470,479 -
    // centred in the WORK area, where centring in the 2160-line display instead
    // would sit it 48px lower, which for a dialog as tall as the work area is
    // the whole of what stays on screen. Forced to 1024x768 with SetWindowPos it
    // becomes exactly that, status line and OK/Cancel row still inside the
    // window (bottoms 1156 and 1219 against the window's 1247); forced to
    // 300x200 it stops at the floor, 800x458, OK still inside. Before this, both
    // of those were refused - 1105 was the minimum as well as the size.
    // RESERVE THE RELEASE NOTE'S REAL HEIGHT BEFORE FIT() MEASURES.
    //
    // Same defect, same fix, as m_catalogIndexNote - whose builder carries the
    // arithmetic - and it became this page's problem on 2026-09-18 when the
    // release picker moved here from Disk Images. The label is created holding a
    // one-line placeholder and updateRomwbwVersionNote() replaces it with text
    // that reaches nine wrapped lines: a release mismatch, a snapshot warning
    // and the ROM-count sentence can all be true at once.
    //
    // ON THE OLD PAGE the excess came out of m_catalogList, the only proportion-1
    // child there. THIS PAGE HAS NO PROPORTIONAL CHILD AT ALL, so a label that
    // grows after the fit pushes the Dazzler box off the bottom instead, with
    // nothing to absorb it.
    //
    // Reserved from the longest form the builder can produce, wrapped exactly as
    // the runtime wraps it, with the longest release string published today. It
    // is done here rather than beside the control because hardWrap() and
    // wrappedLineCount() are defined below buildMachinePage().
    if (m_romwbwVersionNote) {
        const std::string longest =
            hardWrap("Disks for RomWBW 3.7.0-dev.14 need a RomWBW 3.7.0-dev.14 ROM, "
                     "and the machine is running RomWBW 3.7.0. Starting will offer to "
                     "fetch it; without it the guest would report an HBIOS/CBIOS "
                     "version mismatch. "
                     "RomWBW 3.7.0-dev.14 is a pre-release. "
                     "This catalog publishes 2 ROMs; the one chosen below is fetched "
                     "and checked against its published size and checksum before the "
                     "machine starts. Changing the release moves the disks below to "
                     "match it.", kNoteCols);
        m_romwbwVersionNote->SetMinSize(
            wxSize(-1, m_romwbwVersionNote->GetCharHeight() * wrappedLineCount(longest)));
    }

    Fit();
    const wxSize fitted = GetSize();

    // Measured here rather than written down: the dialog minus its notebook is
    // the status line, the OK/Cancel row, the margins around both and the
    // window's own caption and border. Every part of that scales with the
    // display and with the user's font size, which a constant does not. It comes
    // to 202px at 200% here - the 458px floor the window was measured to enforce,
    // less the 256px the notebook was measured at while sitting on it.
    const int furniture = fitted.GetHeight() - m_notebook->GetSize().GetHeight();

    // Fit() has taken its measurement, so the heights the two lists were built
    // with can stop being floors. wx/window.h says of SetInitialSize, which is
    // what a control's ctor size goes through, "Sets the minsize to what was
    // passed in" - so buildKeyboardPage()'s list height propagated up through
    // the page sizer and the notebook and became a floor under the whole dialog.
    // Relaxed here and not at creation because Fit() has to see the full height
    // to open at it. Three lines of the list's own font is about a header and a
    // row; below the dialog's floor there is nothing left for a sizer to give.
    m_keyList->SetMinSize(wxSize(-1, m_keyList->GetCharHeight() * 3));
    m_catalogList->SetMinSize(wxSize(-1, m_catalogList->GetCharHeight() * 3));

    // The notebook is the only item in mainSizer with a proportion, so it is the
    // one that absorbs a shorter dialog - but only down to its own minimum, and
    // an unset minimum means the best size, which is the tallest page. Set it,
    // and mainSizer's minimum becomes this plus the furniture, which is exactly
    // the floor put on the dialog below. The two have to agree: a dialog allowed
    // to be shorter than its sizer's minimum is the buttons falling off again.
    //
    // Eight lines of the dialog's own font, 256px at 200% here. Inside the
    // notebook the same rule applies one level down - the list is the only
    // proportional item on its page, so it is what gives way. A PrintWindow
    // capture of the Keyboard page at a forced 1024x768 (the hard case: this
    // display is at 200%, so that is half the room a 1366x768 laptop would have
    // in lines of text) shows the list down to its header and one row with the
    // Sends box, the hint line, all three shortcut checkboxes, the status line
    // and OK/Cancel all still drawn. At the floor itself the page content does
    // start to overlap; a page that scrolled instead would need each of the four
    // to become a wxScrolledWindow, which is a bigger change than this defect
    // warrants and was not made.
    const int minNotebook = 8 * GetCharHeight();
    m_notebook->SetMinSize(wxSize(-1, minNotebook));

    // The work area - the display minus the taskbar - of the monitor this dialog
    // will open on, not the display rectangle: a dialog sized to the display has
    // its OK row behind the taskbar. wx/display.h documents wxDisplay(wxWindow*)
    // as giving "the display of the given window or the default one if the
    // window display couldn't be found", and the fallback is the case here,
    // because this runs before the dialog is shown.
    const wxRect work = wxDisplay(this).GetClientArea();
    const WxDialogPlacement place = placeDialogInWorkArea(
        900, 800, fitted.GetHeight(), furniture, minNotebook, work);

    SetMinSize(wxSize(place.minWidth, place.minHeight));
    SetSize(wxSize(place.width, place.height));
    // Positioned rather than Centre()d. wx/window.h documents Centre() only as
    // centring "on screen" when there is no parent, and whether that means the
    // display rectangle or the work area was not established here - which is
    // exactly the distinction the clamp above has just made matter, because it
    // can now produce a dialog as tall as the whole work area.
    SetPosition(wxPoint(place.x, place.y));

    char sizeMsg[200];
    sprintf_s(sizeMsg,
              "[Settings] Constructor: fitted %dx%d, furniture %d, notebook floor %d,"
              " work %dx%d at %d,%d -> %dx%d at %d,%d, min %dx%d\n",
              fitted.GetWidth(), fitted.GetHeight(), furniture, minNotebook,
              work.GetWidth(), work.GetHeight(), work.x, work.y,
              place.width, place.height, place.x, place.y,
              place.minWidth, place.minHeight);
    OutputDebugStringA(sizeMsg);

    // THE CATALOG FETCH IS NOT STARTED HERE. It used to be, and it read the
    // wrong catalog every time a machine had one stored.
    //
    // onRefreshCatalog's first act is
    // m_catalog->setCatalogIndexUrl(typedCatalogIndexUrl()), which reads the
    // Catalog index field - and at this point in construction that field is the
    // empty one createControls() just made. The stored URL does not arrive until
    // loadSettings(), which runs from setSettings(), which the caller invokes
    // only AFTER the constructor returns. So the first fetch of every Settings
    // open pushed "" over the index MainWindow::applyConfig had seeded and
    // resolved to the BUILT-IN catalog, while the field underneath correctly
    // showed the custom URL and the note correctly said "In use: <that URL>".
    // The release picker, the ROM list and the disk list were the built-in
    // catalog's, and pressing OK then wrote the built-in catalog's release over
    // the user's - the same silent overwrite the $ROMWBW_INDEX_URL guard in
    // MainWindow was added for, arriving by a route that guard cannot see.
    //
    // It is the one path nothing checked: MANUAL_CHECKS section 11 exercises
    // Refresh with a typed URL, and the environment variable, and the stored
    // setting only as far as "the field shows it again" - never as far as which
    // catalog the picker under it came from. Deterministic, not a race.
    //
    // setSettings() starts it instead, which is also where it belongs: the list
    // this fetch fills in has to be the list the controls above it describe.
    OutputDebugStringA("[Settings] Constructor: done\n");
}

SettingsDialogWx::~SettingsDialogWx() {
    // Shut the DiskCatalog workers out, and do it as the FIRST statement, while
    // every base and member of this dialog is still intact. close() either wins
    // the gate's mutex - after which no worker can ever post to this object
    // again - or blocks until the wxPostEvent that beat it to the lock has
    // finished against the still-whole dialog. This function returning is
    // therefore the proof that nothing on a worker thread will touch *this.
    //
    // This body used to be empty, which is what the shipping crash was: the
    // constructor starts a catalog fetch, the dialog is destroyed the instant
    // ShowModal() returns, and the detached worker posted into the hole. See
    // WorkerPostGate in DiskCatalog.h for the dumps and for why locking a
    // weak_ptr in the worker would not have closed it.
    //
    // Nothing here waits for the download itself. An in-flight fetch or disk
    // download keeps running against the DiskCatalog, which MainWindow owns as
    // m_diskCatalog and which outlives every dialog - so a download the user
    // asked for still lands in the data folder after they close Settings, and
    // closing Settings is never delayed by the network.
    m_postGate->close();
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
    m_keyboardPage = new wxPanel(m_notebook, wxID_ANY);
    m_diskImagesPage = new wxPanel(m_notebook, wxID_ANY);
    m_notebook->AddPage(m_machinePage, "Machine", true);
    m_notebook->AddPage(m_terminalPage, "Terminal");
    m_notebook->AddPage(m_keyboardPage, "Keyboard");
    m_notebook->AddPage(m_diskImagesPage, "Disk Images");

    buildMachinePage();
    buildTerminalPage();
    buildKeyboardPage();
    buildDiskImagesPage();

    // The status line is the one control that stays on the dialog. Its writers
    // do not share a page: onNewDisk is a Machine control's handler, while
    // onRefreshCatalog, onCatalogLoaded, onDownloadDisk, onDownloadComplete and
    // onDeleteDisk all belong to Disk Images, and onOK and setKeySeqValid write
    // it about the Keyboard page. Put it on any one page and most of them would
    // be updating a label the user is not looking at.
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

    // THE RELEASE, FIRST, BECAUSE EVERYTHING BELOW IT IS ABOUT THE RELEASE.
    //
    // This block lived on the Disk Images page until 2026-09-18. It was moved
    // here because it was in the wrong place and a user said so: the release
    // picker decides which ROM the dropdown below offers AND which images the
    // four slots below that can hold, and it was sitting on a third page from
    // both. Picking a release and coming back here showed nothing changed - the
    // ROM names are identical in every release ("EMU AVW", "EMU RCZ80"), and the
    // slot dropdowns list only images already downloaded - so the app looked
    // broken when it was working.
    //
    // The "Show development snapshots" box comes with it for the same reason:
    // filed under Disk Images it was, in the user's words, hard to find.
    //
    // The Disk Images page keeps the download LIBRARY - the list, its buttons
    // and the catalog index field. The division is now: this page is the machine
    // you are about to run, that page is the collection of files on disk.

    // Which RomWBW release the catalog is for, and what that means here.
    //
    // It is at the TOP of this page because it decides everything below it: which
    // ROM the dropdown under it offers, and which images the four slots under
    // that can hold.
    // The list is not compiled in - it is index-v0.json's own list of releases,
    // whole and unfiltered - so it is empty until a catalog has been fetched,
    // which is why populateVersionList() leaves a placeholder rather than an
    // empty control.
    //
    // This said "filtered to the releases the emulator core says it can boot"
    // until 2026-09-17, and the filter it described was deleted that same day -
    // romwbw_emu v1.44 removed the function it asked. populateVersionList(),
    // further down this file, is the one that fills the control and carries the
    // reasoning; the short of it is that the release number is the
    // ROM-to-disk-image pairing and never described what this core can execute.
    m_romwbwVersionChoice = new wxChoice(page, ID_ROMWBW_VERSION);
    wxBoxSizer* versionSizer = new wxBoxSizer(wxHORIZONTAL);
    versionSizer->Add(new wxStaticText(page, wxID_ANY, "RomWBW release:"), 0,
                      wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    versionSizer->Add(m_romwbwVersionChoice, 1, wxALIGN_CENTER_VERTICAL);
    content->Add(versionSizer, 0, wxEXPAND | wxBOTTOM, 4);

    // THE OPT-IN THE CATALOG REQUIRES, directly under the control it governs.
    //
    // romwbw_disks publishes RomWBW DEVELOPMENT SNAPSHOTS beside the releases -
    // index entries carrying `prerelease: true`, the first of them 3.7.0-dev.14
    // on 2026-09-18 - and CATALOG_SCHEMA.md 2.3 says a client "MUST NOT offer a
    // prerelease entry by default; hide it behind an explicit opt-in, a 'show
    // development snapshots' checkbox or equivalent". This is that checkbox, and
    // the wording is the schema's rather than invented, so that a user reading
    // the repository and a user reading this dialog are told the same thing.
    //
    // It is NOT a second release picker. Everything it does happens in
    // populateVersionList(): rows appear and disappear. The release a machine
    // runs is moved only by the choice above it, which is why unticking this
    // cannot strand somebody on a snapshot whose images are mounted - the row
    // for the selected release survives the filter. catalogv0::isOffered holds
    // that rule, in one place, so this dialog and the automatic choice made
    // during a fetch cannot disagree about one machine.
    m_showPrereleaseCheck = new wxCheckBox(page, ID_SHOW_PRERELEASE,
                                           "Show pre release");
    content->Add(m_showPrereleaseCheck, 0, wxBOTTOM, 6);

    // Created carrying the sentence that is true whatever is selected, rather
    // than empty, so that the Fit() in the constructor reserves height for it -
    // this page is laid out before any catalog has been fetched, and a label
    // that grows from nothing to two lines afterwards grows into a page that was
    // measured without it. updateRomwbwVersionNote() replaces the text.
    //
    // It said "z80cpmw boots the ROM it ships with. ROMs in the catalog are not
    // downloaded." - true when it was written, false since the catalog ROM fetch
    // landed, and doubly so since the ROMs were deleted. It is a placeholder
    // that is on screen for as long as the first catalog fetch takes, so it has
    // to be a sentence that stays true rather than a description of a state.
    m_romwbwVersionNote = new wxStaticText(page, wxID_ANY,
        "The ROM and the disk images are downloaded from the RomWBW catalog and "
        "checked against it. Reading the catalog...");
    content->Add(m_romwbwVersionNote, 0, wxEXPAND | wxBOTTOM, 10);

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
    // Empty text, not "14" and not "0E": the string argument is parsed in the
    // base in force at construction, which is 10 until SetBase() below, so any
    // hex spelling of the default would be read wrong here. Empty makes
    // wxSpinCtrl use the numeric `initial` instead, and 0x0E is unambiguous.
    m_dazzlerPortSpin = new wxSpinCtrl(page, wxID_ANY, "", wxDefaultPosition,
                                        wxSize(70, -1), wxSP_ARROW_KEYS, 0, 255, 0x0E);

    // Base 16, so the control matches the label it has carried all along. It was
    // built with the default base 10 and an initial text of "14" - 0x0E written
    // in decimal - which was harmless only while MainWindow neither seeded this
    // field nor read it back. It is now the port the Dazzler is CONSTRUCTED with
    // and the number written to hardware.dazzler[0].port, so a user who read
    // "Port (hex):", typed 0E and pressed OK got a card at port 14, and one who
    // typed 20 got 0x14.
    //
    // Hex rather than relabelling this "(decimal)", because every other
    // statement of the port in the program is hex - DazzlerConfig::port's
    // default is written 0x0E, MainWindow's status line prints "port 0x0E",
    // Dazzler.h documents the card's base port the same way - and so is the
    // Cromemco literature the number is copied out of. A decimal control would
    // have been the only decimal port in the application.
    //
    // Checked rather than assumed: SetBase returns false on a port that cannot
    // do base 16, and there the label is corrected instead, so the two cannot
    // disagree whichever way it goes.
    if (!m_dazzlerPortSpin->SetBase(16)) {
        m_dazzlerPortLabel->SetLabel("Port (decimal):");
    }
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

// Pixel width the explanatory paragraphs wrap at.
//
// Deliberately below the width actually available: a page measures 808px of
// content with the dialog at the 900px SetSize below, read off the running
// window, but SetMinSize allows 800 and the dialog has a resize border. Wrap()
// bakes the line breaks in when the text is created and cannot follow a resize,
// so a paragraph fitted to the wide case is clipped in the narrow one, and
// clipping is exactly the fault this constant exists to fix - the first capture
// of the Keyboard page showed all three of its paragraphs cut off mid-sentence,
// because wxStaticText does not wrap unless it is told to.
static const int kPageTextWrap = 700;

// Characters per line for the two notes on the Disk Images page that are BUILT
// AT RUN TIME rather than written into the constructor.
//
// Those two do not wrap with wxStaticText::Wrap(), and the fixed paragraphs on
// the other pages do. MEASURED, both of them, on 2026-09-08: bellNote and intro
// come back from WM_GETTEXT carrying real newlines and drawn 97px tall, while
// m_romwbwVersionNote and m_catalogIndexNote come back with NO newlines at all,
// 808x33, clipped mid-sentence - after a Wrap(kPageTextWrap) call on the line
// above. The difference between the two groups is that these two are added to
// the sizer with wxEXPAND and are re-labelled after layout; what exactly wx does
// with that combination was not established, and guessing at it is how the note
// came to be shipped clipped in the first place.
//
// So the line breaks are put in by hand, where they can be seen in a screenshot
// and cannot be undone by a later Layout(). A CHARACTER budget rather than a
// pixel one because it needs no device context and no measurement, and cannot be
// defeated by whatever it is that stops Wrap() working here.
//
// 62 is measured, not chosen: bellNote and intro are wrapped by wx itself at
// kPageTextWrap and come back broken at about 62 characters, and those two are
// the paragraphs on this dialog that have always drawn correctly. 72 was tried
// first, on the arithmetic that 808px at this DPI holds about 74 characters, and
// the URL line came out clipped at "catalog-v0/ind" - so the arithmetic is
// wrong, or the width the control draws at is not the width the sizer gives it.
// Being too short costs a line break; being too long costs the text.
extern const size_t kNoteCols;
const size_t kNoteCols = 62;

// Break 'text' into lines of at most 'cols' characters, joined with '\n', which
// wxStaticText draws as separate lines.
//
// A WORD LONGER THAN THE BUDGET IS CUT, and that is not the obvious choice. The
// obvious one - let it overrun, because a URL split across two lines cannot be
// double-clicked to copy - was tried first and is wrong here, because it does
// not overrun: an index URL is 85 characters where the control is 72 wide, and
// wxStaticText drew that line as NOTHING AT ALL. Measured on 2026-09-08: the
// label read "In use: https://github.com/avwohl/..." through WM_GETTEXT, the
// control was two lines tall, and the screen showed "In use:" followed by blank
// space. A URL the user can read across two lines beats a URL they cannot see,
// and the field itself still holds a custom one in copyable form.
static std::string hardWrap(const std::string& text, size_t cols) {
    if (cols == 0) return text;
    std::string out;
    size_t lineLen = 0;
    for (size_t i = 0; i < text.size();) {
        while (i < text.size() && text[i] == ' ') i++;
        if (i >= text.size()) break;
        const size_t end = text.find(' ', i);
        std::string word = text.substr(i, end == std::string::npos ? end : end - i);
        i = (end == std::string::npos) ? text.size() : end;

        // The unbreakable case, taken in whole lines until what is left fits.
        //
        // ON A CHARACTER BOUNDARY, NOT A BYTE ONE. The budget is counted in
        // bytes, which is right for the ASCII this note is made of, but the one
        // part of it the USER writes - the index URL - need not be ASCII. A cut
        // landing inside a UTF-8 sequence leaves the whole wrapped string
        // invalid, and wxString::FromUTF8 answers invalid input with an EMPTY
        // string: the SetLabel at the end of updateCatalogIndexNote() would draw
        // as nothing at all, warning and URL together. That is the same
        // disappearance the hard wrap was written to prevent, arriving through
        // the wrap itself. Named rather than cited by line, because the line
        // moved once already while this comment was being written.
        //
        // Backing up over continuation bytes (0b10xxxxxx) costs at most three of
        // the 62, and cannot loop: a sequence is at most four bytes, so the cut
        // always lands at 59 or more and the word always gets shorter.
        while (word.size() > cols) {
            size_t cut = cols;
            while (cut > 0 && (static_cast<unsigned char>(word[cut]) & 0xC0) == 0x80) cut--;
            if (cut == 0) cut = cols;   // not UTF-8 at all; take the bytes as they are
            if (lineLen != 0) { out += '\n'; lineLen = 0; }
            out += word.substr(0, cut);
            out += '\n';
            word = word.substr(cut);
        }

        if (lineLen == 0) {
            out += word;
            lineLen = word.size();
        } else if (lineLen + 1 + word.size() <= cols) {
            out += ' ' + word;
            lineLen += 1 + word.size();
        } else {
            out += '\n' + word;
            lineLen = word.size();
        }
    }
    return out;
}

// The sentence part of the Catalog index note - everything except the "In use:"
// line - in ONE place, so that the height reserved for the label in
// createControls() is computed from the same words updateCatalogIndexNote()
// will put in it. Having the wording in two places is how the reservation came
// to be three lines for a note that needs seven.
static std::string catalogIndexNoteText(bool haveEnv, bool custom) {
    std::string note;
    if (haveEnv) {
        note = "ROMWBW_INDEX_URL is set for this run and wins over this field. ";
    } else if (custom) {
        note = "Using a custom catalog. ";
    } else {
        note = "Leave empty for the catalog this build ships with. ";
    }
    if (custom) {
        // Said on BOTH custom paths, the environment one included.
        note += "Downloads are still checked against that catalog's own SHA-256, "
                "but the catalog is the thing being trusted. Every catalog reads "
                "and writes this one data folder, so downloading an image whose "
                "name is already there replaces it - save your work out of a "
                "downloaded disk first. ";
    }
    return note;
}

// Lines in a string hardWrap() has already broken.
static int wrappedLineCount(const std::string& s) {
    return 1 + (int)std::count(s.begin(), s.end(), '\n');
}

// How the terminal behaves rather than what the machine is: the scrollback
// history, and whether BEL makes a noise.
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
    content->Add(scrollbackSizer, 0, wxBOTTOM, 12);

    // The bell. TerminalView::setBellEnabled() and AppConfig::bellEnabled both
    // already existed and had no caller and no control; this is the half that
    // makes the setting reachable without editing z80cpmw.json.
    m_bellCheck = new wxCheckBox(page, wxID_ANY, "Sound the bell (BEL, character 7)");
    content->Add(m_bellCheck, 0);
    wxStaticText* bellNote = new wxStaticText(page, wxID_ANY,
        "CP/M software rings the bell freely - WordStar does it on every rejected "
        "keystroke - and it plays the system sound, not a soft click.");
    // wxStaticText does not wrap by itself; without this the sentence is drawn
    // past the edge of the page and simply clipped, which is what the first
    // capture of this page showed.
    bellNote->Wrap(kPageTextWrap);
    content->Add(bellNote, 0, wxLEFT | wxTOP, 22);

    wxBoxSizer* pageSizer = new wxBoxSizer(wxVERTICAL);
    pageSizer->Add(content, 1, wxEXPAND | wxALL, 15);
    page->SetSizer(pageSizer);
}

// Where in the list a key is shown. This is a presentation order and nothing
// else reads it, so a code that is missing from it is not an error: the
// comparator in rebuildKeyRows() sorts anything absent after everything
// present, by code, and a key can therefore never be dropped from the list by
// being left out of this table.
//
// The point of ordering by base key rather than by name is the reserved rows:
// it puts Shift+PageUp directly under PageUp and Ctrl+Home directly under Home,
// which is where a user looking for them will look.
static int keyDisplayRank(int vk) {
    static const int order[] = {
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_HOME, VK_END,
        VK_INSERT, VK_DELETE, VK_PRIOR, VK_NEXT,
    };
    for (int i = 0; i < (int)(sizeof order / sizeof order[0]); ++i) {
        if (order[i] == vk) return i;
    }
    if (vk >= VK_F1 && vk <= VK_F12) return 100 + (vk - VK_F1);
    return 1000 + vk;
}

// The Status column. It says what a row IS relative to the built-in defaults,
// which is the question a user scanning the list is asking: what have I
// changed, and what is off.
wxString SettingsDialogWx::keyRowStatus(const KeyRow& row,
                                        const std::map<unsigned, std::string>& defaults) {
    if (row.purpose) return "Reserved";
    if (row.sequence.empty()) return "Unbound";
    auto it = defaults.find(row.id);
    if (it != defaults.end() && it->second == row.sequence) return "Default";
    return "Custom";
}

// What onOK() puts on the shared status line when it refuses to close, and what
// setKeySeqValid() takes back down again the moment the box becomes valid,
// whichever of the four paths made it so. One spelling, so the retraction cannot
// fail to match the claim.
static const char* const kKeySeqRefusedStatus =
    "That key sequence cannot be used - see the Keyboard page.";

// Every write of m_keySeqValid comes through here, so that the flag and the
// status line cannot disagree - see the declaration in SettingsDialogWx.h for
// the four paths that reach it.
//
// Driven and read back with WM_GETTEXT on the running dialog: with "\Z" in the
// box, OK leaves "That key sequence cannot be used" on the status line and the
// dialog open; Default, Unbind, and typing a sequence that does validate each
// put it back to "Ready". Only the first of those three used to.
void SettingsDialogWx::setKeySeqValid(bool valid) {
    m_keySeqValid = valid;
    if (!valid) return;

    // Retract our own refusal, and only ours: the string is compared rather than
    // the line cleared unconditionally, because the label's other writers are
    // the catalog and download handlers, and correcting a key sequence must not
    // wipe "Download complete". A status line that still says the sequence
    // cannot be used, after it can, is a claim the app has made false.
    if (m_statusText->GetLabel() == kKeySeqRefusedStatus) {
        m_statusText->SetLabel("Ready");
    }
}

// The line under the sequence box when nothing is wrong. It is the escape
// syntax in one line, because this box is the one place in the app where a
// termcap string is typed and docs/CONFIGURATION.md is not on screen.
//
// One line, and short. This label is written at run time, so it cannot be
// Wrap()ped the way the fixed paragraphs above are - Wrap rewrites the label's
// text, and the next SetLabel would undo it. Everything that lands here, this
// string and keymap::validateSequence()'s refusals alike, is kept inside the
// roughly 78 characters the label was measured to hold before it clips.
static wxString keyEditorHint() {
    return "\\E is Escape, ^A is Ctrl+A, ^? is Delete. An empty box unbinds the key.";
}

// Rebinding a key used to mean closing the app and hand-editing z80cpmw.json,
// which todo.txt asked for and which got worse when the map learned modifiers
// and so had more to say.
void SettingsDialogWx::buildKeyboardPage() {
    wxWindow* page = m_keyboardPage;
    wxBoxSizer* content = new wxBoxSizer(wxVERTICAL);

    wxStaticText* intro = new wxStaticText(page, wxID_ANY,
        "What each special key sends to CP/M. CP/M has no function or navigation "
        "keys of its own, so the right bytes depend on the terminal your software "
        "expects.");
    intro->Wrap(kPageTextWrap);
    content->Add(intro, 0, wxBOTTOM, 6);

    // 210 DIP, not a raw 420 pixels. The 420 was measured on a 200% display,
    // where FromDIP hands exactly that back, so this is the same list it always
    // was here. What the raw number did not do was mean the same thing anywhere
    // else: 420 physical pixels is twice as many rows at 100%, and 420 out of
    // the roughly 728 lines of work area a 1366x768 laptop has. This is the
    // largest single contributor to the fitted height, so it is the one that
    // decides whether the fit lands on a small screen at all - see the clamp in
    // the constructor, which is what catches it when it does not.
    //
    // Taller than the 250 the catalog list asks for, deliberately: this list has
    // 31 rows on a default config against the catalog's handful, and at the 250
    // it was first given, a capture of the page showed five whole rows - the
    // first reserved row is the eleventh, so every one of the four rows this
    // page exists to explain was below the fold.
    m_keyList = new wxListCtrl(page, ID_KEY_LIST, wxDefaultPosition,
                               wxSize(-1, page->FromDIP(210)),
                               wxLC_REPORT | wxLC_SINGLE_SEL);
    m_keyList->InsertColumn(0, "Key", wxLIST_FORMAT_LEFT, 200);
    m_keyList->InsertColumn(1, "Sends", wxLIST_FORMAT_LEFT, 300);
    m_keyList->InsertColumn(2, "Status", wxLIST_FORMAT_LEFT, 130);
    content->Add(m_keyList, 1, wxEXPAND | wxBOTTOM, 8);

    m_keySeqText = new wxTextCtrl(page, ID_KEY_SEQUENCE);
    m_keyDefaultBtn = new wxButton(page, ID_KEY_DEFAULT, "Default");
    m_keyUnbindBtn = new wxButton(page, ID_KEY_UNBIND, "Unbind");
    wxBoxSizer* editSizer = new wxBoxSizer(wxHORIZONTAL);
    editSizer->Add(new wxStaticText(page, wxID_ANY, "Sends:"),
                   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    editSizer->Add(m_keySeqText, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    editSizer->Add(m_keyDefaultBtn, 0, wxRIGHT, 5);
    editSizer->Add(m_keyUnbindBtn, 0);
    content->Add(editSizer, 0, wxEXPAND | wxBOTTOM, 4);

    // The one line that answers "why can I not type here?" - a validator's
    // refusal, or the words reservedKeys() carries for a reserved row.
    m_keyHintText = new wxStaticText(page, wxID_ANY, keyEditorHint());
    content->Add(m_keyHintText, 0, wxEXPAND | wxBOTTOM, 12);

    content->Add(new wxStaticLine(page), 0, wxEXPAND | wxBOTTOM, 10);

    wxStaticText* shortcutIntro = new wxStaticText(page, wxID_ANY,
        "Keys the application keeps for itself. A key kept here is swallowed whole "
        "and CP/M never sees it.");
    shortcutIntro->Wrap(kPageTextWrap);
    content->Add(shortcutIntro, 0, wxBOTTOM, 6);
    // Worded as what the app takes, so the box being ticked means the shortcut
    // works - the config members underneath say the opposite thing
    // ("f1ToCpm"), and putting that on screen would make every checkbox on this
    // group read backwards.
    m_f1HelpCheck = new wxCheckBox(page, wxID_ANY, "F1 opens Help Topics");
    m_f5StartStopCheck = new wxCheckBox(page, wxID_ANY,
                                        "F5 and Shift+F5 start and stop the emulator");
    m_ctrlRResetCheck = new wxCheckBox(page, wxID_ANY,
                                       "Ctrl+R resets the machine (CP/M reads ^R as retype-line)");
    content->Add(m_f1HelpCheck, 0, wxBOTTOM, 4);
    content->Add(m_f5StartStopCheck, 0, wxBOTTOM, 4);
    content->Add(m_ctrlRResetCheck, 0);

    wxBoxSizer* pageSizer = new wxBoxSizer(wxVERTICAL);
    pageSizer->Add(content, 1, wxEXPAND | wxALL, 15);
    page->SetSizer(pageSizer);
}

// Turn the "keys" object into the list model.
//
// KEYED BY RESOLVED ID, not by name. Two spellings resolve to one binding, and
// the config walker in Config.cpp already uses that rule; keying by name here
// would show one key as two rows that overwrite each other.
//
// NOTHING IS DROPPED. KeyMap::build deliberately does not filter what it does
// not recognise, precisely so that this dialog's read-modify-write of the whole
// object cannot delete a line the user typed. Whatever no row owns goes into
// m_keyCarry and is written back untouched.
void SettingsDialogWx::rebuildKeyRows() {
    m_keyRows.clear();
    m_keyCarry.clear();
    m_keyDefaults.clear();
    m_keysDirty = false;
    setKeySeqValid(true);
    m_keySelectedRow = -1;

    for (const auto& kv : keymap::defaultBindings()) {
        long id = keymap::keyIdForName(kv.first);
        if (id >= 0) m_keyDefaults[(unsigned)id] = kv.second;
    }

    // id -> (the name to write it back under, the sequence)
    std::map<unsigned, std::pair<std::string, std::string>> owned;
    for (const auto& kv : m_settings.keyBindings) {
        long id = keymap::keyIdForName(kv.first);
        if (id < 0) {
            // A name this build cannot read. It is NOT a row and it is NOT
            // deleted: the configuration report already names it at startup,
            // and the user is the one who has to correct it.
            m_keyCarry[kv.first] = kv.second;
            continue;
        }
        auto it = owned.find((unsigned)id);
        if (it != owned.end()) {
            // Two names for one binding, which a hand edit can produce.
            // KeyMap::build merges into a map keyed by id while walking the
            // overrides in name order, so the LAST name wins there - and
            // m_settings.keyBindings is a std::map, so this loop is walking
            // that same order. Keeping the later one means the row shows the
            // sequence that is actually in force; the earlier one is carried
            // through rather than dropped.
            m_keyCarry[it->second.first] = it->second.second;
        }
        owned[(unsigned)id] = { kv.first, kv.second };
    }

    // Every id worth a row: one for each built-in default, one for anything the
    // file mentions, and one for each reserved combination. The reserved four
    // are here because being absent is what makes them a mystery - a user who
    // tries to bind Shift+PageUp should be told why they cannot, which is what
    // reservedKeys()'s purpose strings were given for.
    std::set<unsigned> ids;
    for (const auto& kv : m_keyDefaults) ids.insert(kv.first);
    for (const auto& kv : owned) ids.insert(kv.first);
    size_t reservedCount = 0;
    const keymap::ReservedKey* reserved = keymap::reservedKeys(&reservedCount);
    for (size_t i = 0; i < reservedCount; ++i) {
        ids.insert(keymap::keyId(reserved[i].vk, reserved[i].mods));
    }

    for (unsigned id : ids) {
        KeyRow row;
        row.id = id;
        row.purpose = keymap::reservedPurpose(id);
        auto o = owned.find(id);
        row.hasEntry = (o != owned.end());
        if (row.hasEntry) {
            row.name = o->second.first;
            row.sequence = o->second.second;
        } else {
            row.name = keymap::nameForKeyId(id);
            auto d = m_keyDefaults.find(id);
            if (d != m_keyDefaults.end()) row.sequence = d->second;
        }
        m_keyRows.push_back(row);
    }

    std::sort(m_keyRows.begin(), m_keyRows.end(),
              [](const KeyRow& a, const KeyRow& b) {
                  const int ra = keyDisplayRank((int)(a.id & 0xFFFFu));
                  const int rb = keyDisplayRank((int)(b.id & 0xFFFFu));
                  if (ra != rb) return ra < rb;
                  return (a.id >> 16) < (b.id >> 16);   // plain key above its modified forms
              });
}

void SettingsDialogWx::populateKeyList() {
    m_keyList->DeleteAllItems();
    for (size_t i = 0; i < m_keyRows.size(); ++i) {
        const KeyRow& row = m_keyRows[i];
        long idx = m_keyList->InsertItem((long)i, wxString::FromUTF8(row.name));
        m_keyList->SetItem(idx, 1, wxString::FromUTF8(row.sequence));
        m_keyList->SetItem(idx, 2, keyRowStatus(row, m_keyDefaults));
        if (row.purpose) {
            // Greyed rather than omitted. wxListCtrl has no disabled-item state
            // in report mode, so the grey is the whole visual signal and the
            // Status column and the hint line carry the rest.
            m_keyList->SetItemTextColour(
                idx, wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        }
    }
    // Nothing is selected at this point, so put the editor into that state
    // rather than leaving it in whatever state the constructor built.
    updateKeyEditor();
    if (!m_keyRows.empty()) {
        // Selecting the first row fires onKeySelected, which is what fills the
        // editor in. Without it the dialog would open on the disabled state
        // just set, which reads as broken rather than as "nothing selected".
        m_keyList->SetItemState(0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    }
}

void SettingsDialogWx::refreshKeyRow(long row) {
    if (row < 0 || (size_t)row >= m_keyRows.size()) return;
    m_keyList->SetItem(row, 1, wxString::FromUTF8(m_keyRows[row].sequence));
    m_keyList->SetItem(row, 2, keyRowStatus(m_keyRows[row], m_keyDefaults));
}

void SettingsDialogWx::updateKeyEditor() {
    const bool haveRow = m_keySelectedRow >= 0 &&
                         (size_t)m_keySelectedRow < m_keyRows.size();
    if (!haveRow) {
        m_keySeqText->ChangeValue("");
        m_keySeqText->Enable(false);
        m_keyDefaultBtn->Enable(false);
        m_keyUnbindBtn->Enable(false);
        m_keyHintText->SetLabel("Select a key to change what it sends.");
        setKeySeqValid(true);
        return;
    }

    const KeyRow& row = m_keyRows[m_keySelectedRow];
    // ChangeValue, not SetValue: SetValue emits wxEVT_TEXT, so filling the box
    // from a selection would run onKeySequenceChanged and mark a row edited
    // that the user had only clicked on.
    m_keySeqText->ChangeValue(wxString::FromUTF8(row.sequence));
    // The refused text has just been dropped on the floor - the box now holds
    // the row's own sequence - so the refusal on the status line goes with it.
    setKeySeqValid(true);

    if (row.purpose) {
        m_keySeqText->Enable(false);
        m_keyDefaultBtn->Enable(false);
        m_keyUnbindBtn->Enable(false);
        m_keyHintText->SetLabel(wxString::FromUTF8(row.name) +
                                " is reserved by the app to " +
                                wxString::FromUTF8(row.purpose) + ".");
        return;
    }

    m_keySeqText->Enable(true);
    // A key with no built-in default has nothing to restore, so the button
    // would do nothing if it were left live.
    m_keyDefaultBtn->Enable(m_keyDefaults.count(row.id) != 0);
    m_keyUnbindBtn->Enable(!row.sequence.empty());
    m_keyHintText->SetLabel(keyEditorHint());
}

// The one place a row's sequence changes, so Default, Unbind and a valid typed
// value cannot drift apart in what they update.
void SettingsDialogWx::applyKeySequenceToRow(const std::string& sequence) {
    if (m_keySelectedRow < 0 || (size_t)m_keySelectedRow >= m_keyRows.size()) return;
    KeyRow& row = m_keyRows[m_keySelectedRow];
    if (row.purpose) return;          // the controls are disabled on a reserved row
    if (row.sequence == sequence) return;   // nothing to record, so nothing is dirty

    row.sequence = sequence;
    row.edited = true;
    m_keysDirty = true;
    refreshKeyRow(m_keySelectedRow);
    m_keyUnbindBtn->Enable(!sequence.empty());
}

// The download catalog, the folder it downloads into, and the warning that
// fires when the guest writes to something downloaded from it.
void SettingsDialogWx::buildDiskImagesPage() {
    wxWindow* page = m_diskImagesPage;
    wxBoxSizer* content = new wxBoxSizer(wxVERTICAL);


    // THE RELEASE PICKER IS NOT HERE ANY MORE. It moved to the Machine page on
    // 2026-09-18, with its "Show development snapshots" box and its note, because
    // it decides what the ROM dropdown and the four disk slots there can hold and
    // it was sitting on a different page from both. What is left here is the
    // download LIBRARY: which catalog to read, what it publishes, and what of it
    // is on this machine.

    // WHERE THE CATALOG ITSELF COMES FROM.
    //
    // The point of compiling in exactly one URL is that everything else is read
    // out of a document at run time. This is that one URL, made changeable: for
    // testing a romwbw_disks release before it is published, and for running
    // your own. Empty means the one this build ships with.
    //
    // First on this page because it decides what the release picker on the
    // Machine page can
    // even offer - a different index publishes a different set of releases.
    wxBoxSizer* indexSizer = new wxBoxSizer(wxHORIZONTAL);
    indexSizer->Add(new wxStaticText(page, wxID_ANY, "Catalog index:"), 0,
                    wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_catalogIndexUrlText = new wxTextCtrl(page, ID_CATALOG_INDEX_URL, wxEmptyString);
    m_catalogIndexUrlText->SetHint("built-in catalog");
    indexSizer->Add(m_catalogIndexUrlText, 1, wxALIGN_CENTER_VERTICAL);
    content->Add(indexSizer, 0, wxEXPAND | wxBOTTOM, 4);

    // Created with a neutral sentence for the same reason as the note above: the
    // page is laid out before any of this is known, and a label that grows
    // afterwards grows into a page measured without it. updateCatalogIndexNote()
    // replaces the text.
    //
    // THE HEIGHT IS COMPUTED, NOT ASSERTED. This comment used to say the label
    // "is sized for THREE lines because that is what the longest form below
    // needs once wrapped", and that was arithmetic nobody had done: wrapping the
    // real literals at kNoteCols gives four lines for the DEFAULT state a fresh
    // install opens on, six for a typed URL and seven under $ROMWBW_INDEX_URL.
    // The placeholder's own sentence is shorter than all three, so Fit()
    // measured this page without the space the note actually needs. The excess
    // comes out of the disk list, which is the only proportion-1 item on the
    // page - and the constructor already records that this page starts to
    // overlap at the dialog's minimum height, which is where that ends.
    //
    // So the reservation is taken from the longest form the builder above can
    // produce, wrapped exactly as the runtime wraps it, over the built-in URL.
    // A user-typed URL longer than that one still grows the label; nothing can
    // reserve for an arbitrary string, and the sizer absorbs the difference.
    //
    // It said "Each catalog keeps its own downloads and settings, so switching
    // never costs your library." That describes ioscpm, which gives each index
    // its own Disks folder; here the downloads are shared, which is the opposite
    // promise and the one place on this page where being wrong costs a user
    // their work. A placeholder has to be a sentence that stays true.
    m_catalogIndexNote = new wxStaticText(page, wxID_ANY,
        "Leave empty for the catalog this build ships with. Every catalog reads "
        "and writes the same data folder, so an image published under a name "
        "already there is replaced when you download it.");
    m_catalogIndexNote->Wrap(kPageTextWrap);
    {
        const std::string longest =
            hardWrap(catalogIndexNoteText(true, true), kNoteCols) + "\n" +
            hardWrap("In use: " + std::string(catalogv0::INDEX_URL), kNoteCols);
        m_catalogIndexNote->SetMinSize(
            wxSize(-1, m_catalogIndexNote->GetCharHeight() * wrappedLineCount(longest)));
    }
    content->Add(m_catalogIndexNote, 0, wxEXPAND | wxBOTTOM, 10);

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
    m_updateBtn = new wxButton(page, ID_UPDATE_DISK, "Update");
    m_deleteBtn = new wxButton(page, ID_DELETE_DISK, "Delete");
    m_progressBar = new wxGauge(page, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 20));
    wxBoxSizer* actionSizer = new wxBoxSizer(wxHORIZONTAL);
    actionSizer->Add(m_downloadBtn, 0, wxRIGHT, 5);
    actionSizer->Add(m_updateBtn, 0, wxRIGHT, 5);
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

void SettingsDialogWx::populateROMList(const std::string& selectId) {
    m_romChoice->Clear();
    m_romFileIds.clear();

    // THE CATALOG'S roms[], and a parallel list of the ids they stand for.
    //
    // This was two hardcoded entries naming the two ROM files the package
    // shipped, with a comment arguing that a catalog ROM is not a stored
    // preference. That argument fell with the bundled ROMs: there is no packaged
    // ROM left to be the peer of anything, every ROM this application can load
    // now comes from the release's catalog, and a client that offered none of
    // them would make the second published ROM - emu_rcz80 - unreachable no
    // matter what romwbw_disks published. Publishing a ROM has to be enough.
    //
    // Ids, never filenames. A filename carries the release, so a preference
    // stored as one would be forgotten the moment the user switched 3.5.1 to
    // 3.6.0; catalogv0::chooseRom matches on id for the same reason.
    const std::vector<catalogv0::RomItem> roms =
        m_catalog ? m_catalog->getCatalogRoms() : std::vector<catalogv0::RomItem>();

    if (roms.empty()) {
        // No catalog yet - the fetch this dialog's constructor started has not
        // come back, or there is no network - or a release whose catalog carries
        // no roms[] at all, which CATALOG_SCHEMA 6.1 allows. Same shape as the
        // release picker's placeholder and for the same reason: an empty
        // dropdown invites a click that can only produce a worse answer than the
        // one already stored, and the empty id makes saveSettings() refuse to
        // write over the stored choice.
        m_romChoice->Append(selectId.empty()
                                ? wxString("(catalog not loaded)")
                                : wxString::FromUTF8(selectId));
        m_romFileIds.push_back(std::string());
        m_romChoice->SetSelection(0);
        m_romChoice->Enable(false);
        return;
    }

    for (const auto& rom : roms) {
        // The catalog's own name, with its id in parentheses so that two
        // releases' spellings of one ROM still read as the same choice. name is
        // free text and may be absent; the id is the thing 6.1 promises.
        std::string label = rom.name.empty() ? rom.id : rom.name;
        if (rom.isDefault) label += " (Default)";
        m_romChoice->Append(wxString::FromUTF8(label));
        m_romFileIds.push_back(rom.id);
    }
    m_romChoice->Enable(true);

    // The id to keep, appended as an unavailable row when this release does not
    // publish it, so that OK writes it back rather than replacing it with this
    // release's default. The preference is still theirs; it is only unavailable
    // here, and switching back must find it intact. chooseRom already treats an
    // unpublished preference as no preference for what actually BOOTS, so
    // keeping it here strands nothing.
    int idx = -1;
    for (size_t i = 0; i < m_romFileIds.size(); i++) {
        if (m_romFileIds[i] == selectId) { idx = static_cast<int>(i); break; }
    }
    if (idx < 0 && !selectId.empty()) {
        idx = m_romChoice->Append(wxString::FromUTF8(selectId) + " (not in this release)");
        m_romFileIds.push_back(selectId);
    }
    m_romChoice->SetSelection(idx < 0 ? 0 : idx);
}

// The release the picker is showing right now, or empty for the placeholder row
// and before the control has been filled. Shaped after selectedRomId() below and
// for the same reason: what the control DISPLAYS is a label the index may reword,
// so the answer comes out of m_romwbwVersionIds and never off the screen.
std::string SettingsDialogWx::selectedRomwbwVersionId() const {
    if (!m_romwbwVersionChoice) return std::string();
    const int sel = m_romwbwVersionChoice->GetSelection();
    if (sel < 0 || (size_t)sel >= m_romwbwVersionIds.size()) return std::string();
    return m_romwbwVersionIds[sel];
}

std::string SettingsDialogWx::selectedRomId() const {
    const int sel = m_romChoice->GetSelection();
    if (sel < 0 || (size_t)sel >= m_romFileIds.size()) return std::string();
    return m_romFileIds[sel];
}

void SettingsDialogWx::populateVersionList() {
    // WHAT THE CONTROL WAS HOLDING, read before Clear() wipes it.
    //
    // This is the release that must survive the snapshot filter, and it is read
    // from the CONTROL rather than only from the catalog because the two can
    // disagree for as long as a fetch is in flight: pick the snapshot, and
    // getSelectedRomwbwVersion() still names the previous release until that
    // fetch lands. Untick the box inside that window and the row the user just
    // chose would vanish underneath them. Empty on the first call and on the
    // placeholder row, which is the ordinary case and means "nothing to keep".
    const std::string showing = selectedRomwbwVersionId();

    m_romwbwVersionChoice->Clear();
    m_romwbwVersionIds.clear();

    // EVERY RELEASE THE INDEX PUBLISHES, and none of them greyed out. This was
    // getRunnableVersions(), a subset kept by asking the emulator core whether
    // it could boot each one; romwbw_emu v1.44 deleted the function behind that
    // question, because the release number is the ROM-to-disk-image pairing and
    // not a property of the core. So there is no "(needs a newer build)" row to
    // render and never was a disabled one - a release this build had not been
    // checked against simply did not appear, which is the harder failure to
    // notice.
    const std::vector<catalogv0::IndexEntry> versions =
        m_catalog ? m_catalog->getIndexVersions() : std::vector<catalogv0::IndexEntry>();

    if (versions.empty()) {
        // No index yet - the fetch this dialog's constructor started has not
        // come back, or it failed, or there is no network. Show what the
        // configuration says and disable the control rather than showing an
        // empty one: an empty dropdown invites a click that can only produce a
        // worse answer than the one already stored.
        //
        // The placeholder's id is empty on purpose, and saveSettings() refuses
        // to write an empty id over a stored one. Otherwise opening Settings on
        // an offline machine and pressing OK would silently forget which RomWBW
        // release the user had chosen - the same shape as the "(None)" erasure
        // the four disk dropdowns had.
        const std::string configured = m_settings.romwbwVersion;
        m_romwbwVersionChoice->Append(configured.empty()
                                          ? wxString("(catalog not loaded)")
                                          : wxString::FromUTF8("RomWBW " + configured));
        m_romwbwVersionIds.push_back(std::string());
        m_romwbwVersionChoice->SetSelection(0);
        m_romwbwVersionChoice->Enable(false);
        updateRomwbwVersionNote();
        return;
    }

    // catalogv0::displayLabel appends the status for anything that is not
    // "stable", so 3.6.0 reads "RomWBW 3.6.0 (preview)". That marking is not
    // decoration: the repository publishes a preview release as not yet
    // recommended, and a user choosing one is choosing images built for a ROM
    // this build does not ship.
    // WHICH ROWS EXIST. A pre-release appears only when the box is ticked, with
    // no exception for the release this machine happens to be on: unticking the
    // box MOVES such a machine back to the index default rather than leaving it
    // somewhere the list cannot show - see onShowPrereleaseChanged.
    //
    // `showing` is read at the top of this function and is still what the
    // re-selection below keys on, because the control and the catalog disagree
    // for as long as a fetch is in flight.
    const std::string selected = m_catalog ? m_catalog->getSelectedRomwbwVersion()
                                           : m_settings.romwbwVersion;
    const bool showPrerelease =
        m_showPrereleaseCheck ? m_showPrereleaseCheck->GetValue()
                              : m_settings.showPrereleaseVersions;

    for (const auto& entry : versions) {
        if (!catalogv0::isOffered(entry, showPrerelease)) continue;
        m_romwbwVersionChoice->Append(wxString::FromUTF8(catalogv0::displayLabel(entry)));
        m_romwbwVersionIds.push_back(entry.romwbwVersion);
    }

    // Every entry was filtered out: an index of nothing but snapshots, with the
    // box unticked and no release selected yet. An empty enabled dropdown is the
    // one state this control must never reach - it invites a click that can only
    // produce a worse answer than the one already stored - so it falls back to
    // the same placeholder the no-catalog path uses, and says which box to tick.
    if (m_romwbwVersionIds.empty()) {
        m_romwbwVersionChoice->Append("(only development snapshots are published)");
        m_romwbwVersionIds.push_back(std::string());
        m_romwbwVersionChoice->SetSelection(0);
        m_romwbwVersionChoice->Enable(false);
        updateRomwbwVersionNote();
        return;
    }

    m_romwbwVersionChoice->Enable(true);

    // Selected on the version the catalog in hand was actually FETCHED for,
    // which is not always the one the user asked for: a preference the index no
    // longer carries falls back to the index's default. Showing the preference
    // instead would tell the user they are looking at a catalog they are not.
    //
    // There was a second clause here - "or that this core cannot boot" - and it
    // went with catalogv0::runnableVersions on 2026-09-17. One way is left for a
    // preference to miss, and it is the document's doing rather than the build's.
    //
    // `selected` is computed above the loop, and `showing` - what the control
    // held before Clear() - wins over it while a fetch is still in flight.
    const std::string want = showing.empty() ? selected : showing;
    int idx = 0;
    for (size_t i = 0; i < m_romwbwVersionIds.size(); i++) {
        if (m_romwbwVersionIds[i] == want) {
            idx = static_cast<int>(i);
            break;
        }
    }
    m_romwbwVersionChoice->SetSelection(idx);
    updateRomwbwVersionNote();
}

void SettingsDialogWx::updateRomwbwVersionNote() {
    const int sel = m_romwbwVersionChoice->GetSelection();
    const std::string chosen =
        (sel >= 0 && (size_t)sel < m_romwbwVersionIds.size()) ? m_romwbwVersionIds[sel]
                                                              : std::string();

    // What this build actually boots, read out of the ROM in the banks rather
    // than from a compile-time pin - there is no pin any more, and the core
    // derives the release from whichever image is loaded.
    const std::string running = m_settings.loadedRomwbwRelease;

    std::string note;
    if (!chosen.empty() && !running.empty()) {
        // romServes AND NOT `==`. A ROM says what it is in two bytes and can
        // only spell "3.7.0"; `chosen` is a catalog version and names the full
        // tag, "3.7.0-dev.14". With `==` this page told a user running the
        // snapshot's own ROM that their disks "need a RomWBW 3.7.0-dev.14 ROM,
        // and the machine is running RomWBW 3.7.0" - a mismatch warning about a
        // pairing that is correct, on the one release where the warning is
        // hardest to check. See CatalogV0.h on romServes.
        if (catalogv0::romServes(chosen, running)) {
            note = "Matches the ROM in use (RomWBW " + running + "). ";
        } else {
            // THE ONE WARNING ON THIS PAGE THAT IS ABOUT DATA AND NOT TASTE,
            // and it is now a statement of what will HAPPEN rather than of what
            // cannot be fixed. RomWBW's CBIOS lives inside the disk image and
            // compares its own version against what HBF_SYSVER returns, printing
            // "*** WARNING: HBIOS/CBIOS Version Mismatch ***" when the major or
            // minor differs - so 3.6.0 disks under a 3.5.1 ROM boot into that
            // banner. The release's own ROM is what removes it, and since it is
            // fetched from the same catalog as the disks, the next start is
            // where that happens. Said here so the offer at Start is not the
            // first the user hears of it.
            note = "Disks for RomWBW " + chosen + " need a RomWBW " + chosen +
                   " ROM, and the machine is running RomWBW " + running +
                   ". Starting will offer to fetch it; without it the guest "
                   "would report an HBIOS/CBIOS version mismatch. ";
        }
    }

    // A SNAPSHOT SAYS SO HERE TOO, not only in the dropdown row.
    //
    // CATALOG_SCHEMA.md 2.3 asks a client to show the release's label "wherever
    // you name the release, not only in the picker, because for a snapshot it is
    // the surface that carries the warning" - and this note names it in the
    // sentence above. A dropdown row is read once, when it is chosen; this is on
    // screen for as long as the page is.
    //
    // Read off the index entry rather than by looking for "dev" in the version
    // string. `prerelease` is a boolean with exactly one meaning; the version is
    // an upstream tag whose shape is not this application's to predict.
    if (!chosen.empty() && m_catalog) {
        for (const auto& entry : m_catalog->getIndexVersions()) {
            if (entry.romwbwVersion != chosen) continue;
            if (entry.prerelease) {
                note += "RomWBW " + chosen + " is a pre-release. ";
            }
            break;
        }
    }

    // And the sentence that is true whichever release is selected. The count is
    // the catalog's own roms[], which may be absent or empty for a release -
    // that is a real answer and the reason this is phrased as a count rather
    // than as a promise.
    const size_t romCount = m_catalog ? m_catalog->getCatalogRoms().size() : 0;
    if (romCount > 0) {
        // "The ROM in the app is kept as the offline fallback" stood at the end
        // of this sentence and outlived the files it described by a few hours:
        // the app ships no ROM. Nothing compiles a string literal, so the only
        // thing that catches one that has stopped being true is somebody reading
        // it - which is exactly how this was caught.
        note += "This catalog publishes " + std::to_string(romCount) + " ROM" +
                (romCount == 1 ? "" : "s") +
                "; the one chosen below is fetched and checked against its "
                "published size and checksum before the machine starts. "
                "Changing the release moves the disks below to match it.";
    } else {
        // A release whose catalog carries no roms[] at all, which
        // CATALOG_SCHEMA 6.1 allows. This used to end "can only be started with
        // the ROM the app ships", which now describes nothing: there is no such
        // ROM, so this release cannot be started at all, and saying so is the
        // honest form. The gate refuses it rather than substituting another
        // release's ROM, which is the whole point of the gate.
        note += "This catalog publishes no ROM, so this release cannot be "
                "started - there is no ROM to fetch and none in the app. "
                "Choose another release.";
    }

    m_romwbwVersionNote->SetLabel(wxString::FromUTF8(hardWrap(note, kNoteCols)));
    // The line breaks come from hardWrap above rather than from Wrap(), which
    // does nothing from this call site - see kNoteCols. It was
    // `Wrap(GetSize().GetWidth())` guarded at >100, and it was unreliable in a
    // second way as well: on any call after the first, the size read back is the
    // size of the already-wrapped block rather than the room available. Measured
    // on 2026-09-08, the "publishes no ROM" form drew on two lines when the
    // dialog opened and on one, cut off at "there is", after pressing Refresh.
    // THE MACHINE PAGE, because that is where this note now is. It said
    // m_diskImagesPage until 2026-09-18 and was right until the release picker
    // moved; left alone it would relayout and repaint a panel the label is not
    // on, which is precisely the clipped-and-stale drawing the comment above
    // records having been measured once already.
    if (m_machinePage->GetSizer()) m_machinePage->Layout();
    if (m_machinePage) m_machinePage->Refresh();   // see updateCatalogIndexNote
}

std::string SettingsDialogWx::typedCatalogIndexUrl() const {
    // utf8_str() and not ToStdString(): the value goes back out through
    // wxString::FromUTF8 and into a UTF-8 JSON document, and ToStdString()
    // converts in the CURRENT LOCALE, which is a different function. This is the
    // one free-text field on the page, so it is the one place the asymmetry
    // could ever have shown - as a URL that came back mangled on a machine whose
    // ANSI code page is not the one it was typed on.
    return catalogv0::normalizedIndexSetting(
        std::string(m_catalogIndexUrlText->GetValue().utf8_str()));
}

// The sentence under the Catalog index field.
//
// SEPARATE FROM loadSettings, WRAPPED, AND RE-RUN ON EVERY EDIT. It was none of
// those three, and each one was a defect on its own:
//
//   - It was set once, inside loadSettings, so "In use:" named the URL the
//     dialog OPENED with. Type a new one and the line under it went on quietly
//     naming the old one, which is the opposite of what a field whose whole job
//     is "which catalog am I reading?" should do.
//   - It was never Wrap()ped, while updateRomwbwVersionNote() directly above
//     wraps the note directly above this one. A wxStaticText does not wrap
//     unless it is told to, so the longest form - the custom-catalog warning -
//     was drawn as one line and clipped after about seventy characters. What
//     was cut off was the whole of the warning about the shared data folder and
//     the whole of the "In use:" URL: on screen the user got "Using a custom
//     catalog. Downloads are still checked against that catalog's" and nothing
//     more. That warning is the mitigation the un-isolated data folder was
//     shipped on, so it was the one line on the page that had to be readable.
//   - The environment branch said only that the variable wins, and skipped the
//     shared-folder warning entirely - even though the variable is the mechanism
//     the header nominates FIRST for pointing at somebody else's catalog, so the
//     path most likely to be used was the one carrying no warning at all.
void SettingsDialogWx::updateCatalogIndexNote() {
    // EVT_TEXT can reach this before the page is finished: the text control is
    // created a few lines before the label it writes to, and wxTextCtrl::SetValue
    // fires the event. Both pointers or nothing.
    if (!m_catalogIndexUrlText || !m_catalogIndexNote) return;

    // One reader of the variable, in catalogv0, so that "set" cannot mean
    // something different here from what indexUrl() acts on. This tested
    // `env != nullptr && *env != '\0'`, which reads a variable holding a single
    // space as set: the field was disabled and the note announced an override
    // that indexUrl() had already thrown away as whitespace, leaving the user
    // unable to edit a setting that was in force.
    std::string fromEnv;
    const bool haveEnv = catalogv0::indexUrlFromEnvironment(fromEnv);

    // What the FIELD says, not what the configuration says: nothing is stored
    // until OK, and this line has to describe the state on screen.
    const std::string typed = typedCatalogIndexUrl();

    m_catalogIndexUrlText->Enable(!haveEnv);

    const std::string inUse = haveEnv ? fromEnv : catalogv0::indexUrl(typed);
    const bool custom = inUse != std::string(catalogv0::INDEX_URL);

    // The wording lives in catalogIndexNoteText(), beside hardWrap, because
    // createControls() has to reserve height for the longest form it can return
    // and cannot do that from a copy of the words kept here.
    const std::string note = catalogIndexNoteText(haveEnv, custom);
    // The URL on a line of its own: it is one unbreakable word, so leaving it in
    // the flow would push the sentence before it onto a short line for nothing,
    // and it is the line most likely to be read and copied.
    m_catalogIndexNote->SetLabel(
        wxString::FromUTF8(hardWrap(note, kNoteCols) + "\n" +
                           hardWrap("In use: " + inUse, kNoteCols)));
    if (m_diskImagesPage->GetSizer()) m_diskImagesPage->Layout();
    // Repainted, not just re-laid-out. Layout() gives the label its new height;
    // it does not paint the strip that height just uncovered, so a note that
    // grew by a line drew the old text and left the new line blank until
    // something else happened to invalidate the page. Measured on 2026-09-08:
    // the "In use:" URL was on the control and readable through WM_GETTEXT
    // while the screen showed "In use:" with nothing after it.
    if (m_diskImagesPage) m_diskImagesPage->Refresh();
}

void SettingsDialogWx::onCatalogIndexUrlChanged(wxCommandEvent& event) {
    // Nothing is fetched and nothing is stored here - see the note on the
    // declaration. Refresh is what goes to the network, OK is what persists.
    updateCatalogIndexNote();
}

void SettingsDialogWx::populateDiskLists() {
    for (int i = 0; i < 4; i++) {
        m_diskChoices[i]->Clear();
        m_diskChoices[i]->Append("(None)");

        // Add downloaded disks from the catalog, under the name the FILE has -
        // which is the name the catalog gives it. The two were different strings
        // for the one release in which the images had been renamed to their
        // interface-v0 names and the catalog had not, and a
        // DiskCatalog::getLocalName() stood here to bridge them; the catalog
        // serves v0 names now, so it mapped nothing and is gone. This list still
        // has to say the name of the FILE, because what MainWindow seeds these
        // controls with is the basename of the configured path and what it reads
        // back out of them is a filename it resolves through
        // DiskCatalog::getDiskPath: offering a name the file does not have makes
        // the seed miss the entry it means and lands a second copy of the same
        // disk in the list.
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
    m_catalogRowFilenames.clear();

    if (!m_catalog) return;

    const auto& entries = m_catalog->getCatalogEntries();
    for (size_t i = 0; i < entries.size(); i++) {
        long idx = m_catalogList->InsertItem(i, wxString::FromUTF8(entries[i].filename));
        // Recorded beside the row, not read back off it. See the note on
        // m_catalogRowFilenames.
        m_catalogRowFilenames.push_back(entries[i].filename);
        m_catalogList->SetItem(idx, 1, wxString::FromUTF8(entries[i].description));
        // The status column answers "have you got it" AND, since 1.0.25,
        // "is it still the one the catalog names" - which the previous line
        // could not, because it read a size and both hd1k_combo.img images are
        // 51,380,224 bytes. DiskLedger::describe is deliberately the only place
        // the wording lives, so a verdict cannot be reworded in one dialog and
        // not the other. It is consulted only for a file that is actually here:
        // freshness stays NotInstalled until a fetch has computed it, and
        // isDownloaded is what a stat says right now.
        m_catalogList->SetItem(idx, 2,
                               entries[i].isDownloaded
                                   ? DiskLedger::describe(entries[i].freshness)
                                   : "Available");
    }
}

void SettingsDialogWx::setSettings(const WxEmulatorSettings& settings) {
    m_settings = settings;
    loadSettings();
    // The keyboard model is built HERE, where m_settings first arrives, and
    // deliberately not inside loadSettings(). loadSettings() only copies
    // m_settings into controls and is safe to run twice; rebuildKeyRows()
    // throws the model away and clears m_keysDirty, so a second run would
    // discard the user's rebinds without a word. That is not hypothetical:
    // onCatalogLoaded used to call loadSettings() from a download callback to
    // reapply the disk dropdowns, which is why it now calls
    // loadDiskSelections() instead.
    rebuildKeyRows();
    populateKeyList();

    // NOW the catalog fetch, and not one line earlier. This is the first moment
    // the Catalog index field holds the stored URL, and onRefreshCatalog reads
    // that field to decide which index to fetch - see the constructor for what
    // starting it there cost. setSettings() is called exactly once per dialog
    // (ShowWxSettingsDialogInternal, immediately after construction), so this
    // starts one fetch, as the constructor did.
    OutputDebugStringA("[Settings] setSettings: starting catalog refresh\n");
    wxCommandEvent evt;
    onRefreshCatalog(evt);
}

void SettingsDialogWx::loadDiskSelections() {
    for (int i = 0; i < 4; i++) {
        if (m_settings.diskFiles[i].empty()) continue;

        const wxString configured = wxString::FromUTF8(m_settings.diskFiles[i]);
        int idx = m_diskChoices[i]->FindString(configured);
        if (idx == wxNOT_FOUND) {
            // APPEND IT AND SELECT IT. Leaving the control where it was - at
            // index 0, "(None)" - is what erased the user's disk configuration,
            // and the whole chain is four steps in three files with nothing said
            // at any of them: this control stays at 0, saveSettings() below
            // writes "" for a selection of 0, MainWindow::onEmulatorSettings
            // reads "" as closeDisk(i) and disks[i] = nullopt, and its
            // saveSettings() persists that. One visit to Settings and one press
            // of OK, no confirmation, no diagnostic.
            //
            // And the list not carrying a configured disk is ordinary, not
            // exceptional. populateDiskLists() offers only catalog entries whose
            // isDownloaded is true, so it never carries a disk opened with
            // File > Load Disk or an image the user put in the data folder by
            // hand; it carries NOTHING at all until the catalog fetch this
            // dialog's constructor started comes back, which on a machine with
            // no network it never does.
            idx = m_diskChoices[i]->Append(configured);
        }
        m_diskChoices[i]->SetSelection(idx);
    }
}

void SettingsDialogWx::repopulateDiskLists() {
    // Read the four controls before emptying them. Index 0 is "(None)", which
    // is a choice in its own right and comes back as the empty string, so a
    // deliberate "(None)" survives a refill as surely as a chosen disk does.
    std::string chosen[4];
    for (int i = 0; i < 4; i++) {
        int sel = m_diskChoices[i]->GetSelection();
        if (sel > 0) chosen[i] = m_diskChoices[i]->GetString(sel).ToStdString();
    }

    populateDiskLists();

    for (int i = 0; i < 4; i++) {
        if (chosen[i].empty()) continue;
        const wxString want = wxString::FromUTF8(chosen[i]);
        int idx = m_diskChoices[i]->FindString(want);
        // Appended for the same reason loadDiskSelections() appends: the list
        // carries only downloaded catalog images, and the disk that was chosen
        // may have just been deleted, or may never have been one of them.
        if (idx == wxNOT_FOUND) idx = m_diskChoices[i]->Append(want);
        m_diskChoices[i]->SetSelection(idx);
    }
}

void SettingsDialogWx::loadSettings() {
    // ROM selection, round-tripped rather than snapped to the first entry.
    //
    // Rebuilt here as well as in the constructor so that this is idempotent: the
    // append inside must not stack up a second copy if this ever runs twice.
    populateROMList(m_settings.romFile);

    // THE SLOTS ARE NOT SETTLED until a catalog has landed and any reconcile it
    // triggers has finished. The constructor already started that fetch, so by
    // the time settings arrive it is in flight and the four dropdowns below are
    // showing whatever the configuration named - which is exactly the state that
    // looked current and was not.
    setSlotsSettling(true);

    // BEFORE populateVersionList(), which reads this control to decide which
    // rows exist. Set from the setting and pushed to the catalog in the same
    // breath, so that a fetch started by anything else while this dialog is open
    // applies the rule the box on screen is showing.
    if (m_showPrereleaseCheck) {
        m_showPrereleaseCheck->SetValue(m_settings.showPrereleaseVersions);
        if (m_catalog) m_catalog->setShowPrereleaseVersions(m_settings.showPrereleaseVersions);
    }

    // Before any catalog has landed this puts up the placeholder, which is why
    // it has to run here as well as in onCatalogLoaded: setSettings() is the
    // first moment the configured release and the running ROM's release are
    // known, and the note under the control is about both of them.
    populateVersionList();

    loadDiskSelections();

    // The catalog index, and whether it is this machine's to change. An
    // environment variable set for the run wins over the setting, so showing an
    // editable field would be a lie in that case.
    //
    // Remembered as well as shown: Refresh pushes whatever is in the field to
    // the DiskCatalog, so Cancel has to be able to put it back, exactly as it
    // does for the RomWBW release.
    m_catalogIndexUrlOnOpen = m_settings.catalogIndexUrl;
    m_catalogIndexUrlText->SetValue(wxString::FromUTF8(m_settings.catalogIndexUrl));
    updateCatalogIndexNote();

    // Debug mode
    m_debugCheck->SetValue(m_settings.debugMode);

    // Warn on manifest writes
    m_warnManifestCheck->SetValue(m_settings.warnManifestWrites);

    // Terminal scrollback
    m_scrollbackSpin->SetValue(m_settings.scrollbackLines);

    // The bell
    m_bellCheck->SetValue(m_settings.bellEnabled);

    // The app-shortcut keys, inverted: the config member says whether the key
    // goes to CP/M, the checkbox says whether the app keeps it.
    m_f1HelpCheck->SetValue(!m_settings.f1ToCpm);
    m_f5StartStopCheck->SetValue(!m_settings.f5ToCpm);
    m_ctrlRResetCheck->SetValue(!m_settings.ctrlRToCpm);

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
    // ROM selection, read out of the id list rather than out of a switch on the
    // index. An unrecognised selection leaves the seeded value alone instead of
    // falling through to a hardcoded name: writing a ROM the user never chose is
    // how the running ROM got replaced by the bundled one, and "the control said
    // something I cannot map" is not a reason to change the machine.
    //
    // The empty id is the placeholder row shown before a catalog has landed, and
    // it is refused for exactly the reason the release picker refuses its own:
    // opening Settings with no network and pressing OK must not turn "I chose
    // emu_rcz80" into "no preference".
    {
        const int sel = m_romChoice->GetSelection();
        if (sel >= 0 && (size_t)sel < m_romFileIds.size() &&
            !m_romFileIds[sel].empty()) {
            m_settings.romFile = m_romFileIds[sel];
        }
    }

    // The catalog's RomWBW release, and ONLY when the control is really holding
    // one. The placeholder row shown before a catalog has been fetched carries
    // an empty id, and writing that back would turn "I chose 3.6.0" into "no
    // preference" for anyone who opened Settings with no network - a silent
    // downgrade of a stored choice by a dialog that could not display it.
    {
        const int sel = m_romwbwVersionChoice->GetSelection();
        if (sel >= 0 && (size_t)sel < m_romwbwVersionIds.size() &&
            !m_romwbwVersionIds[sel].empty()) {
            m_settings.romwbwVersion = m_romwbwVersionIds[sel];
        }
    }

    // Unconditionally, unlike the release above it. The placeholder problem does
    // not exist here: this control is filled from the setting and read back from
    // itself, so an offline machine returns exactly what it was handed. There is
    // no state in which it holds something it could not display.
    if (m_showPrereleaseCheck) {
        m_settings.showPrereleaseVersions = m_showPrereleaseCheck->GetValue();
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

    // The catalog index, trimmed and normalized: EMPTY for the built-in index,
    // whether the field was left blank or the built-in URL was pasted into it.
    //
    // Pasting it in is the obvious thing for a user to do, because the line
    // under the field shows the URL in use and invites copying. Storing that
    // string would pin this install to whatever the default was on the day it
    // was pasted, which is the freeze Config::catalogIndexUrl's own comment says
    // empty exists to prevent - the setting would look like a preference and
    // behave like a version pin.
    m_settings.catalogIndexUrl = typedCatalogIndexUrl();

    // Debug mode
    m_settings.debugMode = m_debugCheck->GetValue();

    // Warn on manifest writes
    m_settings.warnManifestWrites = m_warnManifestCheck->GetValue();

    // Terminal scrollback
    m_settings.scrollbackLines = m_scrollbackSpin->GetValue();

    // The bell
    m_settings.bellEnabled = m_bellCheck->GetValue();

    // The app-shortcut keys, back into the config's own sense.
    m_settings.f1ToCpm = !m_f1HelpCheck->GetValue();
    m_settings.f5ToCpm = !m_f5StartStopCheck->GetValue();
    m_settings.ctrlRToCpm = !m_ctrlRResetCheck->GetValue();

    // The key bindings, only if a row actually changed. Left ungated, every OK
    // would rewrite "keyboard.keys" from this model whether the user had opened
    // the Keyboard page or not.
    if (m_keysDirty) {
        std::map<std::string, std::string> out = m_keyCarry;
        for (const KeyRow& row : m_keyRows) {
            // A row the file had no entry for and nobody touched stays absent.
            // Writing it would mean this dialog inventing entries for keys the
            // user never asked about; ConfigManager::load() is the thing that
            // seeds missing defaults into the file, and it already does.
            if (!row.hasEntry && !row.edited) continue;
            out[row.name] = row.sequence;
        }
        m_settings.keyBindings = out;
        m_settings.keyBindingsDirty = true;
    }

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

    // WHICH INDEX THIS FETCH IS FOR, pushed before it starts and taken from the
    // FIELD - the same thing onRomwbwVersionChanged does with the release a few
    // functions below, and for the identical reason: the list this fetch fills
    // in has to be the list the controls above it are describing.
    //
    // Without this, Refresh re-fetched whatever was in the configuration, and
    // the configuration is written only by OK. So the one workflow the setting
    // exists for - paste the URL of an unpublished romwbw_disks release, press
    // Refresh, look at what it publishes - fetched the BUILT-IN catalog, filled
    // the release dropdown and the disk list from it, and said "Catalog loaded".
    // Nothing on screen said the URL had been ignored. Cancel puts this back.
    m_catalog->setCatalogIndexUrl(typedCatalogIndexUrl());

    m_statusText->SetLabel("Loading disk catalog...");
    m_refreshBtn->Enable(false);
    // And the release control, which is now the OTHER way a fetch can be
    // started. Disabling the button was enough while it was the only one;
    // choosing a release calls straight back into here, so two clicks in a
    // second would leave two fetches in flight. That matters because
    // DiskCatalog writes m_catalogEntries under one mutex and m_selectedVersion
    // under another - deliberately, since neither may be held across the other -
    // so two overlapping workers can leave the list showing one release's disks
    // under the other release's name. The downloads themselves stay honest
    // either way (the URL and the sha256 come out of the entry), but the label
    // above them would be lying. populateVersionList() re-enables it, and it is
    // called from onCatalogLoaded on BOTH the success and the failure path.
    m_romwbwVersionChoice->Enable(false);

    // The raw dialog pointer is still what wxPostEvent needs, but it is no
    // longer what the worker is trusted with: the gate copied alongside it is
    // the permission to use it. fetchCatalog runs this callback on a detached
    // thread seconds later, by which time ShowModal() may have returned and
    // taken the dialog off the stack with it.
    SettingsDialogWx* dlg = this;
    auto gate = m_postGate;

    m_catalog->fetchCatalog([dlg, gate](bool success, const std::vector<DiskEntry>& entries, const std::string& error) {
        // Built outside the gate on purpose: constructing and filling the event
        // touches nothing the dialog owns, and the gate is held by the UI
        // thread's destructor while it waits, so it is kept down to the post.
        wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, ID_CATALOG_LOADED);
        evt.SetInt(success ? 1 : 0);
        evt.SetString(wxString::FromUTF8(error));
        // Post event to main thread - only if the dialog is still there, and
        // atomically with respect to ~SettingsDialogWx deciding that it is not.
        gate->postIfOpen([&] { wxPostEvent(dlg, evt); });
    });
}

void SettingsDialogWx::onCatalogLoaded(wxCommandEvent& event) {
    m_refreshBtn->Enable(true);

    if (event.GetInt()) {
        // The version list first: everything below it is about the release it
        // names, and populateCatalog() has just been handed that release's
        // entries.
        populateVersionList();
        // The ROM list is about the release that just landed, so it is rebuilt
        // beside the version list rather than left holding the previous
        // release's roms[] - or the "(catalog not loaded)" placeholder, which is
        // what every first open of this dialog shows until now. What is kept is
        // the id the control is HOLDING, not the one in m_settings: a user who
        // changed the dropdown while the fetch was in flight must not have it
        // put back, which is the same rule repopulateDiskLists follows below.
        {
            const std::string keep = selectedRomId();
            populateROMList(keep.empty() ? m_settings.romFile : keep);
        }
        populateCatalog();
        // Only the dropdowns, which have to be emptied and refilled now that
        // the catalog names some entries. This used to be loadSettings(), which
        // reset every control on the dialog from m_settings - so a checkbox
        // ticked before the catalog arrived was put back without a word; the
        // same is true one level down of a DISK ticked in the meantime, which is
        // why what goes back is what is chosen and not what was seeded.
        repopulateDiskLists();
        m_statusText->SetLabel("Catalog loaded");

        // RECONCILE WHENEVER THE SLOTS AND THE RELEASE DISAGREE, and not only
        // when the picker was the thing that moved.
        //
        // This was guarded on "did the user just change the release?" and that
        // was too narrow by exactly the case a user hit on 2026-09-18. They had
        // CHOSEN 3.7.0-dev.14 in an earlier session. It was still chosen on the
        // next launch, so the picker never moved, so nothing fired, and Settings
        // opened showing release 3.7.x over four 3.5.1 disks with no 3.7 image
        // available to pick. The disagreement is a property of the machine, not
        // of a control having been touched, so that is what is tested.
        //
        // It cannot loop. Once the slots hold the selected release's images
        // there is nothing to disagree about, and a slot whose id the new
        // release retired resolves to no entry and is left alone - so a second
        // pass over the same state queues nothing.
        if (m_catalog && !m_catalog->getSelectedRomwbwVersion().empty() &&
            slotsDisagreeWithRelease()) {
            m_statusText->SetLabel("Matching the disks to RomWBW " +
                                   wxString::FromUTF8(m_catalog->getSelectedRomwbwVersion()) +
                                   "...");
            captureSlotIds();
            m_switchPlanPending = true;
            tryStartReleaseSwitchFetch();
        }

        // Settled unless a reconcile is actually going to do something. Both
        // flags are cleared by the paths that finish or abandon a chain, so this
        // is the one place that has to decide the no-work case.
        if (!m_switchPlanPending && !m_switchActive) setSlotsSettling(false);
    } else {
        m_statusText->SetLabel("Failed to load catalog: " + event.GetString());
        // Nothing more is coming, so the slots are as settled as they will get.
        // Leaving them disabled would strand the four controls a user with no
        // network most needs.
        setSlotsSettling(false);
        // Refilled on the failure path too, and it does two things here.
        // getIndexVersions() is only ever written by a fetch that SUCCEEDED,
        // so the list itself comes back unchanged - what moves is the
        // SELECTION, which is put back onto getSelectedRomwbwVersion(), the
        // release whose entries are actually in hand. So a switch to 3.6.0 that
        // could not be fetched snaps the control back to 3.5.1, and OK then
        // writes 3.5.1 rather than storing a preference the user never got. It
        // is also what re-enables the control after onRefreshCatalog disabled
        // it; without this call a failed fetch would leave it dead for the rest
        // of the dialog's life.
        populateVersionList();
        // Same reasoning one control up: getCatalogRoms() is only written by a
        // fetch that succeeded, so a failed one leaves the rows alone and this
        // re-enables the control and restores the selection.
        {
            const std::string keep = selectedRomId();
            populateROMList(keep.empty() ? m_settings.romFile : keep);
        }
    }
}

// The user picked another RomWBW release. Tell the catalog and re-fetch, so the
// list below the choice is the one they just asked for.
//
// NOTHING IS DELETED, DOWNLOADED OR UNMOUNTED HERE, and nothing may ever be: a
// version switch is the exact operation that destroyed a library on the iOS
// port, which keyed its "delete the images the catalog names" behaviour on one
// global catalog version. Here there is nothing to invalidate - every v0
// filename carries its release, so 3.5.1 and 3.6.0 images sit side by side in
// one folder - and switching back and forth costs two small HTTP GETs.
//
// The choice is written back to the configuration only by OK, through
// saveSettings(). Cancel puts the catalog's preference back to what it was.
void SettingsDialogWx::onRomwbwVersionChanged(wxCommandEvent& event) {
    if (!m_catalog) return;
    const int sel = m_romwbwVersionChoice->GetSelection();
    if (sel < 0 || (size_t)sel >= m_romwbwVersionIds.size()) return;
    if (m_romwbwVersionIds[sel].empty()) return;   // the placeholder row

    // A SECOND SWITCH ABANDONS THE FIRST. downloadDisk refuses a concurrent
    // download, so without this the new chain's first image would be refused by
    // the old chain's in-flight one and the queue would stall with the slots
    // half-moved.
    abandonReleaseSwitchFetch();

    // The ids are NOT captured here. They were, on the reasoning that only the
    // outgoing catalog could map a filename to an id - true, and beside the
    // point, because the case that actually needed fixing is the one where no
    // catalog in the process has ever seen those filenames. captureSlotIds()
    // runs when the new catalog lands and reads the name when the document
    // cannot help, so there is nothing to do before the refetch.
    m_catalog->setPreferredRomwbwVersion(m_romwbwVersionIds[sel]);
    updateRomwbwVersionNote();
    onRefreshCatalog(event);
}

// The "Show development snapshots" box. ROWS APPEAR AND DISAPPEAR AND NOTHING
// ELSE HAPPENS: no fetch, no download, nothing unmounted, and the release the
// machine runs is not touched.
//
// It does not call onRefreshCatalog, and that is deliberate rather than an
// omission. The index is already in hand - populateVersionList() reads
// getIndexVersions(), which holds every entry the document named, prereleases
// included - so showing them is a filter over data this dialog already has.
// Re-fetching would put a network round trip behind a checkbox and could fail,
// leaving the box ticked and the list unchanged.
//
// The catalog is told too, so that a fetch started later by anything else -
// Refresh, a release change, Start - applies the same rule. What it must NOT be
// allowed to do is move the selection: catalogv0::chooseVersion honours a stored
// preference for a snapshot whatever this says, for the reason written there.
//
// Nothing is stored until OK, like every other control on this page; Cancel puts
// the catalog's copy back.
void SettingsDialogWx::onShowPrereleaseChanged(wxCommandEvent& event) {
    (void)event;
    if (!m_showPrereleaseCheck) return;

    // WRITTEN INTO m_settings HERE, not left to saveSettings(), because this one
    // control is committed by TOGGLING it and not by pressing OK.
    //
    // Everything else on this page describes the machine and is therefore the
    // dialog's to hold until OK. This describes the LIST - which rows the picker
    // above it shows - and toggling it already rearranges that list in front of
    // the user, so it has visibly taken effect before OK is anywhere near. A
    // user unticked it, closed the dialog without OK, and found it ticked again
    // on the next launch; measured afterwards, their z80cpmw.json still said
    // true and had not been rewritten. Reported on 2026-09-18.
    //
    // ShowWxSettingsDialogInternal copies this field back on Cancel as well as
    // on OK, and MainWindow applies and saves it either way.
    const bool show = m_showPrereleaseCheck->GetValue();
    m_settings.showPrereleaseVersions = show;
    if (m_catalog) m_catalog->setShowPrereleaseVersions(show);

    // TURNING IT OFF MOVES A MACHINE THAT IS ON A PRE-RELEASE, ROM and disks
    // included. Reported 2026-09-18: the box was unticked and a `-dev` release
    // stayed selected, which is a machine in a state its own Settings page says
    // it is not in.
    //
    // This is a reversal. The box governed VISIBILITY only, deliberately,
    // because moving the release used to strand the four slots on images built
    // for the release being left - the HBIOS/CBIOS mismatch the picker exists to
    // prevent. The reconcile removed that objection: a release change now takes
    // the disks with it, so the move is safe and is the whole point of the
    // setting.
    //
    // The PREFERENCE is cleared rather than set to the default by name. Empty
    // means "no preference", which catalogv0::chooseVersion answers with the
    // index's own `default: true` entry - so the answer comes from the document
    // rather than from a release number written down here, and re-ticking the
    // box does not silently jump back to a snapshot the user has left.
    if (!show && m_catalog) {
        bool onPrerelease = false;
        const std::string sel = m_catalog->getSelectedRomwbwVersion();
        for (const auto& e : m_catalog->getIndexVersions()) {
            if (e.romwbwVersion == sel && e.prerelease) { onPrerelease = true; break; }
        }
        if (onPrerelease) {
            abandonReleaseSwitchFetch();
            m_catalog->setPreferredRomwbwVersion(std::string());
            setSlotsSettling(true);
            m_statusText->SetLabel("Leaving the pre-release...");
            updateRomwbwVersionNote();
            // The fetch brings the default release's catalog, and the reconcile
            // in onCatalogLoaded moves the four slots onto its images.
            onRefreshCatalog(event);
            return;
        }
    }

    // Re-selects by version STRING and not by the row index it held a moment
    // ago, which is the whole reason this goes through populateVersionList()
    // rather than adding and removing items in place: the filter changes what
    // row 2 means, so a remembered index would silently select another release.
    populateVersionList();
}

void SettingsDialogWx::onDownloadDisk(wxCommandEvent& event) {
    if (!m_catalog) return;

    long sel = m_catalogList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel < 0) {
        wxMessageBox("Please select a disk to download", "Info", wxOK | wxICON_INFORMATION);
        return;
    }

    if ((size_t)sel >= m_catalogRowFilenames.size()) return;
    const std::string filenameStr = m_catalogRowFilenames[sel];
    const wxString filename = wxString::FromUTF8(filenameStr);

    // "Already downloaded" is a question about PROVENANCE, not about size.
    //
    // This was `if (isDiskDownloaded(...)) refuse`, and isDiskDownloaded is a
    // size test - DiskCatalog.cpp, `size >= expectedSize`. Two images published
    // under one name by two catalogs are the same size far more often than not
    // (a 3.5.1 and a 3.6.0 hd1k_combo both being one hd1k slice set), so the one
    // manual way out of a wrong file in the data folder was closed by the check
    // that was supposed to save a redundant download. The freshness column knew
    // the file was superseded and the button still said no.
    //
    // So: refuse only what the ledger says is genuinely the published image, and
    // for anything superseded say what replacing it costs. The warning matters
    // because a downloaded image is a MOUNTED VOLUME - the user's own writes
    // live inside it, and the catalog cannot give those back.
    const DiskFreshness fresh = m_catalog->getFreshness(filenameStr);
    if (fresh == DiskFreshness::Current || fresh == DiskFreshness::UnknownProvenanceMatches) {
        wxMessageBox("This disk is already downloaded, and it matches what the "
                     "catalog publishes now.",
                     "Info", wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (fresh == DiskFreshness::SupersededModified ||
        fresh == DiskFreshness::UnknownProvenanceDiffers) {
        const int answer = wxMessageBox(
            "The copy of " + filename + " in the data folder is not the image "
            "this catalog publishes, and its bytes have changed since it was "
            "written - anything you have saved inside it is in those bytes.\n\n"
            "Downloading replaces the file. That cannot be undone, and the "
            "catalog cannot give your work back.\n\n"
            "Replace it?",
            "Replace a disk you have written to?", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this);
        if (answer != wxYES) return;
    } else if (fresh == DiskFreshness::SupersededPristine) {
        const int answer = wxMessageBox(
            "The copy of " + filename + " in the data folder came from a "
            "different catalog entry. Its bytes are still exactly as they were "
            "downloaded, so replacing it discards nothing of yours.\n\n"
            "Download the version this catalog publishes?",
            "Replace a superseded disk?", wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION, this);
        if (answer != wxYES) return;
    } else if (m_catalog->isDiskDownloaded(filenameStr)) {
        // Unverifiable (the catalog gives no sha256) or NeedsMeasurement (not
        // hashed yet). Nothing may be decided from provenance, so fall back to
        // the size test that was here before rather than inventing an answer.
        wxMessageBox("This disk is already downloaded", "Info", wxOK | wxICON_INFORMATION, this);
        return;
    }

    beginDiskDownload(filenameStr, filename);
}

void SettingsDialogWx::beginDiskDownload(const std::string& filenameStr,
                                         const wxString& filename) {
    m_statusText->SetLabel("Downloading " + filename + "...");
    m_downloadBtn->Enable(false);
    m_updateBtn->Enable(false);
    m_progressBar->SetValue(0);

    // Gated for exactly the reason onRefreshCatalog is, because this is the
    // same shape and was the same crash waiting to be reported: downloadDisk
    // runs both of these on a detached thread against a stack dialog. It is in
    // fact the worse of the two - the progress callback fires once per read
    // block, at most 64KB apiece, for the whole of a multi-megabyte disk image,
    // so it is posting into the dialog over and over for as long as the
    // download lasts, and closing Settings at any point in that window used to
    // be a free access violation.
    // The only reason the catalog fetch is the one with dumps behind it is that
    // it starts by itself from the constructor, where downloading needs a user
    // to have clicked Download or Update first.
    SettingsDialogWx* dlg = this;
    auto gate = m_postGate;

    m_catalog->downloadDisk(filenameStr,
        [dlg, gate](size_t current, size_t total) {
            wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, ID_DOWNLOAD_PROGRESS);
            evt.SetInt(total > 0 ? (int)(current * 100 / total) : 0);
            gate->postIfOpen([&] { wxPostEvent(dlg, evt); });
        },
        [dlg, gate](bool success, const std::string& error) {
            wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, ID_DOWNLOAD_COMPLETE);
            evt.SetInt(success ? 1 : 0);
            evt.SetString(wxString::FromUTF8(error));
            gate->postIfOpen([&] { wxPostEvent(dlg, evt); });
        }
    );
}

//=============================================================================
// Following the four slots across a release switch
//
// See the note on SlotFetch in SettingsDialogWx.h for the rules these functions
// exist to keep.
//=============================================================================

void SettingsDialogWx::captureSlotIds() {
    for (int i = 0; i < 4; i++) m_switchIds[i] = std::string();
    if (!m_catalog) return;

    // THE CATALOG IN HAND FIRST, THEN THE NAME. Both, because neither alone
    // covers the two cases this has to serve.
    //
    // The catalog is the authority whenever it knows the file, and it knows it
    // whenever the slots and the selected release already agree.
    //
    // BUT THE CASE THAT MATTERS MOST IS THE ONE WHERE THEY DO NOT. A machine
    // that stored 3.7.0-dev.14 last session and still has 3.5.1 images in its
    // slots opens with the 3.7.0-dev.14 catalog in hand, and that document has
    // never heard of hd1k_combo-v0-3.5.1.img - so a catalog lookup finds nothing
    // for every slot, and the reconcile that should fix the machine does nothing
    // at all. That is exactly the state a user reported on 2026-09-18: release
    // 3.7.x, disks 3.5.1, and no 3.7 image anywhere to pick.
    //
    // So the filename is read as a fallback, through diskv0::idOfV0Name, whose
    // declaration carries why that does not break CATALOG_SCHEMA 6.1's "key on
    // id, not by parsing filename": the parse yields a CANDIDATE, and
    // beginReleaseSwitchFetch only ever acts on a candidate that an entry in the
    // new catalog actually carries.
    const std::vector<DiskEntry> entries = m_catalog->getCatalogEntries();
    for (int i = 0; i < 4; i++) {
        if (!m_diskChoices[i]) continue;
        const int sel = m_diskChoices[i]->GetSelection();
        if (sel <= 0) continue;              // 0 is "(None)"
        const std::string name = m_diskChoices[i]->GetString(sel).ToStdString();
        for (const auto& e : entries) {
            if (e.filename == name) { m_switchIds[i] = e.id; break; }
        }
        if (m_switchIds[i].empty()) {
            std::string id;
            if (diskv0::idOfV0Name(name, id)) m_switchIds[i] = id;
        }
        // Still empty means no catalog names this file and its name is not one
        // this scheme ever produced - an image the user browsed to themselves.
        // Such a slot is not followed anywhere: it is their file, and moving it
        // would be this application deciding something about a file it never
        // fetched.
    }
}

bool SettingsDialogWx::slotsDisagreeWithRelease() const {
    if (!m_catalog) return false;
    const std::string selected = m_catalog->getSelectedRomwbwVersion();
    if (selected.empty()) return false;

    for (int i = 0; i < 4; i++) {
        if (!m_diskChoices[i]) continue;
        const int sel = m_diskChoices[i]->GetSelection();
        if (sel <= 0) continue;
        const std::string name = m_diskChoices[i]->GetString(sel).ToStdString();
        std::string release;
        // Only a v0 name can disagree. A file with no release in its name is the
        // user's own and is not this function's business.
        if (!diskv0::releaseOfV0Name(name, release)) continue;
        if (release != selected) return true;
    }
    return false;
}

void SettingsDialogWx::tryStartReleaseSwitchFetch() {
    if (!m_switchPlanPending) return;
    // THE ONE-AT-A-TIME LOCK IS NOT RELEASED BY cancelDownload(). That call sets
    // a flag the worker reads between 64KB blocks; the transfer stays
    // Downloading until it notices, and DiskCatalog::downloadDisk refuses a
    // second one SYNCHRONOUSLY with "Download already in progress". So a second
    // release switch that cancelled the first and started immediately would have
    // its own first image refused and the chain would die at "0 of N", blaming
    // the user for a race with themselves.
    //
    // Nothing is timed or slept on. The two things this waits for each end in an
    // event - the new catalog landing, and the old transfer stopping - so
    // whichever arrives second calls this, and only then does the chain start.
    if (m_catalog && m_catalog->getDownloadState() == DownloadState::Downloading) return;
    m_switchPlanPending = false;
    beginReleaseSwitchFetch();
}

void SettingsDialogWx::beginReleaseSwitchFetch() {
    m_switchQueue.clear();
    m_switchNext = 0;
    m_switchActive = false;
    if (!m_catalog) return;

    const std::vector<DiskEntry> entries = m_catalog->getCatalogEntries();
    if (entries.empty()) return;

    // Each captured id, resolved in the release that just landed. An id the new
    // release does not publish resolves to nothing and that slot is left exactly
    // as it is - hd1k_ws4 exists in 3.5.1 and in no later release, so this is a
    // real case and not a defensive one.
    int matched = 0;
    for (int i = 0; i < 4; i++) {
        if (m_switchIds[i].empty()) continue;
        for (const auto& e : entries) {
            if (e.id != m_switchIds[i]) continue;
            matched++;
            m_switchQueue.push_back(SlotFetch{ i, e.filename });
            break;
        }
    }

    // NO SLOT CARRIED OVER: nothing was mounted, or every id has been retired.
    // The default disks are what to offer instead - the same two, in the same
    // two drives, that F5 gives a machine with nothing configured.
    //
    // THE CATALOG DOES NOT NAME A DEFAULT DISK, so this cannot be read out of the
    // document however much one would like to. A roms[] entry carries
    // `default: true`; a disks[] entry carries no such flag. `defaultSlot` reads
    // like the answer and is not: CATALOG_SCHEMA.md 3.3 says it is "the SLICE a
    // client should boot from when it mounts this image" - an index inside
    // hd1k_combo - and says nothing about which drive an image belongs in. Its
    // only published value is 0, so reading it as a drive number is a mistake
    // that works, which is the kind this tree keeps having to unpick. The list
    // is DEFAULT_DISK_IDS in DiskCatalog.h, shared with the F5 path so the two
    // cannot drift.
    if (matched == 0) {
        for (int d = 0; d < 2; d++) {
            // ONLY INTO AN EMPTY DRIVE. "No slot carried over" is not the same
            // as "no slot is occupied": a drive holding an image the user
            // browsed to themselves has no catalog id, so it contributes no
            // match and would otherwise be handed hd1k_combo on top of their
            // file. That is the unmount this must never do, reached by the one
            // path that looks like a default rather than a replacement.
            if (!m_diskChoices[d] || m_diskChoices[d]->GetSelection() > 0) continue;
            for (const auto& e : entries) {
                if (e.id != DEFAULT_DISK_IDS[d]) continue;
                m_switchQueue.push_back(SlotFetch{ d, e.filename });
                break;
            }
        }
    }

    if (m_switchQueue.empty()) { setSlotsSettling(false); return; }

    // ALREADY ON THE MACHINE IS MOUNTED, NEVER RE-FETCHED. The interactive
    // Download button asks before replacing an image whose bytes have changed
    // since it was written, and a chain running off a dropdown change has nobody
    // to ask - so it must never be in a position where that question arises.
    // Anything present is simply used.
    std::vector<SlotFetch> toFetch;
    for (const auto& f : m_switchQueue) {
        if (m_catalog->isDiskDownloaded(f.filename)) {
            mountIntoSlot(f.slot, f.filename);
        } else {
            toFetch.push_back(f);
        }
    }
    m_switchQueue.swap(toFetch);

    if (m_switchQueue.empty()) {
        setSlotsSettling(false);
        m_statusText->SetLabel("Disks for this release are already downloaded");
        return;
    }

    m_switchActive = true;
    m_downloadBtn->Enable(false);
    m_updateBtn->Enable(false);
    advanceReleaseSwitchFetch();
}

void SettingsDialogWx::advanceReleaseSwitchFetch() {
    if (!m_switchActive) return;

    if (m_switchNext >= m_switchQueue.size()) {
        m_switchActive = false;
        m_switchQueue.clear();
        m_switchNext = 0;
        // The chain held these shut for its whole run - see onDownloadComplete.
        m_downloadBtn->Enable(true);
        m_updateBtn->Enable(true);
        setSlotsSettling(false);
        m_statusText->SetLabel("Disks for this release are ready");
        return;
    }

    const SlotFetch& f = m_switchQueue[m_switchNext];
    // The status line says which of how many, because this is the one place in
    // the dialog where a single user action can start several multi-megabyte
    // downloads and the progress bar alone cannot say how much is left.
    m_statusText->SetLabel(
        wxString::Format("Disk %d: downloading %s (%d of %d)...",
                         f.slot, wxString::FromUTF8(f.filename),
                         (int)m_switchNext + 1, (int)m_switchQueue.size()));
    beginDiskDownload(f.filename, wxString::FromUTF8(f.filename));
}

void SettingsDialogWx::mountIntoSlot(int slot, const std::string& filename) {
    if (slot < 0 || slot > 3 || !m_diskChoices[slot]) return;
    const wxString want = wxString::FromUTF8(filename);
    int idx = m_diskChoices[slot]->FindString(want);
    // Appended when the dropdown has not caught up yet - the list is rebuilt
    // from the downloaded set, and this can run in the same turn as the download
    // that created the file. Same append repopulateDiskLists() and
    // loadDiskSelections() already do, and for the same reason.
    if (idx == wxNOT_FOUND) idx = m_diskChoices[slot]->Append(want);
    m_diskChoices[slot]->SetSelection(idx);
}

void SettingsDialogWx::setSlotsSettling(bool settling) {
    if (m_slotsSettling == settling) return;
    m_slotsSettling = settling;

    for (int i = 0; i < 4; i++) {
        if (m_diskChoices[i]) m_diskChoices[i]->Enable(!settling);
        if (m_browseButtons[i]) m_browseButtons[i]->Enable(!settling);
        if (m_newButtons[i]) m_newButtons[i]->Enable(!settling);
    }

    // The cursor, for the part of this the user has no other signal about: the
    // first catalog fetch happens before anything is on screen to show progress
    // against, and a page that simply sits there is indistinguishable from one
    // that has finished.
    if (settling) {
        if (!m_busyCursor) m_busyCursor.reset(new wxBusyCursor());
    } else {
        m_busyCursor.reset();
    }
}

void SettingsDialogWx::abandonReleaseSwitchFetch() {
    if (!m_switchActive) return;
    m_switchActive = false;
    m_switchQueue.clear();
    m_switchNext = 0;
    // The buttons the chain held shut. A second release switch abandons the
    // first and the dialog stays open, so leaving them disabled here would make
    // Download and Update dead for the rest of its life.
    if (m_downloadBtn) m_downloadBtn->Enable(true);
    if (m_updateBtn) m_updateBtn->Enable(true);
    setSlotsSettling(false);
    // The in-flight image is cancelled, not waited for - but see
    // tryStartReleaseSwitchFetch(): cancelling does not free the transfer lock,
    // so the next chain waits for this one's completion rather than racing it.
    if (m_catalog) m_catalog->cancelDownload();
}

bool SettingsDialogWx::diskIsInASlot(const std::string& filename) const {
    const wxString wanted = wxString::FromUTF8(filename);
    for (int i = 0; i < 4; ++i) {
        if (!m_diskChoices[i]) continue;
        if (m_diskChoices[i]->GetStringSelection() == wanted) return true;
    }
    return false;
}

// The Update button. Everything it decides comes out of DiskLedger, which is in
// a suite; nothing here re-derives a verdict.
//
// This is the caller those functions were written for and did not have.
// DiskLedger::action(), plan(), RefreshNow, RefreshAutomatically and
// allowsUserRequestedUpdate appeared nowhere outside DiskLedger and
// tests/test_diskledger.cpp until this handler, so the Settings column could say
// "Update available" and the application had no way to act on it. Download could
// be made to replace a superseded file, but only by a user who already knew that
// is what Download now means.
void SettingsDialogWx::onUpdateDisk(wxCommandEvent& event) {
    (void)event;
    if (!m_catalog) return;

    long sel = m_catalogList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel < 0) {
        wxMessageBox("Please select a disk to update", "Info",
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    if ((size_t)sel >= m_catalogRowFilenames.size()) return;
    const std::string filenameStr = m_catalogRowFilenames[sel];
    const wxString filename = wxString::FromUTF8(filenameStr);

    const DiskFreshness fresh = m_catalog->getFreshness(filenameStr);

    // Say WHICH no it is. "Nothing to update" over a file the column has just
    // called Unverifiable reads as a bug in the button, and the three noes are
    // three different situations.
    if (!DiskLedger::allowsUserRequestedUpdate(fresh)) {
        if (fresh == DiskFreshness::NotInstalled) {
            wxMessageBox("This disk is not downloaded yet - use Download.",
                         "Info", wxOK | wxICON_INFORMATION, this);
        } else if (fresh == DiskFreshness::Unverifiable) {
            wxMessageBox("This catalog publishes no SHA-256 for " + filename +
                         ", so there is nothing to compare the copy on disk "
                         "against and no way to check a replacement. Update "
                         "cannot say whether it would be an improvement.",
                         "Nothing to compare", wxOK | wxICON_INFORMATION, this);
        } else if (fresh == DiskFreshness::NeedsMeasurement) {
            wxMessageBox("This copy has not been hashed yet, so its provenance "
                         "is not known. Press Refresh and try again.",
                         "Not measured yet", wxOK | wxICON_INFORMATION, this);
        } else {
            wxMessageBox(filename + " already matches what this catalog "
                         "publishes.", "Up to date", wxOK | wxICON_INFORMATION, this);
        }
        return;
    }

    switch (DiskLedger::plan(fresh, diskIsInASlot(filenameStr))) {
    case DiskRefreshPlan::DeferredMounted:
        // isMounted is why this decision cannot live in DiskCatalog. Replacing
        // the file under a slot the machine holds is undone by the next flush,
        // so the honest answer is to say what to clear rather than to write
        // bytes that will be overwritten.
        wxMessageBox(filename + " is selected in one of the four disk slots. "
                     "Set that slot to (None) first, or the machine writes its "
                     "own copy back over the new file when it next flushes.",
                     "In use", wxOK | wxICON_INFORMATION, this);
        return;

    case DiskRefreshPlan::RefreshNow:
        // SupersededPristine and not in a slot: the bytes are provably still
        // the ones this application downloaded, so nothing of the user's is in
        // them and there is nothing to warn about.
        break;

    case DiskRefreshPlan::OfferUpdate:
    case DiskRefreshPlan::OfferUpdateLossy: {
        const int answer = wxMessageBox(
            "The copy of " + filename + " in the data folder is not the image "
            "this catalog publishes, and its bytes have changed since it was "
            "written - anything you have saved inside it is in those bytes.\n\n"
            "Updating replaces the file. That cannot be undone, and the catalog "
            "cannot give your work back.\n\n"
            "Replace it?",
            "Replace a disk you have written to?",
            wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this);
        if (answer != wxYES) return;
        break;
    }

    case DiskRefreshPlan::DoNothing:
        return;
    }

    beginDiskDownload(filenameStr, filename);
}

void SettingsDialogWx::onDownloadProgress(wxCommandEvent& event) {
    m_progressBar->SetValue(event.GetInt());
}

void SettingsDialogWx::onDownloadComplete(wxCommandEvent& event) {
    m_progressBar->SetValue(event.GetInt() ? 100 : 0);

    // A CHAIN STEP FINISHING IS NOT THE SAME EVENT AS THE DOWNLOAD BUTTON
    // FINISHING, and the difference is what happens next. Handled before the
    // ordinary path for two reasons: the status line it sets would otherwise be
    // overwritten by "Download complete", which is true and useless while three
    // more images are still coming; and the Download and Update buttons must
    // STAY DISABLED between steps. Re-enabling them here - which is what the
    // ordinary path below does - opens a window in every gap of the chain where
    // a click starts a second download, and DiskCatalog::downloadDisk refuses a
    // concurrent one, so the chain's next image would be the request refused.
    if (m_switchActive) {
        if (event.GetInt()) {
            // Bounds-checked rather than trusted. advanceReleaseSwitchFetch()
            // only ever starts a download while m_switchNext is in range, so
            // this cannot fire today; it is here because the alternative to a
            // cheap test is a read past the end of a vector, on a path driven by
            // a posted event whose ordering is not obvious from this function.
            if (m_switchNext >= m_switchQueue.size()) {
                m_switchActive = false;
                m_switchQueue.clear();
                m_switchNext = 0;
                m_downloadBtn->Enable(true);
                m_updateBtn->Enable(true);
                return;
            }
            const int slot = m_switchQueue[m_switchNext].slot;
            const std::string filename = m_switchQueue[m_switchNext].filename;
            populateCatalog();
            repopulateDiskLists();
            // After repopulateDiskLists, so the row for the image that just
            // arrived exists to be selected.
            mountIntoSlot(slot, filename);
            m_switchNext++;
            // advance() either starts the next download - leaving the buttons
            // disabled, which is what beginDiskDownload does - or finishes the
            // chain and re-enables them.
            advanceReleaseSwitchFetch();
        } else {
            // PARTWAY IS A REAL RESTING PLACE, and it is left honestly rather
            // than rolled back. The slots already moved hold images that are on
            // the machine and belong to the selected release; the ones that did
            // not still hold what they held. Rolling back would mean unmounting,
            // which this must never do, and pressing on would mean asking the
            // network again for something that just failed.
            const size_t done = m_switchNext;
            const size_t total = m_switchQueue.size();
            m_switchActive = false;
            m_switchQueue.clear();
            m_switchNext = 0;
            m_downloadBtn->Enable(true);
            m_updateBtn->Enable(true);
            setSlotsSettling(false);
            m_statusText->SetLabel(
                wxString::Format("Disk download failed after %d of %d: %s",
                                 (int)done, (int)total, event.GetString()));
        }
        return;
    }

    m_downloadBtn->Enable(true);
    m_updateBtn->Enable(true);

    // NOT A CHAIN STEP, so either the Download/Update button finished or an
    // abandoned chain's transfer has just stopped. Either way the one-at-a-time
    // lock is free now, which is the second of the two things a waiting plan
    // needs - see tryStartReleaseSwitchFetch().
    tryStartReleaseSwitchFetch();

    if (event.GetInt()) {
        m_statusText->SetLabel("Download complete");
        populateCatalog();
        // NOT populateDiskLists() on its own, which empties the four dropdowns
        // and puts every one of them back on "(None)". That is what made
        // downloading a disk and then pressing OK write "" for all four slots
        // and unmount the running disks - the same erasure onCatalogLoaded
        // guards against, reached by the most ordinary path in this dialog.
        repopulateDiskLists();
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

    if ((size_t)sel >= m_catalogRowFilenames.size()) return;
    const std::string filenameStr = m_catalogRowFilenames[sel];
    const wxString filename = wxString::FromUTF8(filenameStr);

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
            // Same reason as onDownloadComplete. The deleted image has just
            // dropped out of the list, so a slot that named it keeps naming it
            // rather than being blanked - the mounting loop skips a path that is
            // not there and says nothing was lost, where the alternative erased
            // the other three slots as a side effect of deleting a fourth.
            repopulateDiskLists();
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

void SettingsDialogWx::onKeySelected(wxListEvent& event) {
    // Land the pending edit on the row it was typed for, BEFORE moving. This
    // handler still sees the outgoing row in m_keySelectedRow, which is the
    // only moment at which the two can be matched up.
    commitPendingKeySequence();
    m_keySelectedRow = event.GetIndex();
    updateKeyEditor();
}

// Validate as the user types, but do not store as the user types.
//
// Storing per keystroke was written first and is wrong, and the way it is wrong
// was measured rather than reasoned about: typing \400 into the box left the
// row bound to \40. Every prefix of a sequence gets its own wxEVT_TEXT, and
// some prefixes are perfectly valid on their own - \4 and \40 both are - so a
// live commit cannot tell "finished" from "half typed" and keeps whichever
// prefix happened to be legal last. The row then held a value the user never
// typed and never saw as final, and because moving to another row clears the
// refusal, OK would go on to save it.
void SettingsDialogWx::onKeySequenceChanged(wxCommandEvent& event) {
    (void)event;
    if (m_keySelectedRow < 0 || (size_t)m_keySelectedRow >= m_keyRows.size()) return;
    if (m_keyRows[m_keySelectedRow].purpose) return;   // disabled on a reserved row

    // keymap::decode() has no failure path: everything reaches the guest as
    // bytes, correct or not. This is where a mistake is caught instead.
    if (const char* why = keymap::validateSequence(m_keySeqText->GetValue().ToStdString())) {
        setKeySeqValid(false);
        m_keyHintText->SetLabel(wxString::FromUTF8(why));
        return;
    }
    // setKeySeqValid() is what takes the refusal back off the status line; it
    // used to be done here, which is why the three paths that do not type into
    // the box could leave it standing.
    setKeySeqValid(true);
    m_keyHintText->SetLabel(keyEditorHint());
}

// Move what is in the box onto the selected row. The two callers are the only
// moments at which a pending edit can matter: the selection leaving the row it
// belongs to, and OK, which is about to read the rows. A refused value is not
// committed here either - it is simply dropped, and the row keeps what it had.
void SettingsDialogWx::commitPendingKeySequence() {
    if (!m_keySeqValid) return;
    if (m_keySelectedRow < 0 || (size_t)m_keySelectedRow >= m_keyRows.size()) return;
    if (m_keyRows[m_keySelectedRow].purpose) return;
    applyKeySequenceToRow(m_keySeqText->GetValue().ToStdString());
}

void SettingsDialogWx::onKeyRestoreDefault(wxCommandEvent& event) {
    (void)event;
    if (m_keySelectedRow < 0 || (size_t)m_keySelectedRow >= m_keyRows.size()) return;
    auto it = m_keyDefaults.find(m_keyRows[m_keySelectedRow].id);
    if (it == m_keyDefaults.end()) return;   // the button is disabled in this case
    applyKeySequenceToRow(it->second);
    m_keySeqText->ChangeValue(wxString::FromUTF8(it->second));
    setKeySeqValid(true);
    m_keyHintText->SetLabel(keyEditorHint());
}

void SettingsDialogWx::onKeyUnbind(wxCommandEvent& event) {
    (void)event;
    // An empty value is not an absent entry: KeyMap::build stores it and find()
    // reports the key unbound, which is what stops a modified key falling back
    // to the plain one it was explicitly told not to send.
    applyKeySequenceToRow(std::string());
    m_keySeqText->ChangeValue("");
    setKeySeqValid(true);
    m_keyHintText->SetLabel(keyEditorHint());
}

void SettingsDialogWx::onOK(wxCommandEvent& event) {
    // A refused sequence was never stored, so OK would close over a box that
    // still shows text the user believes they set. Say so and stay open.
    if (!m_keySeqValid) {
        int page = m_notebook->FindPage(m_keyboardPage);
        if (page != wxNOT_FOUND) m_notebook->SetSelection(page);
        m_keySeqText->SetFocus();
        m_statusText->SetLabel(kKeySeqRefusedStatus);
        return;
    }
    commitPendingKeySequence();
    // A chain still running is stopped before the slots are read back, not
    // after. saveSettings() reads the four dropdowns, so whatever has landed by
    // now is what gets written - and an image still in flight would otherwise
    // keep downloading against a dialog that has closed, into slots nothing will
    // read again. The images already placed are on disk and are what OK stores.
    abandonReleaseSwitchFetch();
    m_switchPlanPending = false;
    saveSettings();
    EndModal(wxID_OK);
}

void SettingsDialogWx::onCancel(wxCommandEvent& event) {
    // Put back the RomWBW release the catalog had when this dialog opened.
    // Choosing one in the control tells the catalog immediately, because the
    // list underneath has to change with it; Cancel means that never happened.
    //
    // The entries already fetched for the other release are left in the catalog
    // rather than re-fetched here - a network round trip on Cancel would be a
    // surprise, and nothing reads them without fetching first: this dialog
    // refetches from its constructor, and MainWindow's F5 default-disk path now
    // fetches before it downloads anything.
    // And the catalog index, for exactly the same reason and by the same rule:
    // Refresh pushed whatever was typed so the list underneath would be the one
    // being asked about, and Cancel means none of it happened. Restored from the
    // value the dialog OPENED with rather than from m_settings, which
    // saveSettings() may have overwritten from the control on an earlier OK that
    // was never reached here.
    // A chain still running is stopped first. The images it already placed stay
    // on disk and stay in their slots for as long as this dialog lives, but
    // Cancel means none of the dialog's choices are written back, so nothing
    // about those slots reaches the configuration - see saveSettings(), which
    // only OK reaches.
    abandonReleaseSwitchFetch();
    m_switchPlanPending = false;

    if (m_catalog) {
        m_catalog->setPreferredRomwbwVersion(m_settings.romwbwVersion);
        m_catalog->setCatalogIndexUrl(m_catalogIndexUrlOnOpen);
        // The pre-release toggle is NOT put back. It is committed by toggling
        // it - see onShowPrereleaseChanged - so m_settings already holds what
        // the user chose, and the catalog is already holding the same thing.
        // Restoring it here would undo on Cancel the one control on this page
        // that Cancel is not supposed to reach.
    }
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

        // No Centre() here any more. The constructor sizes the dialog to the
        // display's work area and centres it INSIDE that work area, and a plain
        // Centre() would recompute the position from whatever Centre() centres
        // on - the one thing the constructor is careful not to leave to it.
        // Nothing between the two changes the size: setSettings() only fills
        // controls in from m_settings and rebuilds the key list.

        OutputDebugStringA("[Settings] Showing modal dialog\n");
        s_openSettingsDialog = &dlg;
        result = (dlg.ShowModal() == wxID_OK);
        s_openSettingsDialog = nullptr;
        OutputDebugStringA("[Settings] Dialog closed\n");
        if (result) {
            settings = dlg.getSettings();
        } else {
            // CANCEL STILL RETURNS ONE FIELD. The pre-release toggle is a view
            // preference committed by toggling it, not a description of the
            // machine held until OK - see SettingsDialogWx::onShowPrereleaseChanged
            // for the report that made that distinction necessary. Everything
            // else the dialog holds is discarded here exactly as before.
            settings.showPrereleaseVersions = dlg.getSettings().showPrereleaseVersions;
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
