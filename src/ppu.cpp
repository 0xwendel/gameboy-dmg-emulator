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

    if (m_ly < 144) {
        // Varredura visível das scanlines 0 a 143
        if (m_scanlineTicks < 80) {
            m_mode = ModeOAMSearch;
        } else if (m_scanlineTicks < 252) {
            m_mode = ModePixelTransfer;
        } else {
            m_mode = ModeHBlank;
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
                generateTestPattern(); // Atualiza a tela de teste móvel
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
