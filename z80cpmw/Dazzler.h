/*
 * Dazzler.h - Cromemco Dazzler Graphics Card Emulation
 *
 * Emulates the Cromemco Dazzler color graphics card (1976).
 * Supports both normal resolution (32x32/64x64) and X4 resolution (64x64/128x128) modes.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <chrono>

// Callback when display needs updating
using DazzlerUpdateCallback = std::function<void()>;

// Callback to read memory (handles banked memory correctly)
using DazzlerMemoryReadCallback = std::function<uint8_t(uint16_t addr)>;

class Dazzler {
public:
    // Display modes
    static constexpr int MODE_NORMAL = 0;    // 4-bit per pixel (color/intensity)
    static constexpr int MODE_X4 = 1;        // 1-bit per pixel (on/off)

    // Memory sizes
    static constexpr int MEM_512 = 512;
    static constexpr int MEM_2K = 2048;

    // Maximum resolution
    static constexpr int MAX_WIDTH = 128;
    static constexpr int MAX_HEIGHT = 128;

    Dazzler(uint8_t basePort = 0x0E);
    ~Dazzler();

    // Port I/O
    void portOut(uint8_t port, uint8_t value);
    uint8_t portIn(uint8_t port);

    // Memory access - call when Z80 writes to memory
    void onMemoryWrite(uint16_t addr, uint8_t value);

    // Set memory pointer for reading framebuffer (deprecated - use setMemoryReadCallback)
    void setMemoryPointer(const uint8_t* memory) { m_memory = memory; }

    // Set memory read callback for proper banked memory access
    void setMemoryReadCallback(DazzlerMemoryReadCallback cb) { m_memoryReadCallback = cb; }

    // State queries
    bool isEnabled() const { return m_enabled; }
    uint8_t getBasePort() const { return m_basePort; }
    uint16_t getFramebufferAddress() const { return m_framebufferAddr; }
    int getMemorySize() const { return m_use2K ? MEM_2K : MEM_512; }

    // Display properties
    int getWidth() const;
    int getHeight() const;
    bool isColorMode() const { return m_colorMode; }
    bool isX4Mode() const { return m_x4Mode; }
    bool isHighIntensity() const { return m_highIntensity; }
    uint8_t getColorMask() const { return m_colorMask; }  // RGB enable bits

    // Render the current framebuffer to an RGBA buffer
    // Returns pixel data in RGBA format (4 bytes per pixel)
    // Buffer must be at least getWidth() * getHeight() * 4 bytes
    void render(uint8_t* rgbaBuffer);

    // Get a single pixel color as RGBA
    uint32_t getPixelColor(int x, int y);

    // Update callback
    void setUpdateCallback(DazzlerUpdateCallback cb) { m_updateCallback = cb; }

    // Scaling factor for display window
    void setScale(int scale) { m_scale = scale > 0 ? scale : 1; }
    int getScale() const { return m_scale; }

private:
    // Convert Dazzler 4-bit color to RGBA
    uint32_t colorToRGBA(uint8_t color);

    // Calculate pixel address in framebuffer
    int getPixelOffset(int x, int y);

    // Trigger display update
    void triggerUpdate();

    // Read memory (uses callback if set, otherwise raw pointer)
    uint8_t readMemory(uint16_t addr);

    // Port configuration
    uint8_t m_basePort;

    // Control registers (from port 0xE output)
    bool m_enabled = false;
    uint16_t m_framebufferAddr = 0;  // Starting address of picture memory

    // Format registers (from port 0xF output)
    bool m_x4Mode = false;       // D6: Resolution X4 mode
    bool m_use2K = false;        // D5: Use 2K bytes (vs 512)
    bool m_colorMode = true;     // D4: Color (vs B&W)
    bool m_highIntensity = true; // D3: High intensity
    uint8_t m_colorMask = 0x07;  // D2-D0: RGB enable bits

    // Memory pointer (deprecated)
    const uint8_t* m_memory = nullptr;

    // Memory read callback (preferred)
    DazzlerMemoryReadCallback m_memoryReadCallback;

    // Display scaling
    int m_scale = 2;

    // Timing for input port
    std::chrono::steady_clock::time_point m_startTime;

    // Update callback
    DazzlerUpdateCallback m_updateCallback;
};
