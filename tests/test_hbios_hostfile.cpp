/*
 * test_hbios_hostfile.cpp - what a CP/M guest actually sees for HBF_HOST_CAPS
 * (0xE9) and HBF_HOST_GETNAME (0xE8)
 *
 * The companion suite, test_hostfile.cpp, tests the backend functions
 * directly. This one goes through HBIOSDispatch::handleEXT() with real guest
 * registers and real guest memory, which is the only way to show that the
 * answers this port defines reach the program that asks - W8.COM issues
 * exactly this sequence:
 *
 *     B=0xE9              probe: refuse to send a host path unless bit 0 is set
 *     B=0xE2, DE=path     open the host file for writing
 *     B=0xE8, C=len, DE=buf   ask where it will really land, and print that
 *     B=0xE4 per byte, then B=0xE5 with C=1
 *
 * The probe is why the capability function had to be a backend function rather
 * than a core constant (romwbw_emu/docs/DOWNSTREAM_2026-08-25.md 1b): W8
 * believes the answer, so the answer has to come from the code it is about.
 */

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "qkz80.h"
#include "romwbw_mem.h"
#include "hbios_dispatch.h"
#include "emu_io.h"

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

//=============================================================================
// A minimal machine: enough CPU and memory for the dispatcher to read guest
// strings out of and write guest strings into. Nothing executes here - the
// tests set the registers the way the RST 8 caller would have left them and
// call the handler directly.
//=============================================================================

static const uint16_t PATH_ADDR = 0x2000;
static const uint16_t BUF_ADDR = 0x3000;

struct Machine {
    banked_mem mem;
    qkz80 cpu;
    HBIOSDispatch hbios;

    Machine() : cpu(&mem) {
        hbios.setCPU(&cpu);
        hbios.setMemory(&mem);
        hbios.reset();
    }

    void putString(uint16_t addr, const std::string& s) {
        for (size_t i = 0; i < s.size(); i++)
            mem.store_mem(addr + (uint16_t)i, (uint8_t)s[i]);
        mem.store_mem(addr + (uint16_t)s.size(), 0);
    }

    std::string getString(uint16_t addr, size_t max) {
        std::string out;
        for (size_t i = 0; i < max; i++) {
            uint8_t b = mem.fetch_mem(addr + (uint16_t)i);
            if (!b) break;
            out.push_back((char)b);
        }
        return out;
    }

    void fill(uint16_t addr, size_t n, uint8_t v) {
        for (size_t i = 0; i < n; i++) mem.store_mem(addr + (uint16_t)i, v);
    }

    // Issue one HBIOS extension call the way RST 8 would, and return A.
    uint8_t call(uint8_t b, uint8_t c = 0, uint16_t de = 0) {
        cpu.regs.BC.set_high(b);
        cpu.regs.BC.set_low(c);
        cpu.regs.DE.set_pair16(de);
        hbios.handleEXT();
        return cpu.regs.AF.get_high();
    }
};

static bool fileExists(const std::string& p) {
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string scratchDir() {
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA((DWORD)sizeof(tmp), tmp);
    if (n == 0 || n >= sizeof(tmp)) return std::string();
    std::string dir = std::string(tmp) + "z80cpmw_hbios_test";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

//=============================================================================

static void test_caps_probe() {
    printf("--- HBF_HOST_CAPS (0xE9), the probe W8 makes first ---\n");
    Machine m;

    uint8_t a = m.call(0xE9);
    checkEq(std::to_string((int)a), "0", "A = 0 (the function is implemented)");
    check((m.cpu.regs.DE.get_low() & EMU_HOST_CAP_SAFE_PATHS) != 0,
          "E has HOST_CAP_SAFE_PATHS set, so W8 will send a host path");
    checkEq(std::to_string((int)m.cpu.regs.DE.get_high()), "0",
            "D is the reserved second byte and stays zero");

    // The value the guest sees is this port's own emu_host_path_caps(), not a
    // constant baked into the core - that swap is the whole of section 1b.
    checkEq(std::to_string((int)m.cpu.regs.DE.get_low()),
            std::to_string((int)emu_host_path_caps()),
            "E is exactly what this backend asserts");

    // No inputs and no state: safe to call before anything is open, which is
    // what makes it usable as a probe.
    check(emu_host_file_get_state() == HOST_FILE_IDLE,
          "the probe left the transfer state alone");
}

static void test_getname_reports_the_real_destination() {
    printf("--- HBF_HOST_GETNAME (0xE8) over a real transfer ---\n");
    Machine m;

    std::string dir = scratchDir();
    check(!dir.empty(), "scratch directory created");
    if (dir.empty()) return;
    std::string requested = dir + "\\hbios_getname.txt";
    DeleteFileA(requested.c_str());

    // Before an open there is nothing to report, and the guest's buffer - which
    // still holds the path it typed - must not be disturbed.
    m.fill(BUF_ADDR, 64, 0xAA);
    checkEq(std::to_string((int)m.call(0xE8, 64, BUF_ADDR)), "255",
            "A = 0xFF before any open");
    checkEq(std::to_string((int)m.mem.fetch_mem(BUF_ADDR)), "170",
            "the guest buffer was left untouched");

    // B=0xE2: open for writing, guest path at DE.
    m.putString(PATH_ADDR, requested);
    checkEq(std::to_string((int)m.call(0xE2, 0, PATH_ADDR)), "0",
            "HBF_HOST_OPEN_W succeeded");

    m.fill(BUF_ADDR, 256, 0xAA);
    checkEq(std::to_string((int)m.call(0xE8, 200, BUF_ADDR)), "0",
            "HBF_HOST_GETNAME succeeded");
    std::string reported = m.getString(BUF_ADDR, 200);

    check(!reported.empty(), "the guest was given a destination");
    check(reported.find('.') != 0, "it is not a truncation fragment");

    // B=0xE4 per byte, B=0xE5 C=1 to close - the rest of what W8 does.
    const char* payload = "hbios";
    for (const char* p = payload; *p; p++)
        checkEq(std::to_string((int)m.call(0xE4, 0, (uint16_t)(uint8_t)*p)), "0",
                "HBF_HOST_WRITE accepted a byte");
    checkEq(std::to_string((int)m.call(0xE5, 1)), "0", "HBF_HOST_CLOSE succeeded");

    // The claim under test: what the guest printed names the file that exists.
    check(fileExists(reported), "a file exists at the path the guest was given");
    check(fileExists(requested), "and it is the file the guest asked for");

    // And the window closes with the transfer, so the next W8 cannot be told
    // where the last one went.
    checkEq(std::to_string((int)m.call(0xE8, 200, BUF_ADDR)), "255",
            "A = 0xFF again after close");

    DeleteFileA(requested.c_str());
}

static void test_getname_buffer_bounds() {
    printf("--- HBF_HOST_GETNAME respects the guest's buffer ---\n");
    Machine m;

    // A bare name resolves to the data folder, so this is comfortably longer
    // than the small buffers below - which is the normal case, not a contrived
    // one: every bare name gets the whole data folder prepended.
    m.putString(PATH_ADDR, "hbios_bounds.txt");
    checkEq(std::to_string((int)m.call(0xE2, 0, PATH_ADDR)), "0",
            "HBF_HOST_OPEN_W succeeded");

    std::string full = emu_host_file_get_write_name();
    check(full.size() > 24, "the effective path is longer than the test buffer");

    // C counts the terminator, so with C=24 exactly 23 characters and a NUL may
    // be written, and byte 24 of the buffer must survive.
    const uint8_t C = 24;
    m.fill(BUF_ADDR, 64, 0xAA);
    checkEq(std::to_string((int)m.call(0xE8, C, BUF_ADDR)), "0",
            "HBF_HOST_GETNAME succeeded into a short buffer");
    std::string got = m.getString(BUF_ADDR, 64);

    checkEq(std::to_string((int)got.size()), std::to_string((int)C - 1),
            "the answer fills the buffer but for the terminator");
    checkEq(std::to_string((int)m.mem.fetch_mem(BUF_ADDR + C - 1)), "0",
            "the terminator lands inside the buffer");
    checkEq(std::to_string((int)m.mem.fetch_mem(BUF_ADDR + C)), "170",
            "the byte past the buffer is untouched");

    // Truncation keeps the END of the path, behind "...", so the answer reads
    // as a fragment. Chopping the front off silently could name a real but
    // different directory, which is the failure this call exists to remove.
    check(got.rfind("...", 0) == 0, "a truncated answer is marked with ...");
    check(full.size() >= got.size() - 3 &&
              full.compare(full.size() - (got.size() - 3), got.size() - 3,
                           got.c_str() + 3) == 0,
          "what follows the marker is the tail of the real path");

    // Too small for even one character and a terminator: report failure rather
    // than write anything, so the guest keeps the path it already had.
    m.fill(BUF_ADDR, 8, 0xAA);
    checkEq(std::to_string((int)m.call(0xE8, 1, BUF_ADDR)), "255",
            "A = 0xFF when the buffer cannot hold a string");
    checkEq(std::to_string((int)m.mem.fetch_mem(BUF_ADDR)), "170",
            "and nothing was written to it");

    std::string reported = full;
    checkEq(std::to_string((int)m.call(0xE5, 1)), "0", "HBF_HOST_CLOSE succeeded");
    DeleteFileA(reported.c_str());
}

static void test_failed_open_reports_nothing() {
    printf("--- a failed open leaves nothing to report ---\n");
    Machine m;

    std::string dir = scratchDir();
    if (dir.empty()) { check(false, "scratch directory created"); return; }

    // The directory does not exist, so the write cannot succeed. This backend
    // buffers, so the open still returns success and the failure surfaces at
    // close - the contract allows that, and the guest is told either way.
    std::string bad = dir + "\\no_such_subdir\\out.txt";
    m.putString(PATH_ADDR, bad);
    checkEq(std::to_string((int)m.call(0xE2, 0, PATH_ADDR)), "0",
            "the buffering open reports success");
    checkEq(std::to_string((int)m.call(0xE5, 1)), "255",
            "HBF_HOST_CLOSE reports the write failure");

    m.fill(BUF_ADDR, 64, 0xAA);
    checkEq(std::to_string((int)m.call(0xE8, 64, BUF_ADDR)), "255",
            "and there is no destination to report afterwards");
    checkEq(std::to_string((int)m.mem.fetch_mem(BUF_ADDR)), "170",
            "the guest buffer was left untouched");
}

int main() {
    printf("=== HBIOS host-file extension suite ===\n\n");

    test_caps_probe();
    test_getname_reports_the_real_destination();
    test_getname_buffer_bounds();
    test_failed_open_reports_nothing();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
