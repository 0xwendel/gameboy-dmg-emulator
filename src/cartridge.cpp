#include "cartridge.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

bool Cartridge::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    m_romPath = path;
    return load(data);
}

bool Cartridge::load(const std::vector<uint8_t>& romData) {
    if (romData.size() < 0x150) {
        std::cerr << "ROM too small (incomplete header).\n";
        return false;
    }
    m_rom = romData;
    if (m_rom.size() < 0x8000) {
        m_rom.resize(0x8000, 0xFF);
    }
    parseHeader();
    return true;
}

void Cartridge::parseHeader() {
    m_title.clear();
    for (int i = 0x134; i <= 0x143; ++i) {
        char c = static_cast<char>(m_rom[i]);
        if (c == 0) break;
        if (c >= 32 && c < 127) m_title.push_back(c);
    }

    const uint8_t cartType = m_rom[0x0147];
    m_hasBattery = false;
    m_hasRtc = false;

    switch (cartType) {
        case 0x00:
            m_type = MbcType::None;
            break;
        case 0x01: case 0x02:
            m_type = MbcType::MBC1;
            break;
        case 0x03:
            m_type = MbcType::MBC1;
            m_hasBattery = true;
            break;
        case 0x05:
            m_type = MbcType::MBC2;
            break;
        case 0x06:
            m_type = MbcType::MBC2;
            m_hasBattery = true;
            break;
        case 0x0F: case 0x10:
            m_type = MbcType::MBC3;
            m_hasBattery = true;
            m_hasRtc = true;
            break;
        case 0x11: case 0x12:
            m_type = MbcType::MBC3;
            break;
        case 0x13:
            m_type = MbcType::MBC3;
            m_hasBattery = true;
            break;
        case 0x19: case 0x1A: case 0x1C: case 0x1D:
            m_type = MbcType::MBC5;
            break;
        case 0x1B: case 0x1E:
            m_type = MbcType::MBC5;
            m_hasBattery = true;
            break;
        default:
            m_type = MbcType::MBC1;
            std::cerr << "Unknown cartridge type 0x" << std::hex << (int)cartType
                      << "; using MBC1.\n" << std::dec;
            break;
    }

    uint32_t ramSize = 0;
    if (m_type == MbcType::MBC2) {
        ramSize = 512;
    } else {
        switch (m_rom[0x0149]) {
            case 0x01: ramSize = 2048; break;
            case 0x02: ramSize = 8192; break;
            case 0x03: ramSize = 32768; break;
            case 0x04: ramSize = 131072; break;
            case 0x05: ramSize = 65536; break;
            default: ramSize = 0; break;
        }
    }
    m_ram.assign(ramSize, 0);

    m_ramEnabled = false;
    m_romBankLower = 1;
    m_romBankUpper = 0;
    m_bankingMode = 0;
    m_romBank = 1;
    m_ramBank = 0;
    m_romBank5 = 1;
    m_ramBank5 = 0;

    m_rtc = {};
    m_rtcLatched = {};
    m_rtcLatchData = 0xFF;
    m_rtcLatchedReady = false;
    m_rtcLastSync = std::time(nullptr);

    std::cout << "Cartridge: \"" << m_title << "\" type=";
    switch (m_type) {
        case MbcType::None: std::cout << "ROM-only"; break;
        case MbcType::MBC1: std::cout << "MBC1"; break;
        case MbcType::MBC2: std::cout << "MBC2"; break;
        case MbcType::MBC3: std::cout << "MBC3"; break;
        case MbcType::MBC5: std::cout << "MBC5"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << " ROM=" << (m_rom.size() / 1024) << "KB"
              << " RAM=" << (m_ram.size() / 1024) << "KB"
              << (m_hasBattery ? " [battery]" : "")
              << (m_hasRtc ? " [RTC]" : "")
              << "\n";
}

uint32_t Cartridge::romBankCount() const {
    uint32_t n = static_cast<uint32_t>(m_rom.size() / 0x4000);
    return n == 0 ? 1 : n;
}

uint32_t Cartridge::ramBankCount() const {
    if (m_ram.empty()) return 0;
    if (m_type == MbcType::MBC2) return 1;
    uint32_t n = static_cast<uint32_t>(m_ram.size() / 0x2000);
    return n == 0 ? 1 : n;
}

uint32_t Cartridge::mapRomAddress(uint16_t address) const {
    const uint32_t banks = romBankCount();

    if (address < 0x4000) {
        uint32_t bank = 0;
        if (m_type == MbcType::MBC1 && m_bankingMode == 1) {
            bank = static_cast<uint32_t>(m_romBankUpper) << 5;
        }
        bank %= banks;
        return bank * 0x4000u + address;
    }

    uint32_t bank = 1;
    switch (m_type) {
        case MbcType::None:
            bank = 1;
            break;
        case MbcType::MBC1: {
            uint8_t lower = (m_romBankLower == 0) ? 1 : m_romBankLower;
            bank = (static_cast<uint32_t>(m_romBankUpper) << 5) | lower;
            break;
        }
        case MbcType::MBC2:
            bank = m_romBank ? m_romBank : 1;
            break;
        case MbcType::MBC3:
            bank = m_romBank ? m_romBank : 1;
            break;
        case MbcType::MBC5:
            bank = m_romBank5;
            break;
        default:
            bank = 1;
            break;
    }
    bank %= banks;
    return bank * 0x4000u + (address - 0x4000u);
}

uint32_t Cartridge::mapRamAddress(uint16_t address) const {
    if (m_ram.empty()) return 0;
    if (m_type == MbcType::MBC2) {
        return (address - 0xA000) & 0x1FF;
    }

    uint32_t bank = 0;
    switch (m_type) {
        case MbcType::MBC1:
            if (m_bankingMode == 1) bank = m_romBankUpper;
            break;
        case MbcType::MBC3:
            bank = (m_ramBank <= 0x03) ? m_ramBank : 0;
            break;
        case MbcType::MBC5:
            bank = m_ramBank5;
            break;
        default:
            bank = 0;
            break;
    }
    const uint32_t banks = ramBankCount();
    if (banks > 0) bank %= banks;
    return bank * 0x2000u + (address - 0xA000u);
}

void Cartridge::advanceRtcSeconds(uint64_t seconds) {
    if (!m_hasRtc) return;
    if ((m_rtc.dh & 0x40) != 0) return;

    for (uint64_t i = 0; i < seconds; ++i) {
        m_rtc.s = static_cast<uint8_t>((m_rtc.s + 1) % 60);
        if (m_rtc.s != 0) continue;
        m_rtc.m = static_cast<uint8_t>((m_rtc.m + 1) % 60);
        if (m_rtc.m != 0) continue;
        m_rtc.h = static_cast<uint8_t>((m_rtc.h + 1) % 24);
        if (m_rtc.h != 0) continue;

        uint16_t days = static_cast<uint16_t>((m_rtc.dl) | ((m_rtc.dh & 0x01) << 8));
        days = static_cast<uint16_t>(days + 1);
        if (days > 511) {
            days = 0;
            m_rtc.dh |= 0x80;
        }
        m_rtc.dl = static_cast<uint8_t>(days & 0xFF);
        m_rtc.dh = static_cast<uint8_t>((m_rtc.dh & 0xFE) | ((days >> 8) & 0x01));
    }
}

void Cartridge::updateRtcWallClock() {
    if (!m_hasRtc) return;
    const std::time_t now = std::time(nullptr);
    if (m_rtcLastSync == 0) {
        m_rtcLastSync = now;
        return;
    }
    if (now > m_rtcLastSync) {
        advanceRtcSeconds(static_cast<uint64_t>(now - m_rtcLastSync));
        m_rtcLastSync = now;
    }
}

void Cartridge::latchRtc() {
    m_rtcLatched = m_rtc;
    m_rtcLatchedReady = true;
}

uint8_t Cartridge::readRtc(uint8_t reg) const {
    const RtcRegs& r = m_rtcLatchedReady ? m_rtcLatched : m_rtc;
    switch (reg) {
        case 0x08: return r.s;
        case 0x09: return r.m;
        case 0x0A: return r.h;
        case 0x0B: return r.dl;
        case 0x0C: return r.dh;
        default: return 0xFF;
    }
}

void Cartridge::writeRtc(uint8_t reg, uint8_t value) {
    switch (reg) {
        case 0x08: m_rtc.s = value & 0x3F; break;
        case 0x09: m_rtc.m = value & 0x3F; break;
        case 0x0A: m_rtc.h = value & 0x1F; break;
        case 0x0B: m_rtc.dl = value; break;
        case 0x0C: m_rtc.dh = value & 0xC1; break;
        default: break;
    }
}

uint8_t Cartridge::read(uint16_t address) const {
    if (address <= 0x7FFF) {
        uint32_t off = mapRomAddress(address);
        if (off < m_rom.size()) return m_rom[off];
        return 0xFF;
    }
    if (address >= 0xA000 && address <= 0xBFFF) {
        if (!m_ramEnabled) return 0xFF;
        if (m_type == MbcType::MBC3 && m_hasRtc && m_ramBank >= 0x08 && m_ramBank <= 0x0C) {
            return readRtc(m_ramBank);
        }
        if (m_ram.empty()) return 0xFF;
        uint32_t off = mapRamAddress(address);
        if (off < m_ram.size()) {
            if (m_type == MbcType::MBC2) return m_ram[off] | 0xF0;
            return m_ram[off];
        }
        return 0xFF;
    }
    return 0xFF;
}

void Cartridge::write(uint16_t address, uint8_t value) {
    if (address <= 0x7FFF) {
        switch (m_type) {
            case MbcType::None:
                break;
            case MbcType::MBC1:
                if (address <= 0x1FFF) {
                    m_ramEnabled = ((value & 0x0F) == 0x0A);
                } else if (address <= 0x3FFF) {
                    m_romBankLower = value & 0x1F;
                } else if (address <= 0x5FFF) {
                    m_romBankUpper = value & 0x03;
                } else {
                    m_bankingMode = value & 0x01;
                }
                break;
            case MbcType::MBC2:
                if (address <= 0x3FFF) {
                    if (address & 0x0100) {
                        m_romBank = value & 0x0F;
                        if (m_romBank == 0) m_romBank = 1;
                    } else {
                        m_ramEnabled = ((value & 0x0F) == 0x0A);
                    }
                }
                break;
            case MbcType::MBC3:
                if (address <= 0x1FFF) {
                    m_ramEnabled = ((value & 0x0F) == 0x0A);
                } else if (address <= 0x3FFF) {
                    m_romBank = value & 0x7F;
                    if (m_romBank == 0) m_romBank = 1;
                } else if (address <= 0x5FFF) {
                    m_ramBank = value;
                } else {
                    if (m_rtcLatchData == 0x00 && value == 0x01) {
                        latchRtc();
                    }
                    m_rtcLatchData = value;
                }
                break;
            case MbcType::MBC5:
                if (address <= 0x1FFF) {
                    m_ramEnabled = ((value & 0x0F) == 0x0A);
                } else if (address <= 0x2FFF) {
                    m_romBank5 = (m_romBank5 & 0x100) | value;
                } else if (address <= 0x3FFF) {
                    m_romBank5 = (m_romBank5 & 0xFF) | (static_cast<uint16_t>(value & 1) << 8);
                } else if (address <= 0x5FFF) {
                    m_ramBank5 = value & 0x0F;
                }
                break;
            default:
                break;
        }
        return;
    }

    if (address >= 0xA000 && address <= 0xBFFF) {
        if (!m_ramEnabled) return;
        if (m_type == MbcType::MBC3 && m_hasRtc && m_ramBank >= 0x08 && m_ramBank <= 0x0C) {
            writeRtc(m_ramBank, value);
            return;
        }
        if (m_ram.empty()) return;
        uint32_t off = mapRamAddress(address);
        if (off < m_ram.size()) {
            if (m_type == MbcType::MBC2) {
                m_ram[off] = value & 0x0F;
            } else {
                m_ram[off] = value;
            }
        }
    }
}

std::string Cartridge::defaultSavePath() const {
    if (m_romPath.empty()) return "save.sav";
    std::string p = m_romPath;
    auto dot = p.find_last_of('.');
    if (dot != std::string::npos) {
        p = p.substr(0, dot);
    }
    return p + ".sav";
}

bool Cartridge::saveBattery(const std::string& path) const {
    if (!m_hasBattery) return false;
    if (m_ram.empty() && !m_hasRtc) return false;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    if (!m_ram.empty()) {
        f.write(reinterpret_cast<const char*>(m_ram.data()),
                static_cast<std::streamsize>(m_ram.size()));
    }

    if (m_hasRtc) {
        uint8_t rtcBlob[48]{};
        rtcBlob[0] = m_rtc.s;
        rtcBlob[1] = m_rtc.m;
        rtcBlob[2] = m_rtc.h;
        rtcBlob[3] = m_rtc.dl;
        rtcBlob[4] = m_rtc.dh;
        rtcBlob[5] = m_rtcLatched.s;
        rtcBlob[6] = m_rtcLatched.m;
        rtcBlob[7] = m_rtcLatched.h;
        rtcBlob[8] = m_rtcLatched.dl;
        rtcBlob[9] = m_rtcLatched.dh;
        const uint64_t ts = static_cast<uint64_t>(m_rtcLastSync);
        for (int i = 0; i < 8; ++i) {
            rtcBlob[16 + i] = static_cast<uint8_t>((ts >> (i * 8)) & 0xFF);
        }
        f.write(reinterpret_cast<const char*>(rtcBlob), 48);
    }
    return static_cast<bool>(f);
}

bool Cartridge::loadBattery(const std::string& path) {
    if (!m_hasBattery) return false;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto fileSize = static_cast<size_t>(f.tellg());
    f.seekg(0);

    size_t ramBytes = m_ram.size();
    if (ramBytes > 0) {
        if (fileSize < ramBytes) return false;
        f.read(reinterpret_cast<char*>(m_ram.data()), static_cast<std::streamsize>(ramBytes));
    }

    if (m_hasRtc) {
        const size_t rtcOff = ramBytes;
        if (fileSize >= rtcOff + 48) {
            uint8_t rtcBlob[48]{};
            f.read(reinterpret_cast<char*>(rtcBlob), 48);
            m_rtc.s = rtcBlob[0];
            m_rtc.m = rtcBlob[1];
            m_rtc.h = rtcBlob[2];
            m_rtc.dl = rtcBlob[3];
            m_rtc.dh = rtcBlob[4];
            m_rtcLatched.s = rtcBlob[5];
            m_rtcLatched.m = rtcBlob[6];
            m_rtcLatched.h = rtcBlob[7];
            m_rtcLatched.dl = rtcBlob[8];
            m_rtcLatched.dh = rtcBlob[9];
            uint64_t ts = 0;
            for (int i = 0; i < 8; ++i) {
                ts |= static_cast<uint64_t>(rtcBlob[16 + i]) << (i * 8);
            }
            m_rtcLastSync = static_cast<std::time_t>(ts);
            if (m_rtcLastSync == 0) m_rtcLastSync = std::time(nullptr);
            updateRtcWallClock();
        } else {
            m_rtcLastSync = std::time(nullptr);
        }
    }
    return true;
}
