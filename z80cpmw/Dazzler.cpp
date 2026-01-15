/*
 * Dazzler.cpp - Cromemco Dazzler Graphics Card Emulation Implementation
 */

#include "pch.h"
#include "Dazzler.h"

Dazzler::Dazzler(uint8_t basePort)
    : m_basePort(basePort)
    , m_startTime(std::chrono::steady_clock::now())
{
}

Dazzler::~Dazzler() {
}

void Dazzler::portOut(uint8_t port, uint8_t value) {
    uint8_t portOffset = port - m_basePort;

    if (portOffset == 0) {
        // Port 0xE (Control/Address)
        // D7: Enable (1=on, 0=off)
        // D6-D0: Address bits A15-A9 (left shift 1 to get high byte of buffer addr)
        bool wasEnabled = m_enabled;
        m_enabled = (value & 0x80) != 0;

        // Calculate framebuffer address: bits 6-0 become A15-A9
        // The address is on a 512-byte boundary (A8-A0 = 0)
        m_framebufferAddr = ((uint16_t)(value & 0x7F)) << 9;

        if (m_enabled != wasEnabled || m_enabled) {
            triggerUpdate();
        }
    }
    else if (portOffset == 1) {
        // Port 0xF (Format)
        // D7: Not used
        // D6: Resolution X4 (1=X4, 0=Normal)
        // D5: Memory size (1=2K, 0=512)
        // D4: Color mode (1=color, 0=B&W)
        // D3: High intensity (1=high, 0=low)
        // D2: Blue enable
        // D1: Green enable
        // D0: Red enable
        m_x4Mode = (value & 0x40) != 0;
        m_use2K = (value & 0x20) != 0;
        m_colorMode = (value & 0x10) != 0;
        m_highIntensity = (value & 0x08) != 0;
        m_colorMask = value & 0x07;

        if (m_enabled) {
            triggerUpdate();
        }
    }
}

uint8_t Dazzler::portIn(uint8_t port) {
    uint8_t portOffset = port - m_basePort;

    if (portOffset == 0) {
        // Port 0xE (Status)
        // D7: Odd/Even line (low during odd lines, high during even)
        // D6: End of frame (low for 4ms between frames)

        // Calculate timing based on NTSC timing
        // Frame rate: ~60 Hz (16.67ms per frame)
        // End of frame signal: low for 4ms between frames
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - m_startTime).count();

        // Frame timing (in microseconds)
        const int64_t FRAME_TIME_US = 16667;  // ~60 Hz
        const int64_t VBLANK_TIME_US = 4000;  // 4ms vertical blank
        const int64_t LINE_TIME_US = 63;      // ~63.5us per line (262.5 lines/frame)

        int64_t framePos = elapsed % FRAME_TIME_US;

        uint8_t status = 0;

        // D6: End of frame (low for 4ms at end of frame)
        if (framePos >= (FRAME_TIME_US - VBLANK_TIME_US)) {
            // In vertical blank - D6 is low
            status &= ~0x40;
        } else {
            status |= 0x40;
        }

        // D7: Odd/Even line
        int64_t linePos = framePos / LINE_TIME_US;
        if (linePos & 1) {
            status |= 0x80;  // Even line (high)
        }
        // Odd line: D7 stays low

        return status;
    }

    return 0xFF;  // Floating bus for unhandled ports
}

void Dazzler::onMemoryWrite(uint16_t addr, uint8_t value) {
    if (!m_enabled) return;
    if (!m_memory && !m_memoryReadCallback) return;

    // Check if write is within framebuffer region
    uint16_t bufferEnd = m_framebufferAddr + getMemorySize();
    if (addr >= m_framebufferAddr && addr < bufferEnd) {
        triggerUpdate();
    }
}

// Helper to read memory using callback or raw pointer
uint8_t Dazzler::readMemory(uint16_t addr) {
    if (m_memoryReadCallback) {
        return m_memoryReadCallback(addr);
    }
    if (m_memory && addr < 0x10000) {
        return m_memory[addr];
    }
    return 0;
}

int Dazzler::getWidth() const {
    if (m_x4Mode) {
        // X4 mode: 64x64 for 512 bytes, 128x128 for 2K
        return m_use2K ? 128 : 64;
    } else {
        // Normal mode: 32x32 for 512 bytes, 64x64 for 2K
        return m_use2K ? 64 : 32;
    }
}

int Dazzler::getHeight() const {
    // Height equals width for Dazzler
    return getWidth();
}

void Dazzler::render(uint8_t* rgbaBuffer) {
    if (!rgbaBuffer) return;
    if (!m_memory && !m_memoryReadCallback) return;

    int width = getWidth();
    int height = getHeight();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t color = getPixelColor(x, y);

            int idx = (y * width + x) * 4;
            rgbaBuffer[idx + 0] = (color >> 16) & 0xFF;  // R
            rgbaBuffer[idx + 1] = (color >> 8) & 0xFF;   // G
            rgbaBuffer[idx + 2] = color & 0xFF;          // B
            rgbaBuffer[idx + 3] = 0xFF;                  // A
        }
    }
}

uint32_t Dazzler::getPixelColor(int x, int y) {
    if (!m_memory && !m_memoryReadCallback) return 0xFF000000;  // Black with full alpha

    int width = getWidth();
    int height = getHeight();

    if (x < 0 || x >= width || y < 0 || y >= height) {
        return 0xFF000000;
    }

    if (m_x4Mode) {
        // X4 Resolution mode: 1 bit per pixel
        // Each byte represents 8 pixels in a 2x4 arrangement:
        // Bits map to positions:
        //   D0 D1 | D4 D5
        //   D2 D3 | D6 D7

        // Calculate which byte and bit this pixel is in
        int quadX = x / 4;  // Which column of quads
        int quadY = y / 2;  // Which row of quads
        int subX = x % 4;   // Position within quad column (0-3)
        int subY = y % 2;   // Position within quad row (0-1)

        // Calculate byte offset based on memory layout
        // Memory is organized in quadrants for 2K mode
        int offset;
        if (m_use2K) {
            // 2K mode: 128x128, organized in 4 quadrants
            int qx = x / 64;  // Quadrant x (0 or 1)
            int qy = y / 64;  // Quadrant y (0 or 1)
            int lx = x % 64;  // Local x within quadrant
            int ly = y % 64;  // Local y within quadrant

            int quadrant = qy * 2 + qx;
            int quadrantOffset = quadrant * 512;

            // Within quadrant: calculate byte and bit
            int byteX = lx / 4;
            int byteY = ly / 2;
            int byteOffset = byteY * 16 + byteX;
            offset = quadrantOffset + byteOffset;
        } else {
            // 512 byte mode: 64x64
            int byteX = x / 4;
            int byteY = y / 2;
            offset = byteY * 16 + byteX;
        }

        // Get bit position within byte
        int bitPos = (subY * 2 + (subX & 1)) + ((subX >> 1) * 4);

        uint16_t addr = m_framebufferAddr + offset;

        uint8_t byte = readMemory(addr);
        bool pixelOn = (byte >> bitPos) & 1;

        if (pixelOn) {
            // Use global color from format register
            return colorToRGBA(m_colorMask | (m_highIntensity ? 0x08 : 0));
        } else {
            return 0xFF000000;  // Black
        }
    } else {
        // Normal Resolution mode: 4 bits per pixel
        // Each byte contains two adjacent pixels (low nibble = first, high nibble = second)
        // Nibble format: D0=Red, D1=Green, D2=Blue, D3=Intensity

        int offset;
        if (m_use2K) {
            // 2K mode: 64x64, organized in 4 quadrants
            int qx = x / 32;  // Quadrant x (0 or 1)
            int qy = y / 32;  // Quadrant y (0 or 1)
            int lx = x % 32;  // Local x within quadrant
            int ly = y % 32;  // Local y within quadrant

            int quadrant = qy * 2 + qx;
            int quadrantOffset = quadrant * 512;

            // Within quadrant: 2 pixels per byte, 16 bytes per row
            int byteOffset = ly * 16 + (lx / 2);
            offset = quadrantOffset + byteOffset;
        } else {
            // 512 byte mode: 32x32
            // 2 pixels per byte, 16 bytes per row
            offset = y * 16 + (x / 2);
        }

        uint16_t addr = m_framebufferAddr + offset;

        uint8_t byte = readMemory(addr);

        // Low nibble = even pixel (x%2==0), high nibble = odd pixel (x%2==1)
        uint8_t nibble = (x & 1) ? (byte >> 4) : (byte & 0x0F);

        return colorToRGBA(nibble);
    }
}

uint32_t Dazzler::colorToRGBA(uint8_t color) {
    // Color format: D0=Red, D1=Green, D2=Blue, D3=Intensity
    // Returns 0xAARRGGBB

    if (m_colorMode) {
        // Color mode
        bool red = (color & 0x01) != 0;
        bool green = (color & 0x02) != 0;
        bool blue = (color & 0x04) != 0;
        bool intensity = (color & 0x08) != 0;

        uint8_t r = red ? (intensity ? 255 : 170) : (intensity ? 85 : 0);
        uint8_t g = green ? (intensity ? 255 : 170) : (intensity ? 85 : 0);
        uint8_t b = blue ? (intensity ? 255 : 170) : (intensity ? 85 : 0);

        return 0xFF000000 | (r << 16) | (g << 8) | b;
    } else {
        // Black and white mode - 4-bit intensity (16 shades of grey)
        uint8_t grey = (color & 0x0F) * 17;  // Scale 0-15 to 0-255
        return 0xFF000000 | (grey << 16) | (grey << 8) | grey;
    }
}

void Dazzler::triggerUpdate() {
    if (m_updateCallback) {
        m_updateCallback();
    }
}
