#pragma once

#include "emulator.hpp"

#include "raylib.h"

#include <cstdint>
#include <string>

struct DebugUiInput {
    bool keyUp = false;
    bool keyDown = false;
    bool keyLeft = false;
    bool keyRight = false;
    bool keyA = false;
    bool keyB = false;
    bool keySelect = false;
    bool keyStart = false;

    bool gamepadConnected = false;
    int gamepadIndex = -1;
    const char* gamepadName = nullptr;
};

struct DebugUiState {
    bool showSidebar = false;
    bool showAbout = false;

    // Home, CPU, Video, Audio, Memory, Cart, Input, Display
    int panel = 0;

    int paletteIndex = 0;
    int shaderIndex = 0;
    bool smoothFilter = false;
    bool integerScale = true;

    int memAddress = 0xC000;
    int memBytes = 64;

    std::string status;
    float statusTimer = 0.0f;
};

void DebugUi_Init();
void DebugUi_Shutdown();

float DebugUi_SidebarWidth(const DebugUiState& state);
float DebugUi_MenuBarHeight();

void DebugUi_ApplyPalette(Emulator& emu, DebugUiState& state);

void DebugUi_Draw(Emulator& emu, DebugUiState& state, const DebugUiInput& input,
                  float hostFps, int scale, float gameLeft, float gameTop,
                  float gameW, float gameH);

void DebugUi_SetStatus(DebugUiState& state, const std::string& msg);
void DebugUi_ToggleSidebar(DebugUiState& state);
bool DebugUi_WantCaptureKeyboard();
bool DebugUi_WantCaptureMouse();
// True only while an ImGui text field is active (memory address, etc.).
// Game controls should ignore WantCaptureKeyboard — the menu bar / nav steals Enter.
bool DebugUi_WantTextInput();
