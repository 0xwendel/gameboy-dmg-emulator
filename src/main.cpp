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

    std::cout << "Inicializando Emulador de Game Boy (Fase 3.2: Renderizacao Real de Tiles e Sprites 8x16)..." << std::endl;
    
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
    
    // Configura LCDC na MMU
    // LCDC (0xFF40) = 0x97
    // - Bit 7 = 1: LCD On
    // - Bit 4 = 1: BG/Window Tile Data em 0x8000
    // - Bit 2 = 1: Sprite Size = 8x16
    // - Bit 1 = 1: Sprites Enabled
    // - Bit 0 = 1: BG Enabled
    mmu.writeByte(0xFF40, 0x97);
    
    // Paletas
    mmu.writeByte(0xFF47, 0xE4); // BGP: Paleta padrão do Fundo (11 10 01 00)
    mmu.writeByte(0xFF48, 0xE4); // OBP0: Paleta padrão do Sprite 0 (11 10 01 00)
    mmu.writeByte(0xFF49, 0x1B); // OBP1: Paleta invertida para o Sprite 2 (00 01 10 11)

    // --- CARREGA TILES DE BACKGROUND (0x8000 - 0x801F) ---
    uint8_t smileyTile[16] = {
        0x3C, 0x00, 0x42, 0x00, 0xA5, 0x00, 0x81, 0x00,
        0xA5, 0x00, 0x99, 0x00, 0x42, 0x00, 0x3C, 0x00
    };
    uint8_t darkTile[16] = {
        0xAA, 0xFF, 0x55, 0xFF, 0xAA, 0xFF, 0x55, 0xFF,
        0xAA, 0xFF, 0x55, 0xFF, 0xAA, 0xFF, 0x55, 0xFF
    };
    for (int i = 0; i < 16; ++i) {
        mmu.writeByte(0x8000 + i, smileyTile[i]);
        mmu.writeByte(0x8010 + i, darkTile[i]);
    }
    for (int i = 0; i < 1024; ++i) {
        mmu.writeByte(0x9800 + i, ((i % 2) == ((i / 32) % 2)) ? 0 : 1);
    }

    // --- CARREGA TILE DE SPRITE 8x16 (Tile indices 2 e 3 em 0x8020 - 0x803F) ---
    // Desenha um boneco palito de 8x16
    uint8_t stickFigureTop[16] = {
        0x18, 0x18, // Linha 0 (Cabeça)
        0x3C, 0x3C, // Linha 1
        0x3C, 0x3C, // Linha 2
        0x18, 0x18, // Linha 3
        0x18, 0x18, // Linha 4 (Pescoço/Tronco)
        0x7E, 0x7E, // Linha 5 (Braços abertos)
        0x99, 0x99, // Linha 6
        0x99, 0x99  // Linha 7
    };
    uint8_t stickFigureBottom[16] = {
        0x18, 0x18, // Linha 8 (Tronco baixo)
        0x18, 0x18, // Linha 9
        0x24, 0x24, // Linha 10 (Pernas)
        0x24, 0x24, // Linha 11
        0x42, 0x42, // Linha 12
        0x42, 0x42, // Linha 13
        0x81, 0x81, // Linha 14
        0x81, 0x81  // Linha 15
    };
    for (int i = 0; i < 16; ++i) {
        mmu.writeByte(0x8020 + i, stickFigureTop[i]);
        mmu.writeByte(0x8030 + i, stickFigureBottom[i]);
    }

    // Configurações do Viewport do Raylib
    const int SCREEN_WIDTH = 160;
    const int SCREEN_HEIGHT = 144;
    const int SCALE = 4;
    
    InitWindow(SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE, "Game Boy DMG-01 Emulator - VRAM Tiles & Sprites");
    SetTargetFPS(60);
    
    Image emptyImage = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
    Texture2D screenTexture = LoadTextureFromImage(emptyImage);
    UnloadImage(emptyImage);
    
    uint8_t scx = 0;
    uint8_t scy = 0;
    
    uint8_t sprite0X = 20;
    
    std::cout << "Janela do emulador aberta. Rodando loop de Tiles + Sprites..." << std::endl;
    
    // Loop principal da janela gráfica
    while (!WindowShouldClose()) {
        // Incrementa rolagem do fundo
        scx++;
        mmu.writeByte(0xFF43, scx);
        
        // Movimenta o Sprite 0 horizontalmente
        sprite0X = (sprite0X + 1) % 160;
        
        // Escreve os dados dos 3 Sprites na OAM
        // --- Sprite 0 (Movendo, Paleta 0, no topo) ---
        mmu.writeByte(0xFE00, 80);            // Y = 64 (80 - 16)
        mmu.writeByte(0xFE01, sprite0X + 8);  // X = sprite0X
        mmu.writeByte(0xFE02, 2);             // Tile index = 2 (Consome 2 e 3 em 8x16)
        mmu.writeByte(0xFE03, 0x00);          // Attrs = 0x00 (Normal)

        // --- Sprite 1 (Estático, Espelhado no Eixo X) ---
        mmu.writeByte(0xFE04, 100);           // Y = 84 (100 - 16)
        mmu.writeByte(0xFE05, 50);            // X = 42
        mmu.writeByte(0xFE06, 2);             // Tile index = 2
        mmu.writeByte(0xFE07, 0x20);          // Attrs = 0x20 (X-Flip)

        // --- Sprite 2 (Estático, Paleta OBP1, Prioridade BG (Drawn behind BG)) ---
        // Só ficará visível por cima da cor 0 (clara) do fundo
        mmu.writeByte(0xFE08, 120);           // Y = 104
        mmu.writeByte(0xFE09, 100);           // X = 92
        mmu.writeByte(0xFE02 + 8, 2);         // Tile index = 2
        mmu.writeByte(0xFE03 + 8, 0x90);      // Attrs = 0x90 (OBP1 + BG Priority)

        // Roda a CPU e avança a PPU até concluir a varredura do frame (154 scanlines)
        while (!ppu.isFrameReady()) {
            uint8_t cycles = cpu.step(mmu);
            ppu.tick(cycles, mmu);
        }
        
        // Atualiza textura da Raylib com o frame buffer gerado
        UpdateTexture(screenTexture, ppu.getFrameBuffer());
        
        BeginDrawing();
        ClearBackground(BLACK);
        
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
