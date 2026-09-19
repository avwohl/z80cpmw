/*
 * test_hbios_hostfile.cpp - what a CP/M guest actually sees for HBF_HOST_CAPS
 * (0xE9) and HBF_HOST_GETNAME (0xE8), and for the real-time clock
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
 *
 * WHY THE RTC IS IN THIS FILE, added 2026-09-19.  romwbw_emu e41f686 fixed an
 * overflow this application SHIPPED: HBIOSDispatch counted the guest's RTC
 * offset in `long`, and `long` is 32 bits on LLP64 - which is every Windows
 * compiler, so it was every build of this port, not some of them.  days * 86400
 * passed INT32_MAX in January 2038.
 *
 * It survived here because of a gap this file is the cheapest place to close.
 * run_tests.bat has always compiled ..\romwbw_emu\src\hbios_dispatch.cc with cl
 * and linked it into THIS suite, so the defective arithmetic was built by the
 * very compiler that exposes it on every test run - and no suite ever called
 * handleRTC().  The dispatcher was already here; only the calls were missing.
 * Nothing in the build line changed to add these.
 *
 * No suite can control the host clock - emu_io_windows.cpp defines
 * emu_get_time() and is on this link line, so a stub would be a duplicate
 * symbol.  Nothing needs one: SETTIM stores a DIFFERENCE from the host reading
 * and GETTIM re-reads the host clock and adds it back, so whatever the host
 * clock says cancels out.  What that does leave is elapsed time between the two
 * calls, which is why the checks below set midday and compare the fields that
 * a slow test run cannot move.
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

    // The RTC half.  BF_RTCGETTIM and BF_RTCSETTIM both take a six-byte BCD
    // buffer at HL - YY MM DD HH MM SS - and go through handleRTC() rather
    // than handleEXT().  The year is two digits and the dispatcher adds 2000,
    // so 84 is 2084.
    static const uint16_t RTC_BUF = 0x4000;

    void writeTime(int yy, int mm, int dd, int hh, int mi, int ss) {
        const int v[6] = {yy, mm, dd, hh, mi, ss};
        for (int i = 0; i < 6; i++)
            mem.store_mem((uint16_t)(RTC_BUF + i),
                          (uint8_t)(((v[i] / 10) << 4) | (v[i] % 10)));
    }

    void writeRaw(int i, uint8_t byte) {
        mem.store_mem((uint16_t)(RTC_BUF + i), byte);
    }

    uint8_t rtc(uint8_t func) {
        cpu.regs.BC.set_high(func);
        cpu.regs.HL.set_pair16(RTC_BUF);
        hbios.handleRTC();
        return cpu.regs.AF.get_high();
    }

    int readField(int i) {
        uint8_t b = mem.fetch_mem((uint16_t)(RTC_BUF + i));
        return ((b >> 4) & 0x0F) * 10 + (b & 0x0F);
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

//=============================================================================
// The RTC, and the 32-bit `long` that broke it
//=============================================================================

static void test_rtc_past_2038_reads_back_what_was_set() {
    printf("--- a date past January 2038 survives the round trip ---\n");
    Machine m;

    // 2084 is the year romwbw_emu/tests/rtc_settim.cc picks on purpose, and
    // the arithmetic is why: 41666 days from the epoch * 86400 is
    // 3,599,942,400, which is past INT32_MAX.  Built with a 32-bit `long` it
    // wrapped to -695,024,896 and the guest read back a different date.
    //
    // MIDDAY, not 23:59:58.  The upstream test's leap-day block sets two
    // seconds before midnight and then compares the month and the day, so a
    // slow run rolls 2084-02-29 into 2084-03-01 and fails for a reason that
    // has nothing to do with the bug.  Nothing here is near a boundary: the
    // hour cannot move unless this suite takes twelve hours.
    m.writeTime(84, 2, 29, 12, 0, 0);
    checkEq(std::to_string((int)m.rtc(0x21)), "0",
            "BF_RTCSETTIM accepts 2084-02-29 12:00:00 (a leap day, and past 2038)");

    m.writeTime(0, 0, 0, 0, 0, 0);   // so a no-op read cannot pass by accident
    checkEq(std::to_string((int)m.rtc(0x20)), "0",
            "BF_RTCGETTIM reports success");

    checkEq(std::to_string(m.readField(0)), "84",
            "the year reads back as 84 - the check that fails on a 32-bit long");
    checkEq(std::to_string(m.readField(1)), "2", "the month reads back");
    checkEq(std::to_string(m.readField(2)), "29",
            "the leap day reads back, so February 29 2084 was taken as a real date");
    checkEq(std::to_string(m.readField(3)), "12", "the hour reads back");
}

static void test_rtc_a_second_set_replaces_the_first() {
    printf("--- setting the clock twice leaves it where the SECOND set put it ---\n");
    Machine m;

    // The reported symptom of the overflow was not only a wrong date: because
    // the stored offset is a difference, a wrong one made a second set appear
    // to ADD to the first rather than replace it.  Both dates here are past
    // 2038, so both offsets are in the range that used to wrap.
    m.writeTime(84, 2, 29, 12, 0, 0);
    check(m.rtc(0x21) == 0, "the first set is accepted");
    m.writeTime(70, 6, 15, 12, 0, 0);
    check(m.rtc(0x21) == 0, "the second set is accepted");

    m.writeTime(0, 0, 0, 0, 0, 0);
    m.rtc(0x20);
    checkEq(std::to_string(m.readField(0)), "70",
            "the year is the second one, not the sum of two offsets");
    checkEq(std::to_string(m.readField(1)), "6", "and so is the month");
    checkEq(std::to_string(m.readField(2)), "15", "and the day");
}

static void test_rtc_refuses_what_is_not_a_date() {
    printf("--- a buffer that is not a date is refused, not accepted ---\n");
    Machine m;

    // Answering success while discarding the value is the failure mode the
    // dispatcher calls the worst there is, because a caller cannot defend
    // against it.  These check it does not do that.
    m.writeTime(84, 13, 1, 12, 0, 0);
    check(m.rtc(0x21) != 0, "month 13 is refused");

    m.writeTime(84, 2, 29, 25, 0, 0);
    check(m.rtc(0x21) != 0, "hour 25 is refused");

    m.writeTime(84, 2, 29, 12, 0, 0);
    m.writeRaw(1, 0x1F);            // 0x1F is not valid BCD
    check(m.rtc(0x21) != 0, "a byte that is not valid BCD is refused");

    // And a refusal must not have moved the clock.
    m.writeTime(84, 2, 29, 12, 0, 0);
    check(m.rtc(0x21) == 0, "a real date is still accepted afterwards");
    m.writeTime(0, 0, 0, 0, 0, 0);
    m.rtc(0x20);
    checkEq(std::to_string(m.readField(0)), "84",
            "and the refusals left no offset of their own behind");
}

int main() {
    printf("=== HBIOS host-file extension and RTC suite ===\n\n");

    test_caps_probe();
    test_getname_reports_the_real_destination();
    test_getname_buffer_bounds();
    test_failed_open_reports_nothing();
    test_rtc_past_2038_reads_back_what_was_set();
    test_rtc_a_second_set_replaces_the_first();
    test_rtc_refuses_what_is_not_a_date();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
