#include "mmu.hpp"
#include "apu.hpp"

#include <cstring>

MMU::MMU() {
    reset();
}

void MMU::reset() {
    std::memset(m_vram, 0, sizeof(m_vram));
    std::memset(m_wram, 0, sizeof(m_wram));
    std::memset(m_oam, 0, sizeof(m_oam));
    std::memset(m_io, 0, sizeof(m_io));
    std::memset(m_hram, 0, sizeof(m_hram));
    m_ie = 0;
    m_divCounter = 0;
    m_bootRomActive = false;
    m_joypadSelect = 0x30;
    m_joypadDirections = 0x0F;
    m_joypadActions = 0x0F;
    m_dmaActive = false;
    m_dmaSource = 0;
    m_dmaIndex = 0;
    m_ppuMode = PpuAccessMode::HBlank;
}

void MMU::attachCartridge(Cartridge* cart) {
    m_cart = cart;
}

void MMU::attachAPU(APU* apu) {
    m_apu = apu;
}

void MMU::applyPostBootState() {
    // Valores típicos do DMG após a boot ROM
    m_io[0x05] = 0x00; // TIMA
    m_io[0x06] = 0x00; // TMA
    m_io[0x07] = 0x00; // TAC
    m_io[0x10] = 0x80;
    m_io[0x11] = 0xBF;
    m_io[0x12] = 0xF3;
    m_io[0x14] = 0xBF;
    m_io[0x16] = 0x3F;
    m_io[0x17] = 0x00;
    m_io[0x19] = 0xBF;
    m_io[0x1A] = 0x7F;
    m_io[0x1B] = 0xFF;
    m_io[0x1C] = 0x9F;
    m_io[0x1E] = 0xBF;
    m_io[0x20] = 0xFF;
    m_io[0x21] = 0x00;
    m_io[0x22] = 0x00;
    m_io[0x23] = 0xBF;
    m_io[0x24] = 0x77;
    m_io[0x25] = 0xF3;
    m_io[0x26] = 0xF1;
    m_io[0x40] = 0x91; // LCDC
    m_io[0x41] = 0x85; // STAT
    m_io[0x42] = 0x00; // SCY
    m_io[0x43] = 0x00; // SCX
    m_io[0x44] = 0x00; // LY
    m_io[0x45] = 0x00; // LYC
    m_io[0x47] = 0xFC; // BGP
    m_io[0x48] = 0xFF; // OBP0
    m_io[0x49] = 0xFF; // OBP1
    m_io[0x4A] = 0x00; // WY
    m_io[0x4B] = 0x00; // WX
    m_io[0x0F] = 0xE1; // IF
    m_ie = 0x00;
    m_bootRomActive = false;
    m_divCounter = 0xABCC; // valor comum pós-boot (aprox.)
    setDivHigh();
}

void MMU::setDivHigh() {
    m_io[0x04] = static_cast<uint8_t>(m_divCounter >> 8);
}

void MMU::setLY(uint8_t ly) {
    m_io[0x44] = ly;
}

void MMU::setLCDMode(uint8_t mode) {
    m_io[0x41] = static_cast<uint8_t>((m_io[0x41] & 0xFC) | (mode & 0x03));
    m_ppuMode = static_cast<PpuAccessMode>(mode & 0x03);
}

bool MMU::vramBlocked() const {
    return m_ppuMode == PpuAccessMode::Transfer;
}

bool MMU::oamBlocked() const {
    return m_ppuMode == PpuAccessMode::OAM || m_ppuMode == PpuAccessMode::Transfer || m_dmaActive;
}

void MMU::startDMA(uint8_t page) {
    m_dmaSource = static_cast<uint16_t>(page) << 8;
    m_dmaIndex = 0;
    m_dmaActive = true;
    m_io[0x46] = page;
}

void MMU::tickDMA(uint8_t mCycles) {
    for (uint8_t i = 0; i < mCycles && m_dmaActive; ++i) {
        // Durante DMA, a fonte é lida do barramento (exceto que OAM destino é direto)
        uint8_t value = readByteDirect(static_cast<uint16_t>(m_dmaSource + m_dmaIndex));
        m_oam[m_dmaIndex] = value;
        m_dmaIndex++;
        if (m_dmaIndex >= 160) {
            m_dmaActive = false;
        }
    }
}

uint8_t MMU::readByteDirect(uint16_t address) const {
    if (address <= 0x7FFF || (address >= 0xA000 && address <= 0xBFFF)) {
        if (m_cart) return m_cart->read(address);
        return 0xFF;
    }
    if (address >= 0x8000 && address <= 0x9FFF) {
        return m_vram[address - 0x8000];
    }
    if (address >= 0xC000 && address <= 0xDFFF) {
        return m_wram[address - 0xC000];
    }
    if (address >= 0xE000 && address <= 0xFDFF) {
        return m_wram[address - 0xE000];
    }
    if (address >= 0xFE00 && address <= 0xFE9F) {
        return m_oam[address - 0xFE00];
    }
    if (address >= 0xFEA0 && address <= 0xFEFF) {
        return 0xFF;
    }
    if (address >= 0xFF00 && address <= 0xFF7F) {
        if (address == 0xFF00) {
            uint8_t res = 0xC0 | m_joypadSelect;
            uint8_t buttons = 0x0F;
            if (!(m_joypadSelect & 0x10)) buttons &= m_joypadDirections;
            if (!(m_joypadSelect & 0x20)) buttons &= m_joypadActions;
            return static_cast<uint8_t>(res | (buttons & 0x0F));
        }
        if (m_apu && address >= 0xFF10 && address <= 0xFF3F) {
            return m_apu->readRegister(address);
        }
        if (address == 0xFF04) {
            return static_cast<uint8_t>(m_divCounter >> 8);
        }
        if (address == 0xFF0F) {
            return m_io[0x0F] | 0xE0; // bits superiores abertos
        }
        return m_io[address - 0xFF00];
    }
    if (address >= 0xFF80 && address <= 0xFFFE) {
        return m_hram[address - 0xFF80];
    }
    if (address == 0xFFFF) {
        return m_ie;
    }
    return 0xFF;
}

void MMU::writeByteDirect(uint16_t address, uint8_t value) {
    if (address <= 0x7FFF || (address >= 0xA000 && address <= 0xBFFF)) {
        if (m_cart) m_cart->write(address, value);
        return;
    }
    if (address >= 0x8000 && address <= 0x9FFF) {
        m_vram[address - 0x8000] = value;
        return;
    }
    if (address >= 0xC000 && address <= 0xDFFF) {
        m_wram[address - 0xC000] = value;
        return;
    }
    if (address >= 0xE000 && address <= 0xFDFF) {
        m_wram[address - 0xE000] = value;
        return;
    }
    if (address >= 0xFE00 && address <= 0xFE9F) {
        m_oam[address - 0xFE00] = value;
        return;
    }
    if (address >= 0xFEA0 && address <= 0xFEFF) {
        return;
    }
    if (address >= 0xFF00 && address <= 0xFF7F) {
        if (address == 0xFF00) {
            m_joypadSelect = value & 0x30;
            return;
        }
        if (address == 0xFF04) {
            // Reset DIV: pode causar falling edge no TIMA (tratado no Timer via contador)
            m_divCounter = 0;
            setDivHigh();
            return;
        }
        if (address == 0xFF46) {
            startDMA(value);
            return;
        }
        if (address == 0xFF50 && value != 0) {
            m_bootRomActive = false;
        }
        if (m_apu && address >= 0xFF10 && address <= 0xFF3F) {
            m_apu->writeRegister(address, value);
            m_io[address - 0xFF00] = value;
            return;
        }
        if (address == 0xFF41) {
            // Bits 0-2 read-only (mode + coincidence); preserva
            m_io[0x41] = static_cast<uint8_t>((m_io[0x41] & 0x07) | (value & 0x78) | 0x80);
            return;
        }
        if (address == 0xFF44) {
            // LY read-only
            return;
        }
        if (address == 0xFF0F) {
            m_io[0x0F] = value & 0x1F;
            return;
        }
        m_io[address - 0xFF00] = value;
        return;
    }
    if (address >= 0xFF80 && address <= 0xFFFE) {
        m_hram[address - 0xFF80] = value;
        return;
    }
    if (address == 0xFFFF) {
        m_ie = value;
    }
}

uint8_t MMU::readByte(uint16_t address) const {
    // Durante DMA, CPU só acessa HRAM de forma confiável; simplificamos
    // bloqueando VRAM/OAM conforme modo da PPU.
    if (address >= 0x8000 && address <= 0x9FFF && vramBlocked()) {
        return 0xFF;
    }
    if (address >= 0xFE00 && address <= 0xFE9F && oamBlocked()) {
        return 0xFF;
    }
    return readByteDirect(address);
}

void MMU::writeByte(uint16_t address, uint8_t value) {
    if (address >= 0x8000 && address <= 0x9FFF && vramBlocked()) {
        return;
    }
    if (address >= 0xFE00 && address <= 0xFE9F && oamBlocked()) {
        return;
    }
    writeByteDirect(address, value);
}

void MMU::setJoypadState(uint8_t directionState, uint8_t actionState) {
    bool interrupt = false;
    const uint8_t prevDirections = m_joypadDirections;
    const uint8_t prevActions = m_joypadActions;

    m_joypadDirections = directionState & 0x0F;
    m_joypadActions = actionState & 0x0F;

    if (!(m_joypadSelect & 0x10)) {
        if ((prevDirections & ~m_joypadDirections) & 0x0F) interrupt = true;
    }
    if (!(m_joypadSelect & 0x20)) {
        if ((prevActions & ~m_joypadActions) & 0x0F) interrupt = true;
    }
    if (interrupt) {
        m_io[0x0F] |= 0x10;
    }
}

void MMU::serialize(std::vector<uint8_t>& out) const {
    auto push16 = [&](uint16_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>(v >> 8));
    };
    push16(m_divCounter);
    out.insert(out.end(), m_vram, m_vram + sizeof(m_vram));
    out.insert(out.end(), m_wram, m_wram + sizeof(m_wram));
    out.insert(out.end(), m_oam, m_oam + sizeof(m_oam));
    out.insert(out.end(), m_io, m_io + sizeof(m_io));
    out.insert(out.end(), m_hram, m_hram + sizeof(m_hram));
    out.push_back(m_ie);
    out.push_back(m_joypadSelect);
    out.push_back(m_joypadDirections);
    out.push_back(m_joypadActions);
    out.push_back(m_dmaActive ? 1 : 0);
    push16(m_dmaSource);
    out.push_back(m_dmaIndex);
    out.push_back(static_cast<uint8_t>(m_ppuMode));
}

bool MMU::deserialize(const uint8_t*& ptr, const uint8_t* end) {
    auto need = [&](size_t n) { return static_cast<size_t>(end - ptr) >= n; };
    if (!need(2 + 0x2000 + 0x2000 + 0xA0 + 0x80 + 0x7F + 8)) return false;

    m_divCounter = static_cast<uint16_t>(ptr[0] | (ptr[1] << 8));
    ptr += 2;
    std::memcpy(m_vram, ptr, 0x2000); ptr += 0x2000;
    std::memcpy(m_wram, ptr, 0x2000); ptr += 0x2000;
    std::memcpy(m_oam, ptr, 0xA0); ptr += 0xA0;
    std::memcpy(m_io, ptr, 0x80); ptr += 0x80;
    std::memcpy(m_hram, ptr, 0x7F); ptr += 0x7F;
    m_ie = *ptr++;
    m_joypadSelect = *ptr++;
    m_joypadDirections = *ptr++;
    m_joypadActions = *ptr++;
    m_dmaActive = (*ptr++ != 0);
    m_dmaSource = static_cast<uint16_t>(ptr[0] | (ptr[1] << 8)); ptr += 2;
    m_dmaIndex = *ptr++;
    m_ppuMode = static_cast<PpuAccessMode>(*ptr++);
    return true;
}
