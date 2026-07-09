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

    // Método para atualizar o registrador divisor DIV
    void tickDIV(uint8_t dots);

    // Método para atualizar o estado físico do Joypad (a partir do teclado do emulador)
    void setJoypadState(uint8_t directionState, uint8_t actionState);

    // Carrega a ROM do jogo para memória
    bool loadROM(const std::vector<uint8_t>& romData);

private:
    // Contador interno do registrador divisor DIV
    uint16_t m_divCounter;
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

    // --- Estado do Controle Joypad ---
    uint8_t m_joypadSelect;     // Bits 4 e 5 escritos pela CPU em 0xFF00 (0x30 por padrão)
    uint8_t m_joypadDirections; // Estado ativo em nível baixo (low) dos direcionais (0xF = solto)
    uint8_t m_joypadActions;    // Estado ativo em nível baixo (low) das ações (0xF = solto)
};
