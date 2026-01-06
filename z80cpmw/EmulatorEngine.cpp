/*
 * EmulatorEngine.cpp - Z80/RomWBW Emulator Engine Implementation
 *
 * Uses the shared hbios_cpu class with HBIOSCPUDelegate pattern.
 */

#include "pch.h"
#include "EmulatorEngine.h"
#include "hbios_cpu.h"
#include "romwbw_mem.h"
#include "hbios_dispatch.h"
#include "emu_io.h"
#include "emu_init.h"
#include "Dazzler.h"

// External callback setters from emu_io_windows.cpp
extern "C" {
    void emu_io_set_output_callback(void(*cb)(uint8_t));
    void emu_io_set_video_callback(void(*cb)(int, int, int, uint8_t));
    void emu_io_set_beep_callback(void(*cb)(int));
}

// Global engine pointer for callbacks (single instance)
static EmulatorEngine* g_engine = nullptr;

// Output callback wrapper - routes emu_console_write_char to the engine callback
static void outputCallbackWrapper(uint8_t ch) {
    if (g_engine && g_engine->getOutputCallback()) {
        g_engine->getOutputCallback()(ch);
    }
}

EmulatorEngine::EmulatorEngine() {
    g_engine = this;
    initCPU();
    emu_io_init();
    emu_io_set_output_callback(outputCallbackWrapper);
}

EmulatorEngine::~EmulatorEngine() {
    stop();
    g_engine = nullptr;
    emu_io_cleanup();
}

void EmulatorEngine::initCPU() {
    m_memory = std::make_unique<banked_mem>();
    m_memory->enable_banking();

    m_hbios = std::make_unique<HBIOSDispatch>();
    m_hbios->setMemory(m_memory.get());
    m_hbios->setSkipRet(true);
    m_hbios->setBlockingAllowed(false);

    m_cpu = std::make_unique<hbios_cpu>(m_memory.get(), this);
    m_hbios->setCPU(m_cpu.get());

    m_hbios->setResetCallback([this](uint8_t resetType) {
        (void)resetType;
        m_memory->select_bank(0);
        emu_console_clear_queue();
        m_cpu->regs.PC.set_pair16(0);
        m_initializedRamBanks = 0;
    });

    // Register bank init callback for SYSSETBNK
    // This ensures SYSSETBNK uses the same initialized_ram_banks bitmap as port I/O
    m_hbios->setBankInitCallback([this](uint8_t bank) {
        emu_init_ram_bank(m_memory.get(), bank, &m_initializedRamBanks);
    });
}

void EmulatorEngine::initializeRamBankIfNeeded(uint8_t bank) {
    // Use shared initialization to copy page zero and HCB to RAM bank
    // This is required for CP/M 3 bank switching to work correctly
    emu_init_ram_bank(m_memory.get(), bank, &m_initializedRamBanks);
}

void EmulatorEngine::onHalt() {
    if (m_debug) {
        emu_log("[EMU] CPU halted at PC=0x%04X\n", m_cpu->regs.PC.get_pair16());
    }
}

void EmulatorEngine::onUnimplementedOpcode(uint8_t opcode, uint16_t pc) {
    emu_error("[EMU] Unimplemented opcode 0x%02X at PC=0x%04X\n", opcode, pc);
}

void EmulatorEngine::logDebug(const char* fmt, ...) {
    if (!m_debug) return;
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    emu_log("%s", buffer);
    va_end(args);
}

bool EmulatorEngine::loadROM(const std::string& path) {
    std::vector<uint8_t> data;
    if (!emu_file_load(path, data)) return false;
    return loadROMFromData(data.data(), data.size());
}

bool EmulatorEngine::loadROMFromData(const uint8_t* data, size_t size) {
    if (!m_memory || !data || size == 0) return false;

    // Reset RAM bank initialization tracking
    m_initializedRamBanks = 0;

    // Use shared ROM loading function
    // Note: emu_complete_init() is called later in start() after disks are loaded
    if (!emu_load_rom_from_buffer(m_memory.get(), data, size)) {
        return false;
    }

    return true;
}

bool EmulatorEngine::loadDisk(int unit, const std::string& path) {
    if (unit < 0 || unit >= 4) return false;
    std::vector<uint8_t> data;
    if (!emu_file_load(path, data)) return false;
    if (m_hbios->loadDisk(unit, data.data(), data.size())) {
        m_diskPaths[unit] = path;
        return true;
    }
    return false;
}

bool EmulatorEngine::loadDiskFromData(int unit, const uint8_t* data, size_t size) {
    if (unit < 0 || unit >= 4) return false;
    return m_hbios->loadDisk(unit, data, size);
}

void EmulatorEngine::closeDisk(int unit) {
    if (unit < 0 || unit >= 4) return;
    m_hbios->closeDisk(unit);
    m_diskPaths[unit].clear();
}

bool EmulatorEngine::saveDisk(int unit, const std::string& path) {
    if (unit < 0 || unit >= 4) return false;
    auto data = getDiskData(unit);
    if (data.empty()) return false;
    return emu_file_save(path, data);
}

std::vector<uint8_t> EmulatorEngine::getDiskData(int unit) {
    if (unit < 0 || unit >= 4) return {};
    const auto& disk = m_hbios->getDisk(unit);
    if (!disk.is_open) return {};
    return disk.data;
}

bool EmulatorEngine::isDiskLoaded(int unit) const {
    if (unit < 0 || unit >= 4) return false;
    return m_hbios->isDiskLoaded(unit);
}

void EmulatorEngine::setDiskPath(int unit, const std::string& path) {
    if (unit >= 0 && unit < 4) m_diskPaths[unit] = path;
}

const std::string& EmulatorEngine::getDiskPath(int unit) const {
    static const std::string empty;
    if (unit < 0 || unit >= 4) return empty;
    return m_diskPaths[unit];
}

void EmulatorEngine::setDiskSliceCount(int unit, int slices) {
    if (unit >= 0 && unit < 4 && m_hbios) {
        m_hbios->setDiskSliceCount(unit, slices);
    }
}

void EmulatorEngine::start() {
    if (m_running) return;
    m_stopRequested = false;
    m_running = true;

    // Initialize CPU state for fresh start
    m_cpu->regs.PC.set_pair16(0);
    m_cpu->regs.SP.set_pair16(0);
    m_cpu->regs.IFF1 = 0;
    m_cpu->regs.IFF2 = 0;
    m_memory->select_bank(0);
    m_initializedRamBanks = 0;

    // Complete initialization AFTER all disks are loaded
    // This ensures disk unit table includes all attached disks
    // Handles: APITYPE patching, HCB copy, HBIOS ident, memory disks, disk tables
    emu_complete_init(m_memory.get(), m_hbios.get(), nullptr);

    // Set up HBIOS proxy at 0xFFF0 in common RAM (bank 0x8F)
    // Proxy code: OUT (0xEF), A; RET  =>  D3 EF C9
    // This is required because RST 08 at 0x0008 jumps to 0xFFF0
    uint8_t* ram = m_memory->get_ram();
    if (ram) {
        const uint32_t COMMON_BASE = 0x0F * 0x8000;  // Bank 0x8F = index 15
        uint32_t proxy_phys = COMMON_BASE + (0xFFF0 - 0x8000);
        ram[proxy_phys + 0] = 0xD3;  // OUT (n), A
        ram[proxy_phys + 1] = 0xEF;  // port 0xEF
        ram[proxy_phys + 2] = 0xC9;  // RET
    }

    if (!m_bootString.empty()) {
        for (char c : m_bootString) emu_console_queue_char(c);
        emu_console_queue_char('\r');
    }
    sendStatus("Running");
}

void EmulatorEngine::stop() {
    if (!m_running) return;
    m_stopRequested = true;
    m_running = false;
    sendStatus("Stopped");
}

void EmulatorEngine::reset() {
    bool wasRunning = m_running;
    stop();
    m_cpu->regs.PC.set_pair16(0);
    m_cpu->regs.SP.set_pair16(0);
    m_cpu->regs.IFF1 = 0;
    m_cpu->regs.IFF2 = 0;
    m_memory->select_bank(0);
    m_initializedRamBanks = 0;
    emu_console_clear_queue();
    m_hbios->reset();
    m_escapeState = EscapeState::Normal;
    m_escapeParams.clear();
    m_escapeCurrentParam.clear();
    m_instructionCount = 0;
    if (wasRunning) start();
    sendStatus("Reset");
}

void EmulatorEngine::sendChar(char ch) { emu_console_queue_char(ch); }

void EmulatorEngine::sendString(const std::string& str) {
    for (char c : str) emu_console_queue_char(c);
}

void EmulatorEngine::setDebug(bool enable) {
    m_debug = enable;
    if (m_hbios) m_hbios->setDebug(enable);
}

uint16_t EmulatorEngine::getProgramCounter() const {
    return m_cpu ? m_cpu->regs.PC.get_pair16() : 0;
}

uint64_t EmulatorEngine::getInstructionCount() const { return m_instructionCount; }

void EmulatorEngine::runBatch() {
    if (!m_running) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    for (int i = 0; i < BATCH_SIZE && m_running && !m_stopRequested; i++) {
        m_cpu->execute();
        m_instructionCount++;
    }
    if (m_hbios->isWaitingForInput() && emu_console_has_input()) {
        m_hbios->clearWaitingForInput();
    }
}

void EmulatorEngine::flushOutput() {
    if (!m_hbios || !m_outputCallback) return;
    std::vector<uint8_t> chars = m_hbios->getOutputChars();
    for (uint8_t ch : chars) {
        m_outputCallback(ch);
    }
}

void EmulatorEngine::handleHBIOS() { m_hbios->handlePortDispatch(); }

void EmulatorEngine::sendStatus(const std::string& status) {
    if (m_statusCallback) m_statusCallback(status);
}

void EmulatorEngine::processChar(uint8_t ch) {
    if (m_outputCallback) m_outputCallback(ch);
}

std::string EmulatorEngine::getAppDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) *lastSlash = '\0';
    return std::string(path);
}

std::string EmulatorEngine::getUserDataDirectory() {
    // Use LocalAppData for user-writable files
    // This is essential for Microsoft Store apps which can't write to Program Files
    wchar_t* localAppData = nullptr;
    std::string userDir;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        // Convert wide string to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, localAppData, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string path(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, localAppData, -1, &path[0], len, nullptr, nullptr);
            userDir = path + "\\z80cpmw";
        }
        CoTaskMemFree(localAppData);
    }

    // Fallback to app directory if LocalAppData fails (shouldn't happen)
    if (userDir.empty()) {
        userDir = getAppDirectory();
    }

    // Ensure directory exists
    CreateDirectoryA(userDir.c_str(), nullptr);

    return userDir;
}

void EmulatorEngine::enableDazzler(uint8_t basePort, int scale) {
    if (m_dazzler) return;  // Already enabled

    m_dazzler = std::make_unique<Dazzler>(basePort);
    m_dazzler->setScale(scale);

    // Set memory read callback for Dazzler to read framebuffer
    // This properly handles banked memory (lower 32K from current bank, upper 32K from common)
    if (m_memory) {
        m_dazzler->setMemoryReadCallback([this](uint16_t addr) -> uint8_t {
            return m_memory->fetch_mem(addr);
        });

        // Set up memory write callback for framebuffer updates
        m_memory->set_write_callback([this](uint16_t addr, uint8_t value) {
            if (m_dazzler) {
                m_dazzler->onMemoryWrite(addr, value);
            }
        });
    }
}

void EmulatorEngine::disableDazzler() {
    if (!m_dazzler) return;

    // Clear memory write callback
    if (m_memory) {
        m_memory->set_write_callback(nullptr);
    }

    m_dazzler.reset();
}