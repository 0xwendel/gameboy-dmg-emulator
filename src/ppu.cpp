#include "ppu.hpp"

#include <algorithm>
#include <cstring>

PPU::PPU() {
    reset();
}

void PPU::reset() {
    m_scanlineTicks = 0;
    m_ly = 0;
    m_mode = ModeOAMSearch;
    m_frameReady = false;
    m_statInterruptLine = false;
    m_lcdEnabled = true;
    m_windowLineCounter = 0;
    m_windowLineActive = false;
    std::memset(m_scanlineBGColorIndex, 0, sizeof(m_scanlineBGColorIndex));
    fillWhite();
}

void PPU::fillWhite() {
    for (int i = 0; i < 160 * 144; ++i) {
        m_frameBuffer[i] = PALETTE[0];
    }
}

bool PPU::isFrameReady() {
    if (m_frameReady) {
        m_frameReady = false;
        return true;
    }
    return false;
}

void PPU::setMode(Mode mode, MMU& mmu) {
    m_mode = mode;
    mmu.setLCDMode(static_cast<uint8_t>(mode));
}

void PPU::updateStat(MMU& mmu) {
    uint8_t stat = mmu.io()[0x41];
    const uint8_t lyc = mmu.io()[0x45];

    bool ppuSignal = false;
    if ((m_mode == ModeHBlank) && (stat & 0x08)) ppuSignal = true;
    if ((m_mode == ModeVBlank) && (stat & 0x10)) ppuSignal = true;
    if ((m_mode == ModeOAMSearch) && (stat & 0x20)) ppuSignal = true;

    const bool coincidence = (m_ly == lyc);
    if (coincidence) {
        stat |= 0x04;
        if (stat & 0x40) ppuSignal = true;
    } else {
        stat = static_cast<uint8_t>(stat & ~0x04);
    }

    stat = static_cast<uint8_t>((stat & 0xFC) | (static_cast<uint8_t>(m_mode) & 0x03));
    mmu.io()[0x41] = stat;
    mmu.setPpuAccessMode(static_cast<MMU::PpuAccessMode>(m_mode));

    if (ppuSignal && !m_statInterruptLine) {
        mmu.io()[0x0F] |= 0x02;
    }
    m_statInterruptLine = ppuSignal;
}

void PPU::tick(uint8_t mCycles, MMU& mmu) {
    const uint8_t lcdc = mmu.io()[0x40];
    const bool lcdOn = (lcdc & 0x80) != 0;

    if (!lcdOn) {
        if (m_lcdEnabled) {
            // LCD acabou de desligar: reseta estado e limpa tela
            m_lcdEnabled = false;
            m_scanlineTicks = 0;
            m_ly = 0;
            m_mode = ModeHBlank;
            m_windowLineCounter = 0;
            mmu.setLY(0);
            setMode(ModeHBlank, mmu);
            fillWhite();
            m_statInterruptLine = false;
        }
        // Com LCD off o hardware não gera VBlank; ainda assim sinalizamos
        // frames virtuais (~70224 dots) para o host não travar.
        m_scanlineTicks += static_cast<uint32_t>(mCycles) * 4u;
        if (m_scanlineTicks >= 70224u) {
            m_scanlineTicks -= 70224u;
            m_frameReady = true;
        }
        return;
    }

    if (!m_lcdEnabled) {
        // LCD religado
        m_lcdEnabled = true;
        m_scanlineTicks = 0;
        m_ly = 0;
        m_windowLineCounter = 0;
        setMode(ModeOAMSearch, mmu);
        mmu.setLY(0);
    }

    m_scanlineTicks += static_cast<uint32_t>(mCycles) * 4u;
    Mode oldMode = m_mode;

    if (m_ly < 144) {
        if (m_scanlineTicks < 80) {
            m_mode = ModeOAMSearch;
        } else if (m_scanlineTicks < 252) {
            m_mode = ModePixelTransfer;
        } else {
            m_mode = ModeHBlank;
        }

        if (oldMode == ModePixelTransfer && m_mode == ModeHBlank) {
            renderScanline(mmu);
        }

        if (m_scanlineTicks >= 456) {
            m_scanlineTicks -= 456;
            m_ly++;
            mmu.setLY(m_ly);

            if (m_ly == 144) {
                m_mode = ModeVBlank;
                m_frameReady = true;
                mmu.io()[0x0F] |= 0x01; // VBlank IRQ
            } else {
                m_mode = ModeOAMSearch;
            }
        }
    } else {
        m_mode = ModeVBlank;
        if (m_scanlineTicks >= 456) {
            m_scanlineTicks -= 456;
            m_ly++;
            if (m_ly == 154) {
                m_ly = 0;
                m_mode = ModeOAMSearch;
                m_windowLineCounter = 0;
            }
            mmu.setLY(m_ly);
        }
    }

    mmu.setLCDMode(static_cast<uint8_t>(m_mode));
    updateStat(mmu);
}

void PPU::renderScanline(MMU& mmu) {
    const uint8_t lcdc = mmu.io()[0x40];
    if (!(lcdc & 0x80)) return;

    for (int i = 0; i < 160; ++i) {
        m_scanlineBGColorIndex[i] = 0;
        // Se BG desligado, pixels ficam brancos (cor 0) no DMG com comportamento especial
        m_frameBuffer[m_ly * 160 + i] = PALETTE[0];
    }

    // No DMG, LCDC.0 desliga o background (pixels 0) e força sprites à frente
    if (lcdc & 0x01) {
        renderBG(mmu);
    }

    m_windowLineActive = false;
    if (lcdc & 0x20) {
        renderWindow(mmu);
    }
    if (m_windowLineActive) {
        m_windowLineCounter++;
    }

    if (lcdc & 0x02) {
        renderSprites(mmu);
    }
}

void PPU::renderBG(MMU& mmu) {
    const uint8_t lcdc = mmu.io()[0x40];
    const uint8_t scy = mmu.io()[0x42];
    const uint8_t scx = mmu.io()[0x43];
    const uint8_t bgp = mmu.io()[0x47];

    const uint16_t mapBase = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    const bool isSigned = !(lcdc & 0x10);

    const uint8_t bgY = static_cast<uint8_t>(m_ly + scy);
    const uint8_t tileRow = bgY / 8;
    const uint8_t pixelRow = bgY % 8;

    for (int x = 0; x < 160; ++x) {
        const uint8_t bgX = static_cast<uint8_t>(x + scx);
        const uint8_t tileCol = bgX / 8;
        const uint8_t pixelCol = bgX % 8;

        const uint16_t mapOffset = static_cast<uint16_t>(tileRow * 32 + tileCol);
        const uint8_t tileIndex = mmu.readByteDirect(static_cast<uint16_t>(mapBase + mapOffset));

        uint16_t tileAddress;
        if (isSigned) {
            tileAddress = static_cast<uint16_t>(0x9000 + static_cast<int8_t>(tileIndex) * 16);
        } else {
            tileAddress = static_cast<uint16_t>(0x8000 + tileIndex * 16);
        }

        const uint8_t byteA = mmu.readByteDirect(static_cast<uint16_t>(tileAddress + pixelRow * 2));
        const uint8_t byteB = mmu.readByteDirect(static_cast<uint16_t>(tileAddress + pixelRow * 2 + 1));

        const uint8_t bitShift = static_cast<uint8_t>(7 - pixelCol);
        const uint8_t colorIndex = static_cast<uint8_t>((((byteB >> bitShift) & 1) << 1) | ((byteA >> bitShift) & 1));

        m_scanlineBGColorIndex[x] = colorIndex;
        const uint8_t paletteShade = (bgp >> (colorIndex * 2)) & 0x03;
        m_frameBuffer[m_ly * 160 + x] = PALETTE[paletteShade];
    }
}

void PPU::renderWindow(MMU& mmu) {
    const uint8_t lcdc = mmu.io()[0x40];
    const uint8_t wy = mmu.io()[0x4A];
    const uint8_t wx = mmu.io()[0x4B];
    const uint8_t bgp = mmu.io()[0x47];

    if (m_ly < wy || wx > 166) {
        return;
    }

    // Precisa de pelo menos um pixel da window na linha
    const int16_t windowXStart = static_cast<int16_t>(wx) - 7;
    if (windowXStart >= 160) return;

    const uint16_t mapBase = (lcdc & 0x40) ? 0x9C00 : 0x9800;
    const bool isSigned = !(lcdc & 0x10);

    const uint8_t winY = m_windowLineCounter;
    const uint8_t tileRow = winY / 8;
    const uint8_t pixelRow = winY % 8;

    bool drew = false;

    for (int x = 0; x < 160; ++x) {
        if (x < windowXStart) continue;

        const uint8_t winX = static_cast<uint8_t>(x - windowXStart);
        const uint8_t tileCol = winX / 8;
        const uint8_t pixelCol = winX % 8;

        const uint16_t mapOffset = static_cast<uint16_t>(tileRow * 32 + tileCol);
        const uint8_t tileIndex = mmu.readByteDirect(static_cast<uint16_t>(mapBase + mapOffset));

        uint16_t tileAddress;
        if (isSigned) {
            tileAddress = static_cast<uint16_t>(0x9000 + static_cast<int8_t>(tileIndex) * 16);
        } else {
            tileAddress = static_cast<uint16_t>(0x8000 + tileIndex * 16);
        }

        const uint8_t byteA = mmu.readByteDirect(static_cast<uint16_t>(tileAddress + pixelRow * 2));
        const uint8_t byteB = mmu.readByteDirect(static_cast<uint16_t>(tileAddress + pixelRow * 2 + 1));

        const uint8_t bitShift = static_cast<uint8_t>(7 - pixelCol);
        const uint8_t colorIndex = static_cast<uint8_t>((((byteB >> bitShift) & 1) << 1) | ((byteA >> bitShift) & 1));

        m_scanlineBGColorIndex[x] = colorIndex;
        const uint8_t paletteShade = (bgp >> (colorIndex * 2)) & 0x03;
        m_frameBuffer[m_ly * 160 + x] = PALETTE[paletteShade];
        drew = true;
    }

    m_windowLineActive = drew;
}

void PPU::renderSprites(MMU& mmu) {
    const uint8_t lcdc = mmu.io()[0x40];
    const bool is8x16 = (lcdc & 0x04) != 0;
    const uint8_t spriteHeight = is8x16 ? 16 : 8;
    // No DMG, se LCDC.0=0, sprites sempre têm prioridade sobre o "BG"
    const bool bgMasterPriority = (lcdc & 0x01) != 0;

    std::vector<ScanlineSprite> visibleSprites;
    visibleSprites.reserve(10);

    for (uint8_t i = 0; i < 40; ++i) {
        const uint16_t oamBase = static_cast<uint16_t>(0xFE00 + i * 4);
        const uint8_t yByte = mmu.readByteDirect(oamBase);
        const uint8_t xByte = mmu.readByteDirect(static_cast<uint16_t>(oamBase + 1));
        const uint8_t tile = mmu.readByteDirect(static_cast<uint16_t>(oamBase + 2));
        const uint8_t attrs = mmu.readByteDirect(static_cast<uint16_t>(oamBase + 3));

        const int16_t yPos = static_cast<int16_t>(yByte) - 16;
        const int16_t xPos = static_cast<int16_t>(xByte) - 8;

        if (m_ly >= yPos && m_ly < (yPos + spriteHeight)) {
            visibleSprites.push_back({xPos, yPos, tile, attrs, i});
            if (visibleSprites.size() == 10) break;
        }
    }

    std::sort(visibleSprites.begin(), visibleSprites.end(), [](const ScanlineSprite& a, const ScanlineSprite& b) {
        if (a.x != b.x) return a.x > b.x;
        return a.oamIndex > b.oamIndex;
    });

    for (const auto& sprite : visibleSprites) {
        const bool bgPriority = (sprite.attrs & 0x80) != 0;
        const bool yFlip = (sprite.attrs & 0x40) != 0;
        const bool xFlip = (sprite.attrs & 0x20) != 0;
        const uint16_t paletteReg = (sprite.attrs & 0x10) ? 0xFF49 : 0xFF48;
        const uint8_t obp = mmu.readByteDirect(paletteReg);

        uint8_t spriteRow = static_cast<uint8_t>(m_ly - sprite.y);
        if (yFlip) spriteRow = static_cast<uint8_t>(spriteHeight - 1 - spriteRow);

        uint8_t tileIndex = sprite.tile;
        uint8_t tileRow = spriteRow;
        if (is8x16) {
            tileIndex &= 0xFE;
            if (spriteRow >= 8) {
                tileIndex |= 1;
                tileRow = static_cast<uint8_t>(spriteRow - 8);
            }
        }

        const uint16_t tileAddress = static_cast<uint16_t>(0x8000 + tileIndex * 16);
        const uint8_t byteA = mmu.readByteDirect(static_cast<uint16_t>(tileAddress + tileRow * 2));
        const uint8_t byteB = mmu.readByteDirect(static_cast<uint16_t>(tileAddress + tileRow * 2 + 1));

        for (int col = 0; col < 8; ++col) {
            const int16_t pixelX = static_cast<int16_t>(sprite.x + col);
            if (pixelX < 0 || pixelX >= 160) continue;

            uint8_t tileCol = static_cast<uint8_t>(col);
            if (xFlip) tileCol = static_cast<uint8_t>(7 - tileCol);

            const uint8_t bitShift = static_cast<uint8_t>(7 - tileCol);
            const uint8_t colorIndex = static_cast<uint8_t>((((byteB >> bitShift) & 1) << 1) | ((byteA >> bitShift) & 1));
            if (colorIndex == 0) continue;

            if (bgMasterPriority && bgPriority && m_scanlineBGColorIndex[pixelX] != 0) {
                continue;
            }

            const uint8_t paletteShade = (obp >> (colorIndex * 2)) & 0x03;
            m_frameBuffer[m_ly * 160 + pixelX] = PALETTE[paletteShade];
        }
    }
}

void PPU::serialize(std::vector<uint8_t>& out) const {
    auto push32 = [&](uint32_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    push32(m_scanlineTicks);
    out.push_back(m_ly);
    out.push_back(static_cast<uint8_t>(m_mode));
    out.push_back(m_frameReady ? 1 : 0);
    out.push_back(m_statInterruptLine ? 1 : 0);
    out.push_back(m_lcdEnabled ? 1 : 0);
    out.push_back(m_windowLineCounter);
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(m_frameBuffer),
               reinterpret_cast<const uint8_t*>(m_frameBuffer) + sizeof(m_frameBuffer));
}

bool PPU::deserialize(const uint8_t*& ptr, const uint8_t* end) {
    if (static_cast<size_t>(end - ptr) < 4 + 6 + sizeof(m_frameBuffer)) return false;
    m_scanlineTicks = static_cast<uint32_t>(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24));
    ptr += 4;
    m_ly = *ptr++;
    m_mode = static_cast<Mode>(*ptr++);
    m_frameReady = (*ptr++ != 0);
    m_statInterruptLine = (*ptr++ != 0);
    m_lcdEnabled = (*ptr++ != 0);
    m_windowLineCounter = *ptr++;
    std::memcpy(m_frameBuffer, ptr, sizeof(m_frameBuffer));
    ptr += sizeof(m_frameBuffer);
    return true;
}
