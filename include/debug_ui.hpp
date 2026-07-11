#pragma once

#include "emulator.hpp"

#include "raylib.h"

#include <cstdint>
#include <string>

// Input do joypad espelhado na UI de debug.
struct DebugUiInput {
    bool keyUp = false;
    bool keyDown = false;
    bool keyLeft = false;
    bool keyRight = false;
    bool keyA = false;
    bool keyB = false;
    bool keySelect = false;
    bool keyStart = false;
};

// Estado da sidebar de debug (uma coluna, abas — sem janelas flutuantes soltas).
struct DebugUiState {
    bool showSidebar = true;

    // Memory peek
    int memAddress = 0xC000;
    int memBytes = 64;

    // Mensagem de status temporária (save state, SRAM, etc.)
    std::string status;
    float statusTimer = 0.0f;
};

void DebugUi_Init();
void DebugUi_Shutdown();

// Largura da coluna de debug (0 se oculta). Use para posicionar a tela GB.
float DebugUi_SidebarWidth(const DebugUiState& state);

// Chamar entre BeginDrawing/EndDrawing, após desenhar a tela GB.
// gameLeft/gameTop: origem da tela GB em pixels de tela.
void DebugUi_Draw(Emulator& emu, DebugUiState& state, const DebugUiInput& input,
                  float hostFps, int scale, float gameLeft, float gameTop,
                  float gameW, float gameH);

void DebugUi_SetStatus(DebugUiState& state, const std::string& msg);
void DebugUi_ToggleSidebar(DebugUiState& state);
bool DebugUi_WantCaptureKeyboard();
bool DebugUi_WantCaptureMouse();
