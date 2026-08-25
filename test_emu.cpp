/*
 * test_emu.cpp - Console test harness for the emulator
 * Build and run: run_test.bat
 *
 * The core is not in this repository. z80cpmw.vcxproj compiles it straight out
 * of the sibling checkouts, and so does this harness: qkz80* comes from
 * ../cpmemu/src and romwbw_mem.h from ../romwbw_emu/src. The old paths under
 * z80cpmw/Core stopped resolving when the core moved out, which left this file
 * and both of its build scripts unable to compile at all.
 */

#define _CRT_SECURE_NO_WARNINGS

#ifdef _WIN32
#include <windows.h>
#define usleep(us) Sleep((us) / 1000)
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>

// Include emulator core
#include "qkz80.h"
#include "romwbw_mem.h"

// Globals for test
static banked_mem* g_mem = nullptr;
static qkz80* g_cpu = nullptr;
static std::vector<uint8_t> g_output;
static int g_portOutCount = 0;
static int g_portInCount = 0;

// Simple emu_io stubs for the test
extern "C" {
    void emu_console_write_char(uint8_t ch) {
        g_output.push_back(ch);
        if (ch >= 32 && ch < 127) {
            putchar(ch);
        } else if (ch == '\r') {
            putchar('\n');
        }
        fflush(stdout);
    }

    bool emu_console_has_input() { return false; }
    int emu_console_read_char() { return -1; }
    void emu_console_queue_char(int ch) { (void)ch; }
    void emu_console_clear_queue() {}
}

bool load_rom(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("ERROR: Cannot open ROM file: %s\n", filename);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("ROM file size: %ld bytes\n", size);

    uint8_t* rom = g_mem->get_rom();
    size_t read = fread(rom, 1, size, f);
    fclose(f);

    printf("Read %zu bytes into ROM\n", read);
    printf("First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", rom[i]);
    }
    printf("\n");

    return read > 0;
}

// Port I/O reaches the guest by overriding qkz80's virtuals, the way
// romwbw_emu's hbios_cpu does. It used to arrive through
// set_port_out_callback() / set_port_in_callback(), which the core no longer
// has; together with the include paths under the removed z80cpmw/Core, that is
// why this harness had stopped compiling.
class test_cpu : public qkz80 {
public:
    explicit test_cpu(banked_mem* memory) : qkz80(memory) {}

    void port_out(qkz80_uint8 port, qkz80_uint8 value) override {
        g_portOutCount++;
        if (g_portOutCount <= 50) {
            printf("[OUT] port=0x%02X value=0x%02X\n", port, value);
        }

        switch (port) {
        case 0x78:
        case 0x7C:
            g_mem->select_bank(value);
            printf("  -> Bank select: 0x%02X\n", value);
            break;
        case 0x68:
            emu_console_write_char(value);
            break;
        }
    }

    qkz80_uint8 port_in(qkz80_uint8 port) override {
        g_portInCount++;
        if (g_portInCount <= 20) {
            printf("[IN] port=0x%02X\n", port);
        }

        switch (port) {
        case 0x69:
            return 0x02;  // TX ready
        default:
            return 0xFF;
        }
    }
};

int main(int argc, char* argv[]) {
    printf("=== Z80 Emulator Test Harness ===\n\n");

    // Create memory
    g_mem = new banked_mem();
    g_mem->enable_banking();
    printf("Memory initialized: banking=%d\n", g_mem->is_banking_enabled());

    // Create CPU. Port I/O comes from test_cpu's overrides, not from a pair of
    // callbacks set here - see the class above.
    g_cpu = new test_cpu(g_mem);
    printf("CPU initialized\n");

    // Find and load ROM
    const char* romPaths[] = {
        "bin/Debug/roms/emu_avw.rom",
        "roms/emu_avw.rom",
        "../roms/emu_avw.rom",
        "z80cpmw/roms/emu_avw.rom",
    };

    bool romLoaded = false;
    for (const char* path : romPaths) {
        printf("Trying ROM path: %s\n", path);
        if (load_rom(path)) {
            romLoaded = true;
            break;
        }
    }

    if (!romLoaded) {
        printf("ERROR: Could not find ROM file\n");
        return 1;
    }

    // Check what's at address 0
    printf("\nMemory at 0x0000 (bank 0x%02X): ", g_mem->get_current_bank());
    for (int i = 0; i < 16; i++) {
        printf("%02X ", g_mem->fetch_mem(i));
    }
    printf("\n");

    // Run CPU
    printf("\n=== Starting CPU execution ===\n");
    printf("Initial PC: 0x%04X\n", g_cpu->regs.PC.get_pair16());

    int instructionCount = 0;
    int maxInstructions = 1000000;  // 1M instructions max
    uint16_t lastPC = 0xFFFF;
    int samePC = 0;

    for (int i = 0; i < maxInstructions; i++) {
        uint16_t pc = g_cpu->regs.PC.get_pair16();

        // Check for stuck CPU
        if (pc == lastPC) {
            samePC++;
            if (samePC > 100) {
                printf("\nCPU stuck at PC=0x%04X after %d instructions\n", pc, i);
                break;
            }
        } else {
            samePC = 0;
            lastPC = pc;
        }

        // Trace first 20 instructions
        if (i < 20) {
            uint8_t opcode = g_mem->fetch_mem(pc);
            printf("  [%d] PC=0x%04X opcode=0x%02X\n", i, pc, opcode);
        }

        g_cpu->execute();
        instructionCount++;

        // Progress report every 100K instructions
        if (i > 0 && i % 100000 == 0) {
            printf("... %d instructions, PC=0x%04X, bank=0x%02X, output=%zu chars\n",
                   i, g_cpu->regs.PC.get_pair16(), g_mem->get_current_bank(), g_output.size());
        }

        // Stop if we have significant output
        if (g_output.size() > 500) {
            printf("\nGot %zu output characters, stopping.\n", g_output.size());
            break;
        }
    }

    printf("\n=== Execution complete ===\n");
    printf("Instructions executed: %d\n", instructionCount);
    printf("Final PC: 0x%04X\n", g_cpu->regs.PC.get_pair16());
    printf("Final bank: 0x%02X\n", g_mem->get_current_bank());
    printf("Port OUT calls: %d\n", g_portOutCount);
    printf("Port IN calls: %d\n", g_portInCount);
    printf("Output characters: %zu\n", g_output.size());

    if (!g_output.empty()) {
        printf("\n=== Output ===\n");
        for (uint8_t ch : g_output) {
            if (ch >= 32 && ch < 127) putchar(ch);
            else if (ch == '\r' || ch == '\n') putchar('\n');
        }
        printf("\n");
    }

    delete g_cpu;
    delete g_mem;

    return 0;
}
