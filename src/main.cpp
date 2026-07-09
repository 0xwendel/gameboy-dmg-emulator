#include "mmu.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "raylib.h"
#include <iostream>
#include <vector>
#include <cassert>

void testMBC1() {
    std::cout << "Executando testes unitarios do MBC1..." << std::endl;

    MMU mmu;
    std::vector<uint8_t> mockROM(64 * 1024, 0x00);

    for (size_t i = 0x0000; i < 0x4000; ++i) mockROM[i] = 0x00;
    for (size_t i = 0x4000; i < 0x8000; ++i) mockROM[i] = 0x11;
    for (size_t i = 0x8000; i < 0xC000; ++i) mockROM[i] = 0x22;
    for (size_t i = 0xC000; i < 0x10000; ++i) mockROM[i] = 0x33;

    mockROM[0x0149] = 0x02; 

    bool loaded = mmu.loadROM(mockROM);
    assert(loaded == true);

    mmu.writeByte(0xFF50, 1);

    assert(mmu.readByte(0x4000) == 0x11);
    std::cout << "[Teste 1] Banco inicial padrao (Banco 1) ok." << std::endl;

    mmu.writeByte(0x2000, 2);
    assert(mmu.readByte(0x4000) == 0x22);
    std::cout << "[Teste 2] Chaveamento de banco (Banco 2) ok." << std::endl;

    mmu.writeByte(0x2000, 3);
    assert(mmu.readByte(0x4000) == 0x33);
    std::cout << "[Teste 3] Chaveamento de banco (Banco 3) ok." << std::endl;

    mmu.writeByte(0x2000, 0);
    assert(mmu.readByte(0x4000) == 0x11);
    std::cout << "[Teste 4] Escrita de banco 0 corrigida para banco 1 ok." << std::endl;

    assert(mmu.readByte(0xA000) == 0xFF); 

    mmu.writeByte(0x0000, 0x0A);
    mmu.writeByte(0xA000, 0x77);
    assert(mmu.readByte(0xA000) == 0x77);
    std::cout << "[Teste 5] Escrita e leitura na RAM externa (ativada) ok." << std::endl;

    mmu.writeByte(0x0000, 0x00);
    assert(mmu.readByte(0xA000) == 0xFF);
    std::cout << "[Teste 6] RAM desativada retorna 0xFF ok." << std::endl;

    std::cout << "Todos os testes unitarios do MBC1 passaram!\n" << std::endl;
}

int main() {
    testMBC1();

    std::cout << "Inicializando Emulador de Game Boy (Fase 3.1: Renderizacao Real de Tiles da VRAM)..." << std::endl;
    
    MMU mmu;
    CPU cpu;
    PPU ppu;
    
    // Prepara uma ROM com um loop infinito básico (NOP + JR -3) para manter a CPU rodando
    std::vector<uint8_t> testROM(0x200, 0x00);
    testROM[0x0100] = 0x00;
    testROM[0x0101] = 0x18;
    testROM[0x0102] = 0xFD;
    
    if (!mmu.loadROM(testROM)) {
        std::cerr << "Falha ao carregar a ROM de teste!" << std::endl;
        return 1;
    }
    
    // Desabilita a Boot ROM
    mmu.writeByte(0xFF50, 1);
    
    // Configura os registradores da PPU na MMU
    // LCDC (0xFF40) = 0x81 (LCD ligado, Background ligado, Tile Data em 0x8000 unsigned)
    mmu.writeByte(0xFF40, 0x81);
    // BGP (0xFF47) = 0xE4 (Paleta padrão: 11 10 01 00)
    mmu.writeByte(0xFF47, 0xE4);
    
    // Carrega dados de Tiles reais na VRAM (faixa 0x8000 - 0x9FFF)
    // Vamos desenhar um Tile 0 (uma carinha sorridente retro) e Tile 1 (um padrão preenchido escuro)
    
    // Tile 0: Carinha sorridente (16 bytes)
    // Cada linha leva 2 bytes. Byte B = 0x00 (cor com bit alto zerado), Byte A = padrão de bits
    uint8_t smileyTile[16] = {
        0x3C, 0x00, // Linha 0 (00111100)
        0x42, 0x00, // Linha 1 (01000010)
        0xA5, 0x00, // Linha 2 (10100101) - Olhos
        0x81, 0x00, // Linha 3 (10000001)
        0xA5, 0x00, // Linha 4 (10100101) - Nariz/Boca
        0x99, 0x00, // Linha 5 (10011001) - Sorriso
        0x42, 0x00, // Linha 6 (01000010)
        0x3C, 0x00  // Linha 7 (00111100)
    };
    
    // Tile 1: Padrão preenchido escuro
    uint8_t darkTile[16] = {
        0xAA, 0xFF, // Mistura de cores 2 e 3
        0x55, 0xFF,
        0xAA, 0xFF,
        0x55, 0xFF,
        0xAA, 0xFF,
        0x55, 0xFF,
        0xAA, 0xFF,
        0x55, 0xFF
    };
    
    // Escreve os Tiles na VRAM (Tile 0 em 0x8000, Tile 1 em 0x8010)
    for (int i = 0; i < 16; ++i) {
        mmu.writeByte(0x8000 + i, smileyTile[i]);
        mmu.writeByte(0x8010 + i, darkTile[i]);
    }
    
    // Preenche o mapa de tela do Fundo (Background Tile Map 0 em 0x9800-0x9BFF)
    // Criaremos um tabuleiro xadrez alternando Tile 0 e Tile 1
    for (int i = 0; i < 1024; ++i) {
        uint8_t tileIdx = ((i % 2) == ((i / 32) % 2)) ? 0 : 1;
        mmu.writeByte(0x9800 + i, tileIdx);
    }
    
    // Configurações do Viewport do Raylib
    const int SCREEN_WIDTH = 160;
    const int SCREEN_HEIGHT = 144;
    const int SCALE = 4;
    
    InitWindow(SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE, "Game Boy DMG-01 Emulator - Real VRAM Tiles");
    SetTargetFPS(60);
    
    Image emptyImage = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
    Texture2D screenTexture = LoadTextureFromImage(emptyImage);
    UnloadImage(emptyImage);
    
    uint8_t scx = 0;
    uint8_t scy = 0;
    
    std::cout << "Janela do emulador aberta. Rodando loop principal de renderizacao de VRAM..." << std::endl;
    
    // Loop principal da janela gráfica
    while (!WindowShouldClose()) {
        // Incrementa rolagem de tela para simular movimento e verificar funcionalidade de rolagem (SCX/SCY)
        scx++;
        scy++;
        mmu.writeByte(0xFF43, scx); // SCX
        mmu.writeByte(0xFF42, scy); // SCY
        
        // Roda a CPU e avança a PPU até concluir a varredura do frame (154 scanlines)
        while (!ppu.isFrameReady()) {
            uint8_t cycles = cpu.step(mmu);
            ppu.tick(cycles, mmu);
        }
        
        // Atualiza textura da Raylib com a tela da PPU renderizada a partir da VRAM real
        UpdateTexture(screenTexture, ppu.getFrameBuffer());
        
        BeginDrawing();
        ClearBackground(BLACK);
        
        // Desenha a textura original escalada 4x no centro da tela
        DrawTexturePro(
            screenTexture,
            Rectangle{0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT)},
            Rectangle{0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH * SCALE), static_cast<float>(SCREEN_HEIGHT * SCALE)},
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE
        );
        
        DrawFPS(10, 10);
        
        EndDrawing();
    }
    
    UnloadTexture(screenTexture);
    CloseWindow();
    
    std::cout << "Emulador fechado com sucesso." << std::endl;
    return 0;
}
