#include "mmu.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "raylib.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "Inicializando Emulador de Game Boy (Fase 3: Visualizacao PPU com Raylib)..." << std::endl;
    
    MMU mmu;
    CPU cpu;
    PPU ppu;
    
    // Prepara uma ROM com um loop infinito básico (NOP + JR -3) para a CPU ficar ocupada
    // 0x0100: NOP (0x00)
    // 0x0101: JR -3 (0x18 0xFD -> salta de volta para 0x0100)
    std::vector<uint8_t> testROM(0x200, 0x00);
    testROM[0x0100] = 0x00;
    testROM[0x0101] = 0x18;
    testROM[0x0102] = 0xFD;
    
    if (!mmu.loadROM(testROM)) {
        std::cerr << "Falha ao carregar a ROM de teste!" << std::endl;
        return 1;
    }
    
    // Desabilita a Boot ROM na MMU
    mmu.writeByte(0xFF50, 1);
    
    // Configurações do Viewport do Raylib
    const int SCREEN_WIDTH = 160;
    const int SCREEN_HEIGHT = 144;
    const int SCALE = 4;
    
    InitWindow(SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE, "Game Boy DMG-01 Emulator - PPU Screen");
    SetTargetFPS(60);
    
    // Cria uma imagem vazia na Raylib e converte para textura para podermos atualizá-la a cada frame
    Image emptyImage = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
    Texture2D screenTexture = LoadTextureFromImage(emptyImage);
    UnloadImage(emptyImage);
    
    std::cout << "Janela do emulador aberta. Rodando loop principal a 60 FPS..." << std::endl;
    
    // Loop principal da janela gráfica
    while (!WindowShouldClose()) {
        // Roda a CPU e avança a PPU até que o frame gráfico esteja pronto (154 scanlines varridas)
        while (!ppu.isFrameReady()) {
            uint8_t cycles = cpu.step(mmu);
            ppu.tick(cycles, mmu);
        }
        
        // Atualiza a textura da tela com o frame buffer gerado pela PPU (Xadrez clássico em movimento)
        UpdateTexture(screenTexture, ppu.getFrameBuffer());
        
        // Renderização Raylib
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
        
        // Exibe estatísticas de desempenho para validação de ciclos e quadros
        DrawFPS(10, 10);
        
        EndDrawing();
    }
    
    // Finalização e limpeza de recursos
    UnloadTexture(screenTexture);
    CloseWindow();
    
    std::cout << "Emulador fechado com sucesso." << std::endl;
    return 0;
}
