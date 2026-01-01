/*
 * EmulatorEngine.h - Z80/RomWBW Emulator Engine
 *
 * Wraps the Z80 CPU and HBIOS to provide a clean interface for the GUI.
 * Implements HBIOSCPUDelegate for the shared hbios_cpu class.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <cstdarg>
#include "hbios_cpu.h"

// Forward declarations
class hbios_cpu;
class banked_mem;
class HBIOSDispatch;

// Callback types
using OutputCharCallback = std::function<void(uint8_t ch)>;
using StatusCallback = std::function<void(const std::string& status)>;

class EmulatorEngine : public HBIOSCPUDelegate {
public:
    EmulatorEngine();
    ~EmulatorEngine() override;

    // ROM management
    bool loadROM(const std::string& path);
    bool loadROMFromData(const uint8_t* data, size_t size);
    void setROMName(const std::string& name) { m_romName = name; }
    const std::string& getROMName() const { return m_romName; }

    // Disk management (units 0-3)
    bool loadDisk(int unit, const std::string& path);
    bool loadDiskFromData(int unit, const uint8_t* data, size_t size);
    bool saveDisk(int unit, const std::string& path);
    std::vector<uint8_t> getDiskData(int unit);
    bool isDiskLoaded(int unit) const;
    void setDiskPath(int unit, const std::string& path);
    const std::string& getDiskPath(int unit) const;
    void setDiskSliceCount(int unit, int slices);

    // Execution control
    void start();
    void stop();
    void reset();
    bool isRunning() const { return m_running; }

    // Input
    void sendChar(char ch);
    void sendString(const std::string& str);

    // Boot string (auto-type at boot menu)
    void setBootString(const std::string& bootStr) { m_bootString = bootStr; }
    const std::string& getBootString() const { return m_bootString; }

    // Debug
    void setDebug(bool enable);
    uint16_t getProgramCounter() const;
    uint64_t getInstructionCount() const;

    // Callbacks
    void setOutputCallback(OutputCharCallback cb) { m_outputCallback = cb; }
    void setStatusCallback(StatusCallback cb) { m_statusCallback = cb; }
    OutputCharCallback getOutputCallback() const { return m_outputCallback; }

    // Execute a batch of instructions (call from timer)
    void runBatch();

    // Flush buffered output to callback (call after runBatch)
    void flushOutput();

    // Get application directory (for read-only resources like ROMs)
    static std::string getAppDirectory();

    // Get user data directory (for writable files like settings, downloaded disks)
    // Uses LocalAppData on Windows, which is writable even for Store apps
    static std::string getUserDataDirectory();

    //=========================================================================
    // HBIOSCPUDelegate interface implementation
    //=========================================================================
    banked_mem* getMemory() override { return m_memory.get(); }
    HBIOSDispatch* getHBIOS() override { return m_hbios.get(); }
    void initializeRamBankIfNeeded(uint8_t bank) override;
    void onHalt() override;
    void onUnimplementedOpcode(uint8_t opcode, uint16_t pc) override;
    void logDebug(const char* fmt, ...) override;

private:
    void initCPU();
    void emulatorThread();
    void handleHBIOS();
    void sendStatus(const std::string& status);
    void processChar(uint8_t ch);

    std::unique_ptr<banked_mem> m_memory;
    std::unique_ptr<hbios_cpu> m_cpu;
    std::unique_ptr<HBIOSDispatch> m_hbios;

    std::string m_romName;
    std::string m_diskPaths[4];
    std::string m_bootString;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    std::thread m_thread;
    std::mutex m_mutex;

    OutputCharCallback m_outputCallback;
    StatusCallback m_statusCallback;

    // VT100 escape sequence state
    enum class EscapeState {
        Normal,
        Escape,
        CSI,
        CSIParam
    };
    EscapeState m_escapeState = EscapeState::Normal;
    std::vector<int> m_escapeParams;
    std::string m_escapeCurrentParam;

    // Instruction count for throttling
    uint64_t m_instructionCount = 0;
    static constexpr int BATCH_SIZE = 100000;

    // RAM bank initialization tracking (bitmask for banks 0x80-0x8F)
    uint16_t m_initializedRamBanks = 0;

    // Debug flag
    bool m_debug = false;
};
