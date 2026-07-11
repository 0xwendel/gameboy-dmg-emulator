#pragma once

#include "emulator.hpp"

#include "raylib.h"

#include <cstdint>
#include <string>

// Input do joypad espelhado na UI de debug (teclado + gamepad).
struct DebugUiInput {
    bool keyUp = false;
    bool keyDown = false;
    bool keyLeft = false;
    bool keyRight = false;
    bool keyA = false;
    bool keyB = false;
    bool keySelect = false;
    bool keyStart = false;

    // Gamepad (Xbox 360 / XInput)
    bool gamepadConnected = false;
    int gamepadIndex = -1;
    const char* gamepadName = nullptr;
};

// Estado da UI de debug + opções de display.
// O inspector é overlay: NÃO reduz a área do display do jogo.
struct DebugUiState {
    bool showSidebar = false; // F12 — padrão off: foco no display GB

    // 0=Home 1=CPU 2=Video 3=Audio 4=Memory 5=Cart 6=Input 7=Display
    int panel = 0;

    // Display
    int paletteIndex = 0;
    int shaderIndex = 0;
    bool smoothFilter = false;
    bool integerScale = true;

    // Memory peek
    int memAddress = 0xC000;
    int memBytes = 64;

    // Mensagem de status temporária
    std::string status;
    float statusTimer = 0.0f;
};

void DebugUi_Init();
void DebugUi_Shutdown();

// Sempre 0: o inspector é overlay e não reserva largura do viewport do jogo.
float DebugUi_SidebarWidth(const DebugUiState& state);

// Altura da menubar ImGui (para letterbox do display).
float DebugUi_MenuBarHeight();

void DebugUi_ApplyPalette(Emulator& emu, DebugUiState& state);

void DebugUi_Draw(Emulator& emu, DebugUiState& state, const DebugUiInput& input,
                  float hostFps, int scale, float gameLeft, float gameTop,
                  float gameW, float gameH);

void DebugUi_SetStatus(DebugUiState& state, const std::string& msg);
void DebugUi_ToggleSidebar(DebugUiState& state);
bool DebugUi_WantCaptureKeyboard();
bool DebugUi_WantCaptureMouse();
