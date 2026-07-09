#include "ppu.hpp"
#include <iostream>

PPU::PPU() : m_scanlineTicks(0), m_ly(0), m_mode(ModeOAMSearch), m_frameReady(false) {
    // Inicializa o buffer com a cor mais clara da paleta
    for (int i = 0; i < 160 * 144; ++i) {
        m_frameBuffer[i] = PALETTE[0];
    }
}

bool PPU::isFrameReady() {
    if (m_frameReady) {
        m_frameReady = false;
        return true;
    }
    return false;
}

void PPU::generateTestPattern() {
    // Offset estático que incrementa a cada quadro para simular movimento na tela
    static uint8_t frameCount = 0;
    frameCount++;

    for (int y = 0; y < 144; ++y) {
        for (int x = 0; x < 160; ++x) {
            // Padrão 1: Um xadrez em movimento nos tons clássicos do Game Boy
            bool chess = (((x + frameCount) / 16) % 2) == (((y + frameCount) / 16) % 2);
            
            // Padrão 2: Adiciona uma borda escura ao redor do viewport para simular a moldura
            if (x < 2 || x >= 158 || y < 2 || y >= 142) {
                m_frameBuffer[y * 160 + x] = PALETTE[3]; // Moldura Preta
            } else if (chess) {
                m_frameBuffer[y * 160 + x] = PALETTE[0]; // Off-White
            } else {
                m_frameBuffer[y * 160 + x] = PALETTE[2]; // Cinza escuro
            }
        }
    }
}

void PPU::tick(uint8_t mCycles, MMU& mmu) {
    // 1 M-cycle da CPU equivale a 4 dots (ticks) da PPU
    m_scanlineTicks += mCycles * 4;

    Mode oldMode = m_mode;

    if (m_ly < 144) {
        // Varredura visível das scanlines 0 a 143
        if (m_scanlineTicks < 80) {
            m_mode = ModeOAMSearch;
        } else if (m_scanlineTicks < 252) {
            m_mode = ModePixelTransfer;
        } else {
            m_mode = ModeHBlank;
        }

        // Se acabamos de entrar em H-Blank, renderiza a scanline atual
        if (oldMode == ModePixelTransfer && m_mode == ModeHBlank) {
            renderScanline(mmu);
        }

        // Fim de uma linha horizontal (456 dots por scanline)
        if (m_scanlineTicks >= 456) {
            m_scanlineTicks -= 456;
            m_ly++;
            mmu.setLY(m_ly);

            if (m_ly == 144) {
                // Entra no modo V-Blank
                m_mode = ModeVBlank;
                m_frameReady = true;
                // Mantém o padrão de teste apenas se a tela estiver desativada,
                // caso contrário renderiza VRAM real.
                uint8_t lcdc = mmu.readByte(0xFF40);
                if (!(lcdc & 0x80)) {
                    generateTestPattern();
                }
            } else {
                m_mode = ModeOAMSearch;
            }
        }
    } else {
        // Período de V-Blank (scanlines 144 a 153)
        m_mode = ModeVBlank;

        // Fim de uma linha em V-Blank
        if (m_scanlineTicks >= 456) {
            m_scanlineTicks -= 456;
            m_ly++;

            if (m_ly == 154) {
                // Reinicia o frame de volta ao topo da tela
                m_ly = 0;
                m_mode = ModeOAMSearch;
            }
            mmu.setLY(m_ly);
        }
    }

    // Sincroniza o modo atual da PPU com o registrador de status (STAT) na MMU
    mmu.setLCDMode(static_cast<uint8_t>(m_mode));
}

void PPU::renderScanline(MMU& mmu) {
    uint8_t lcdc = mmu.readByte(0xFF40);
    // Se o LCD/Tela estiver desativado (Bit 7), não desenha nada
    if (!(lcdc & 0x80)) {
        return;
    }

    // Renderiza a camada de Background se ativada (Bit 0)
    if (lcdc & 0x01) {
        renderBG(mmu);
    }

    // Renderiza a camada de Window se ativada (Bit 5)
    if (lcdc & 0x20) {
        renderWindow(mmu);
    }
}

void PPU::renderBG(MMU& mmu) {
    uint8_t lcdc = mmu.readByte(0xFF40);
    uint8_t scy = mmu.readByte(0xFF42);
    uint8_t scx = mmu.readByte(0xFF43);
    uint8_t bgp = mmu.readByte(0xFF47);

    // Determina qual mapa de blocos usar para o fundo (Bit 3: 0 = 0x9800, 1 = 0x9C00)
    uint16_t mapBase = (lcdc & 0x08) ? 0x9C00 : 0x9800;

    // Determina o modo de endereçamento de dados do tile (Bit 4: 0 = 0x8800 signed, 1 = 0x8000 unsigned)
    bool isSigned = !(lcdc & 0x10);

    // Linha virtual do pixel na tela de 256x256 do Background
    uint8_t bgY = (m_ly + scy) & 0xFF;
    uint8_t tileRow = bgY / 8;
    uint8_t pixelRow = bgY % 8; // Linha de pixels interna do tile (0-7)

    for (int x = 0; x < 160; ++x) {
        uint8_t bgX = (x + scx) & 0xFF;
        uint8_t tileCol = bgX / 8;
        uint8_t pixelCol = bgX % 8; // Coluna de pixels interna do tile (0-7)

        // Lê o índice do tile na tabela de mapas de tela
        uint16_t mapOffset = tileRow * 32 + tileCol;
        uint8_t tileIndex = mmu.readByte(mapBase + mapOffset);

        // Encontra o endereço base do Tile na VRAM
        uint16_t tileAddress;
        if (isSigned) {
            int8_t signedIndex = static_cast<int8_t>(tileIndex);
            tileAddress = 0x9000 + (signedIndex * 16);
        } else {
            tileAddress = 0x8000 + (tileIndex * 16);
        }

        // Cada linha do tile ocupa 2 bytes consecutivos de dados
        uint8_t byteA = mmu.readByte(tileAddress + pixelRow * 2);
        uint8_t byteB = mmu.readByte(tileAddress + pixelRow * 2 + 1);

        // Extrai a cor correspondente usando a codificação 2bpp planar
        uint8_t bitShift = 7 - pixelCol;
        uint8_t lowBit = (byteA >> bitShift) & 1;
        uint8_t highBit = (byteB >> bitShift) & 1;
        uint8_t colorIndex = (highBit << 1) | lowBit;

        // Mapeia a cor através da paleta BGP
        uint8_t paletteShade = (bgp >> (colorIndex * 2)) & 0x03;

        m_frameBuffer[m_ly * 160 + x] = PALETTE[paletteShade];
    }
}

void PPU::renderWindow(MMU& mmu) {
    uint8_t lcdc = mmu.readByte(0xFF40);
    uint8_t wy = mmu.readByte(0xFF4A);
    uint8_t wx = mmu.readByte(0xFF4B);
    uint8_t bgp = mmu.readByte(0xFF47);

    // Condições físicas de exibição da Window
    if (m_ly < wy || wx > 166) {
        return;
    }

    // Determina qual mapa usar para a janela (Bit 6: 0 = 0x9800, 1 = 0x9C00)
    uint16_t mapBase = (lcdc & 0x40) ? 0x9C00 : 0x9800;

    // Determina o modo de endereçamento (Bit 4)
    bool isSigned = !(lcdc & 0x10);

    uint8_t winY = m_ly - wy;
    uint8_t tileRow = winY / 8;
    uint8_t pixelRow = winY % 8;

    for (int x = 0; x < 160; ++x) {
        // A janela só aparece à direita de WX - 7
        if (x < (wx - 7)) {
            continue;
        }

        uint8_t winX = x - (wx - 7);
        uint8_t tileCol = winX / 8;
        uint8_t pixelCol = winX % 8;

        uint16_t mapOffset = tileRow * 32 + tileCol;
        uint8_t tileIndex = mmu.readByte(mapBase + mapOffset);

        uint16_t tileAddress;
        if (isSigned) {
            int8_t signedIndex = static_cast<int8_t>(tileIndex);
            tileAddress = 0x9000 + (signedIndex * 16);
        } else {
            tileAddress = 0x8000 + (tileIndex * 16);
        }

        uint8_t byteA = mmu.readByte(tileAddress + pixelRow * 2);
        uint8_t byteB = mmu.readByte(tileAddress + pixelRow * 2 + 1);

        uint8_t bitShift = 7 - pixelCol;
        uint8_t lowBit = (byteA >> bitShift) & 1;
        uint8_t highBit = (byteB >> bitShift) & 1;
        uint8_t colorIndex = (highBit << 1) | lowBit;

        uint8_t paletteShade = (bgp >> (colorIndex * 2)) & 0x03;

        m_frameBuffer[m_ly * 160 + x] = PALETTE[paletteShade];
    }
}
