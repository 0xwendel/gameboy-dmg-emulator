#pragma once

#include "cartridge.hpp"
#include "serial.hpp"

#include <cstdint>
#include <string>
#include <vector>

class APU;

class MMU {
public:
    enum class PpuAccessMode : uint8_t {
        HBlank = 0,
        VBlank = 1,
        OAM = 2,
        Transfer = 3
    };

    MMU();
    ~MMU() = default;

    void reset();
    void attachCartridge(Cartridge* cart);
    void attachAPU(APU* apu);

    // Boot ROM opcional (256 bytes DMG). Se carregada, mapeia 0x0000-0x00FF até FF50.
    bool loadBootRom(const std::string& path);
    bool loadBootRom(const std::vector<uint8_t>& data);
    bool bootRomLoaded() const { return m_bootRomLoaded; }
    bool bootRomActive() const { return m_bootRomActive; }
    void enableBootRom(bool enable);

    uint8_t readByte(uint16_t address) const;
    void writeByte(uint16_t address, uint8_t value);

    // Acesso interno sem bloqueio de VRAM/OAM (PPU, DMA).
    uint8_t readByteDirect(uint16_t address) const;
    void writeByteDirect(uint16_t address, uint8_t value);

    void setLY(uint8_t ly);
    void setLCDMode(uint8_t mode);
    void setPpuAccessMode(PpuAccessMode mode) { m_ppuMode = mode; }
    PpuAccessMode ppuAccessMode() const { return m_ppuMode; }

    // Contador DIV de 16 bits (Timer e APU leem bits internos).
    uint16_t& divCounter() { return m_divCounter; }
    uint16_t divCounter() const { return m_divCounter; }
    void setDivHigh();

    void setJoypadState(uint8_t directionState, uint8_t actionState);

    // DMA OAM: 1 byte por M-cycle, 160 M-cycles no total.
    void startDMA(uint8_t page);
    void tickDMA(uint8_t mCycles);
    bool dmaActive() const { return m_dmaActive; }

    void applyPostBootState();

    Serial& serial() { return m_serial; }
    const Serial& serial() const { return m_serial; }

    uint8_t* vram() { return m_vram; }
    uint8_t* oam() { return m_oam; }
    uint8_t* io() { return m_io; }
    uint8_t* wram() { return m_wram; }
    uint8_t* hram() { return m_hram; }
    uint8_t& ie() { return m_ie; }

    void serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const uint8_t*& ptr, const uint8_t* end);

private:
    Cartridge* m_cart = nullptr;
    APU* m_apu = nullptr;
    Serial m_serial;

    uint16_t m_divCounter = 0;
    uint8_t m_vram[0x2000]{};
    uint8_t m_wram[0x2000]{};
    uint8_t m_oam[0xA0]{};
    uint8_t m_io[0x80]{};
    uint8_t m_hram[0x7F]{};
    uint8_t m_ie = 0;

    uint8_t m_bootRom[0x100]{};
    bool m_bootRomLoaded = false;
    bool m_bootRomActive = false;

    uint8_t m_joypadSelect = 0x30;
    uint8_t m_joypadDirections = 0x0F;
    uint8_t m_joypadActions = 0x0F;

    // DMA
    bool m_dmaActive = false;
    uint16_t m_dmaSource = 0;
    uint8_t m_dmaIndex = 0;

    PpuAccessMode m_ppuMode = PpuAccessMode::HBlank;

    bool vramBlocked() const;
    bool oamBlocked() const;
};
