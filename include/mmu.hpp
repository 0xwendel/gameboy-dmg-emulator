#pragma once

#include <cstdint>
#include <vector>

class MMU {
public:
    MMU();
    ~MMU() = default;

    // Métodos fundamentais para leitura e escrita no barramento de 16-bits
    uint8_t readByte(uint16_t address) const;
    void writeByte(uint16_t address, uint8_t value);

    // Métodos para a PPU atualizar o estado interno dos registradores de E/S
    void setLY(uint8_t ly);
    void setLCDMode(uint8_t mode);

    // Carrega a ROM do jogo para memória
    bool loadROM(const std::vector<uint8_t>& romData);

private:
    // ROM do Cartucho (tamanho dinâmico dependendo da ROM)
    std::vector<uint8_t> m_cartROM;

    // RAM Externa do Cartucho (se houver, gerenciada por MBC)
    std::vector<uint8_t> m_cartRAM;

    // Memória interna do Game Boy
    uint8_t m_vram[0x2000];   // 8KB Video RAM (0x8000 - 0x9FFF)
    uint8_t m_wram[0x2000];   // 8KB Work RAM (0xC000 - 0xDFFF)
    uint8_t m_oam[0xA0];      // 160 bytes Sprite Attribute Table (0xFE00 - 0xFE9F)
    uint8_t m_io[0x80];       // 128 bytes I/O Registers (0xFF00 - 0xFF7F)
    uint8_t m_hram[0x7F];     // 127 bytes High RAM (0xFF80 - 0xFFFE)
    uint8_t m_ie;             // 1 byte Interrupt Enable Register (0xFFFF)

    // Flag para indicar se a Boot ROM (BIOS) interna está ativa
    bool m_bootRomActive;

    // --- Estado do Mapeador de Cartucho MBC1 ---
    bool m_ramEnabled;        // RAM habilitada quando escrito 0x0A em 0x0000-0x1FFF
    uint8_t m_romBankLower;   // Bits 0-4 do banco de ROM (escrito em 0x2000-0x3FFF)
    uint8_t m_romBankUpper;   // Bits 5-6 do banco de ROM / Banco de RAM (escrito em 0x4000-0x5FFF)
    uint8_t m_bankingMode;    // Modo de mapeamento: 0 = ROM (padrão), 1 = RAM (escrito em 0x6000-0x7FFF)
};
