#pragma once

#include "mmu.hpp"
#include <cstdint>
#include <vector>

class PPU {
public:
    enum Mode : uint8_t {
        ModeHBlank = 0,
        ModeVBlank = 1,
        ModeOAMSearch = 2,
        ModePixelTransfer = 3
    };

    PPU();
    ~PPU() = default;

    void reset();
    void tick(uint8_t mCycles, MMU& mmu);

    bool isFrameReady();
    const uint32_t* getFrameBuffer() const { return m_frameBuffer; }

    uint8_t ly() const { return m_ly; }
    Mode mode() const { return m_mode; }

    void serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const uint8_t*& ptr, const uint8_t* end);

private:
    uint32_t m_frameBuffer[160 * 144]{};
    uint32_t m_scanlineTicks = 0;
    uint8_t m_ly = 0;
    Mode m_mode = ModeOAMSearch;
    bool m_frameReady = false;
    bool m_statInterruptLine = false;
    bool m_lcdEnabled = true;

    // Contador interno da Window (só avança quando a window desenha).
    uint8_t m_windowLineCounter = 0;
    bool m_windowLineActive = false;

    uint8_t m_scanlineBGColorIndex[160]{};

    static constexpr uint32_t makeRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               r;
    }

    const uint32_t PALETTE[4] = {
        makeRGBA(0xE0, 0xF8, 0xD0),
        makeRGBA(0x88, 0xC0, 0x70),
        makeRGBA(0x34, 0x68, 0x56),
        makeRGBA(0x08, 0x18, 0x20)
    };

    void fillWhite();
    void updateStat(MMU& mmu);
    void setMode(Mode mode, MMU& mmu);

    void renderScanline(MMU& mmu);
    void renderBG(MMU& mmu);
    void renderWindow(MMU& mmu);
    void renderSprites(MMU& mmu);

    struct ScanlineSprite {
        int16_t x;
        int16_t y;
        uint8_t tile;
        uint8_t attrs;
        uint8_t oamIndex;
    };
};
