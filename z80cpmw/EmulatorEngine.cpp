/*
 * EmulatorEngine.cpp - Z80/RomWBW Emulator Engine Implementation
 */

#include "pch.h"
#include "EmulatorEngine.h"
#include "Core/qkz80.h"
#include "Core/romwbw_mem.h"
#include "Core/hbios_dispatch.h"
#include "Core/emu_io.h"

// External callback setters from emu_io_windows.cpp
extern "C" {
    void emu_io_set_output_callback(void(*cb)(uint8_t));
    void emu_io_set_video_callback(void(*cb)(int, int, int, uint8_t));
    void emu_io_set_beep_callback(void(*cb)(int));
}

// Global engine pointer for callbacks (single instance)
static EmulatorEngine* g_engine = nullptr;

// Output callback wrapper
static void outputCallbackWrapper(uint8_t ch) {
    if (g_engine) {
        // Route through the engine's callback
        // Note: This is called from the emulator thread
    }
}

EmulatorEngine::EmulatorEngine() {
    g_engine = this;
    initCPU();
    emu_io_init();
}

EmulatorEngine::~EmulatorEngine() {
    stop();
    g_engine = nullptr;
    emu_io_cleanup();
}

void EmulatorEngine::initCPU() {
    // Create banked memory (512KB ROM + 512KB RAM)
    m_memory = std::make_unique<banked_mem>();
    m_memory->enable_banking();  // Enable banked memory mode

    // Create Z80 CPU
    m_cpu = std::make_unique<qkz80>(m_memory.get());

    // Create HBIOS dispatcher
    m_hbios = std::make_unique<HBIOSDispatch>();
    m_hbios->setCPU(m_cpu.get());
    m_hbios->setMemory(m_memory.get());
    m_hbios->setSkipRet(true);  // I/O port dispatch mode
    m_hbios->setBlockingAllowed(false);  // Non-blocking for GUI

    // Set up reset callback
    m_hbios->setResetCallback([this](uint8_t resetType) {
        (void)resetType;
        m_memory->select_bank(0);  // Switch to ROM bank 0
        emu_console_clear_queue();
        m_cpu->regs.PC.set_pair16(0);
    });
}

bool EmulatorEngine::loadROM(const std::string& path) {
    std::vector<uint8_t> data;
    if (!emu_file_load(path, data)) {
        return false;
    }
    return loadROMFromData(data.data(), data.size());
}

bool EmulatorEngine::loadROMFromData(const uint8_t* data, size_t size) {
    if (!m_memory || !data || size == 0) {
        return false;
    }

    // Make sure banking is enabled
    if (!m_memory->is_banking_enabled()) {
        m_memory->enable_banking();
    }

    // Clear RAM before loading new ROM
    m_memory->clear_ram();

    // Copy ROM data directly to the ROM buffer
    uint8_t* rom = m_memory->get_rom();
    if (!rom) {
        return false;
    }

    size_t copySize = std::min(size, (size_t)banked_mem::ROM_SIZE);
    memcpy(rom, data, copySize);

    // Initialize memory disks after ROM load
    m_hbios->initMemoryDisks();

    return true;
}

bool EmulatorEngine::loadDisk(int unit, const std::string& path) {
    if (unit < 0 || unit >= 4) return false;

    std::vector<uint8_t> data;
    if (!emu_file_load(path, data)) {
        return false;
    }

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
    if (unit >= 0 && unit < 4) {
        m_diskPaths[unit] = path;
    }
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

    // Send boot string characters if set
    if (!m_bootString.empty()) {
        for (char c : m_bootString) {
            emu_console_queue_char(c);
        }
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

    // Reset CPU
    m_cpu->regs.PC.set_pair16(0);
    m_cpu->regs.SP.set_pair16(0);
    m_cpu->regs.IFF1 = 0;
    m_cpu->regs.IFF2 = 0;

    // Reset to ROM bank 0
    m_memory->select_bank(0);

    // Clear input queue
    emu_console_clear_queue();

    // Reset HBIOS
    m_hbios->reset();

    // Reset escape state
    m_escapeState = EscapeState::Normal;
    m_escapeParams.clear();
    m_escapeCurrentParam.clear();

    m_instructionCount = 0;

    if (wasRunning) {
        start();
    }

    sendStatus("Reset");
}

void EmulatorEngine::sendChar(char ch) {
    emu_console_queue_char(ch);
}

void EmulatorEngine::sendString(const std::string& str) {
    for (char c : str) {
        emu_console_queue_char(c);
    }
}

void EmulatorEngine::setDebug(bool enable) {
    if (m_hbios) {
        m_hbios->setDebug(enable);
    }
}

uint16_t EmulatorEngine::getProgramCounter() const {
    return m_cpu ? m_cpu->regs.PC.get_pair16() : 0;
}

uint64_t EmulatorEngine::getInstructionCount() const {
    return m_instructionCount;
}

void EmulatorEngine::runBatch() {
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (int i = 0; i < BATCH_SIZE && m_running && !m_stopRequested; i++) {
        // Execute one instruction
        m_cpu->execute();
        m_instructionCount++;

        // Check for HBIOS call via OUT port 0xEF
        // The Z80 proxy in ROM does: OUT (0xEF), A to trigger HBIOS
        // We detect this in the I/O handler
    }

    // Check if waiting for input
    if (m_hbios->isWaitingForInput()) {
        if (emu_console_has_input()) {
            m_hbios->clearWaitingForInput();
        }
    }
}

void EmulatorEngine::handleHBIOS() {
    m_hbios->handlePortDispatch();
}

void EmulatorEngine::sendStatus(const std::string& status) {
    if (m_statusCallback) {
        m_statusCallback(status);
    }
}

void EmulatorEngine::processChar(uint8_t ch) {
    if (m_outputCallback) {
        m_outputCallback(ch);
    }
}

std::string EmulatorEngine::getAppDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);

    // Remove filename to get directory
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
    }

    return std::string(path);
}
