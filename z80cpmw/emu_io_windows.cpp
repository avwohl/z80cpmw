/*
 * emu_io_windows.cpp - Windows I/O Implementation
 *
 * This implementation provides the emu_io interface for Windows.
 * Console I/O is routed through callbacks to the GUI.
 */

#include "pch.h"
#include "emu_io.h"
#include <queue>
#include <mutex>
#include <random>
#include <cstdarg>
#include <vector>
#include <string>
#include <shlobj.h>
#include <commdlg.h>

//=============================================================================
// Disk Image Format Definitions
//=============================================================================

enum emu_disk_format {
    EMU_DISK_HD1K_SINGLE,  // 8MB single-unit disk
    EMU_DISK_HD1K_COMBO,   // 128MB combo disk (16 slices)
};

// HD1K disk sizes (512-byte sectors)
static const size_t EMU_HD1K_SINGLE_SIZE = 8 * 1024 * 1024;      // 8MB
static const size_t EMU_HD1K_COMBO_SIZE = 128 * 1024 * 1024;     // 128MB

//=============================================================================
// Callback Interface for GUI Integration
//=============================================================================

// Callback function types
using OutputCharCallback = void(*)(uint8_t ch);
using VideoCallback = void(*)(int cmd, int p1, int p2, uint8_t p3);
using BeepCallback = void(*)(int durationMs);

static OutputCharCallback g_outputCallback = nullptr;
static VideoCallback g_videoCallback = nullptr;
static BeepCallback g_beepCallback = nullptr;

// Set callbacks (called from EmulatorEngine)
extern "C" {
    void emu_io_set_output_callback(OutputCharCallback cb) {
        g_outputCallback = cb;
    }
    void emu_io_set_video_callback(VideoCallback cb) {
        g_videoCallback = cb;
    }
    void emu_io_set_beep_callback(BeepCallback cb) {
        g_beepCallback = cb;
    }
}

//=============================================================================
// Platform Utilities Implementation
//=============================================================================

void emu_sleep_ms(int ms) {
    Sleep(ms);
}

int emu_strcasecmp(const char* s1, const char* s2) {
    return _stricmp(s1, s2);
}

int emu_strncasecmp(const char* s1, const char* s2, size_t n) {
    return _strnicmp(s1, s2, n);
}

//=============================================================================
// Input Queue
//=============================================================================

static std::queue<int> g_inputQueue;
static std::mutex g_inputMutex;
static std::mt19937 g_rng(std::random_device{}());
static bool g_debugEnabled = false;

void emu_io_init() {
    // Nothing special needed for Windows GUI
}

void emu_io_cleanup() {
    // Nothing special needed
}

bool emu_console_has_input() {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return !g_inputQueue.empty();
}

int emu_console_read_char() {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    if (g_inputQueue.empty()) {
        return -1;
    }
    int ch = g_inputQueue.front();
    g_inputQueue.pop();
    // Convert LF to CR for CP/M
    if (ch == '\n') ch = '\r';
    return ch;
}

void emu_console_queue_char(int ch) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_inputQueue.push(ch);
}

void emu_console_clear_queue() {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    while (!g_inputQueue.empty()) {
        g_inputQueue.pop();
    }
}

void emu_console_write_char(uint8_t ch) {
    if (g_outputCallback) {
        g_outputCallback(ch & 0x7F);
    }
}

bool emu_console_check_escape(char escape_char) {
    (void)escape_char;
    return false; // Not used in GUI mode
}

bool emu_console_check_ctrl_c_exit(int ch, int count) {
    (void)ch;
    (void)count;
    return false; // Not used in GUI mode
}

//=============================================================================
// Auxiliary Device I/O (Stub Implementation)
//=============================================================================

static FILE* g_printerFile = nullptr;
static FILE* g_auxInFile = nullptr;
static FILE* g_auxOutFile = nullptr;

void emu_printer_set_file(const char* path) {
    if (g_printerFile) {
        fclose(g_printerFile);
        g_printerFile = nullptr;
    }
    if (path && *path) {
        g_printerFile = fopen(path, "w");
    }
}

void emu_printer_out(uint8_t ch) {
    if (g_printerFile) {
        fputc(ch & 0x7F, g_printerFile);
        fflush(g_printerFile);
    }
}

bool emu_printer_ready() {
    return true;
}

void emu_aux_set_input_file(const char* path) {
    if (g_auxInFile) {
        fclose(g_auxInFile);
        g_auxInFile = nullptr;
    }
    if (path && *path) {
        g_auxInFile = fopen(path, "r");
    }
}

void emu_aux_set_output_file(const char* path) {
    if (g_auxOutFile) {
        fclose(g_auxOutFile);
        g_auxOutFile = nullptr;
    }
    if (path && *path) {
        g_auxOutFile = fopen(path, "w");
    }
}

int emu_aux_in() {
    if (g_auxInFile) {
        int ch = fgetc(g_auxInFile);
        if (ch == EOF) ch = 0x1A; // ^Z on EOF
        return ch & 0x7F;
    }
    return 0x1A;
}

void emu_aux_out(uint8_t ch) {
    if (g_auxOutFile) {
        fputc(ch & 0x7F, g_auxOutFile);
        fflush(g_auxOutFile);
    }
}

//=============================================================================
// Debug/Log Output
//=============================================================================

void emu_set_debug(bool enable) {
    g_debugEnabled = enable;
}

// Diagnostics also go to a log file in the user data directory so users can
// send them from a machine with no debugger attached (OutputDebugString is
// invisible to them). Path is set once at startup by wWinMain. The handle is
// kept open (debug mode can log from inside the 10ms emulator batch, so no
// fopen per message); at the size cap the file rotates to .old so recent
// history survives.
static char g_logPath[MAX_PATH] = {};
static FILE* g_logFile = nullptr;
static long g_logBytes = 0;
static std::mutex g_logMutex;
static const long LOG_MAX_BYTES = 1024 * 1024;

extern "C" void emu_io_set_log_path(const char* path) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    if (!path) {
        g_logPath[0] = '\0';
        return;
    }
    strncpy(g_logPath, path, sizeof(g_logPath) - 1);
    g_logPath[sizeof(g_logPath) - 1] = '\0';
}

static void logToFile(const char* text) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_logPath[0]) return;

    if (!g_logFile) {
        g_logFile = fopen(g_logPath, "a");
        if (!g_logFile) {
            g_logPath[0] = '\0';  // don't retry on every message
            return;
        }
        fseek(g_logFile, 0, SEEK_END);
        g_logBytes = ftell(g_logFile);
        if (g_logBytes < 0) g_logBytes = 0;
    }

    if (g_logBytes > LOG_MAX_BYTES) {
        fclose(g_logFile);
        std::string oldPath = std::string(g_logPath) + ".old";
        remove(oldPath.c_str());
        rename(g_logPath, oldPath.c_str());
        g_logFile = fopen(g_logPath, "w");
        if (!g_logFile) {
            g_logPath[0] = '\0';
            return;
        }
        g_logBytes = 0;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    size_t len = strlen(text);
    int written = fprintf(g_logFile, "%04u-%02u-%02u %02u:%02u:%02u %s%s",
                          st.wYear, st.wMonth, st.wDay,
                          st.wHour, st.wMinute, st.wSecond,
                          text, (len > 0 && text[len - 1] == '\n') ? "" : "\n");
    if (written > 0) g_logBytes += written;
    fflush(g_logFile);
}

void emu_log(const char* fmt, ...) {
    if (!g_debugEnabled) return;
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    OutputDebugStringA(buffer);
    logToFile(buffer);
}

void emu_error(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    OutputDebugStringA(buffer);
    logToFile(buffer);
}

[[noreturn]] void emu_fatal(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    logToFile(buffer);
    MessageBoxA(nullptr, buffer, "Fatal Error", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

void emu_status(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    OutputDebugStringA(buffer);
    logToFile(buffer);
}

//=============================================================================
// File I/O
//=============================================================================

bool emu_file_load(const std::string& path, std::vector<uint8_t>& data) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        data.clear();
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    data.resize(size);
    size_t bytesRead = fread(data.data(), 1, size, f);
    fclose(f);

    if (bytesRead != size) {
        data.clear();
        return false;
    }
    return true;
}

size_t emu_file_load_to_mem(const std::string& path, uint8_t* mem,
                            size_t mem_size, size_t offset) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    size_t toRead = fileSize;
    if (offset + toRead > mem_size) {
        toRead = mem_size - offset;
    }

    size_t bytesRead = fread(mem + offset, 1, toRead, f);
    fclose(f);
    return bytesRead;
}

bool emu_file_save(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    size_t written = fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return written == data.size();
}

bool emu_file_exists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

size_t emu_file_size(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    LARGE_INTEGER size;
    size.HighPart = attrs.nFileSizeHigh;
    size.LowPart = attrs.nFileSizeLow;
    return static_cast<size_t>(size.QuadPart);
}

//=============================================================================
// Disk Image I/O
//=============================================================================

#include <set>

struct disk_file {
    FILE* fp;
    size_t size;
};

// Track all open disks for emu_disk_flush_all
static std::set<disk_file*> g_openDisks;

emu_disk_handle emu_disk_open(const std::string& path, const char* mode) {
    const char* fmode;
    if (strcmp(mode, "r") == 0) {
        fmode = "rb";
    } else if (strcmp(mode, "rw") == 0) {
        fmode = "r+b";
    } else if (strcmp(mode, "rw+") == 0) {
        fmode = "r+b";
        FILE* f = fopen(path.c_str(), fmode);
        if (!f) {
            f = fopen(path.c_str(), "w+b");
        }
        if (!f) return nullptr;

        disk_file* disk = new disk_file;
        disk->fp = f;
        fseek(f, 0, SEEK_END);
        disk->size = ftell(f);
        g_openDisks.insert(disk);
        return disk;
    } else {
        return nullptr;
    }

    FILE* f = fopen(path.c_str(), fmode);
    if (!f) return nullptr;

    disk_file* disk = new disk_file;
    disk->fp = f;
    fseek(f, 0, SEEK_END);
    disk->size = ftell(f);
    g_openDisks.insert(disk);
    return disk;
}

void emu_disk_close(emu_disk_handle handle) {
    if (!handle) return;
    disk_file* disk = static_cast<disk_file*>(handle);
    g_openDisks.erase(disk);
    if (disk->fp) fclose(disk->fp);
    delete disk;
}

size_t emu_disk_read(emu_disk_handle handle, size_t offset,
                     uint8_t* buffer, size_t count) {
    if (!handle) return 0;
    disk_file* disk = static_cast<disk_file*>(handle);
    if (!disk->fp) return 0;

    fseek(disk->fp, (long)offset, SEEK_SET);
    return fread(buffer, 1, count, disk->fp);
}

size_t emu_disk_write(emu_disk_handle handle, size_t offset,
                      const uint8_t* buffer, size_t count) {
    if (!handle) return 0;
    disk_file* disk = static_cast<disk_file*>(handle);
    if (!disk->fp) return 0;

    fseek(disk->fp, (long)offset, SEEK_SET);
    size_t written = fwrite(buffer, 1, count, disk->fp);

    size_t newEnd = offset + written;
    if (newEnd > disk->size) {
        disk->size = newEnd;
    }

    return written;
}

void emu_disk_flush(emu_disk_handle handle) {
    if (!handle) return;
    disk_file* disk = static_cast<disk_file*>(handle);
    if (disk->fp) fflush(disk->fp);
}

void emu_disk_flush_all() {
    for (disk_file* disk : g_openDisks) {
        if (disk && disk->fp) {
            fflush(disk->fp);
        }
    }
}

size_t emu_disk_size(emu_disk_handle handle) {
    if (!handle) return 0;
    disk_file* disk = static_cast<disk_file*>(handle);
    return disk->size;
}

//=============================================================================
// Disk Image Creation
//=============================================================================

bool emu_disk_create(const std::string& path, emu_disk_format format) {
    size_t size;
    switch (format) {
        case EMU_DISK_HD1K_SINGLE:
            size = EMU_HD1K_SINGLE_SIZE;
            break;
        case EMU_DISK_HD1K_COMBO:
            size = EMU_HD1K_COMBO_SIZE;
            break;
        default:
            return false;
    }

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    // Write zeros to create the disk image
    std::vector<uint8_t> zeros(65536, 0);  // 64KB buffer
    size_t remaining = size;
    while (remaining > 0) {
        size_t toWrite = (remaining < zeros.size()) ? remaining : zeros.size();
        size_t written = fwrite(zeros.data(), 1, toWrite, f);
        if (written != toWrite) {
            fclose(f);
            return false;
        }
        remaining -= written;
    }

    fclose(f);
    return true;
}

std::vector<uint8_t> emu_disk_create_memory(emu_disk_format format) {
    size_t size;
    switch (format) {
        case EMU_DISK_HD1K_SINGLE:
            size = EMU_HD1K_SINGLE_SIZE;
            break;
        case EMU_DISK_HD1K_COMBO:
            size = EMU_HD1K_COMBO_SIZE;
            break;
        default:
            return {};
    }

    return std::vector<uint8_t>(size, 0);
}

//=============================================================================
// Time
//=============================================================================

void emu_get_time(emu_time* t) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    t->year = st.wYear;
    t->month = st.wMonth;
    t->day = st.wDay;
    t->hour = st.wHour;
    t->minute = st.wMinute;
    t->second = st.wSecond;
    t->weekday = st.wDayOfWeek;
}

//=============================================================================
// Random Numbers
//=============================================================================

unsigned int emu_random(unsigned int min, unsigned int max) {
    if (min >= max) return min;
    std::uniform_int_distribution<unsigned int> dist(min, max);
    return dist(g_rng);
}

//=============================================================================
// Video/Display (delegated to callbacks)
//=============================================================================

// Video command IDs for callback
enum VideoCmd {
    VCMD_CLEAR = 0,
    VCMD_SET_CURSOR = 1,
    VCMD_WRITE_CHAR = 2,
    VCMD_SCROLL_UP = 3,
    VCMD_SET_ATTR = 4,
};

static int g_cursorRow = 0;
static int g_cursorCol = 0;
static uint8_t g_textAttr = 0x07;

void emu_video_get_caps(emu_video_caps* caps) {
    caps->has_text_display = true;
    caps->has_pixel_display = false;
    caps->has_dsky = false;
    caps->text_rows = 25;
    caps->text_cols = 80;
    caps->pixel_width = 0;
    caps->pixel_height = 0;
}

void emu_video_clear() {
    g_cursorRow = 0;
    g_cursorCol = 0;
    if (g_videoCallback) {
        g_videoCallback(VCMD_CLEAR, 0, 0, 0);
    }
}

void emu_video_set_cursor(int row, int col) {
    g_cursorRow = row;
    g_cursorCol = col;
    if (g_videoCallback) {
        g_videoCallback(VCMD_SET_CURSOR, row, col, 0);
    }
}

void emu_video_get_cursor(int* row, int* col) {
    *row = g_cursorRow;
    *col = g_cursorCol;
}

void emu_video_write_char(uint8_t ch) {
    if (g_videoCallback) {
        g_videoCallback(VCMD_WRITE_CHAR, g_cursorRow, g_cursorCol, ch);
    }
    g_cursorCol++;
}

void emu_video_write_char_at(int row, int col, uint8_t ch) {
    if (g_videoCallback) {
        g_videoCallback(VCMD_WRITE_CHAR, row, col, ch);
    }
}

void emu_video_scroll_up(int lines) {
    if (g_videoCallback) {
        g_videoCallback(VCMD_SCROLL_UP, lines, 0, 0);
    }
}

void emu_video_set_attr(uint8_t attr) {
    g_textAttr = attr;
    if (g_videoCallback) {
        g_videoCallback(VCMD_SET_ATTR, 0, 0, attr);
    }
}

uint8_t emu_video_get_attr() {
    return g_textAttr;
}

// DSKY operations - not implemented in Windows version
void emu_dsky_show_hex(uint8_t position, uint8_t value) {
    (void)position;
    (void)value;
}

void emu_dsky_show_segments(uint8_t position, uint8_t segments) {
    (void)position;
    (void)segments;
}

void emu_dsky_set_leds(uint8_t leds) {
    (void)leds;
}

void emu_dsky_beep(int duration_ms) {
    if (g_beepCallback) {
        g_beepCallback(duration_ms);
    } else {
        Beep(800, duration_ms);
    }
}

int emu_dsky_get_key() {
    return -1;
}

//=============================================================================
// Host File Transfer - for R8/W8 utilities
//=============================================================================

static emu_host_file_state g_hostFileState = HOST_FILE_IDLE;
static std::vector<uint8_t> g_hostReadBuffer;
static size_t g_hostReadPos = 0;
static std::vector<uint8_t> g_hostWriteBuffer;
static std::string g_hostWriteFilename;
static HWND g_mainWindowHwnd = nullptr;

// Set main window handle for file dialogs
extern "C" void emu_io_set_main_window(HWND hwnd) {
    g_mainWindowHwnd = hwnd;
}

// Get the data folder path (same as EmulatorEngine::getUserDataDirectory() + "\data")
static std::string getDataFolder() {
    wchar_t* localAppData = nullptr;
    std::string dataDir;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        int len = WideCharToMultiByte(CP_UTF8, 0, localAppData, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string path(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, localAppData, -1, &path[0], len, nullptr, nullptr);
            dataDir = path + "\\z80cpmw\\data";
        }
        CoTaskMemFree(localAppData);
    }

    // Ensure directory exists
    if (!dataDir.empty()) {
        CreateDirectoryA((dataDir.substr(0, dataDir.length() - 5)).c_str(), nullptr);  // Create z80cpmw
        CreateDirectoryA(dataDir.c_str(), nullptr);  // Create data
    }

    return dataDir;
}

// True if the name is already a full path (drive-letter, UNC, or rooted), in
// which case it should be used verbatim instead of being placed in the data
// folder. Lets R8/W8 accept paths like C:\Users\me\Desktop\getkey2.com.
static bool isAbsolutePath(const std::string& p) {
    if (p.size() >= 2 && p[1] == ':') return true;              // C:\ or C:/
    if (!p.empty() && (p[0] == '\\' || p[0] == '/')) return true; // \server\... or rooted
    return false;
}

// Resolve a host filename to a full path: absolute paths are used as-is,
// bare names are placed in the data folder. Returns empty on failure.
static std::string resolveHostPath(const std::string& filename) {
    if (isAbsolutePath(filename)) {
        return filename;
    }
    std::string dataFolder = getDataFolder();
    if (dataFolder.empty()) {
        return std::string();
    }
    return dataFolder + "\\" + filename;
}

// Resolve a (possibly virtualized) path to its real on-disk location. For a
// packaged (MSIX/Store) build, writes to %LOCALAPPDATA% are redirected by the
// OS to ...\Packages\<family>\LocalCache\Local\..., so the path the app uses
// is not the path Explorer shows. Opening a handle and asking for the final
// path name follows that redirection without us having to guess the layout.
static std::string resolveRealPath(const std::string& path) {
    if (path.empty()) return path;

    // Must exist before we can open a handle to it.
    CreateDirectoryA(path.c_str(), nullptr);

    HANDLE h = CreateFileA(path.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return path;

    char buf[1024] = {};
    DWORD n = GetFinalPathNameByHandleA(h, buf, (DWORD)sizeof(buf),
                                        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);

    if (n == 0 || n >= sizeof(buf)) return path;

    std::string real(buf, n);
    // Drop the \\?\ prefix GetFinalPathNameByHandle prepends.
    if (real.rfind("\\\\?\\", 0) == 0) {
        real = real.substr(4);
    }
    return real;
}

// Real on-disk location of the data folder (where disks and R8/W8 transfers
// live), for display in the UI. Cached so callers can keep the pointer briefly.
extern "C" const char* emu_io_get_data_folder_display() {
    static std::string cached;
    cached = resolveRealPath(getDataFolder());
    return cached.c_str();
}

emu_host_file_state emu_host_file_get_state() {
    return g_hostFileState;
}

bool emu_host_file_open_read(const char* filename) {
    // Close any existing read operation
    g_hostReadBuffer.clear();
    g_hostReadPos = 0;

    if (!filename || !*filename) {
        g_hostFileState = HOST_FILE_IDLE;
        return false;
    }

    // Resolve to a full path: absolute paths used as-is, bare names go in the
    // data folder.
    std::string fullPath = resolveHostPath(filename);
    if (fullPath.empty()) {
        g_hostFileState = HOST_FILE_IDLE;
        return false;
    }

    // Read the file
    FILE* f = fopen(fullPath.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);
        g_hostReadBuffer.resize(size);
        fread(g_hostReadBuffer.data(), 1, size, f);
        fclose(f);
        g_hostFileState = HOST_FILE_READING;
        return true;
    }

    g_hostFileState = HOST_FILE_IDLE;
    return false;
}

bool emu_host_file_open_write(const char* filename) {
    g_hostWriteBuffer.clear();
    g_hostWriteFilename = filename ? filename : "export.txt";
    g_hostFileState = HOST_FILE_WRITING;
    return true;
}

int emu_host_file_read_byte() {
    if (g_hostFileState != HOST_FILE_READING) {
        return -1;
    }

    if (g_hostReadPos >= g_hostReadBuffer.size()) {
        return -1;  // EOF
    }

    return g_hostReadBuffer[g_hostReadPos++];
}

bool emu_host_file_write_byte(uint8_t byte) {
    if (g_hostFileState != HOST_FILE_WRITING) {
        return false;
    }

    g_hostWriteBuffer.push_back(byte);
    return true;
}

void emu_host_file_close_read() {
    g_hostReadBuffer.clear();
    g_hostReadPos = 0;
    g_hostFileState = HOST_FILE_IDLE;
}

void emu_host_file_close_write() {
    if (g_hostFileState == HOST_FILE_WRITING && !g_hostWriteBuffer.empty()) {
        // Use provided filename or default to export.txt. Absolute paths are
        // written verbatim; bare names go in the data folder.
        std::string filename = g_hostWriteFilename.empty() ? "export.txt" : g_hostWriteFilename;
        std::string fullPath = resolveHostPath(filename);

        // Write the buffer
        FILE* f = fullPath.empty() ? nullptr : fopen(fullPath.c_str(), "wb");
        if (f) {
            fwrite(g_hostWriteBuffer.data(), 1, g_hostWriteBuffer.size(), f);
            fclose(f);
        }
    }

    g_hostWriteBuffer.clear();
    g_hostWriteFilename.clear();
    g_hostFileState = HOST_FILE_IDLE;
}

void emu_host_file_provide_data(const uint8_t* data, size_t size) {
    // For providing data after file picker callback
    g_hostReadBuffer.assign(data, data + size);
    g_hostReadPos = 0;
    if (size > 0) {
        g_hostFileState = HOST_FILE_READING;
    }
}

const uint8_t* emu_host_file_get_write_data() {
    if (g_hostFileState != HOST_FILE_WRITING) {
        return nullptr;
    }
    return g_hostWriteBuffer.data();
}

size_t emu_host_file_get_write_size() {
    if (g_hostFileState != HOST_FILE_WRITING) {
        return 0;
    }
    return g_hostWriteBuffer.size();
}

const char* emu_host_file_get_write_name() {
    if (g_hostFileState != HOST_FILE_WRITING) {
        return nullptr;
    }
    return g_hostWriteFilename.c_str();
}
