#include "emulator.hpp"

#include <cstring>
#include <fstream>
#include <iostream>

namespace {
constexpr uint32_t kStateMagic = 0x31424745; // EGB1
constexpr uint32_t kStateVersion = 3;
}

Emulator::Emulator() {
    m_mmu.attachCartridge(&m_cart);
    m_mmu.attachAPU(&m_apu);
}

void Emulator::applyPostBootIO() {
    m_mmu.applyPostBootState();
    m_apu.reset();
    m_apu.writeRegister(0xFF10, 0x80);
    m_apu.writeRegister(0xFF11, 0xBF);
    m_apu.writeRegister(0xFF12, 0xF3);
    m_apu.writeRegister(0xFF14, 0xBF);
    m_apu.writeRegister(0xFF16, 0x3F);
    m_apu.writeRegister(0xFF17, 0x00);
    m_apu.writeRegister(0xFF19, 0xBF);
    m_apu.writeRegister(0xFF1A, 0x7F);
    m_apu.writeRegister(0xFF1B, 0xFF);
    m_apu.writeRegister(0xFF1C, 0x9F);
    m_apu.writeRegister(0xFF1E, 0xBF);
    m_apu.writeRegister(0xFF20, 0xFF);
    m_apu.writeRegister(0xFF21, 0x00);
    m_apu.writeRegister(0xFF22, 0x00);
    m_apu.writeRegister(0xFF23, 0xBF);
    m_apu.writeRegister(0xFF24, 0x77);
    m_apu.writeRegister(0xFF25, 0xF3);
    m_apu.writeRegister(0xFF26, 0xF1);
}

bool Emulator::loadBootRom(const std::string& path) {
    if (!m_mmu.loadBootRom(path)) {
        m_useBootRom = false;
        return false;
    }
    m_useBootRom = true;
    return true;
}

bool Emulator::loadRom(const std::string& path) {
    if (!m_cart.loadFromFile(path)) {
        std::cerr << "Failed to load ROM: " << path << "\n";
        return false;
    }
    m_romPath = path;
    reset();
    loadBattery();
    return true;
}

bool Emulator::loadRom(const std::vector<uint8_t>& data, const std::string& pathHint) {
    if (!m_cart.load(data)) return false;
    m_romPath = pathHint;
    reset();
    if (!pathHint.empty()) loadBattery();
    return true;
}

void Emulator::reset() {
    m_mmu.reset();
    m_mmu.attachCartridge(&m_cart);
    m_mmu.attachAPU(&m_apu);
    m_timer.reset();
    m_ppu.reset();

    if (m_useBootRom && m_mmu.bootRomLoaded()) {
        m_mmu.enableBootRom(true);
        m_cpu.reset(true);
        m_apu.reset();
    } else {
        m_mmu.enableBootRom(false);
        m_cpu.reset(false);
        applyPostBootIO();
    }
    m_paused = false;
}

void Emulator::setMuted(bool m) {
    m_muted = m;
    m_apu.setEnabled(!m);
}

void Emulator::setJoypad(uint8_t directions, uint8_t actions) {
    m_mmu.setJoypadState(directions, actions);
}

void Emulator::advancePeripherals(uint8_t mCycles) {
    m_mmu.tickDMA(mCycles);
    const uint32_t tCycles = static_cast<uint32_t>(mCycles) * 4u;
    for (uint32_t i = 0; i < tCycles; ++i) {
        const uint16_t divBefore = m_mmu.divCounter();
        m_timer.tickTCycle(m_mmu);
        m_apu.tickTCycle(divBefore, m_mmu.divCounter());
    }
    m_mmu.serial().tick(tCycles, m_mmu);
    m_ppu.tick(mCycles, m_mmu);
}

uint8_t Emulator::stepInstruction() {
    const uint8_t cycles = m_cpu.step(m_mmu);
    advancePeripherals(cycles);
    return cycles;
}

void Emulator::runFrame() {
    if (m_paused) return;

    m_cart.updateRtcWallClock();

    int safety = 0;
    const int maxSteps = 200000;
    while (!m_ppu.isFrameReady() && safety < maxSteps) {
        stepInstruction();
        safety++;
    }
}

bool Emulator::saveBattery() const {
    if (!m_cart.hasBattery()) return false;
    const std::string path = m_cart.defaultSavePath();
    const_cast<Cartridge&>(m_cart).updateRtcWallClock();
    if (m_cart.saveBattery(path)) {
        std::cout << "SRAM" << (m_cart.hasRtc() ? "+RTC" : "") << " saved to " << path << "\n";
        return true;
    }
    return false;
}

bool Emulator::loadBattery() {
    if (!m_cart.hasBattery()) return false;
    const std::string path = m_cart.defaultSavePath();
    if (m_cart.loadBattery(path)) {
        std::cout << "SRAM" << (m_cart.hasRtc() ? "+RTC" : "") << " loaded from " << path << "\n";
        return true;
    }
    return false;
}

bool Emulator::saveState(const std::string& path) const {
    std::vector<uint8_t> buf;
    auto push32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    push32(kStateMagic);
    push32(kStateVersion);
    m_cpu.serialize(buf);
    m_mmu.serialize(buf);
    m_ppu.serialize(buf);
    m_timer.serialize(buf);
    m_apu.serialize(buf);

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return static_cast<bool>(f);
}

bool Emulator::loadState(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (buf.size() < 8) return false;

    const uint8_t* ptr = buf.data();
    const uint8_t* end = buf.data() + buf.size();
    auto read32 = [&]() -> uint32_t {
        uint32_t v = static_cast<uint32_t>(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24));
        ptr += 4;
        return v;
    };

    if (read32() != kStateMagic) return false;
    const uint32_t ver = read32();
    if (ver < 2 || ver > kStateVersion) return false;
    if (!m_cpu.deserialize(ptr, end)) return false;
    if (!m_mmu.deserialize(ptr, end)) return false;
    if (!m_ppu.deserialize(ptr, end)) return false;
    if (!m_timer.deserialize(ptr, end)) return false;
    if (!m_apu.deserialize(ptr, end)) return false;

    m_mmu.attachCartridge(&m_cart);
    m_mmu.attachAPU(&m_apu);
    m_apu.clearSampleBuffer();
    return true;
}
