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

    // Simula uma ROM de 64KB (4 bancos de 16KB cada)
    std::vector<uint8_t> mockROM(64 * 1024, 0x00);

    // Escreve assinaturas únicas em cada banco
    // Banco 0: 0x0000 - 0x3FFF (assina com 0x00)
    // Banco 1: 0x4000 - 0x7FFF (assina com 0x11)
    // Banco 2: 0x8000 - 0xBFFF (assina com 0x22)
    // Banco 3: 0xC000 - 0xFFFF (assina com 0x33)
    for (size_t i = 0x0000; i < 0x4000; ++i) mockROM[i] = 0x00;
    for (size_t i = 0x4000; i < 0x8000; ++i) mockROM[i] = 0x11;
    for (size_t i = 0x8000; i < 0xC000; ++i) mockROM[i] = 0x22;
    for (size_t i = 0xC000; i < 0x10000; ++i) mockROM[i] = 0x33;

    // Define no header da ROM o código 0x02 para RAM de 8KB (endereço 0x0149)
    mockROM[0x0149] = 0x02; 

    // Carrega a ROM na MMU
    bool loaded = mmu.loadROM(mockROM);
    assert(loaded == true);

    // Desabilita a Boot ROM
    mmu.writeByte(0xFF50, 1);

    // 1. Por padrão, a janela 0x4000-0x7FFF deve apontar para o Banco 1 (0x11)
    assert(mmu.readByte(0x4000) == 0x11);
    std::cout << "[Teste 1] Banco inicial padrao (Banco 1) ok." << std::endl;

    // 2. Chaveia para o Banco 2 escrevendo no registrador 0x2000-0x3FFF
    mmu.writeByte(0x2000, 2);
    assert(mmu.readByte(0x4000) == 0x22);
    std::cout << "[Teste 2] Chaveamento de banco (Banco 2) ok." << std::endl;

    // 3. Chaveia para o Banco 3
    mmu.writeByte(0x2000, 3);
    assert(mmu.readByte(0x4000) == 0x33);
    std::cout << "[Teste 3] Chaveamento de banco (Banco 3) ok." << std::endl;

    // 4. Se escrever 0 em 0x2000, o MBC1 deve traduzir para Banco 1 (0x11)
    mmu.writeByte(0x2000, 0);
    assert(mmu.readByte(0x4000) == 0x11);
    std::cout << "[Teste 4] Escrita de banco 0 corrigida para banco 1 ok." << std::endl;

    // 5. Verifica comportamento da RAM externa (SRAM) desativada por padrão
    assert(mmu.readByte(0xA000) == 0xFF); 

    // Ativa RAM externa escrevendo 0x0A no registrador 0x0000-0x1FFF
    mmu.writeByte(0x0000, 0x0A);
    
    // Escreve um valor teste na RAM externa
    mmu.writeByte(0xA000, 0x77);
    assert(mmu.readByte(0xA000) == 0x77);
    std::cout << "[Teste 5] Escrita e leitura na RAM externa (ativada) ok." << std::endl;

    // Desativa a RAM externa escrevendo 0x00 em 0x0000
    mmu.writeByte(0x0000, 0x00);
    assert(mmu.readByte(0xA000) == 0xFF);
    std::cout << "[Teste 6] RAM desativada retorna 0xFF ok." << std::endl;

    std::cout << "Todos os testes unitarios do MBC1 passaram!" << std::endl;
}

int main() {
    // Executa validação de mapeamento de cartucho primeiro
    testMBC1();

    std::cout << "\nInicializando Emulador de Game Boy (Fase 3: Visualizacao PPU com Raylib)..." << std::endl;
    
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
