#include "mmu.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "timer.hpp"
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

void testTimersAndInterrupts() {
    std::cout << "Executando testes unitarios de Timers e Interrupcoes..." << std::endl;

    MMU mmu;
    CPU cpu;
    Timer timer;

    // Cria uma ROM simulada
    std::vector<uint8_t> mockROM(0x200, 0x00);

    // Endereço 0x0100: EI (0xFB) -> Ativa interrupções
    mockROM[0x0100] = 0xFB;
    // Endereço 0x0101: HALT (0x76) -> Põe a CPU para dormir esperando interrupção
    mockROM[0x0101] = 0x76;
    // Endereço 0x0102: NOP (0x00) -> Instrução de retorno
    mockROM[0x0102] = 0x00;

    // Vetor de Interrupção de Timer (0x0050)
    // 0x0050: DI (0xF3) -> desativa IME para sabermos que entramos aqui
    mockROM[0x0050] = 0xF3;
    // 0x0051: RET (0xC9)
    mockROM[0x0051] = 0xC9;

    mmu.loadROM(mockROM);
    mmu.writeByte(0xFF50, 1); // Desliga a Boot ROM

    // Configura o Timer no barramento:
    mmu.writeByte(0xFF05, 0xFE); // TIMA = 254 (1 estoiro de 255 e depois reload)
    mmu.writeByte(0xFF06, 0xAA); // TMA = 0xAA (Valor de recarga)
    mmu.writeByte(0xFF07, 0x05); // TAC = 0x05 (Habilitado, Frequência 262144 Hz -> Ticks a cada 4 M-cycles)

    // Habilita a interrupção de Timer em IE (0xFFFF)
    mmu.writeByte(0xFFFF, 0x04); // Bit 2 ativo = Timer enabled

    // Executa as instruções
    uint8_t cycles = 0;

    // 1. Executa EI (0x0100)
    cycles = cpu.step(mmu);
    timer.tick(cycles, mmu);
    assert(cycles == 1);
    // IME ainda está em delay de 1 instrução (não ativo) ou já ativo no final do ciclo

    // 2. Executa HALT (0x0101) -> CPU entra em modo HALT
    cycles = cpu.step(mmu);
    timer.tick(cycles, mmu);
    assert(cycles == 1);
    
    // Ticks acumulados em TIMA: EI (1) + HALT (1) = 2 M-cycles. TIMA ainda é 254.

    // 3. CPU está em HALT. Vamos rodar steps ociosos de 1 ciclo.
    // Ticking 1 cycle
    cycles = cpu.step(mmu); timer.tick(cycles, mmu); // total 3 M-cycles
    assert(cycles == 1);

    // Ticking 1 cycle
    cycles = cpu.step(mmu); timer.tick(cycles, mmu); // total 4 M-cycles (TIMA incrementa de 254 para 255!)
    assert(cycles == 1);
    assert(mmu.readByte(0xFF05) == 0xFF);
    std::cout << "[Teste Timer] TIMA incrementado para 0xFF ok." << std::endl;

    // Ticking 4 mais cycles (para totalizar 8)
    for (int i = 0; i < 4; ++i) {
        cycles = cpu.step(mmu);
        timer.tick(cycles, mmu);
    }
    
    // Totalizando 8 M-cycles acumulados na frequência /4:
    // TIMA passa de 0xFF para 0x00 (estouro!)
    // TIMA deve recarregar TMA = 0xAA
    // Bit 2 de IF (0xFF0F) deve ser setado para 1.
    assert(mmu.readByte(0xFF05) == 0xAA);
    assert((mmu.readByte(0xFF0F) & 0x04) != 0);
    std::cout << "[Teste Timer] TIMA estourou, recarregou TMA (0xAA) e disparou IF ok." << std::endl;

    // Como o bit de interrupção está ativo em IF e habilitado em IE, no próximo passo a CPU:
    // 1. Acorda do HALT.
    // 2. Executa a rotina de interrupção (IME fica false, PC vai para 0x0050, SP decrementa e guarda PC=0x0102).
    // O desvio consome 5 M-cycles.
    cycles = cpu.step(mmu);
    timer.tick(cycles, mmu);
    assert(cycles == 5);

    // Verifica que PC mudou para 0x0050 e salvou o PC de retorno (0x0102) na pilha
    assert(cpu.getRegs().pc == 0x0050);
    uint16_t sp = cpu.getRegs().sp;
    assert(mmu.readByte(sp) == 0x02);
    assert(mmu.readByte(sp + 1) == 0x01);
    assert(cpu.getIme() == false);
    std::cout << "[Teste Interrupcao] CPU acordou do HALT e desviou para o vetor 0x0050 ok." << std::endl;

    // 4. Executa o tratador de interrupção em 0x0050: DI (0xF3)
    cycles = cpu.step(mmu);
    timer.tick(cycles, mmu);
    assert(cycles == 1);
    assert(cpu.getRegs().pc == 0x0051);

    // 5. Executa RET (0xC9) para voltar para 0x0102
    cycles = cpu.step(mmu);
    timer.tick(cycles, mmu);
    assert(cycles == 4);
    assert(cpu.getRegs().pc == 0x0102);
    assert(cpu.getRegs().sp == sp + 2);
    std::cout << "[Teste Interrupcao] CPU retornou com sucesso do tratador para o PC original 0x0102 ok." << std::endl;

    // 6. Teste de escrita em DIV resetando para 0
    mmu.writeByte(0xFF04, 0x88); // Tenta escrever qualquer valor
    assert(mmu.readByte(0xFF04) == 0x00);
    std::cout << "[Teste DIV] Escrita em DIV resetou o registrador para 0 ok." << std::endl;

    std::cout << "Todos os testes unitarios de Timers e Interrupcoes passaram!\n" << std::endl;
}

int main() {
    testMBC1();
    testTimersAndInterrupts();

    std::cout << "Inicializando Emulador de Game Boy (Fase 4: Loop Completo com Timers e Graficos)..." << std::endl;
    
    MMU mmu;
    CPU cpu;
    PPU ppu;
    Timer timer;
    
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
    mmu.writeByte(0xFF40, 0x97);
    
    // Paletas
    mmu.writeByte(0xFF47, 0xE4); // BGP
    mmu.writeByte(0xFF48, 0xE4); // OBP0
    mmu.writeByte(0xFF49, 0x1B); // OBP1

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
    uint8_t stickFigureTop[16] = {
        0x18, 0x18, 0x3C, 0x3C, 0x3C, 0x3C, 0x18, 0x18,
        0x18, 0x18, 0x7E, 0x7E, 0x99, 0x99, 0x99, 0x99
    };
    uint8_t stickFigureBottom[16] = {
        0x18, 0x18, 0x18, 0x18, 0x24, 0x24, 0x24, 0x24,
        0x42, 0x42, 0x42, 0x42, 0x81, 0x81, 0x81, 0x81
    };
    for (int i = 0; i < 16; ++i) {
        mmu.writeByte(0x8020 + i, stickFigureTop[i]);
        mmu.writeByte(0x8030 + i, stickFigureBottom[i]);
    }

    // Configurações do Viewport do Raylib
    const int SCREEN_WIDTH = 160;
    const int SCREEN_HEIGHT = 144;
    const int SCALE = 4;
    
    InitWindow(SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE, "Game Boy DMG-01 Emulator - Timers & Interrupts");
    SetTargetFPS(60);
    
    Image emptyImage = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
    Texture2D screenTexture = LoadTextureFromImage(emptyImage);
    UnloadImage(emptyImage);
    
    uint8_t scx = 0;
    uint8_t sprite0X = 20;
    
    std::cout << "Janela do emulador aberta. Rodando loop principal de renderizacao..." << std::endl;
    
    // Loop principal da janela gráfica
    while (!WindowShouldClose()) {
        // Incrementa rolagem do fundo
        scx++;
        mmu.writeByte(0xFF43, scx);
        
        // Movimenta o Sprite 0 horizontalmente
        sprite0X = (sprite0X + 1) % 160;
        
        // Escreve os dados dos 3 Sprites na OAM
        // --- Sprite 0 (Movendo, Paleta 0, no topo) ---
        mmu.writeByte(0xFE00, 80);            // Y = 64
        mmu.writeByte(0xFE01, sprite0X + 8);  // X = sprite0X
        mmu.writeByte(0xFE02, 2);             // Tile index = 2
        mmu.writeByte(0xFE03, 0x00);          // Attrs = 0x00 (Normal)

        // --- Sprite 1 (Estático, Espelhado no Eixo X) ---
        mmu.writeByte(0xFE04, 100);           // Y = 84
        mmu.writeByte(0xFE05, 50);            // X = 42
        mmu.writeByte(0xFE06, 2);             // Tile index = 2
        mmu.writeByte(0xFE07, 0x20);          // Attrs = 0x20 (X-Flip)

        // --- Sprite 2 (Estático, Paleta OBP1, Prioridade BG (Drawn behind BG)) ---
        mmu.writeByte(0xFE08, 120);           // Y = 104
        mmu.writeByte(0xFE09, 100);           // X = 92
        mmu.writeByte(0xFE02 + 8, 2);         // Tile index = 2
        mmu.writeByte(0xFE03 + 8, 0x90);      // Attrs = 0x90

        // Roda a CPU e avança a PPU + Timer até concluir a varredura do frame (154 scanlines)
        while (!ppu.isFrameReady()) {
            uint8_t cycles = cpu.step(mmu);
            ppu.tick(cycles, mmu);
            timer.tick(cycles, mmu); // Sincroniza o Timer com a CPU
        }
        
        // Atualiza textura da Raylib
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
