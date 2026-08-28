/*
 * HelpAssets.cpp - The state-free half of the help system.
 *
 * See HelpAssets.h for why these four live apart from HelpWindow.
 */

#include "HelpAssets.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace help_assets {

//=============================================================================
// Markdown rendering
//=============================================================================

std::string markdownToText(const std::string& markdown) {
    std::stringstream result;
    std::istringstream stream(markdown);
    std::string line;

    // Table parsing state
    std::vector<std::vector<std::string>> tableRows;
    std::vector<size_t> colWidths;
    bool inTable = false;

    // Fenced code block state - see the fence branch in the loop below.
    bool inFence = false;

    // Strip the inline markers: **bold** and `code` both become their contents.
    //
    // This used to be written out at the bottom of the loop, after the header,
    // bullet and table branches had each already run "continue", so the three
    // kinds of line that most often carry inline markup were the three that
    // never had it removed. Measured over the eight assets published in
    // avwohl/ioscpm: 63 bullet lines carry ** or a backtick, and six table rows
    // in help_quick_start.md carry backticks, all of which rendered with the
    // markers visible. It is a lambda now so every branch that emits text can
    // call it and no future branch can forget to.
    //
    // An unpaired marker is left alone rather than deleted - "a ** b" and a
    // lone backtick are ordinary prose, and swallowing the rest of the line
    // after one would be worse than showing it.
    auto applyInline = [](std::string s) -> std::string {
        size_t pos = 0;
        while ((pos = s.find("**", pos)) != std::string::npos) {
            size_t end = s.find("**", pos + 2);
            if (end == std::string::npos) break;
            s = s.substr(0, pos) + s.substr(pos + 2, end - pos - 2) + s.substr(end + 2);
        }
        pos = 0;
        while ((pos = s.find('`', pos)) != std::string::npos) {
            size_t end = s.find('`', pos + 1);
            if (end == std::string::npos) break;
            s = s.substr(0, pos) + s.substr(pos + 1, end - pos - 1) + s.substr(end + 1);
        }
        return s;
    };

    // Helper to parse a table row. The cells are rendered here rather than at
    // output time because flushTable measures them to pick the column widths,
    // and a width measured on "`DIR B:`" pads the column two characters too
    // wide once the backticks are gone.
    auto parseTableRow = [&applyInline](const std::string& row) -> std::vector<std::string> {
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
            cells.push_back(applyInline(cell));
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
        bool blank = (start == std::string::npos);

        // Fenced code blocks. There was no branch for these at all, so every
        // fence line reached the fall-through and printed its own backticks:
        // 170 such lines across the eight published assets, 60 of them in
        // help_cpm22.md, where a two-line example is wrapped in a pair. The
        // fence marker itself is never printed, and what it encloses is emitted
        // verbatim - a fence exists to say "these characters are not markdown",
        // so applyInline must not run on it, and neither must the bullet branch
        // (a diff or a shell transcript inside a fence legitimately begins with
        // a dash).
        //
        // Indented by four, which is what the two bundled topics already use
        // for their code blocks; those reach the pane indented because the
        // fall-through prints the original line rather than the trimmed one, so
        // a fenced block and an indented block now look the same in the pane.
        //
        // Any line whose trimmed form STARTS with three backticks toggles,
        // rather than only a line that is exactly three, so an info string
        // ("```asm") is consumed with its fence. An unterminated fence runs to
        // the end of the document; that is a defect in the document, and
        // printing the rest of it indented is more useful than printing the
        // stray backticks.
        if (!blank && line.compare(start, 3, "```") == 0) {
            if (inTable) {
                flushTable();
                inTable = false;
            }
            inFence = !inFence;
            continue;
        }

        if (inFence) {
            if (blank) {
                result << "\r\n";
            } else {
                result << "    " << line << "\r\n";
            }
            continue;
        }

        if (blank) {
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

        // Convert headers (# to underlined text). The underline is measured on
        // the rendered text, not the source, so a header carrying inline markup
        // is not underlined past its own width.
        if (trimmed[0] == '#') {
            size_t level = 0;
            while (level < trimmed.length() && trimmed[level] == '#') level++;
            std::string headerText = trimmed.substr(level);
            // Trim leading space
            if (!headerText.empty() && headerText[0] == ' ') {
                headerText = headerText.substr(1);
            }
            headerText = applyInline(headerText);
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
            result << "  * " << applyInline(trimmed.substr(2)) << "\r\n";
            continue;
        }

        // Ordinary text. The whole line, not the trimmed one: the indentation
        // is how the bundled topics mark their code blocks.
        result << applyInline(line) << "\r\n";
    }

    // Flush any remaining table
    if (inTable) {
        flushTable();
    }

    return result.str();
}

//=============================================================================
// help_index.json
//=============================================================================

bool parseIndexJson(const std::string& json, std::vector<HelpTopic>& topics, std::string& error) {
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

//=============================================================================
// Names and text
//=============================================================================

bool isSafeAssetName(const std::string& name) {
    // Empty, or long enough to be an attack on something else downstream. The
    // seven names in the published help_index.json run from 11 characters
    // (help_qpm.md) to 21 (help_file_transfer.md).
    if (name.empty() || name.size() > 96) return false;

    // This is the rule that refuses "..", "../evil.md" and "..\evil.md". A name
    // that has to be a single path component - which the whitelist below
    // enforces, by refusing both separators - can only climb by starting with a
    // dot, so the two rules together are the whole of the traversal defence.
    // The same test refuses a leading dash, which anything downstream that
    // takes a command line reads as an option rather than as a file.
    //
    // A separate "no .. anywhere" test was written and then removed: with a
    // leading dot already refused and every separator refused below, a dot-dot
    // in the middle of a name cannot climb anywhere, so that test rejected only
    // harmless names like "a..b.md" and no case in tests\test_help.cpp could
    // tell whether it was there. A guard that cannot change an answer is not
    // defence in depth, it is a claim nobody can check.
    if (name[0] == '.' || name[0] == '-') return false;

    // Whitelist. Everything not listed here is refused, which is the point:
    // this does not have to enumerate what is dangerous. It is what turns away
    // a slash, a backslash, a colon (drive letter, and alternate data stream),
    // a wildcard, a quote, a space, a control character and any byte over 0x7F.
    for (unsigned char c : name) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }

    // Windows resolves a device name through its extension: "NUL.md" is the
    // null device, not a file, so the cache write the next commit makes would
    // succeed and store nothing, and "CON.md" would read from the console. The
    // comparison is against the stem alone and case-insensitive, which is how
    // the name itself resolves. COM10 is not a device and is not on the list.
    static const char* const kDevices[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
    };
    std::string stem = name.substr(0, name.find('.'));
    for (char& c : stem) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    }
    for (const char* device : kDevices) {
        if (stem == device) return false;
    }

    return true;
}

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();

    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    if (needed <= 0) return std::wstring();

    std::wstring out((size_t)needed, L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(),
                                      &out[0], needed);
    if (written <= 0) return std::wstring();
    out.resize((size_t)written);
    return out;
}

bool downloadIsComplete(long long declaredLength,
                        unsigned long long assembled,
                        unsigned long readError,
                        std::string& error) {
    // The failed read is tested first because it is the CAUSE of whatever
    // shortfall the length test would otherwise report, and the WinHTTP code
    // is the only thing here that says what went wrong on the wire.
    //
    // The rule is absolute - a failed read is a failed download even when the
    // announced length was already satisfied - and that costs nothing, because
    // downloadToString's loop stops at the first "0 bytes available" and so
    // cannot reach a failing call after the body is complete. Softening it to
    // "a failed read only matters if the body is also short" was rejected for
    // buying nothing and having to be re-argued every time that loop changes.
    if (readError != 0) {
        error = "The download failed part-way through (WinHTTP error "
                + std::to_string(readError) + ").";
        return false;
    }

    // declaredLength < 0 is "the response announced no length" - a chunked
    // response, which is the shape this must not refuse. See HelpAssets.h for
    // what the asset host was measured to send and why the absence is allowed.
    if (declaredLength >= 0 && assembled != (unsigned long long)declaredLength) {
        // The over-length branch cannot fire against WinHTTP as this program
        // uses it. WINHTTP_OPTION_DECOMPRESSION is set nowhere in this tree, so
        // nothing is inflated on the way in and the bytes counted here are the
        // bytes on the wire: a probe with the same options WinHttpOpen is given
        // in downloadToString sent "GET /full HTTP/1.1 | Connection: Keep-Alive
        // | User-Agent: ... | Host: ..." and no Accept-Encoding at all, read
        // off a localhost server on 2026-08-28. The branch is written anyway so
        // that "matches" means matches rather than "is at least", and so that a
        // commit that turns decompression on finds a message here instead of a
        // silent pass.
        error = assembled < (unsigned long long)declaredLength
            ? "The download stopped early: " + std::to_string(assembled)
              + " bytes of the " + std::to_string(declaredLength)
              + " the server announced."
            : "The download ran past the length the server announced: "
              + std::to_string(assembled) + " bytes of "
              + std::to_string(declaredLength) + ".";
        return false;
    }

    return true;
}

//=============================================================================
// The on-disk cache
//
// See HelpAssets.h for what this is for and why setCacheRoot is a seam rather
// than a fourth copy of the known-folder snippet.
//=============================================================================

// Empty means "setCacheRoot was never called", not "the cache is off": cacheDir
// computes the fallback in that case. Written once at startup and read from the
// thread fetchTopic() detaches - see setCacheRoot's comment in the header.
static std::string g_cacheRoot;

// A ceiling on what readCached will pull into memory and what writeCached will
// store. Fetched from the URL HelpWindow uses - the eight assets on
// avwohl/ioscpm's "latest" release - the whole set is 23,997 bytes and the
// largest single one is help_cpm22.md at 5,147, so this is about two hundred
// times the largest thing it is meant to hold. It is deliberately not tight:
// the job is to bound a read of a file some other process may have grown, not
// to police the size of a help topic.
static const size_t kMaxCacheBytes = 1024 * 1024;

// UTF-8 out of a wide Win32 result. This is NOT the getUserDataDirectory
// snippet the header refuses to copy - there is no SHGetKnownFolderPath and no
// CoTaskMemFree here, and it exists only because defaultCacheDir reads an
// environment variable with the W entry point, to keep a profile path outside
// the ANSI code page working.
static std::string toUtf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return std::string();
    std::string out((size_t)needed, '\0');
    int written = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(),
                                      &out[0], needed, nullptr, nullptr);
    if (written <= 0) return std::string();
    out.resize((size_t)written);
    return out;
}

// %LOCALAPPDATA%\z80cpmw\help. Used only until MainWindow calls setCacheRoot;
// the header records that no caller exists yet and what the call should be.
static std::string defaultCacheDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::string();   // unset, or absurd
    std::string root = toUtf8(std::wstring(buf, n));
    if (root.empty()) return std::string();
    while (!root.empty() && (root.back() == '\\' || root.back() == '/')) root.pop_back();
    if (root.empty()) return std::string();
    return root + "\\z80cpmw\\help";
}

// Create dir and, if an intermediate component is missing, its parents first.
// Only ERROR_PATH_NOT_FOUND recurses, which is Windows saying "a component
// above the leaf does not exist" - so the walk stops at the first level that
// exists rather than climbing to the drive letter every time. A drive root
// ("C:") therefore never reaches CreateDirectoryW unless the volume itself is
// missing, in which case the failure is the honest answer.
static bool ensureDirectory(const std::string& dir) {
    if (dir.empty()) return false;

    std::wstring wide = toWide(dir);
    if (wide.empty()) return false;
    if (CreateDirectoryW(wide.c_str(), nullptr)) return true;

    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) return true;
    if (err != ERROR_PATH_NOT_FOUND) return false;

    size_t sep = dir.find_last_of("\\/");
    if (sep == std::string::npos || sep == 0) return false;
    if (!ensureDirectory(dir.substr(0, sep))) return false;

    return CreateDirectoryW(wide.c_str(), nullptr) ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

// "YYYY-MM-DD HH:MM" local, from a file's last-write time, or empty. Minutes
// and no seconds: this is shown to a reader to say how stale their offline copy
// is, and a second-accurate answer to that question is noise.
static std::string fileTimeText(const std::string& path) {
    std::wstring wide = toWide(path);
    if (wide.empty()) return std::string();

    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &data)) return std::string();

    FILETIME local = {};
    SYSTEMTIME st = {};
    if (!FileTimeToLocalFileTime(&data.ftLastWriteTime, &local)) return std::string();
    if (!FileTimeToSystemTime(&local, &st)) return std::string();

    char buf[32];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u",
             (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
             (unsigned)st.wHour, (unsigned)st.wMinute);
    return std::string(buf);
}

// Join a directory and a leaf without doubling a separator the caller left on.
static std::string joinPath(const std::string& dir, const std::string& leaf) {
    if (dir.empty()) return std::string();
    if (dir.back() == '\\' || dir.back() == '/') return dir + leaf;
    return dir + "\\" + leaf;
}

void setCacheRoot(const std::string& dir) {
    g_cacheRoot = dir;
}

std::string cacheDir() {
    if (!g_cacheRoot.empty()) return g_cacheRoot;
    return defaultCacheDir();
}

std::string cachePath(const std::string& assetName) {
    // First statement, deliberately: assetName is HelpTopic::filename, which
    // came out of help_index.json over the network. Nothing below this line can
    // be reached with a name that carries a separator, a drive letter, a
    // leading dot or a device name.
    if (!isSafeAssetName(assetName)) return std::string();
    return joinPath(cacheDir(), assetName);
}

std::string cacheTempPath(const std::string& assetName) {
    if (!isSafeAssetName(assetName)) return std::string();
    char suffix[32];
    snprintf(suffix, sizeof(suffix), ".%lu.tmp", (unsigned long)GetCurrentProcessId());
    return joinPath(cacheDir(), assetName + suffix);
}

bool readCached(const std::string& assetName, std::string& content) {
    content.clear();

    std::string path = cachePath(assetName);
    if (path.empty()) return false;

    std::wstring wide = toWide(path);
    if (wide.empty()) return false;

    // Shared every way, so a second instance of the app reading or deleting
    // this file does not make this read fail.
    //
    // It does NOT let a concurrent writer's rename through, which is what this
    // comment first claimed. Measured on this machine, with a scratch pair of
    // files: MoveFileExW(MOVEFILE_REPLACE_EXISTING) onto a file another handle
    // has open fails with ERROR_ACCESS_DENIED (5) whether or not that handle
    // allowed FILE_SHARE_DELETE - both trials failed identically. Losing that
    // race costs one refresh of one cached topic and the reader still sees the
    // download, so it does not earn a lock; the read is one ReadFile and the
    // handle is closed on the next line.
    HANDLE h = CreateFileW(wide.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 ||
        size.QuadPart > (LONGLONG)kMaxCacheBytes) {
        CloseHandle(h);
        return false;   // empty or implausible: see readCached in HelpAssets.h
    }

    std::string buffer((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(h, &buffer[0], (DWORD)size.QuadPart, &read, nullptr);
    CloseHandle(h);

    // A short read is a failure, not a topic. The whole point of this cache is
    // that a partial file is never shown as a whole one.
    if (!ok || read != (DWORD)size.QuadPart) return false;

    content.swap(buffer);
    return true;
}

bool writeCached(const std::string& assetName, const std::string& content) {
    if (content.empty() || content.size() > kMaxCacheBytes) return false;

    std::string path = cachePath(assetName);
    std::string temp = cacheTempPath(assetName);
    if (path.empty() || temp.empty()) return false;   // isSafeAssetName said no

    if (!ensureDirectory(cacheDir())) return false;

    std::wstring widePath = toWide(path);
    std::wstring wideTemp = toWide(temp);
    if (widePath.empty() || wideTemp.empty()) return false;

    // CREATE_ALWAYS, so a scratch file left behind by a killed run is truncated
    // rather than appended to. No sharing: nothing should be reading a file
    // that does not have its real name yet.
    HANDLE h = CreateFileW(wideTemp.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(h, content.data(), (DWORD)content.size(), &written, nullptr) &&
              written == (DWORD)content.size();

    // Flush before the rename, not for speed but for order: the rename is what
    // publishes the name, so the bytes have to be on the disk before the name
    // is. This does NOT make the pair transactional - a power loss between the
    // two leaves a scratch file, which the next write truncates - it only stops
    // the name appearing over content that never reached the platter.
    if (ok) FlushFileBuffers(h);
    CloseHandle(h);

    if (!ok) {
        DeleteFileW(wideTemp.c_str());
        return false;
    }

    // The whole reason for the scratch file. MOVEFILE_REPLACE_EXISTING because
    // the target is the previous copy of the same topic and refreshing it is
    // the normal case.
    if (!MoveFileExW(wideTemp.c_str(), widePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(wideTemp.c_str());   // leave no litter behind a failure
        return false;
    }

    return true;
}

ResolvedTopic resolveTopic(const std::string& assetName,
                           const std::string& downloaded,
                           const std::string& bundled) {
    ResolvedTopic out;

    // 1. The download. Cached on the way past - outside tests\test_help.cpp
    //    this is the only call to writeCached anywhere, so "what the reader was
    //    shown" and "what is on disk" cannot drift apart, and there is one
    //    place to look when they seem to. Its return value is ignored on
    //    purpose:
    //    a full disk or a refused name is a reason not to have an offline copy
    //    next time, not a reason to withhold the text now.
    if (!downloaded.empty()) {
        writeCached(assetName, downloaded);
        out.source = TopicSource::Downloaded;
        out.content = downloaded;
        return out;
    }

    // 2. The cache.
    std::string cached;
    if (readCached(assetName, cached)) {
        out.source = TopicSource::Cached;
        out.content = cached;
        out.savedWhen = fileTimeText(cachePath(assetName));
        return out;
    }

    // 3. The copy in the binary, which today exists for no topic that reaches
    //    here - see resolveTopic in HelpAssets.h.
    if (!bundled.empty()) {
        out.source = TopicSource::Bundled;
        out.content = bundled;
        return out;
    }

    return out;   // TopicSource::None, content empty
}

const char* sourceLabel(TopicSource source) {
    switch (source) {
    case TopicSource::Downloaded: return "downloaded";
    case TopicSource::Cached:     return "offline copy";
    case TopicSource::Bundled:    return "bundled with the app";
    case TopicSource::None:       break;
    }
    return "unavailable";
}

}  // namespace help_assets
