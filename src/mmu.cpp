#include "mmu.hpp"
#include <cstring>
#include <iostream>

MMU::MMU() : m_ie(0), m_bootRomActive(true) {
    // Inicializa a memória interna com zero
    std::memset(m_vram, 0, sizeof(m_vram));
    std::memset(m_wram, 0, sizeof(m_wram));
    std::memset(m_oam, 0, sizeof(m_oam));
    std::memset(m_io, 0, sizeof(m_io));
    std::memset(m_hram, 0, sizeof(m_hram));
}

bool MMU::loadROM(const std::vector<uint8_t>& romData) {
    m_cartROM = romData;
    // Opcional: Alocar espaço para RAM externa se o cartucho exigir
    m_cartRAM.resize(0x2000, 0); // Exemplo de tamanho padrão (8KB)
    return true;
}

void MMU::setLY(uint8_t ly) {
    m_io[0x44] = ly;
}

void MMU::setLCDMode(uint8_t mode) {
    m_io[0x41] = (m_io[0x41] & 0xFC) | (mode & 0x03);
}

uint8_t MMU::readByte(uint16_t address) const {
    // 1. ROM do Cartucho (e BIOS se ativa)
    if (address <= 0x7FFF) {
        // TODO: Mapeamento de BIOS de 256 bytes (0x0000 - 0x00FF)
        if (m_bootRomActive && address < 0x0100) {
            // Retorna byte da boot ROM (a ser implementado)
            return 0; 
        }
        if (address < m_cartROM.size()) {
            return m_cartROM[address];
        }
        return 0xFF; // Retorno padrão para ROM vazia/fora de limites
    }
    
    // 2. Video RAM (VRAM)
    if (address >= 0x8000 && address <= 0x9FFF) {
        return m_vram[address - 0x8000];
    }
    
    // 3. External RAM (SRAM) no cartucho
    if (address >= 0xA000 && address <= 0xBFFF) {
        uint16_t offset = address - 0xA000;
        if (offset < m_cartRAM.size()) {
            return m_cartRAM[offset];
        }
        return 0xFF;
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
    
    // 7. Área Inutilizável (Normalmente retorna 0 ou comportamento indefinido)
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
        // TODO: Tratar comandos MBC (Memory Bank Controller)
        return;
    }
    
    // 2. Video RAM (VRAM)
    if (address >= 0x8000 && address <= 0x9FFF) {
        m_vram[address - 0x8000] = value;
        return;
    }
    
    // 3. External RAM (SRAM) no cartucho
    if (address >= 0xA000 && address <= 0xBFFF) {
        uint16_t offset = address - 0xA000;
        if (offset < m_cartRAM.size()) {
            m_cartRAM[offset] = value;
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
