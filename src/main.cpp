#include "mmu.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "timer.hpp"
#include "raylib.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
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
    assert(mmu.readByte(0xFF00) == 0xFF);
    std::cout << "[Teste Joypad 1] Inicializacao de JOYP ok." << std::endl;

    mmu.setJoypadState(0x0C, 0x06);

    mmu.writeByte(0xFF00, 0x20);
    assert(mmu.readByte(0xFF00) == 0xEC);
    std::cout << "[Teste Joypad 2] Selecao de Direcionais ok." << std::endl;

    mmu.writeByte(0xFF00, 0x10);
    assert(mmu.readByte(0xFF00) == 0xD6);
    std::cout << "[Teste Joypad 3] Selecao de Acoes ok." << std::endl;

    mmu.writeByte(0xFF00, 0x00);
    assert(mmu.readByte(0xFF00) == 0xC4);
    std::cout << "[Teste Joypad 4] Selecao de Ambos ok." << std::endl;

    mmu.writeByte(0xFF00, 0x20); 
    mmu.writeByte(0xFF0F, 0x00); 

    mmu.setJoypadState(0x08, 0x06);
    assert((mmu.readByte(0xFF0F) & 0x10) != 0);
    std::cout << "[Teste Joypad 5] Disparo de interrupcao de entrada ok." << std::endl;

    std::cout << "Todos os testes unitarios do Joypad passaram!\n" << std::endl;
}

int main() {
    testMBC1();
    testTimersAndInterrupts();
    testJoypad();

    std::cout << "Carregando ROM do Castlevania II..." << std::endl;
    
    // Caminho da ROM adicionada
    std::string romPath = "roms/Castlevania II - Belmont's Revenge (USA, Europe)/Castlevania II - Belmont's Revenge (USA, Europe).gb";
    std::ifstream file(romPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERRO: Nao foi possivel abrir a ROM em: " << romPath << std::endl;
        return 1;
    }
    
    std::vector<uint8_t> romData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    std::cout << "Tamanho da ROM lido: " << romData.size() << " bytes." << std::endl;
    
    MMU mmu;
    CPU cpu;
    PPU ppu;
    Timer timer;
    
    if (!mmu.loadROM(romData)) {
        std::cerr << "Falha ao inicializar a ROM no barramento da MMU!" << std::endl;
        return 1;
    }
    
    // Desliga a Boot ROM interna para comecar a execucao diretamente em 0x0100
    mmu.writeByte(0xFF50, 1);
    
    // Configurações do Viewport do Raylib
    const int SCREEN_WIDTH = 160;
    const int SCREEN_HEIGHT = 144;
    const int SCALE = 4;
    const int SIDEBAR_WIDTH = 300;
    
    InitWindow(SCREEN_WIDTH * SCALE + SIDEBAR_WIDTH, SCREEN_HEIGHT * SCALE, "Game Boy DMG-01 Emulator - Castlevania II");
    SetTargetFPS(60);
    
    Image emptyImage = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
    Texture2D screenTexture = LoadTextureFromImage(emptyImage);
    UnloadImage(emptyImage);
    
    std::cout << "Janela do emulador aberta. Executando jogo..." << std::endl;
    
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
        
        // Converte para formato do Joypad (Active Low)
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
        
        // Atualiza o estado físico do controle na MMU
        mmu.setJoypadState(directions, actions);

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
        ss << "BC: 0x" << std::hex << std::uppercase << cpu.getRegs().bc() 
           << "  DE: 0x" << cpu.getRegs().de();
        DrawText(ss.str().c_str(), startX, y, 14, LIGHTGRAY);
        y += 20;

        ss.str(""); ss.clear();
        ss << "HL: 0x" << std::hex << std::uppercase << cpu.getRegs().hl();
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
        
        DrawFPS(10, 10);
        
        EndDrawing();
    }
    
    UnloadTexture(screenTexture);
    CloseWindow();
    
    std::cout << "Emulador fechado com sucesso." << std::endl;
    return 0;
}
