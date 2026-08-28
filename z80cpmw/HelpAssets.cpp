/*
 * HelpAssets.cpp - The state-free half of the help system.
 *
 * See HelpAssets.h for why these four live apart from HelpWindow.
 */

#include "HelpAssets.h"

#include <windows.h>

#include <algorithm>
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

}  // namespace help_assets
