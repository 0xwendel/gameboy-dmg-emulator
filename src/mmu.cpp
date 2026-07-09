#include "mmu.hpp"
#include <cstring>
#include <iostream>

MMU::MMU() : m_ie(0), m_bootRomActive(true), m_ramEnabled(false), m_romBankLower(1), m_romBankUpper(0), m_bankingMode(0), m_divCounter(0) {
    // Inicializa a memória interna com zero
    std::memset(m_vram, 0, sizeof(m_vram));
    std::memset(m_wram, 0, sizeof(m_wram));
    std::memset(m_oam, 0, sizeof(m_oam));
    std::memset(m_io, 0, sizeof(m_io));
    std::memset(m_hram, 0, sizeof(m_hram));
}

bool MMU::loadROM(const std::vector<uint8_t>& romData) {
    m_cartROM = romData;
    
    // Reseta estado do MBC1
    m_ramEnabled = false;
    m_romBankLower = 1;
    m_romBankUpper = 0;
    m_bankingMode = 0;
    
    // Determina o tamanho da RAM do Cartucho baseado no Header (endereço 0x0149)
    uint32_t ramSize = 0;
    if (m_cartROM.size() > 0x0149) {
        uint8_t ramCode = m_cartROM[0x0149];
        switch (ramCode) {
            case 0x01: ramSize = 2048; break;    // 2KB
            case 0x02: ramSize = 8192; break;    // 8KB (1 banco)
            case 0x03: ramSize = 32768; break;   // 32KB (4 bancos de 8KB)
            case 0x04: ramSize = 131072; break;  // 128KB (16 bancos de 8KB)
            case 0x05: ramSize = 65536; break;   // 64KB (8 bancos de 8KB)
            default: ramSize = 0; break;
        }
    }
    m_cartRAM.assign(ramSize, 0);
    return true;
}

void MMU::setLY(uint8_t ly) {
    m_io[0x44] = ly;
}

void MMU::setLCDMode(uint8_t mode) {
    m_io[0x41] = (m_io[0x41] & 0xFC) | (mode & 0x03);
}

void MMU::tickDIV(uint8_t dots) {
    m_divCounter += dots;
    m_io[0x04] = m_divCounter >> 8;
}

uint8_t MMU::readByte(uint16_t address) const {
    // 1. ROM do Cartucho (e BIOS se ativa)
    if (address <= 0x7FFF) {
        if (m_bootRomActive && address < 0x0100) {
            return 0; // Retorna byte da boot ROM (a ser implementado)
        }
        
        uint32_t numRomBanks = m_cartROM.size() / 0x4000;
        if (numRomBanks == 0) numRomBanks = 1;

        if (address < 0x4000) {
            // ROM Bank 0 Window: Mapeia para o banco 0 (ou bancos 0x00/0x20/0x40/0x60 no Modo 1 de RAM)
            uint32_t bank = 0;
            if (m_bankingMode == 1) {
                bank = (m_romBankUpper << 5);
            }
            bank = bank % numRomBanks;
            uint32_t targetAddress = (bank * 0x4000) + address;
            if (targetAddress < m_cartROM.size()) {
                return m_cartROM[targetAddress];
            }
            return 0xFF;
        } else {
            // ROM Bank 1 Window: Mapeia de acordo com m_romBankLower e m_romBankUpper
            uint8_t lower = (m_romBankLower == 0) ? 1 : m_romBankLower;
            uint32_t bank = (m_romBankUpper << 5) | lower;
            bank = bank % numRomBanks;
            uint32_t targetAddress = (bank * 0x4000) + (address - 0x4000);
            if (targetAddress < m_cartROM.size()) {
                return m_cartROM[targetAddress];
            }
            return 0xFF;
        }
    }
    
    // 2. Video RAM (VRAM)
    if (address >= 0x8000 && address <= 0x9FFF) {
        return m_vram[address - 0x8000];
    }
    
    // 3. External RAM (SRAM) no cartucho
    if (address >= 0xA000 && address <= 0xBFFF) {
        if (m_ramEnabled && !m_cartRAM.empty()) {
            uint32_t numRamBanks = m_cartRAM.size() / 0x2000;
            if (numRamBanks == 0) numRamBanks = 1;
            
            uint32_t bank = 0;
            if (m_bankingMode == 1) {
                bank = m_romBankUpper;
            }
            bank = bank % numRamBanks;
            uint32_t ramAddress = (bank * 0x2000) + (address - 0xA000);
            if (ramAddress < m_cartRAM.size()) {
                return m_cartRAM[ramAddress];
            }
        }
        return 0xFF; // Retorna 0xFF se RAM desativada
    }
    
    // 4. Work RAM (WRAM)
    if (address >= 0xC000 && address <= 0xDFFF) {
        return m_wram[address - 0xC000];
    }
    
    // 5. Echo RAM (Espelho de 0xC000 - 0xDDFF)
    if (address >= 0xE000 && address <= 0xFDFF) {
        return m_wram[address - 0xE000];
    }
    
    // 6. Object Attribute Memory (OAM)
    if (address >= 0xFE00 && address <= 0xFE9F) {
        return m_oam[address - 0xFE00];
    }
    
    // 7. Área Inutilizável (Normalmente retorna 0xFF)
    if (address >= 0xFEA0 && address <= 0xFEFF) {
        return 0xFF;
    }
    
    // 8. I/O Registers
    if (address >= 0xFF00 && address <= 0xFF7F) {
        return m_io[address - 0xFF00];
    }
    
    // 9. High RAM (HRAM)
    if (address >= 0xFF80 && address <= 0xFFFE) {
        return m_hram[address - 0xFF80];
    }
    
    // 10. Interrupt Enable Register (IE)
    if (address == 0xFFFF) {
        return m_ie;
    }
    
    return 0xFF;
}

void MMU::writeByte(uint16_t address, uint8_t value) {
    // 1. ROM / MBC do Cartucho (escritas controlam troca de bancos de ROM/RAM)
    if (address <= 0x7FFF) {
        if (address <= 0x1FFF) {
            // Habilita/Desabilita SRAM externa
            m_ramEnabled = ((value & 0x0F) == 0x0A);
        } else if (address >= 0x2000 && address <= 0x3FFF) {
            // Banco de ROM (lower 5 bits)
            m_romBankLower = value & 0x1F;
        } else if (address >= 0x4000 && address <= 0x5FFF) {
            // Banco de RAM / Banco de ROM (upper 2 bits)
            m_romBankUpper = value & 0x03;
        } else if (address >= 0x6000 && address <= 0x7FFF) {
            // Modo de mapeamento
            m_bankingMode = value & 0x01;
        }
        return;
    }
    
    // 2. Video RAM (VRAM)
    if (address >= 0x8000 && address <= 0x9FFF) {
        m_vram[address - 0x8000] = value;
        return;
    }
    
    // 3. External RAM (SRAM) no cartucho
    if (address >= 0xA000 && address <= 0xBFFF) {
        if (m_ramEnabled && !m_cartRAM.empty()) {
            uint32_t numRamBanks = m_cartRAM.size() / 0x2000;
            if (numRamBanks == 0) numRamBanks = 1;
            
            uint32_t bank = 0;
            if (m_bankingMode == 1) {
                bank = m_romBankUpper;
            }
            bank = bank % numRamBanks;
            uint32_t ramAddress = (bank * 0x2000) + (address - 0xA000);
            if (ramAddress < m_cartRAM.size()) {
                m_cartRAM[ramAddress] = value;
            }
        }
        return;
    }
    
    // 4. Work RAM (WRAM)
    if (address >= 0xC000 && address <= 0xDFFF) {
        m_wram[address - 0xC000] = value;
        return;
    }
    
    // 5. Echo RAM (Espelho de WRAM)
    if (address >= 0xE000 && address <= 0xFDFF) {
        m_wram[address - 0xE000] = value;
        return;
    }
    
    // 6. Object Attribute Memory (OAM)
    if (address >= 0xFE00 && address <= 0xFE9F) {
        m_oam[address - 0xFE00] = value;
        return;
    }
    
    // 7. Área Inutilizável
    if (address >= 0xFEA0 && address <= 0xFEFF) {
        return;
    }
    
    // 8. I/O Registers
    if (address >= 0xFF00 && address <= 0xFF7F) {
        if (address == 0xFF04) {
            m_divCounter = 0;
            m_io[0x04] = 0;
            return;
        }
        // Registrador especial para desabilitar a BIOS
        if (address == 0xFF50 && value != 0) {
            m_bootRomActive = false;
        }
        m_io[address - 0xFF00] = value;
        return;
    }
    
    // 9. High RAM (HRAM)
    if (address >= 0xFF80 && address <= 0xFFFE) {
        m_hram[address - 0xFF80] = value;
        return;
    }
    
    // 10. Interrupt Enable Register (IE)
    if (address == 0xFFFF) {
        m_ie = value;
        return;
    }
}
