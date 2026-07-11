#pragma once

#include "apu.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "mmu.hpp"
#include "ppu.hpp"
#include "timer.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Orquestra CPU, PPU, Timer, APU e cartucho em um único passo de emulação.
class Emulator {
public:
    Emulator();

    bool loadRom(const std::string& path);
    bool loadRom(const std::vector<uint8_t>& data, const std::string& pathHint = "");

    // Executa até completar um frame (~70224 T-cycles / VBlank).
    void runFrame();

    // Executa uma instrução e avança periféricos pelos M-cycles gastos.
    uint8_t stepInstruction();

    void reset();
    void setJoypad(uint8_t directions, uint8_t actions);

    // Bateria do cartucho
    bool saveBattery() const;
    bool loadBattery();

    // Save state simples (binário)
    bool saveState(const std::string& path) const;
    bool loadState(const std::string& path);

    const uint32_t* frameBuffer() const { return m_ppu.getFrameBuffer(); }
    bool takeFrameReady() { return m_ppu.isFrameReady(); }

    CPU& cpu() { return m_cpu; }
    const CPU& cpu() const { return m_cpu; }
    MMU& mmu() { return m_mmu; }
    const MMU& mmu() const { return m_mmu; }
    PPU& ppu() { return m_ppu; }
    APU& apu() { return m_apu; }
    const Cartridge& cart() const { return m_cart; }

    size_t popAudio(int16_t* out, size_t maxFrames) { return m_apu.popSamples(out, maxFrames); }

    bool paused() const { return m_paused; }
    void setPaused(bool p) { m_paused = p; }
    void togglePause() { m_paused = !m_paused; }

    float speed() const { return m_speed; }
    void setSpeed(float s) { m_speed = s < 0.25f ? 0.25f : (s > 8.0f ? 8.0f : s); }

    bool muted() const { return m_muted; }
    void setMuted(bool m);

private:
    void advancePeripherals(uint8_t mCycles);
    void applyPostBootIO();

    Cartridge m_cart;
    MMU m_mmu;
    CPU m_cpu;
    PPU m_ppu;
    Timer m_timer;
    APU m_apu;

    bool m_paused = false;
    bool m_muted = false;
    float m_speed = 1.0f;
    std::string m_romPath;
};
