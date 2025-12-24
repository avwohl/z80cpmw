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