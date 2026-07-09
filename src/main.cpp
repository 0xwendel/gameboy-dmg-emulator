#include "mmu.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "timer.hpp"
#include "raylib.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
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

    std::vector<uint8_t> mockROM(0x200, 0x00);
    mockROM[0x0100] = 0xFB; // EI
    mockROM[0x0101] = 0x76; // HALT
    mockROM[0x0102] = 0x00; // NOP

    mockROM[0x0050] = 0xF3; // DI
    mockROM[0x0051] = 0xC9; // RET

    mmu.loadROM(mockROM);
    mmu.writeByte(0xFF50, 1);

    mmu.writeByte(0xFF05, 0xFE); 
    mmu.writeByte(0xFF06, 0xAA); 
    mmu.writeByte(0xFF07, 0x05); 
    mmu.writeByte(0xFFFF, 0x04); 

    uint8_t cycles = cpu.step(mmu); timer.tick(cycles, mmu);
    cycles = cpu.step(mmu); timer.tick(cycles, mmu);
    cycles = cpu.step(mmu); timer.tick(cycles, mmu);
    cycles = cpu.step(mmu); timer.tick(cycles, mmu);

    for (int i = 0; i < 4; ++i) {
        cycles = cpu.step(mmu);
        timer.tick(cycles, mmu);
    }
    
    assert(mmu.readByte(0xFF05) == 0xAA);
    assert((mmu.readByte(0xFF0F) & 0x04) != 0);

    cycles = cpu.step(mmu);
    timer.tick(cycles, mmu);
    assert(cycles == 5);

    assert(cpu.getRegs().pc == 0x0050);
    uint16_t sp = cpu.getRegs().sp;
    assert(mmu.readByte(sp) == 0x02);
    assert(mmu.readByte(sp + 1) == 0x01);
    assert(cpu.getIme() == false);

    cycles = cpu.step(mmu); timer.tick(cycles, mmu);
    cycles = cpu.step(mmu); timer.tick(cycles, mmu);
    assert(cpu.getRegs().pc == 0x0102);
    std::cout << "Todos os testes unitarios de Timers e Interrupcoes passaram!\n" << std::endl;
}

void testJoypad() {
    std::cout << "Executando testes unitarios do Joypad..." << std::endl;

    MMU mmu;

    // 1. Por padrão, sem seleções, JOYP (0xFF00) retorna 0xCF (Bits 6-7 = 11, Bits 4-5 = 00, Bits 0-3 = 1111)
    // Esperado: 0xC0 (bits 6-7) | 0x30 (bits 4-5) | 0x0F (bits 0-3) = 0xFF
    assert(mmu.readByte(0xFF00) == 0xFF);
    std::cout << "[Teste Joypad 1] Inicializacao de JOYP ok." << std::endl;

    // 2. Simula o aperto de botões:
    // Direcionais: Down solto (1), Up solto (1), Left pressionado (0), Right pressionado (0) -> 0b1100 = 0x0C
    // Ações: Start pressionado (0), Select solto (1), B solto (1), A pressionado (0) -> 0b0110 = 0x06
    mmu.setJoypadState(0x0C, 0x06);

    // Seleciona apenas Direcionais (escreve bit 4 = 0, bit 5 = 1 -> 0x20)
    mmu.writeByte(0xFF00, 0x20);
    // Esperado ler: 0xC0 | 0x20 | 0x0C = 0xEC
    assert(mmu.readByte(0xFF00) == 0xEC);
    std::cout << "[Teste Joypad 2] Selecao de Direcionais ok." << std::endl;

    // Seleciona apenas Ações (escreve bit 4 = 1, bit 5 = 0 -> 0x10)
    mmu.writeByte(0xFF00, 0x10);
    // Esperado ler: 0xC0 | 0x10 | 0x06 = 0xD6
    assert(mmu.readByte(0xFF00) == 0xD6);
    std::cout << "[Teste Joypad 3] Selecao de Acoes ok." << std::endl;

    // Seleciona ambos (escreve 0x00)
    mmu.writeByte(0xFF00, 0x00);
    // Esperado ler: 0xC0 | 0x00 | (0x0C & 0x06) = 0xC0 | 0x04 = 0xC4
    assert(mmu.readByte(0xFF00) == 0xC4);
    std::cout << "[Teste Joypad 4] Selecao de Ambos ok." << std::endl;

    // 3. Teste de detecção de interrupção por transição de botão
    mmu.writeByte(0xFF00, 0x20); // Seleciona Direcionais
    mmu.writeByte(0xFF0F, 0x00); // Limpa interrupções pendentes

    // Aperta o botão "Up" (bit 2 de 1 -> 0, ou seja, de 0x0C para 0x08)
    mmu.setJoypadState(0x08, 0x06);
    // Deve disparar a interrupção de Joypad (Bit 4 de IF -> 0x10)
    assert((mmu.readByte(0xFF0F) & 0x10) != 0);
    std::cout << "[Teste Joypad 5] Disparo de interrupcao de entrada ok." << std::endl;

    std::cout << "Todos os testes unitarios do Joypad passaram!\n" << std::endl;
}

int main() {
    testMBC1();
    testTimersAndInterrupts();
    testJoypad();

    std::cout << "Inicializando Emulador de Game Boy com Diagnose Panel & Input Polling..." << std::endl;
    
    MMU mmu;
    CPU cpu;
    PPU ppu;
    Timer timer;
    
    std::vector<uint8_t> testROM(0x200, 0x00);
    
    // --- MAIN PROGRAM (0x0100) ---
    // 0x0100: LD A, 0x05 (Habilita Timer, freq 262144Hz)
    testROM[0x0100] = 0x3E; testROM[0x0101] = 0x05;
    // 0x0102: LDH [0x07], A (Escreve em TAC)
    testROM[0x0102] = 0xE0; testROM[0x0103] = 0x07;
    // 0x0104: LD A, 0x04 (Habilita interrupção de Timer)
    testROM[0x0104] = 0x3E; testROM[0x0105] = 0x04;
    // 0x0106: LDH [0xFF], A (Escreve em IE)
    testROM[0x0106] = 0xE0; testROM[0x0107] = 0xFF;
    // 0x0108: EI (Habilita interrupções)
    testROM[0x0108] = 0xFB;
    // 0x0109: HALT (Dorme aguardando interrupção)
    testROM[0x0109] = 0x76;
    // 0x010A: JR -3 (0x0109 - Volta para o HALT)
    testROM[0x010A] = 0x18; testROM[0x010B] = 0xFD;
    
    // --- TIMER INTERRUPT VECTOR (0x0050) ---
    testROM[0x0050] = 0xF5;
    testROM[0x0051] = 0xFA; testROM[0x0052] = 0x00; testROM[0x0053] = 0xC0;
    testROM[0x0054] = 0x3C;
    testROM[0x0055] = 0xEA; testROM[0x0056] = 0x00; testROM[0x0057] = 0xC0;
    testROM[0x0058] = 0xF1;
    testROM[0x0059] = 0xD9;

    if (!mmu.loadROM(testROM)) {
        std::cerr << "Falha ao carregar a ROM de teste!" << std::endl;
        return 1;
    }
    
    mmu.writeByte(0xFF50, 1);
    mmu.writeByte(0xFF40, 0x97);
    mmu.writeByte(0xFF47, 0xE4); 
    mmu.writeByte(0xFF48, 0xE4); 
    mmu.writeByte(0xFF49, 0x1B); 

    // --- CARREGA TILES DE BACKGROUND ---
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

    // --- CARREGA TILE DE SPRITE 8x16 ---
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
    const int SIDEBAR_WIDTH = 300;
    
    InitWindow(SCREEN_WIDTH * SCALE + SIDEBAR_WIDTH, SCREEN_HEIGHT * SCALE, "Game Boy DMG-01 Emulator - Joypad Control");
    SetTargetFPS(60);
    
    Image emptyImage = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
    Texture2D screenTexture = LoadTextureFromImage(emptyImage);
    UnloadImage(emptyImage);
    
    uint8_t scx = 0;
    uint8_t sprite0X = 20;
    
    std::cout << "Janela do emulador aberta. Pronto para interacao com o Joypad..." << std::endl;
    
    // Loop principal da janela gráfica
    while (!WindowShouldClose()) {
        // --- 1. CAPTURA POLLED KEYBOARD INPUTS ---
        bool keyUp = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
        bool keyDown = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
        bool keyLeft = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
        bool keyRight = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
        
        bool keyA = IsKeyDown(KEY_K) || IsKeyDown(KEY_Z);
        bool keyB = IsKeyDown(KEY_J) || IsKeyDown(KEY_X);
        bool keySelect = IsKeyDown(KEY_BACKSPACE) || IsKeyDown(KEY_SPACE);
        bool keyStart = IsKeyDown(KEY_ENTER);
        
        // Converte para formato do Joypad (Active Low - Bit a 0 significa pressionado)
        uint8_t directions = 0x0F;
        if (keyRight)  directions &= ~0x01; // Bit 0
        if (keyLeft)   directions &= ~0x02; // Bit 1
        if (keyUp)     directions &= ~0x04; // Bit 2
        if (keyDown)   directions &= ~0x08; // Bit 3
        
        uint8_t actions = 0x0F;
        if (keyA)      actions &= ~0x01; // Bit 0
        if (keyB)      actions &= ~0x02; // Bit 1
        if (keySelect) actions &= ~0x04; // Bit 2
        if (keyStart)  actions &= ~0x08; // Bit 3
        
        // Atualiza a MMU
        mmu.setJoypadState(directions, actions);
        
        // Incrementa rolagem do fundo
        scx++;
        mmu.writeByte(0xFF43, scx);
        
        // Movimenta o Sprite 0 horizontalmente
        sprite0X = (sprite0X + 1) % 160;
        
        // Escreve os dados dos 3 Sprites na OAM
        mmu.writeByte(0xFE00, 80);            // Y = 64
        mmu.writeByte(0xFE01, sprite0X + 8);  // X = sprite0X
        mmu.writeByte(0xFE02, 2);             // Tile index = 2
        mmu.writeByte(0xFE03, 0x00);          // Attrs = 0x00

        mmu.writeByte(0xFE04, 100);           // Y = 84
        mmu.writeByte(0xFE05, 50);            // X = 42
        mmu.writeByte(0xFE06, 2);             // Tile index = 2
        mmu.writeByte(0xFE07, 0x20);          // Attrs = 0x20

        mmu.writeByte(0xFE08, 120);           // Y = 104
        mmu.writeByte(0xFE09, 100);           // X = 92
        mmu.writeByte(0xFE02 + 8, 2);         // Tile index = 2
        mmu.writeByte(0xFE03 + 8, 0x90);      // Attrs = 0x90

        // Roda a CPU e avança a PPU + Timer até concluir a varredura do frame (154 scanlines)
        while (!ppu.isFrameReady()) {
            uint8_t cycles = cpu.step(mmu);
            ppu.tick(cycles, mmu);
            timer.tick(cycles, mmu); 
        }
        
        // Atualiza textura da Raylib
        UpdateTexture(screenTexture, ppu.getFrameBuffer());
        
        BeginDrawing();
        ClearBackground(GetColor(0x111115FF));
        
        // 1. Desenha a tela do Game Boy escalada 4x
        DrawTexturePro(
            screenTexture,
            Rectangle{0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT)},
            Rectangle{0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH * SCALE), static_cast<float>(SCREEN_HEIGHT * SCALE)},
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE
        );
        
        // 2. Desenha o painel de diagnósticos à direita
        int startX = SCREEN_WIDTH * SCALE + 20;
        int y = 20;
        
        DrawText("DIAGNOSE PANEL", startX, y, 18, RAYWHITE);
        y += 30;
        
        DrawLine(startX, y, startX + 260, y, GRAY);
        y += 15;
        
        // --- CPU REGISTERS ---
        DrawText("CPU REGISTERS:", startX, y, 14, GREEN);
        y += 20;
        
        std::stringstream ss;
        ss << "PC: 0x" << std::hex << std::uppercase << cpu.getRegs().pc 
           << "   SP: 0x" << cpu.getRegs().sp;
        DrawText(ss.str().c_str(), startX, y, 14, LIGHTGRAY);
        y += 20;
        
        ss.str(""); ss.clear();
        ss << "A: 0x" << std::hex << std::uppercase << (int)cpu.getRegs().a 
           << "   F: 0x" << (int)cpu.getRegs().f;
        DrawText(ss.str().c_str(), startX, y, 14, LIGHTGRAY);
        y += 20;

        ss.str(""); ss.clear();
        ss << "IME: " << (cpu.getIme() ? "ON" : "OFF");
        DrawText(ss.str().c_str(), startX, y, 14, cpu.getIme() ? GREEN : ORANGE);
        y += 25;

        // --- JOYPAD CONTROLLER STATUS ---
        DrawText("JOYPAD (0xFF00) INPUTS:", startX, y, 14, GREEN);
        y += 20;

        ss.str(""); ss.clear();
        ss << "JOYP Value: 0x" << std::hex << std::uppercase << (int)mmu.readByte(0xFF00);
        DrawText(ss.str().c_str(), startX, y, 14, YELLOW);
        y += 25;

        // Desenha status das teclas direcionais
        DrawText("D-PAD:", startX, y, 14, LIGHTGRAY);
        DrawText(keyUp ? " UP" : " --", startX + 60, y, 14, keyUp ? GREEN : GRAY);
        DrawText(keyDown ? " DOWN" : " ----", startX + 100, y, 14, keyDown ? GREEN : GRAY);
        DrawText(keyLeft ? " LEFT" : " ----", startX + 160, y, 14, keyLeft ? GREEN : GRAY);
        DrawText(keyRight ? " RIGHT" : " -----", startX + 210, y, 14, keyRight ? GREEN : GRAY);
        y += 20;

        // Desenha status das teclas de ação
        DrawText("BUTTONS:", startX, y, 14, LIGHTGRAY);
        DrawText(keyA ? " A" : " -", startX + 80, y, 14, keyA ? RED : GRAY);
        DrawText(keyB ? " B" : " -", startX + 110, y, 14, keyB ? RED : GRAY);
        DrawText(keySelect ? " SELECT" : " ------", startX + 140, y, 14, keySelect ? ORANGE : GRAY);
        DrawText(keyStart ? " START" : " -----", startX + 210, y, 14, keyStart ? ORANGE : GRAY);
        y += 30;

        // --- INTERRUPTS ---
        DrawText("INTERRUPTS:", startX, y, 14, GREEN);
        y += 20;

        ss.str(""); ss.clear();
        ss << "IE (0xFFFF): 0x" << std::hex << std::uppercase << (int)mmu.readByte(0xFFFF);
        DrawText(ss.str().c_str(), startX, y, 14, LIGHTGRAY);
        y += 20;

        ss.str(""); ss.clear();
        ss << "IF (0xFF0F): 0x" << std::hex << std::uppercase << (int)mmu.readByte(0xFF0F);
        DrawText(ss.str().c_str(), startX, y, 14, LIGHTGRAY);
        y += 25;

        // --- TIMER SERVICE COUNT ---
        uint8_t intCounter = mmu.readByte(0xC000);
        ss.str(""); ss.clear();
        ss << "TIMER INT COUNT: " << std::dec << (int)intCounter;
        DrawText(ss.str().c_str(), startX, y, 14, ORANGE);
        
        DrawFPS(10, 10);
        
        EndDrawing();
    }
    
    UnloadTexture(screenTexture);
    CloseWindow();
    
    std::cout << "Emulador fechado com sucesso." << std::endl;
    return 0;
}
