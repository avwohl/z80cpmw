/*
 * HelpWindow.cpp - Remote Help System Window Implementation
 */

#include "pch.h"
#include "HelpWindow.h"
#include "resource.h"
#include "Version.h"
#include <thread>
#include <sstream>
#include <commctrl.h>

// Remote help intentionally tracks "latest" (unlike the disk catalog, which is
// pinned in DiskCatalog.cpp): help text is low-risk content with no ROM coupling,
// so improvements can ship without cutting a new release.
const std::wstring HelpWindow::INDEX_URL =
    L"https://github.com/avwohl/ioscpm/releases/latest/download/help_index.json";
const std::wstring HelpWindow::CONTENT_BASE_URL =
    L"https://github.com/avwohl/ioscpm/releases/latest/download/";

static const wchar_t* HELP_WINDOW_CLASS = L"Z80CPM_HelpWindow";
static bool g_helpClassRegistered = false;
static HelpWindow* g_helpWindow = nullptr;

// Markdown for the bundled "Getting Started" topic (served locally, no network).
// Shown in the scrollable help window so it does not overflow the terminal.
//
// The "File Transfer (R8 / W8)" section describes the R8 and W8 on the disk images
// this build actually ships, which are older than the ones romwbw_emu builds today.
// Measured 2026-08-28 over bin/Release/disks: hd1k_combo.img's w8.com prints
// "Usage: W8 <cpmname>" with no [hostpath] and carries none of the 06 E9 CF bytes of
// the HBF_HOST_CAPS probe, and hd1k_games.img holds no r8.com or w8.com directory
// entry at all. Three blocks below exist only to say so, and are deleted rather than
// reworded once the refreshed images land: the paragraph opening "W8 does not take a
// host path yet", the paragraph opening "Two cautions until then", and the sentence
// opening "The games disk carries neither utility". They do not share one condition,
// so they do not all come out at the same moment.
//
// packaging/scripts/verify-disk-assets.sh is the gate for the first two, and it gates
// less than its name suggests. A missing utility is severity-split by image there:
// only when the diskdef is wbw_hd1k_0 - an image over 8 MB carrying a 55 AA MBR
// signature, i.e. the boot image - does the miss reach bad(); on a plain 8 MB image it
// prints "nothing to check" and leaves $fail alone, on that script's reasoning that a
// secondary data disk carrying neither utility is a choice, not a fault. A copy that
// IS present is checked wherever it sits. So a PASS says: the boot image carries both
// utilities, every w8.com found on any staged image probes HBF_HOST_CAPS, and every
// copy matches what romwbw_emu/src builds today - with um80/ul80 missing it exits 2
// instead, which is not a pass. An armed w8.com necessarily takes a host path, because
// the probe landed upstream after the [hostpath] tail did, so a PASS retires the
// "W8 does not take a host path yet" paragraph outright. It does not on its own retire
// "Two cautions until then": that gate compares bytes, not behaviour, and those two
// hazards are gone only once upstream r8.asm and w8.asm have dropped the unfiltered
// F_DELETE and the 1Ah truncation. Re-read them before deleting that block.
//
// The games-disk sentence has its own condition and the gate will never supply it:
// hd1k_games.img is a plain 8 MB image, so verify-disk-assets.sh passes whether or not
// it still carries r8.com and w8.com. Delete that sentence only once a run over the
// staged disks prints no "carries no r8.com" / "carries no w8.com" info line for
// hd1k_games.img - equivalently, once cpmls -f wbw_hd1k lists both names on it.
//
// Deleting any of the three ahead of its own condition was rejected: the R8 wildcard
// erasure named in the second block is live in what ships today, not merely in an old
// lineage. The paragraph opening "A bare name" is NOT on the list and must not be
// pulled onto it by acquiring a clause. It briefly read "A bare name, and so every W8
// export, uses the app's data folder", which is true only while the shipped W8 ignores
// the command tail, so deleting the three blocks around it would have left it asserting
// exactly what docs/FILE_TRANSFER.md records as the false statement that document used
// to carry. The clause is gone and the paragraph is permanently true as it stands. The
// matching turnover in README.md and docs/FILE_TRANSFER.md is a single [RELEASE] item
// in todo.txt, and the wording here is deliberately README.md's so the two cannot drift
// apart while they wait.
std::string help_topics::gettingStartedMarkdown() {
    return R"DOC(# Getting Started

  1. Download disk images:
     Open Emulator > Settings and find the "Download Disk Images" list,
     select a disk (for example QPM or Games) and click Download.
     (The per-disk "New" button creates a blank image and "Browse" picks an
     existing file - neither one downloads the prebuilt disks.)

  2. Assign disks to units:
     In Settings, choose downloaded disks for Disk 0, Disk 1, and so on.

  3. Start the emulator:
     Press F5, or Emulator > Start.

  4. At the RomWBW boot menu:
     Type the unit number of your hard disk and press Enter. With the default
     ROM and the downloadable hd1k disks that number is 2 - units 0 and 1 are
     the on-board ROM/RAM disks and carry no operating system, so typing 0
     reports "No system image on disk". Press D at the boot menu to list the
     disk units and L to list the ROM applications. (This boot-menu unit
     number is not the same as the Settings "Disk 0-3" slot.) Press W to
     save your choice as the autoboot default so you do not have to type it
     each time.

## File Transfer (R8 / W8)

R8 filename - import a file from the host into CP/M.
W8 filename - export a file from CP/M to the host.

R8 takes a full path and reads exactly that file, even on the Store build:

    R8 C:\Users\me\Desktop\getkey2.com

W8 does not take a host path yet, whatever it is given: the W8.COM on the
bundled disk images reads only the parsed FCB and never the command tail, so
every export goes to the app's data folder instead. A host path for W8 is
upstream and arrives when the images are refreshed.

Two cautions until then, both properties of the utilities on the bundled images
rather than of the app: the R8 on those images hands an unfiltered host basename
to F_DELETE, so importing a host file whose name contains ? or * erases every
matching CP/M file on the disk without saying so; and that W8 truncates a binary
export at the first 1Ah byte.

The games disk carries neither utility at all, so nothing transfers from it -
run R8 and W8 from the combo disk.

A bare name uses the app's data folder. Its exact location (which any MSIX
install redirects - the Store build and the signed sideload beta alike) is shown
in Emulator > Settings and in Help > About; the Open Folder button there opens it
in Explorer.

## Keyboard

| Key | Action |
| --- | --- |
| F5 | Start emulator |
| Shift+F5 | Stop emulator |
| F1 | Help |

Reset is on the Emulator menu. It has no shortcut by default, because the
obvious one, Ctrl+R, is a character CP/M itself uses (^R retypes the current
line, and WordStar-style editors bind it too). Set "ctrlRToCpm": false in the
config to take Ctrl+R back as the Reset shortcut.

F2 through F12, Insert, and PageUp / PageDown are sent to CP/M. The exact bytes
are configurable - see the "Configuration File" topic. By default F1 and F5 are
reserved for the app; enable f1ToCpm / f5ToCpm in the config to send them to CP/M.

## Scrollback

Lines that scroll off the top of the screen are kept so you can read them again -
handy for long directory listings. Scroll back with the mouse wheel or
Shift+PageUp, and forward again with Shift+PageDown. Ctrl+Home jumps to the oldest
line and Ctrl+End returns to the live screen; typing anything also returns to the
live screen. Plain PageUp / PageDown (without Shift) are still sent to CP/M.

The buffer holds 1000 lines by default. Change it with "Terminal scrollback" in
Emulator > Settings (set it to 0 to turn scrollback off).

## Mouse

Drag with the mouse to select text, then right-click for Copy and Paste.
Ctrl+C and Ctrl+V are left untouched so they still reach CP/M as ^C and ^V.
)DOC";
}

// Markdown for the bundled Configuration topic. Kept in sync with
// docs/CONFIGURATION.md. Its code blocks are indented rather than fenced. That
// used to be forced - markdownToText had no branch for a fence and printed the
// backticks - and is now only a house preference, since the renderer understands
// both and indents a fenced block to match an indented one.
// Note: backslashes are doubled here so the displayed text matches the doubled
// backslashes the user actually sees in z80cpmw.json.
std::string help_topics::configurationMarkdown() {
    return R"DOC(# Configuration File (z80cpmw.json)

z80cpmw keeps its settings in a JSON file you can edit by hand:

    %LOCALAPPDATA%\z80cpmw\z80cpmw.json

Tip: Emulator > Settings > Open Folder opens the data folder
(...\z80cpmw\data, where disks and R8/W8 transfers live). The z80cpmw.json file
is one level up, in the z80cpmw folder. On any MSIX install - the Microsoft Store
build or the signed sideload beta - both live under
...\Packages\AaronWohl.Z80CPM_<hash>\LocalCache\Local\z80cpmw\ - you don't need to
type that: Settings and Help > About show the real, resolved path.

Close z80cpmw before editing the file, then restart for changes to take effect.

## Keyboard Map

CP/M is pure ASCII and has no built-in function or navigation keys. Each CP/M
terminal defined its own escape sequences for them, so there is no single
standard - the correct bytes depend on the terminal your CP/M software expects
(VT100, ADM-3A, Televideo, Kaypro, and so on).

z80cpmw lets you bind each special key to whatever bytes you choose, written as
termcap-style escape strings under "keyboard" in z80cpmw.json:

    "keyboard": {
      "f1ToCpm": false,
      "f5ToCpm": false,
      "ctrlRToCpm": true,
      "keys": {
        "Insert": "\\E[2~",
        "F2": "\\EOQ"
      }
    }

Because JSON uses the backslash for its own escaping, every backslash is written
twice in the file. The Escape character (\E in termcap) becomes \\E, so Insert
is stored as "\\E[2~".

### Escape Syntax

| Notation | Meaning |
| --- | --- |
| \E | Escape, 0x1B (written \\E in JSON) |
| \n \r \t | Newline, Return, Tab |
| \b \f \s | Backspace, Form-feed, Space |
| \NNN | One byte in octal, e.g. \033 = Escape |
| ^X | Control-X, e.g. ^C = 0x03 |
| ^? | Delete, 0x7F |

Any other character stands for itself. An empty value unbinds that key.

### Bindable Keys

Up, Down, Left, Right, Home, End, Insert, Delete, PageUp, PageDown, and F1
through F12. Names are case-insensitive; Ins, Del, PgUp and PgDn also work.

### Default Bindings

VT220 / xterm defaults, shown as written in the file (doubled backslashes):

| Key | Sends |
| --- | --- |
| Up | \\E[A |
| Down | \\E[B |
| Right | \\E[C |
| Left | \\E[D |
| Home | \\E[H |
| End | \\E[F |
| Insert | \\E[2~ |
| Delete | ^? |
| PageUp | \\E[5~ |
| PageDown | \\E[6~ |
| F1 | \\EOP |
| F2 | \\EOQ |
| F3 | \\EOR |
| F4 | \\EOS |
| F5 | \\E[15~ |
| F6 | \\E[17~ |
| F7 | \\E[18~ |
| F8 | \\E[19~ |
| F9 | \\E[20~ |
| F10 | \\E[21~ |
| F11 | \\E[23~ |
| F12 | \\E[24~ |

Change any line to suit your CP/M program. For example, to make F1-F4 easier to
parse in a hand-written key reader, give them the same CSI form as the rest:

    "F1": "\\E[11~", "F2": "\\E[12~", "F3": "\\E[13~", "F4": "\\E[14~"

### Application Shortcut Keys

F1 opens Help and F5 / Shift+F5 Start and Stop the emulator, so by default those
keys are not sent to CP/M. To deliver them to CP/M instead, set:

    "f1ToCpm": true     sends F1 to CP/M (Help stays on the Help menu)
    "f5ToCpm": true     sends F5 and Shift+F5 to CP/M

Ctrl+R goes the other way. CP/M has no function keys, so reserving F1 and F5
costs nothing, but ^R (0x12) is ordinary ASCII that CP/M reads: it retypes the
current line at the command prompt, and WordStar-family editors bind it as
well. z80cpmw therefore sends Ctrl+R to CP/M by default and leaves Reset on the
Emulator menu:

    "ctrlRToCpm": false  makes Ctrl+R the Reset shortcut again (not sent to CP/M)

A key that is reserved for the app is swallowed whole - CP/M never sees it - so
these three settings decide who receives the keystroke, not what it sends. The
menu updates its own shortcut hints to match, so an item never advertises a key
that is no longer bound.

F10 normally opens the Windows menu bar; z80cpmw delivers it to CP/M when it is
bound in the keymap.

## Mouse Copy and Paste

Drag with the mouse to select text in the terminal, then right-click for Copy
and Paste. Ctrl+C and Ctrl+V are left untouched so they still reach CP/M as ^C
and ^V. Paste works only while the emulator is running.

## Other Settings

| Setting | Meaning |
| --- | --- |
| display.fontSize | Terminal font size, in points |
| display.scrollbackLines | Lines of history kept for scrollback (0 = off) |
| core.rom | ROM image to load at startup |
| core.bootString | Text typed automatically at the boot menu |
| disks | Disk images assigned to units 0-3 |

Most of these are easier to change from Emulator > Settings.
)DOC";
}

// Control IDs
#define IDC_TOPIC_LIST      1001
#define IDC_CONTENT_VIEW    1002
#define IDC_STATUS_LABEL    1003

// Payload for WM_APP + 2, "a topic's content is ready". Allocated and posted
// by the download thread fetchTopic() detaches; read and deleted by the thread
// that owns m_hwnd, because PostMessage() queues to the thread that created
// the window and only that thread runs handleMessage().
//
// topicId is here so the window thread can DROP a result the reader has moved
// off. It is not a nicety: the bundled-topic branch at the top of fetchTopic()
// runs AHEAD of the "if (m_loading) return" guard, so clicking Getting Started
// while a remote download is still in flight really does reassign
// m_currentTopicId and repaint the pane. A late failure then used to paste
// "This topic could not be downloaded." over a topic nobody had asked about.
// The same holds for a late success, and dropping it costs nothing because
// cacheContent() has already stored it - reselecting the topic shows it at
// once from the cache.
//
// Rejected: having the download thread compare m_currentTopicId itself before
// posting. m_currentTopicId is a std::string the window thread assigns without
// a lock, so reading it off-thread is a data race, and the answer would be
// stale by the time the message was dequeued anyway. Comparing inside the
// handler is a same-thread read of a value only that thread writes (fetchTopic
// is reached from WM_COMMAND/LBN_SELCHANGE and from show(), which MainWindow
// calls from its own WindowProc), so it needs no synchronisation.
//
// status is the whole status line to show; both arms of the download thread
// fill it in, and an empty one would leave the line reading "Loading topic..."
// under a pane that had finished loading. It travels in the same message as the
// pane text so the two are dropped together - a single message cannot be
// half-accepted, which is why this replaced a second PostMessage whose
// correctness rested on posted messages coming back in the order they went in.
// That pairing matters more now than it did: the status line names WHICH COPY
// of the topic the pane is showing, so a status that outlived its pane text
// would be a claim about the wrong bytes.
struct HelpContentMsg {
    std::string topicId;
    std::string content;
    std::string status;
};

// The status line for a topic that is on screen: which topic, and WHICH COPY of
// it the reader is looking at.
//
// The second half is the point. With an on-disk cache there are four possible
// answers - the network, m_cache from earlier in this session, the cache
// directory, and (once the bundling commit lands) the binary itself - and a
// reader who cannot tell them apart cannot tell a topic that is current from
// one saved before a release. Every path that puts text in the pane goes
// through here or through a failure line, which is why displayContent() no
// longer writes a status of its own: it is handed markdown and has no idea
// where the markdown came from, so the only status it could write is one that
// does not answer the question.
static std::string viewingStatus(const std::string& title, const std::string& source) {
    return "Viewing: " + title + "  (" + source + ")";
}

// The title help_index.json gave a topic, or the id when the list has no such
// entry - which is what show() passes when MainWindow opens a topic by name
// before the index has loaded.
static std::string titleOf(const std::vector<help_assets::HelpTopic>& topics,
                           const std::string& topicId) {
    for (const auto& topic : topics) {
        if (topic.id == topicId) return topic.title;
    }
    return topicId;
}

HelpWindow::HelpWindow() {
}

HelpWindow::~HelpWindow() {
    close();
}

bool HelpWindow::show(HWND parent, const std::string& topicId) {
    m_parent = parent;

    // If window exists, just show it (and switch topic if one was requested)
    if (m_hwnd && IsWindow(m_hwnd)) {
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        if (!topicId.empty()) {
            fetchTopic(topicId);
            selectTopicInList(topicId);
        }
        return true;
    }

    // Register window class
    if (!g_helpClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_APPICON));
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = HELP_WINDOW_CLASS;
        wc.hIconSm = wc.hIcon;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        g_helpClassRegistered = true;
    }

    // Size relative to DPI so the window is not tiny on high-DPI monitors, and
    // center it on the primary work area.
    UINT dpi = parent ? GetDpiForWindow(parent) : 96;
    if (dpi == 0) dpi = 96;
    int winW = MulDiv(900, dpi, 96);
    int winH = MulDiv(680, dpi, 96);
    int winX = CW_USEDEFAULT, winY = CW_USEDEFAULT;
    RECT wa{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0) &&
        wa.right > wa.left && wa.bottom > wa.top) {
        winW = (std::min)(winW, (int)(wa.right - wa.left));
        winH = (std::min)(winH, (int)(wa.bottom - wa.top));
        winX = wa.left + ((wa.right - wa.left) - winW) / 2;
        winY = wa.top + ((wa.bottom - wa.top) - winH) / 2;
    }

    // Create window
    m_hwnd = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        HELP_WINDOW_CLASS,
        L"z80cpmw Help",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        winX, winY,
        winW, winH,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!m_hwnd) {
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    // Bundled topics were seeded in WM_CREATE, so they display immediately.
    if (!topicId.empty()) {
        fetchTopic(topicId);
        selectTopicInList(topicId);
    }

    return true;
}

void HelpWindow::close() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
    }
    m_hwnd = nullptr;
}

bool HelpWindow::isVisible() const {
    return m_hwnd && IsWindow(m_hwnd) && IsWindowVisible(m_hwnd);
}

LRESULT CALLBACK HelpWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    HelpWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<HelpWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->m_hwnd = hwnd;
    } else {
        window = reinterpret_cast<HelpWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT HelpWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        createControls();
        seedLocalTopics();   // bundled topics available before the network call
        updateTopicList();
        fetchIndex();
        return 0;

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        int listWidth = 250;
        int statusHeight = 25;
        int padding = 5;

        if (m_topicList) {
            SetWindowPos(m_topicList, nullptr,
                padding, padding,
                listWidth - padding * 2, height - statusHeight - padding * 2,
                SWP_NOZORDER);
        }

        if (m_contentView) {
            SetWindowPos(m_contentView, nullptr,
                listWidth + padding, padding,
                width - listWidth - padding * 2, height - statusHeight - padding * 2,
                SWP_NOZORDER);
        }

        if (m_statusLabel) {
            SetWindowPos(m_statusLabel, nullptr,
                padding, height - statusHeight,
                width - padding * 2, statusHeight,
                SWP_NOZORDER);
        }
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TOPIC_LIST && HIWORD(wParam) == LBN_SELCHANGE) {
            int sel = (int)SendMessage(m_topicList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)m_topics.size()) {
                fetchTopic(m_topics[sel].id);
            }
        }
        return 0;

    case WM_CLOSE:
        ShowWindow(m_hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        m_hwnd = nullptr;
        return 0;

    case WM_APP: {
        // Error message from background thread
        std::string* errMsg = reinterpret_cast<std::string*>(lParam);
        if (errMsg) {
            SetWindowTextW(m_statusLabel, help_assets::toWide(*errMsg).c_str());
            delete errMsg;
        }
        return 0;
    }

    case WM_APP + 1:
        // Index loaded successfully
        updateTopicList();
        return 0;

    case WM_APP + 2: {
        // Topic content loaded. Discarded outright if the reader has selected
        // another topic since the download started - see HelpContentMsg for
        // why that can happen while a fetch is in flight. Called payload and
        // not msg because handleMessage's own parameter is msg, which /W4
        // rightly flags as a shadow (C4457).
        HelpContentMsg* payload = reinterpret_cast<HelpContentMsg*>(lParam);
        if (payload) {
            if (payload->topicId == m_currentTopicId) {
                displayContent(payload->content);
                if (!payload->status.empty()) {
                    SetWindowTextW(m_statusLabel, help_assets::toWide(payload->status).c_str());
                }
            }
            delete payload;
        }
        return 0;
    }
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void HelpWindow::createControls() {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // Topic list
    m_topicList = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        0, 0, 250, 400,
        m_hwnd,
        (HMENU)IDC_TOPIC_LIST,
        hInst,
        nullptr
    );
    SendMessage(m_topicList, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Content view (read-only edit control with vertical scrollbar)
    m_contentView = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        250, 0, 530, 400,
        m_hwnd,
        (HMENU)IDC_CONTENT_VIEW,
        hInst,
        nullptr
    );

    // Use a monospace font for content. Scale the height by the window's DPI:
    // the app is per-monitor DPI v2 aware, so CreateFontW's height is in raw
    // device pixels. Without this, 16px is tiny on a 4K @ 200% screen, while the
    // left-hand list (which uses DEFAULT_GUI_FONT) is scaled by the system and
    // looks correct. See the matching fix in TerminalView::createFont().
    UINT dpi = GetDpiForWindow(m_hwnd);
    if (dpi == 0) dpi = 96;
    HFONT hMonoFont = CreateFontW(
        MulDiv(16, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
        L"Consolas"
    );
    SendMessage(m_contentView, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

    // Status label
    m_statusLabel = CreateWindowExW(
        0,
        L"STATIC",
        L"Loading help index...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 780, 25,
        m_hwnd,
        (HMENU)IDC_STATUS_LABEL,
        hInst,
        nullptr
    );
    SendMessage(m_statusLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Trigger initial layout
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    SendMessage(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
}

void HelpWindow::fetchIndex() {
    if (m_loading) return;
    m_loading = true;

    SetWindowTextW(m_statusLabel, L"Loading help index...");

    std::thread([this]() {
        std::string json;
        std::string error;

        if (!downloadToString(INDEX_URL, json, error)) {
            // Online index unavailable: still offer the bundled local topics.
            seedLocalTopics();
            PostMessage(m_hwnd, WM_APP, 0, (LPARAM)new std::string("Online help unavailable - showing local topics."));
            m_loading = false;
            PostMessage(m_hwnd, WM_APP + 1, 0, 0);
            return;
        }

        std::vector<help_assets::HelpTopic> topics;
        if (!help_assets::parseIndexJson(json, topics, error)) {
            seedLocalTopics();
            PostMessage(m_hwnd, WM_APP, 0, (LPARAM)new std::string("Could not parse online index - showing local topics."));
            m_loading = false;
            PostMessage(m_hwnd, WM_APP + 1, 0, 0);
            return;
        }

        m_topics = topics;
        seedLocalTopics();  // bundled topics appear above the online ones
        m_loading = false;

        // Update UI on main thread
        PostMessage(m_hwnd, WM_APP + 1, 0, 0);
    }).detach();
}

void HelpWindow::seedLocalTopics() {
    // Ensure the bundled topics are present at the top of the list, without
    // duplicating them across reloads. Inserted in reverse so Getting Started
    // ends up first.
    auto ensureFront = [this](const char* id, const char* title, const char* desc) {
        for (const auto& t : m_topics) {
            if (t.id == id) return;
        }
        help_assets::HelpTopic t;
        t.id = id;
        t.title = title;
        t.description = desc;
        t.filename.clear();  // local: served from the app, not downloaded
        m_topics.insert(m_topics.begin(), t);
    };
    ensureFront(help_topics::Configuration, "Configuration File (z80cpmw.json)", "Keyboard map and settings");
    ensureFront(help_topics::GettingStarted, "Getting Started", "First steps, file transfer, keys");
}

bool HelpWindow::isLocalTopic(const std::string& topicId) const {
    return topicId == help_topics::GettingStarted ||
           topicId == help_topics::Configuration;
}

std::string HelpWindow::localTopicContent(const std::string& topicId) const {
    if (topicId == help_topics::GettingStarted) return help_topics::gettingStartedMarkdown();
    if (topicId == help_topics::Configuration)  return help_topics::configurationMarkdown();
    return std::string();
}

void HelpWindow::selectTopicInList(const std::string& topicId) {
    if (!m_topicList) return;
    for (size_t i = 0; i < m_topics.size(); ++i) {
        if (m_topics[i].id == topicId) {
            SendMessage(m_topicList, LB_SETCURSEL, (WPARAM)i, 0);
            return;
        }
    }
}

void HelpWindow::fetchTopic(const std::string& topicId) {
    // Bundled topics are rendered directly, with no network access. This is
    // step three of the resolve order arriving first, which is not a
    // contradiction: these two topics have no remote copy to prefer, so there
    // is nothing for a download to be ahead of.
    if (isLocalTopic(topicId)) {
        m_currentTopicId = topicId;
        displayContent(localTopicContent(topicId));
        SetWindowTextW(m_statusLabel, help_assets::toWide(viewingStatus(
            titleOf(m_topics, topicId),
            help_assets::sourceLabel(help_assets::TopicSource::Bundled))).c_str());
        return;
    }

    if (m_loading) return;

    // Check the in-memory cache first - m_cache, fifteen minutes, lost at exit;
    // the on-disk one is consulted inside resolveTopic on the download thread
    // below, after the download it is a fallback for.
    //
    // m_currentTopicId moves with the pane here as well as on the fetch path:
    // it is what says which topic is on screen, and it is read in two places -
    // the staleness test in the WM_APP + 2 handler, and updateTopicList's guard
    // against taking the status line back. Leaving it behind on a cache hit -
    // because the download thread clears m_loading just BEFORE it posts - would
    // let a result still sitting in the queue pass that test and repaint over
    // the cached topic the reader had just switched to.
    std::string* cached = findCachedContent(topicId);
    if (cached) {
        m_currentTopicId = topicId;
        displayContent(*cached);
        // "this session's copy" and not one of sourceLabel()'s answers, because
        // m_cache does not record which of them its entry came from - it may be
        // a download from a minute ago or the offline copy that download failed
        // over to. HelpCache lives in HelpWindow.h, which this commit does not
        // own, so the honest answer is the vague one rather than a guess. What
        // it does say truthfully is that no network read happened just now.
        SetWindowTextW(m_statusLabel, help_assets::toWide(viewingStatus(
            titleOf(m_topics, topicId), "this session's copy")).c_str());
        return;
    }

    m_loading = true;
    m_currentTopicId = topicId;

    // Find filename for topic
    std::string filename;
    std::string topicTitle = topicId;
    for (const auto& topic : m_topics) {
        if (topic.id == topicId) {
            filename = topic.filename;
            topicTitle = topic.title;
            break;
        }
    }

    // The name from help_index.json is judged BEFORE it is pasted into a URL or
    // turned into a path. It arrived over the network, and isSafeAssetName is a
    // whitelist of what a plain file name may contain, so a name carrying a
    // separator, a drive letter, a leading dot or a device stem never reaches
    // WinHttpCrackUrl or CreateFileW. help_assets::cachePath applies the same
    // test again on its own first statement - that is not redundancy for its
    // own sake, it is the file-system half refusing to depend on this caller
    // having remembered.
    //
    // Both arms report IN THE PANE. Leaving the pane alone was the earlier
    // behaviour and it is the same defect the download failure arm was fixed
    // for: m_currentTopicId has already moved, so the reader would be looking
    // at the previous topic under this topic's name, with a status line that
    // still said "Viewing:" the old one. The rejected name is deliberately not
    // echoed - it is unbounded, attacker-chosen text, and it would tell the
    // reader nothing they can act on.
    if (filename.empty() || !help_assets::isSafeAssetName(filename)) {
        m_loading = false;
        std::string why = filename.empty()
            ? "The help index lists this topic with no file name."
            : "The help index gives this topic a file name that is not a plain "
              "file name, so this build will not fetch it or store it.";
        displayContent("# " + topicTitle + "\n\nThis topic cannot be loaded.\n\n" + why + "\n");
        SetWindowTextW(m_statusLabel,
                       help_assets::toWide("Cannot load topic: " + topicTitle).c_str());
        return;
    }

    SetWindowTextW(m_statusLabel, L"Loading topic...");

    std::thread([this, filename, topicId, topicTitle]() {
        // toWide rather than the char-by-char widening that stood here. The
        // isSafeAssetName test above makes the two identical - a name that
        // passes it is ASCII by construction - so this is not a fix, it is the
        // file having one rule for narrow-to-wide instead of two.
        std::wstring url = CONTENT_BASE_URL + help_assets::toWide(filename);

        std::string content;
        std::string error;

        bool downloaded = downloadToString(url, content, error);

        // todo.txt's order, and the only place this window expresses it:
        // download, then cache, then the copy in the binary. resolveTopic
        // writes the cache when the download succeeded, reads it when it did
        // not, and falls through to a bundled copy - which today is empty for
        // every topic that reaches here, so the chain really ends at the cache.
        // The bundling commit changes the third argument and nothing else.
        help_assets::ResolvedTopic resolved = help_assets::resolveTopic(
            filename, downloaded ? content : std::string(), std::string());

        if (resolved.source == help_assets::TopicSource::None) {
            // Report the failure IN THE PANE, not only on the status line. An
            // earlier version posted only the status line, so the pane went on
            // showing the topic the reader had been reading before - a failed
            // click looked like a click that had not registered, and a reader
            // who then scrolled was reading the wrong topic under the new
            // topic's name.
            //
            // Pane text and status line travel in one HelpContentMsg tagged
            // with topicId, so the window thread shows both or neither. What
            // stood here instead was two posts (WM_APP + 2 then WM_APP) whose
            // correctness was argued from posted messages arriving in order,
            // resting on the claim that m_currentTopicId was still "the topic
            // that just failed". That claim is false the moment the reader
            // picks a bundled topic mid-download, because isLocalTopic() is
            // handled above the m_loading guard - so the error could land on
            // top of Getting Started.
            //
            // m_loading is cleared before the post, not after, so that the
            // "select the topic again" this message advises is not swallowed
            // by the guard when the reader acts on it immediately.
            m_loading = false;

            // A download that succeeded and returned nothing lands here too:
            // resolveTopic treats empty as absent, and downloadToString reports
            // success for an HTTP 200 with an empty body, so "error" would
            // otherwise be an empty string in the middle of the message.
            if (downloaded) error = "The server returned an empty file.";

            HelpContentMsg* msg = new HelpContentMsg;
            msg->topicId = topicId;
            msg->content = "# " + topicTitle + "\n\nThis topic could not be downloaded.\n\n"
                + error + "\n\nIt is fetched from the network when you open it; "
                  "check the connection and select the topic again. No offline "
                  "copy of it has been saved on this machine - one is kept for "
                  "each topic after the first time you read it.\n";
            msg->status = "Failed to load topic: " + error;
            if (!PostMessage(m_hwnd, WM_APP + 2, 0, (LPARAM)msg)) {
                delete msg;   // window already gone; nothing will dequeue it
            }
            return;
        }

        // The in-memory cache takes whatever was resolved, not only a fresh
        // download: a reader who is offline should not wait out a WinHTTP
        // timeout again to reread the topic they just read from disk. The
        // fifteen-minute TTL still expires it, so the network is retried.
        //
        // This assignment stays BEFORE the m_loading store, which is what
        // publishes it. m_cache is written here on the download thread and read
        // by findCachedContent on the window thread, and the only thing
        // ordering the two is that the reader tests the atomic m_loading first.
        cacheContent(topicId, resolved.content);
        m_loading = false;

        std::string source = help_assets::sourceLabel(resolved.source);
        if (!resolved.savedWhen.empty()) source += ", saved " + resolved.savedWhen;

        HelpContentMsg* msg = new HelpContentMsg;
        msg->topicId = topicId;
        msg->content = resolved.content;
        msg->status = viewingStatus(topicTitle, source);
        if (!PostMessage(m_hwnd, WM_APP + 2, 0, (LPARAM)msg)) {
            delete msg;
        }
    }).detach();
}

void HelpWindow::updateTopicList() {
    if (!m_topicList) return;

    SendMessage(m_topicList, LB_RESETCONTENT, 0, 0);

    for (const auto& topic : m_topics) {
        std::wstring title = help_assets::toWide(topic.title);
        SendMessageW(m_topicList, LB_ADDSTRING, 0, (LPARAM)title.c_str());
    }

    // Only claim the status line while the pane is empty. This runs from
    // WM_CREATE and again from WM_APP + 1 when the index arrives, and show()
    // can put a bundled topic on screen in between - so the unconditional write
    // that stood here replaced "Viewing: Getting Started (bundled with the
    // app)" with "Select a topic from the list" a moment after the reader was
    // shown the topic they asked for. The status line answers "which copy of
    // what am I reading"; it must not be taken back by an event that changed
    // neither.
    if (m_currentTopicId.empty()) {
        SetWindowTextW(m_statusLabel, L"Select a topic from the list");
    }
}

void HelpWindow::displayContent(const std::string& markdown) {
    if (!m_contentView) return;

    std::wstring wtext = help_assets::toWide(help_assets::markdownToText(markdown));
    SetWindowTextW(m_contentView, wtext.c_str());

    // The status line is the caller's, not this function's. It used to write
    // "Viewing: <title>" here, and as of this commit all four callers write
    // their own line straight afterwards - the three synchronous branches of
    // fetchTopic, and the WM_APP + 2 handler with HelpContentMsg::status, which
    // both arms of the download thread now fill in - so keeping it would mean
    // setting the line twice and never showing the first value.
    //
    // It is also the one value that could not be right. This function is handed
    // markdown and cannot tell whether it came from the network, from
    // help_assets' cache or from the binary, and with a cache in the picture
    // that is exactly what the reader needs to know. Leaving the write here
    // would let a future caller inherit a status line that names the topic and
    // quietly lies about which copy of it is on screen.
}

bool HelpWindow::downloadToString(const std::wstring& url, std::string& result, std::string& error) {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    bool success = false;

    // Parse URL
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp)) {
        error = "Invalid URL";
        return false;
    }

    // Open session
    hSession = WinHttpOpen(L"z80cpmw/" VERSION_STRING_W,
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        error = "Failed to open HTTP session";
        goto cleanup;
    }

    // Connect
    hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        error = "Failed to connect to server";
        goto cleanup;
    }

    // Open request
    hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                                   nullptr, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        error = "Failed to create request";
        goto cleanup;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = "Failed to send request";
        goto cleanup;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        error = "Failed to receive response";
        goto cleanup;
    }

    // Check for redirect (GitHub releases redirect)
    {
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           nullptr, &statusCode, &statusCodeSize, nullptr);

        if (statusCode >= 300 && statusCode < 400) {
            // Get redirect URL
            wchar_t redirectUrl[2048] = {};
            DWORD redirectSize = sizeof(redirectUrl);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, nullptr,
                                   redirectUrl, &redirectSize, nullptr)) {
                // Close current handles and follow redirect
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return downloadToString(redirectUrl, result, error);
            }
        }

        if (statusCode != 200) {
            error = "HTTP error: " + std::to_string(statusCode);
            goto cleanup;
        }
    }

    // Read data, and then decide whether what was read is the whole document.
    //
    // This loop used to end with an unconditional "success = true", which made
    // both of its exits silent failures: a WinHttpQueryDataAvailable that
    // returned FALSE was taken for the end of the body, and a WinHttpReadData
    // that returned FALSE dropped a chunk on the floor and kept looping. The
    // caller could not tell a half-topic from a topic, so fetchTopic cached the
    // fragment over the complete offline copy and told the reader
    // "(downloaded)". help_assets::downloadIsComplete carries the rule and the
    // measurements behind it; the two things this half owes it are the
    // Content-Length and an honest error code.
    {
        std::stringstream ss;
        DWORD bytesAvailable = 0;
        DWORD bytesRead = 0;
        char buffer[8192];
        DWORD readError = 0;

        // NUMBER64 rather than NUMBER: the 32-bit form cannot represent a
        // length above 4 GB, and this file should not have to reason about
        // what it does with one.
        //
        // -1 is "the response announced no length" and is NOT the same as
        // zero. A chunked response carries no Content-Length at all - measured
        // against a localhost server, WinHttpQueryHeaders failed it with
        // ERROR_WINHTTP_HEADER_NOT_FOUND (12150) - and refusing those would
        // take remote help offline the day the asset host switched to chunked.
        // downloadIsComplete is where that decision is written down.
        //
        // The redirects are already behind us here: WinHTTP follows GitHub's
        // two 302s itself, and the same probe pointed at the published
        // help_cpm22.md URL saw status 200 with Content-Length 5147, so this
        // reads the asset's own header rather than a redirect's.
        long long declaredLength = -1;
        {
            DWORD64 contentLength = 0;
            DWORD lengthSize = sizeof(contentLength);
            if (WinHttpQueryHeaders(hRequest,
                                    WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &contentLength,
                                    &lengthSize, WINHTTP_NO_HEADER_INDEX)
                && contentLength <= (DWORD64)MAXLONGLONG) {
                declaredLength = (long long)contentLength;
            }
        }

        do {
            bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
                // The connection dropped, or the server sent something WinHTTP
                // could not make a body out of. Either way the rest of the
                // document is not coming; breaking without recording it is what
                // turned this into an end-of-body.
                //
                // The zero guard is not decoration: downloadIsComplete reads 0
                // as "every read returned", so a failed call whose last-error
                // happened to be clear would be laundered back into a success.
                readError = GetLastError();
                if (readError == 0) readError = ERROR_WINHTTP_INTERNAL_ERROR;
                break;
            }

            if (bytesAvailable == 0) {
                break;   // the ONLY clean exit from this loop
            }

            DWORD toRead = (bytesAvailable < sizeof(buffer)) ? bytesAvailable : (DWORD)sizeof(buffer);
            if (!WinHttpReadData(hRequest, buffer, toRead, &bytesRead)) {
                // Skipping the chunk and looping - what stood here - assembled a
                // document with a hole in the middle and reported success. Same
                // zero guard as above, for the same reason.
                readError = GetLastError();
                if (readError == 0) readError = ERROR_WINHTTP_INTERNAL_ERROR;
                break;
            }
            ss.write(buffer, bytesRead);
        } while (bytesAvailable > 0);

        result = ss.str();
        success = help_assets::downloadIsComplete(declaredLength, result.size(),
                                                  readError, error);

        // The fragment does not leave this function even as an out-parameter.
        // fetchTopic already reads "result" only when this returns true, but
        // that is the caller's discipline and this is the one place that knows
        // the bytes are a fragment.
        if (!success) result.clear();
    }

cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    return success;
}

std::string* HelpWindow::findCachedContent(const std::string& topicId) {
    DWORD now = GetTickCount();

    for (auto& entry : m_cache) {
        if (entry.topicId == topicId) {
            // Check if cache is still valid
            if (now - entry.timestamp < CACHE_TTL_MS) {
                return &entry.content;
            }
            // Cache expired, remove it
            break;
        }
    }
    return nullptr;
}

void HelpWindow::cacheContent(const std::string& topicId, const std::string& content) {
    // Remove old entry if exists
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->topicId == topicId) {
            m_cache.erase(it);
            break;
        }
    }

    // Add new entry
    HelpCache entry;
    entry.topicId = topicId;
    entry.content = content;
    entry.timestamp = GetTickCount();
    m_cache.push_back(entry);

    // Limit cache size
    while (m_cache.size() > 20) {
        m_cache.erase(m_cache.begin());
    }
}

// Global helper function
void ShowHelpWindow(HWND parent, const std::string& topicId) {
    if (!g_helpWindow) {
        g_helpWindow = new HelpWindow();
    }
    g_helpWindow->show(parent, topicId);
}
