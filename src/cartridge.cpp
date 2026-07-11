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
        std::cerr << "ROM muito pequena (header incompleto).\n";
        return false;
    }
    m_rom = romData;
    // Garante tamanho mínimo de 32KB para mapeamento
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
            // Fallback: tenta MBC1 (mais comum em homebrew/antigos)
            m_type = MbcType::MBC1;
            std::cerr << "Tipo de cartucho 0x" << std::hex << (int)cartType
                      << " desconhecido; usando MBC1.\n" << std::dec;
            break;
    }

    uint32_t ramSize = 0;
    if (m_type == MbcType::MBC2) {
        ramSize = 512; // 512 x 4-bit, armazenamos em 512 bytes
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

    std::cout << "Cartucho: \"" << m_title << "\" tipo=";
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
            // Bancos 0x08-0x0C são RTC; tratamos como RAM bank 0 por simplicidade
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

uint8_t Cartridge::read(uint16_t address) const {
    if (address <= 0x7FFF) {
        uint32_t off = mapRomAddress(address);
        if (off < m_rom.size()) return m_rom[off];
        return 0xFF;
    }
    if (address >= 0xA000 && address <= 0xBFFF) {
        if (!m_ramEnabled || m_ram.empty()) return 0xFF;
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
                }
                // 0x6000-0x7FFF: latch RTC (ignorado se sem RTC completo)
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
        if (!m_ramEnabled || m_ram.empty()) return;
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
    if (!m_hasBattery || m_ram.empty()) return false;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(m_ram.data()), static_cast<std::streamsize>(m_ram.size()));
    return static_cast<bool>(f);
}

bool Cartridge::loadBattery(const std::string& path) {
    if (!m_hasBattery || m_ram.empty()) return false;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char*>(m_ram.data()), static_cast<std::streamsize>(m_ram.size()));
    return true;
}
