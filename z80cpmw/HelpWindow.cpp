/*
 * HelpWindow.cpp - Remote Help System Window Implementation
 */

#include "pch.h"
#include "HelpWindow.h"
#include "resource.h"
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
static std::string getLocalGettingStartedMarkdown() {
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

Give a full path (recommended), for example:

    R8 C:\Users\me\Desktop\getkey2.com

A bare name uses the app's data folder. Its exact location (which the Store build
redirects) is shown in Emulator > Settings and in Help > About; the Open Folder
button there opens it in Explorer.

## Keyboard

| Key | Action |
| --- | --- |
| F5 | Start emulator |
| Shift+F5 | Stop emulator |
| Ctrl+R | Reset emulator |
| F1 | Help |

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
// docs/CONFIGURATION.md. Uses indented blocks instead of ``` fences because the
// in-app markdown renderer (markdownToText) does not understand code fences.
// Note: backslashes are doubled here so the displayed text matches the doubled
// backslashes the user actually sees in z80cpmw.json.
static std::string getLocalConfigHelpMarkdown() {
    return R"DOC(# Configuration File (z80cpmw.json)

z80cpmw keeps its settings in a JSON file you can edit by hand:

    %LOCALAPPDATA%\z80cpmw\z80cpmw.json

Tip: Emulator > Settings > Open Folder opens the data folder
(...\z80cpmw\data, where disks and R8/W8 transfers live). The z80cpmw.json file
is one level up, in the z80cpmw folder. On the Microsoft Store build both live
under ...\Packages\AaronWohl.Z80CPM_<hash>\LocalCache\Local\z80cpmw\ - you don't
need to type that: Settings and Help > About show the real, resolved path.

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

### F1 and F5

F1 opens Help and F5 / Shift+F5 Start and Stop the emulator, so by default those
keys are not sent to CP/M. To deliver them to CP/M instead, set:

    "f1ToCpm": true     sends F1 to CP/M (Help stays on the Help menu)
    "f5ToCpm": true     sends F5 and Shift+F5 to CP/M

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
            std::wstring wErr(errMsg->begin(), errMsg->end());
            SetWindowTextW(m_statusLabel, wErr.c_str());
            delete errMsg;
        }
        return 0;
    }

    case WM_APP + 1:
        // Index loaded successfully
        updateTopicList();
        return 0;

    case WM_APP + 2: {
        // Topic content loaded
        std::string* content = reinterpret_cast<std::string*>(lParam);
        if (content) {
            displayContent(*content);
            delete content;
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

        std::vector<HelpTopic> topics;
        if (!parseIndexJson(json, topics, error)) {
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
        HelpTopic t;
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
    if (topicId == help_topics::GettingStarted) return getLocalGettingStartedMarkdown();
    if (topicId == help_topics::Configuration)  return getLocalConfigHelpMarkdown();
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
    // Bundled topics are rendered directly, with no network access.
    if (isLocalTopic(topicId)) {
        m_currentTopicId = topicId;
        displayContent(localTopicContent(topicId));
        return;
    }

    if (m_loading) return;

    // Check cache first
    std::string* cached = findCachedContent(topicId);
    if (cached) {
        displayContent(*cached);
        return;
    }

    m_loading = true;
    m_currentTopicId = topicId;

    // Find filename for topic
    std::string filename;
    for (const auto& topic : m_topics) {
        if (topic.id == topicId) {
            filename = topic.filename;
            break;
        }
    }

    if (filename.empty()) {
        m_loading = false;
        return;
    }

    SetWindowTextW(m_statusLabel, L"Loading topic...");

    std::thread([this, filename, topicId]() {
        // Build URL
        std::wstring url = CONTENT_BASE_URL;
        for (char c : filename) {
            url += static_cast<wchar_t>(c);
        }

        std::string content;
        std::string error;

        if (!downloadToString(url, content, error)) {
            PostMessage(m_hwnd, WM_APP, 0, (LPARAM)new std::string("Failed to load topic: " + error));
            m_loading = false;
            return;
        }

        // Cache the content
        cacheContent(topicId, content);
        m_loading = false;

        // Update UI on main thread
        PostMessage(m_hwnd, WM_APP + 2, 0, (LPARAM)new std::string(content));
    }).detach();
}

void HelpWindow::updateTopicList() {
    if (!m_topicList) return;

    SendMessage(m_topicList, LB_RESETCONTENT, 0, 0);

    for (const auto& topic : m_topics) {
        std::wstring title(topic.title.begin(), topic.title.end());
        SendMessageW(m_topicList, LB_ADDSTRING, 0, (LPARAM)title.c_str());
    }

    SetWindowTextW(m_statusLabel, L"Select a topic from the list");
}

void HelpWindow::displayContent(const std::string& markdown) {
    if (!m_contentView) return;

    std::string text = markdownToText(markdown);
    std::wstring wtext(text.begin(), text.end());
    SetWindowTextW(m_contentView, wtext.c_str());

    // Find current topic title
    for (const auto& topic : m_topics) {
        if (topic.id == m_currentTopicId) {
            std::wstring status = L"Viewing: ";
            status += std::wstring(topic.title.begin(), topic.title.end());
            SetWindowTextW(m_statusLabel, status.c_str());
            break;
        }
    }
}

std::string HelpWindow::markdownToText(const std::string& markdown) {
    std::stringstream result;
    std::istringstream stream(markdown);
    std::string line;

    // Table parsing state
    std::vector<std::vector<std::string>> tableRows;
    std::vector<size_t> colWidths;
    bool inTable = false;

    // Helper to parse a table row
    auto parseTableRow = [](const std::string& row) -> std::vector<std::string> {
        std::vector<std::string> cells;
        size_t start = 0;
        if (!row.empty() && row[0] == '|') start = 1;

        size_t pos = start;
        while (pos < row.length()) {
            size_t next = row.find('|', pos);
            if (next == std::string::npos) next = row.length();

            std::string cell = row.substr(pos, next - pos);
            // Trim whitespace
            size_t first = cell.find_first_not_of(" \t");
            size_t last = cell.find_last_not_of(" \t");
            if (first != std::string::npos && last != std::string::npos) {
                cell = cell.substr(first, last - first + 1);
            } else {
                cell = "";
            }
            cells.push_back(cell);
            pos = next + 1;
        }
        // Remove trailing empty cell if line ended with |
        if (!cells.empty() && cells.back().empty()) {
            cells.pop_back();
        }
        return cells;
    };

    // Helper to check if line is table separator (|---|---|)
    auto isTableSeparator = [](const std::string& row) -> bool {
        for (char c : row) {
            if (c != '|' && c != '-' && c != ':' && c != ' ' && c != '\t') {
                return false;
            }
        }
        return row.find('-') != std::string::npos;
    };

    // Helper to flush table
    auto flushTable = [&]() {
        if (tableRows.empty()) return;

        // Calculate column widths
        colWidths.clear();
        for (const auto& row : tableRows) {
            for (size_t i = 0; i < row.size(); i++) {
                if (i >= colWidths.size()) {
                    colWidths.push_back(row[i].length());
                } else {
                    colWidths[i] = (std::max)(colWidths[i], row[i].length());
                }
            }
        }

        // Output table with proper spacing
        bool firstRow = true;
        for (const auto& row : tableRows) {
            std::stringstream rowOut;
            for (size_t i = 0; i < row.size(); i++) {
                if (i > 0) rowOut << "  ";
                rowOut << row[i];
                if (i < colWidths.size()) {
                    size_t padding = colWidths[i] - row[i].length();
                    rowOut << std::string(padding, ' ');
                }
            }
            result << rowOut.str() << "\r\n";

            // Add separator line after header
            if (firstRow && tableRows.size() > 1) {
                for (size_t i = 0; i < colWidths.size(); i++) {
                    if (i > 0) result << "  ";
                    result << std::string(colWidths[i], '-');
                }
                result << "\r\n";
                firstRow = false;
            }
        }

        tableRows.clear();
        colWidths.clear();
    };

    while (std::getline(stream, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Remove leading/trailing whitespace for processing
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            if (inTable) {
                flushTable();
                inTable = false;
            }
            result << "\r\n";
            continue;
        }

        std::string trimmed = line.substr(start);

        // Check for table row (starts with |)
        if (trimmed[0] == '|') {
            if (isTableSeparator(trimmed)) {
                // Skip separator row, we draw our own
                inTable = true;
                continue;
            }

            std::vector<std::string> cells = parseTableRow(trimmed);
            if (!cells.empty()) {
                tableRows.push_back(cells);
                inTable = true;
            }
            continue;
        }

        // Not a table row - flush any pending table
        if (inTable) {
            flushTable();
            inTable = false;
        }

        // Convert headers (# to underlined text)
        if (trimmed[0] == '#') {
            size_t level = 0;
            while (level < trimmed.length() && trimmed[level] == '#') level++;
            std::string headerText = trimmed.substr(level);
            // Trim leading space
            if (!headerText.empty() && headerText[0] == ' ') {
                headerText = headerText.substr(1);
            }
            result << headerText << "\r\n";
            if (level == 1) {
                result << std::string(headerText.length(), '=') << "\r\n";
            } else if (level == 2) {
                result << std::string(headerText.length(), '-') << "\r\n";
            }
            continue;
        }

        // Convert bullet points
        if (trimmed.length() >= 2 && (trimmed[0] == '-' || trimmed[0] == '*') && trimmed[1] == ' ') {
            result << "  * " << trimmed.substr(2) << "\r\n";
            continue;
        }

        // Convert bold (**text** or __text__)
        std::string processed = line;
        size_t pos = 0;
        while ((pos = processed.find("**", pos)) != std::string::npos) {
            size_t end = processed.find("**", pos + 2);
            if (end != std::string::npos) {
                std::string bold = processed.substr(pos + 2, end - pos - 2);
                processed = processed.substr(0, pos) + bold + processed.substr(end + 2);
            } else {
                break;
            }
        }

        // Convert inline code (`code`)
        pos = 0;
        while ((pos = processed.find("`", pos)) != std::string::npos) {
            size_t end = processed.find("`", pos + 1);
            if (end != std::string::npos) {
                std::string code = processed.substr(pos + 1, end - pos - 1);
                processed = processed.substr(0, pos) + code + processed.substr(end + 1);
            } else {
                break;
            }
        }

        result << processed << "\r\n";
    }

    // Flush any remaining table
    if (inTable) {
        flushTable();
    }

    return result.str();
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
    hSession = WinHttpOpen(L"z80cpmw/1.0",
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

    // Read data
    {
        std::stringstream ss;
        DWORD bytesAvailable = 0;
        DWORD bytesRead = 0;
        char buffer[8192];

        do {
            bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
                break;
            }

            if (bytesAvailable == 0) {
                break;
            }

            DWORD toRead = (bytesAvailable < sizeof(buffer)) ? bytesAvailable : (DWORD)sizeof(buffer);
            if (WinHttpReadData(hRequest, buffer, toRead, &bytesRead)) {
                ss.write(buffer, bytesRead);
            }
        } while (bytesAvailable > 0);

        result = ss.str();
        success = true;
    }

cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    return success;
}

bool HelpWindow::parseIndexJson(const std::string& json, std::vector<HelpTopic>& topics, std::string& error) {
    topics.clear();

    // Simple JSON parsing for help_index.json format:
    // { "version": 1, "base_url": "...", "topics": [ { "id": "", "title": "", "description": "", "filename": "" }, ... ] }

    size_t topicsStart = json.find("\"topics\"");
    if (topicsStart == std::string::npos) {
        error = "No topics array found";
        return false;
    }

    size_t arrayStart = json.find('[', topicsStart);
    if (arrayStart == std::string::npos) {
        error = "Invalid topics format";
        return false;
    }

    size_t pos = arrayStart + 1;

    while (true) {
        // Find next topic object
        size_t objStart = json.find('{', pos);
        if (objStart == std::string::npos) break;

        size_t objEnd = json.find('}', objStart);
        if (objEnd == std::string::npos) break;

        std::string obj = json.substr(objStart, objEnd - objStart + 1);
        HelpTopic topic;

        // Extract fields
        auto extractField = [&obj](const std::string& field) -> std::string {
            std::string key = "\"" + field + "\"";
            size_t keyPos = obj.find(key);
            if (keyPos == std::string::npos) return "";

            size_t colonPos = obj.find(':', keyPos);
            if (colonPos == std::string::npos) return "";

            size_t valueStart = obj.find('"', colonPos);
            if (valueStart == std::string::npos) return "";

            size_t valueEnd = obj.find('"', valueStart + 1);
            if (valueEnd == std::string::npos) return "";

            return obj.substr(valueStart + 1, valueEnd - valueStart - 1);
        };

        topic.id = extractField("id");
        topic.title = extractField("title");
        topic.description = extractField("description");
        topic.filename = extractField("filename");

        if (!topic.id.empty() && !topic.title.empty()) {
            topics.push_back(topic);
        }

        pos = objEnd + 1;
    }

    if (topics.empty()) {
        error = "No valid topics found";
        return false;
    }

    return true;
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
