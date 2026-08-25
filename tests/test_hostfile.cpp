/*
 * test_hostfile.cpp - the host-file backend's side of the emu_io.h contract
 *
 * Built and run by tests\run_tests.bat. Like the terminal suite, it needs
 * neither wxWidgets nor the vcpkg tree: emu_io_windows.cpp includes only pch.h
 * (Win32 + the standard library) and emu_io.h from ..\romwbw_emu\src, so this
 * compiles against the SDK and the sibling core checkout alone.
 *
 * What it is here to hold down, from docs/DOWNSTREAM_2026-08-25.md in
 * romwbw_emu (core v1.36):
 *
 *   - emu_host_path_caps() exists and sets EMU_HOST_CAP_SAFE_PATHS. The core
 *     declares it and deliberately does not define it, so that a port asserts
 *     the guarantee only in the code that makes it true.
 *   - emu_host_file_get_write_name() reports the EFFECTIVE destination, not an
 *     echo of what the guest typed. W8 prints this string, so the test that
 *     matters is not "what does it say" but "does a file exist at the place it
 *     said" - which is what most of these checks assert.
 *   - It reports nothing outside an open write, so W8 cannot be told the
 *     previous transfer's destination.
 *   - Answering the query creates nothing. The redirection is followed through
 *     the parent directory precisely so a query cannot leave a directory behind
 *     named after the file that is about to be written.
 */

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "emu_io.h"

// Declared where its callers declare it (MainWindow.cpp, SettingsDialogWx.cpp)
// rather than in a header - it is this port's own, not part of emu_io.h.
extern "C" const char* emu_io_get_data_folder_display();

static int g_checks = 0;
static int g_failures = 0;

static void check(bool cond, const char* what) {
    g_checks++;
    if (!cond) {
        g_failures++;
        printf("  FAIL: %s\n", what);
    }
}

static void checkEq(const std::string& got, const std::string& want,
                    const char* what) {
    g_checks++;
    if (got != want) {
        g_failures++;
        printf("  FAIL: %s\n        got  \"%s\"\n        want \"%s\"\n",
               what, got.c_str(), want.c_str());
    }
}

static bool iequals(const std::string& a, const std::string& b) {
    return a.size() == b.size() && _stricmp(a.c_str(), b.c_str()) == 0;
}

static bool endsWithNoCase(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() &&
           _stricmp(s.c_str() + (s.size() - tail.size()), tail.c_str()) == 0;
}

static bool fileExists(const std::string& p) {
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool dirExists(const std::string& p) {
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string readWholeFile(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return std::string();
    std::string out;
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return out;
}

// Report what emu_host_file_get_write_name() says right now, with nullptr and
// "" collapsed to "" - the contract lets a backend return either outside an
// open write, and callers must tolerate both.
static std::string writeName() {
    const char* n = emu_host_file_get_write_name();
    return n ? std::string(n) : std::string();
}

static void writeBytes(const char* s) {
    for (const char* p = s; *p; p++) {
        check(emu_host_file_write_byte((uint8_t)*p), "write_byte accepted");
    }
}

// A scratch directory of our own, so the absolute-path checks do not touch
// anything the user cares about.
static std::string makeScratchDir() {
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA((DWORD)sizeof(tmp), tmp);
    if (n == 0 || n >= sizeof(tmp)) return std::string();
    std::string dir = std::string(tmp) + "z80cpmw_hostfile_test";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dirExists(dir) ? dir : std::string();
}

//=============================================================================

static void test_caps() {
    printf("--- emu_host_path_caps ---\n");

    // The bit this port asserts, and the reason it may: open-write creates or
    // replaces the one named file and does nothing else with the path. Honouring
    // an absolute path is not what the bit forbids - see the enum comment in
    // emu_io.h - so a change here should be a change in behaviour, not a tidy-up.
    check((emu_host_path_caps() & EMU_HOST_CAP_SAFE_PATHS) != 0,
          "EMU_HOST_CAP_SAFE_PATHS is set");

    // HBF_HOST_CAPS hands the low byte to the guest; nothing above bit 0 is
    // defined yet, so anything else set would reach W8 as an unknown promise.
    checkEq(std::to_string((int)emu_host_path_caps()),
            std::to_string((int)EMU_HOST_CAP_SAFE_PATHS),
            "no undefined capability bits are claimed");
}

static void test_name_window() {
    printf("--- get_write_name is valid only during a write ---\n");

    check(emu_host_file_get_state() == HOST_FILE_IDLE, "idle at start");
    checkEq(writeName(), "", "nothing to report before any open");

    check(emu_host_file_open_write("windowcheck.txt"), "open_write succeeded");
    check(emu_host_file_get_state() == HOST_FILE_WRITING, "state is WRITING");
    check(!writeName().empty(), "a destination is reported while writing");

    std::string reported = writeName();
    check(emu_host_file_close_write(), "close_write succeeded");

    // The failure this guards: W8 asking after the fact, or on the next
    // transfer, and being told where the LAST file went.
    checkEq(writeName(), "", "nothing to report after close");
    check(emu_host_file_get_state() == HOST_FILE_IDLE, "idle after close");

    DeleteFileA(reported.c_str());
}

static void test_bare_name_reports_the_data_folder() {
    printf("--- a bare name reports the data folder, not the name ---\n");

    const char* leaf = "hostfile_bare_test.txt";
    check(emu_host_file_open_write(leaf), "open_write succeeded");

    std::string reported = writeName();

    // The whole point of HBF_HOST_GETNAME: a bare name is not where the file
    // lands. Echoing "hostfile_bare_test.txt" would name nothing the user can
    // open, which is the bug this call was added to remove.
    check(reported != leaf, "the report is not an echo of the requested name");
    check(reported.find('\\') != std::string::npos,
          "the report is a path, not a name");
    check(endsWithNoCase(reported, leaf), "the report ends in the requested leaf");

    // It must agree with emu_io_get_data_folder_display(), which is the same
    // location as shown in the UI - two answers to "where do transfers go"
    // that disagreed would be worse than one.
    std::string folder = emu_io_get_data_folder_display();
    check(!folder.empty(), "the data folder resolves");
    check(iequals(reported, folder + "\\" + leaf),
          "the report is the displayed data folder joined to the leaf");

    writeBytes("bare");
    check(emu_host_file_close_write(), "close_write succeeded");

    // The check that actually matters: the string W8 printed names a real file
    // holding the bytes the guest sent.
    check(fileExists(reported), "a file exists at the reported path");
    checkEq(readWholeFile(reported), "bare", "it holds the exported bytes");

    DeleteFileA(reported.c_str());
}

static void test_absolute_path_is_honoured_and_reported() {
    printf("--- an absolute path is honoured, and reported as resolved ---\n");

    std::string dir = makeScratchDir();
    check(!dir.empty(), "scratch directory created");
    if (dir.empty()) return;

    std::string requested = dir + "\\hostfile_abs_test.txt";
    DeleteFileA(requested.c_str());

    check(emu_host_file_open_write(requested.c_str()), "open_write succeeded");
    std::string reported = writeName();

    // This port honours absolute paths deliberately (R8/W8 are meant to reach
    // C:\Users\me\Desktop). The report may differ from the request in case or
    // canonical form - that is the resolution doing its job - but it has to
    // name the same file, and the assertion below is what proves it.
    check(endsWithNoCase(reported, "\\hostfile_abs_test.txt"),
          "the report ends in the requested leaf");

    writeBytes("absolute");
    check(emu_host_file_close_write(), "close_write succeeded");

    check(fileExists(requested), "the file was written where the guest asked");
    check(fileExists(reported), "a file exists at the reported path");
    checkEq(readWholeFile(reported), "absolute", "it holds the exported bytes");

    DeleteFileA(requested.c_str());
}

static void test_the_query_creates_nothing() {
    printf("--- answering the query creates nothing ---\n");

    std::string dir = makeScratchDir();
    check(!dir.empty(), "scratch directory created");
    if (dir.empty()) return;

    std::string requested = dir + "\\hostfile_nocreate.txt";
    DeleteFileA(requested.c_str());
    RemoveDirectoryA(requested.c_str());

    check(emu_host_file_open_write(requested.c_str()), "open_write succeeded");
    (void)writeName();

    // Following the MSIX redirection means opening a handle, and a handle needs
    // something that exists. Resolving the FILE path would have to create the
    // file - and CreateDirectoryA on it would create a DIRECTORY with the file's
    // name, after which the export's own fopen(..., "wb") fails. So the parent
    // is what gets resolved, and neither of these may appear yet.
    check(!dirExists(requested), "no directory was created for the file name");
    check(!fileExists(requested), "the file itself does not exist before close");

    check(emu_host_file_close_write(), "close_write succeeded");
    check(fileExists(requested), "the file appears at close");

    DeleteFileA(requested.c_str());
}

static void test_nonexistent_parent_still_reports_something() {
    printf("--- an unresolvable parent still yields a usable answer ---\n");

    std::string dir = makeScratchDir();
    if (dir.empty()) { check(false, "scratch directory created"); return; }

    // A directory that is not there: the handle open fails, so there is no
    // redirection to follow. The requested path is then the best answer
    // available, and it must not come back empty - an empty answer makes the
    // core report failure and W8 fall back to printing the typed path, which
    // is the same string, so this is about not losing information.
    std::string requested = dir + "\\no_such_subdir\\out.txt";
    check(emu_host_file_open_write(requested.c_str()), "open_write succeeded");
    check(endsWithNoCase(writeName(), "\\out.txt"),
          "the report still ends in the requested leaf");
    check(!dirExists(dir + "\\no_such_subdir"),
          "the missing parent was not conjured up");

    // The export itself fails, and the guest is told so - fopen cannot create a
    // file in a directory that does not exist.
    check(!emu_host_file_close_write(), "close_write reports the failure");
    checkEq(writeName(), "", "nothing to report after a failed transfer");
}

static void test_null_and_empty_names() {
    printf("--- a null or empty name falls back once, not twice ---\n");

    // close_write has always defaulted to export.txt. open_write now applies the
    // same fallback, so the name reported and the name written cannot disagree.
    check(emu_host_file_open_write(nullptr), "open_write(nullptr) succeeded");
    std::string reportedNull = writeName();
    check(endsWithNoCase(reportedNull, "\\export.txt"),
          "nullptr reports the export.txt fallback");
    writeBytes("n");
    check(emu_host_file_close_write(), "close_write succeeded");
    check(fileExists(reportedNull), "the fallback file exists where reported");
    DeleteFileA(reportedNull.c_str());

    check(emu_host_file_open_write(""), "open_write(\"\") succeeded");
    std::string reportedEmpty = writeName();
    check(endsWithNoCase(reportedEmpty, "\\export.txt"),
          "an empty name reports the same fallback");
    writeBytes("e");
    check(emu_host_file_close_write(), "close_write succeeded");
    check(fileExists(reportedEmpty), "the fallback file exists where reported");
    checkEq(reportedNull, reportedEmpty, "both fallbacks name the same file");
    DeleteFileA(reportedEmpty.c_str());
}

static void test_emu_rename() {
    printf("--- emu_rename replaces an existing target ---\n");

    std::string dir = makeScratchDir();
    if (dir.empty()) { check(false, "scratch directory created"); return; }

    std::string from = dir + "\\rename_from.tmp";
    std::string to = dir + "\\rename_to.tmp";

    FILE* f = fopen(from.c_str(), "wb");
    check(f != nullptr, "source created");
    if (f) { fputs("new", f); fclose(f); }
    f = fopen(to.c_str(), "wb");
    check(f != nullptr, "target created");
    if (f) { fputs("old", f); fclose(f); }

    // The reason the shim exists: ISO C leaves rename() undefined when the
    // target exists and the MSVC CRT refuses it outright. emu_file_save writes
    // a temp file and renames it over the image precisely so a failed write
    // cannot destroy the previous one, so on Windows plain rename() would make
    // the safe path the broken one.
    checkEq(std::to_string(emu_rename(from.c_str(), to.c_str())), "0",
            "emu_rename over an existing target succeeds");
    check(!fileExists(from), "the source is gone");
    checkEq(readWholeFile(to), "new", "the target holds the new contents");

    check(emu_rename((dir + "\\no_such_source.tmp").c_str(), to.c_str()) != 0,
          "emu_rename reports failure for a missing source");

    DeleteFileA(to.c_str());
}

int main() {
    printf("=== Host file transfer suite ===\n\n");

    test_caps();
    test_name_window();
    test_bare_name_reports_the_data_folder();
    test_absolute_path_is_honoured_and_reported();
    test_the_query_creates_nothing();
    test_nonexistent_parent_still_reports_something();
    test_null_and_empty_names();
    test_emu_rename();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
