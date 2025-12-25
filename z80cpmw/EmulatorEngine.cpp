/*
 * EmulatorEngine.cpp - Z80/RomWBW Emulator Engine Implementation
 *
 * Uses the shared hbios_cpu class with HBIOSCPUDelegate pattern.
 */

#include "pch.h"
#include "EmulatorEngine.h"
#include "Core/hbios_cpu.h"
#include "Core/romwbw_mem.h"
#include "Core/hbios_dispatch.h"
#include "Core/emu_io.h"
#include "Core/emu_init.h"

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

    // Debug: verify disk unit table was written
    uint8_t* rom = m_memory->get_rom();
    FILE* dbg = fopen("C:\\temp\\z80start.txt", "a");
    if (dbg && rom) {
        fprintf(dbg, "[START] After emu_complete_init, disk unit table at ROM 0x160:\n");
        for (int i = 0; i < 8; i++) {
            uint8_t type = rom[0x160 + i * 4];
            uint8_t unit = rom[0x160 + i * 4 + 1];
            if (type != 0xFF) {
                fprintf(dbg, "  Entry %d: type=0x%02X unit=%d\n", i, type, unit);
            }
        }
        fprintf(dbg, "[START] Disks loaded: ");
        for (int i = 0; i < 4; i++) {
            fprintf(dbg, "%d=%s ", i, m_hbios->isDiskLoaded(i) ? "yes" : "no");
        }
        fprintf(dbg, "\n");
        fclose(dbg);
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