#pragma once

#include "mmu.hpp"
#include <cstdint>

class PPU {
public:
    enum Mode : uint8_t {
        ModeHBlank = 0,
        ModeVBlank = 1,
        ModeOAMSearch = 2,
        ModePixelTransfer = 3
    };

    PPU();
    ~PPU() = default;

    // Avança o estado da PPU com base nos M-cycles executados pela CPU
    void tick(uint8_t mCycles, MMU& mmu);

    // Retorna se um novo frame foi concluído (e limpa a flag)
    bool isFrameReady();

    // Retorna o ponteiro para o buffer de pixels 160x144 (RGBA8888)
    const uint32_t* getFrameBuffer() const { return m_frameBuffer; }

private:
    // Buffer de tela: 160x144 pixels de 32-bits (RGBA)
    uint32_t m_frameBuffer[160 * 144];

    // Contadores de ciclo internos (medidos em dots/ticks de clock)
    uint32_t m_scanlineTicks; 

    // Estado da PPU
    uint8_t m_ly;       // Linha atual (0-153)
    Mode m_mode;        // Modo de operação (0-3)
    bool m_frameReady;  // Indica se a tela inteira foi varrida

    // Função auxiliar para gerar cores no formato RGBA8888 (Little-Endian)
    static constexpr uint32_t makeRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(g) << 8)  |
               r;
    }

    // Paleta de cores do Game Boy Clássico (tons de verde)
    const uint32_t PALETTE[4] = {
        makeRGBA(0xE0, 0xF8, 0xD0), // 0: Off-White (Mais claro)
        makeRGBA(0x88, 0xC0, 0x70), // 1: Cinza esverdeado claro
        makeRGBA(0x34, 0x68, 0x56), // 2: Cinza esverdeado escuro
        makeRGBA(0x08, 0x18, 0x20)  // 3: Preto esverdeado (Mais escuro)
    };

    // Gera um padrão visual estático para testar o loop de renderização do Raylib
    void generateTestPattern();

    // Métodos internos de renderização de scanlines
    void renderScanline(MMU& mmu);
    void renderBG(MMU& mmu);
    void renderWindow(MMU& mmu);
};
