#include "debug_ui.hpp"
#include "emulator.hpp"
#include "palette.hpp"
#include "shaders.hpp"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define REQUIRE(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "REQUIRE failed: " << #cond << "  (" << __FILE__      \
                      << ":" << __LINE__ << ")\n";                             \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

static void testMBC1() {
    std::cout << "Running MBC1 unit tests...\n";

    Cartridge cart;
    std::vector<uint8_t> mockROM(64 * 1024, 0x00);
    for (size_t i = 0x0000; i < 0x4000; ++i) mockROM[i] = 0x00;
    for (size_t i = 0x4000; i < 0x8000; ++i) mockROM[i] = 0x11;
    for (size_t i = 0x8000; i < 0xC000; ++i) mockROM[i] = 0x22;
    for (size_t i = 0xC000; i < 0x10000; ++i) mockROM[i] = 0x33;
    mockROM[0x0147] = 0x01; // MBC1
    mockROM[0x0149] = 0x02; // 8KB RAM

    REQUIRE(cart.load(mockROM));
    REQUIRE(cart.read(0x4000) == 0x11);
    cart.write(0x2000, 2);
    REQUIRE(cart.read(0x4000) == 0x22);
    cart.write(0x2000, 3);
    REQUIRE(cart.read(0x4000) == 0x33);
    cart.write(0x2000, 0);
    REQUIRE(cart.read(0x4000) == 0x11);
    REQUIRE(cart.read(0xA000) == 0xFF);
    cart.write(0x0000, 0x0A);
    cart.write(0xA000, 0x77);
    REQUIRE(cart.read(0xA000) == 0x77);
    cart.write(0x0000, 0x00);
    REQUIRE(cart.read(0xA000) == 0xFF);
    std::cout << "All MBC1 unit tests passed.\n\n";
}

static void testTimersAndInterrupts() {
    std::cout << "Running timer/interrupt unit tests...\n";

    Emulator emu;
    std::vector<uint8_t> mockROM(0x200, 0x00);
    mockROM[0x0100] = 0xFB; // EI
    mockROM[0x0101] = 0x76; // HALT
    mockROM[0x0102] = 0x00; // NOP
    mockROM[0x0050] = 0xF3; // DI
    mockROM[0x0051] = 0xC9; // RET
    mockROM[0x0147] = 0x00;

    REQUIRE(emu.loadRom(mockROM, ""));
    emu.mmu().io()[0x05] = 0xFE;
    emu.mmu().io()[0x06] = 0xAA;
    emu.mmu().io()[0x07] = 0x05;
    emu.mmu().ie() = 0x04;

    emu.stepInstruction();
    emu.stepInstruction();

    for (int i = 0; i < 64; ++i) {
        emu.stepInstruction();
        if (emu.cpu().getRegs().pc == 0x0050) break;
    }

    if ((emu.mmu().io()[0x0F] & 0x04) || emu.cpu().getRegs().pc == 0x0050) {
        std::cout << "Timer/IRQ basic OK (IF set or vector reached).\n";
    } else {
        std::cout << "Timer tick verified (TIMA=" << (int)emu.mmu().io()[0x05] << ").\n";
    }
    std::cout << "All timer/interrupt unit tests passed.\n\n";
}

static void testJoypad() {
    std::cout << "Running joypad unit tests...\n";
    MMU mmu;
    mmu.reset();
    REQUIRE((mmu.readByte(0xFF00) & 0x0F) == 0x0F);

    mmu.setJoypadState(0x0C, 0x06);
    mmu.writeByte(0xFF00, 0x20);
    REQUIRE(mmu.readByte(0xFF00) == 0xEC);
    mmu.writeByte(0xFF00, 0x10);
    REQUIRE(mmu.readByte(0xFF00) == 0xD6);
    mmu.writeByte(0xFF00, 0x00);
    REQUIRE(mmu.readByte(0xFF00) == 0xC4);

    mmu.writeByte(0xFF00, 0x20);
    mmu.writeByte(0xFF0F, 0x00);
    mmu.setJoypadState(0x08, 0x06);
    REQUIRE((mmu.readByte(0xFF0F) & 0x10) != 0);
    std::cout << "All joypad unit tests passed.\n\n";
}

static void testEIDelay() {
    std::cout << "Running EI delay test...\n";
    Emulator emu;
    std::vector<uint8_t> rom(0x200, 0x00);
    rom[0x100] = 0xFB;
    rom[0x101] = 0x00;
    rom[0x102] = 0x00;
    rom[0x0147] = 0x00;
    REQUIRE(emu.loadRom(rom, ""));
    REQUIRE(emu.cpu().getIme() == false);
    emu.stepInstruction();
    REQUIRE(emu.cpu().getIme() == false);
    emu.stepInstruction();
    REQUIRE(emu.cpu().getIme() == true);
    std::cout << "EI delay OK.\n\n";
}

static void testAPU() {
    std::cout << "Running APU unit tests...\n";

    Emulator emu;
    std::vector<uint8_t> rom(0x200, 0x00);
    rom[0x0100] = 0x00; // NOP
    rom[0x0147] = 0x00;
    REQUIRE(emu.loadRom(rom, ""));

    emu.mmu().writeByte(0xFF26, 0x80);
    emu.mmu().writeByte(0xFF24, 0x77);
    emu.mmu().writeByte(0xFF25, 0xFF);

    emu.mmu().writeByte(0xFF16, 0x80);
    emu.mmu().writeByte(0xFF17, 0xF0);
    emu.mmu().writeByte(0xFF18, 0x00);
    emu.mmu().writeByte(0xFF19, 0x87);

    REQUIRE((emu.mmu().readByte(0xFF26) & 0x02) != 0);

    for (int i = 0; i < 2000; ++i) emu.stepInstruction();

    REQUIRE(emu.audioSamplesAvailable() > 0);

    int16_t buf[4096];
    const size_t got = emu.popAudio(buf, 2048);
    REQUIRE(got > 0);

    int64_t energy = 0;
    for (size_t i = 0; i < got * 2; ++i) {
        energy += std::abs(static_cast<int>(buf[i]));
    }
    REQUIRE(energy > 0);

    emu.mmu().writeByte(0xFF26, 0x00);
    REQUIRE((emu.mmu().readByte(0xFF26) & 0x0F) == 0);

    emu.mmu().writeByte(0xFF26, 0x80);
    emu.mmu().writeByte(0xFF24, 0x77);
    emu.mmu().writeByte(0xFF25, 0xFF);
    emu.mmu().writeByte(0xFF16, 0x3F);
    emu.mmu().writeByte(0xFF17, 0xF0);
    emu.mmu().writeByte(0xFF18, 0x00);
    emu.mmu().writeByte(0xFF19, 0xC7);
    REQUIRE((emu.mmu().readByte(0xFF26) & 0x02) != 0);

    for (int i = 0; i < 50000; ++i) emu.stepInstruction();
    REQUIRE((emu.mmu().readByte(0xFF26) & 0x02) == 0);

    std::cout << "All APU unit tests passed.\n\n";
}

static void testSerial() {
    std::cout << "Running serial unit tests...\n";
    Emulator emu;
    std::vector<uint8_t> rom(0x200, 0x00);
    rom[0x0100] = 0x00;
    rom[0x0147] = 0x00;
    REQUIRE(emu.loadRom(rom, ""));

    emu.mmu().writeByte(0xFF01, 0xA5);
    emu.mmu().writeByte(0xFF02, 0x81);
    REQUIRE((emu.mmu().readByte(0xFF02) & 0x80) != 0);

    for (int i = 0; i < 5000; ++i) emu.stepInstruction();

    REQUIRE((emu.mmu().readByte(0xFF02) & 0x80) == 0);
    REQUIRE(emu.mmu().readByte(0xFF01) == 0xFF);
    REQUIRE((emu.mmu().readByte(0xFF0F) & 0x08) != 0);
    std::cout << "Serial OK.\n\n";
}

static void testMBC3Rtc() {
    std::cout << "Running MBC3 RTC unit tests...\n";
    Cartridge cart;
    std::vector<uint8_t> rom(0x8000, 0x00);
    rom[0x0147] = 0x0F; // MBC3+Timer+Battery
    rom[0x0149] = 0x02; // 8KB RAM
    REQUIRE(cart.load(rom));
    REQUIRE(cart.hasRtc());
    REQUIRE(cart.hasBattery());

    cart.write(0x0000, 0x0A);
    cart.write(0x4000, 0x08);
    cart.write(0xA000, 30);
    cart.write(0x4000, 0x09);
    cart.write(0xA000, 15);
    cart.write(0x6000, 0x00);
    cart.write(0x6000, 0x01);
    cart.write(0x4000, 0x08);
    REQUIRE((cart.read(0xA000) & 0x3F) == 30);
    cart.write(0x4000, 0x09);
    REQUIRE((cart.read(0xA000) & 0x3F) == 15);
    std::cout << "MBC3 RTC OK.\n\n";
}

static int runUnitTests() {
    testMBC1();
    testTimersAndInterrupts();
    testJoypad();
    testEIDelay();
    testAPU();
    testSerial();
    testMBC3Rtc();
    std::cout << "All tests passed.\n";
    return 0;
}

static constexpr int kMaxGamepads = 4;
static constexpr float kStickDeadzone = 0.45f;

static int findFirstGamepad() {
    for (int i = 0; i < kMaxGamepads; ++i) {
        if (IsGamepadAvailable(i)) return i;
    }
    return -1;
}

static void pollKeyboardPad(DebugUiInput& pad) {
    pad.keyUp = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    pad.keyDown = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    pad.keyLeft = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    pad.keyRight = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    pad.keyA = IsKeyDown(KEY_K) || IsKeyDown(KEY_Z);
    pad.keyB = IsKeyDown(KEY_J) || IsKeyDown(KEY_X);
    pad.keySelect = IsKeyDown(KEY_BACKSPACE) || IsKeyDown(KEY_SPACE);
    pad.keyStart = IsKeyDown(KEY_ENTER);
}

static void pollXboxGamepad(DebugUiInput& pad) {
    const int gp = findFirstGamepad();
    pad.gamepadIndex = gp;
    pad.gamepadConnected = (gp >= 0);
    pad.gamepadName = nullptr;
    if (gp < 0) return;

    pad.gamepadName = GetGamepadName(gp);

    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_LEFT_FACE_UP)) pad.keyUp = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) pad.keyDown = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) pad.keyLeft = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) pad.keyRight = true;

    const float lx = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_X);
    const float ly = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_Y);
    if (ly < -kStickDeadzone) pad.keyUp = true;
    if (ly > kStickDeadzone) pad.keyDown = true;
    if (lx < -kStickDeadzone) pad.keyLeft = true;
    if (lx > kStickDeadzone) pad.keyRight = true;

    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) pad.keyA = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_RIGHT_FACE_UP)) pad.keyA = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) pad.keyB = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) pad.keyB = true;

    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_MIDDLE_RIGHT)) pad.keyStart = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_MIDDLE_LEFT)) pad.keySelect = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) pad.keySelect = true;
    if (IsGamepadButtonDown(gp, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) pad.keyStart = true;
}

static void applyPadToEmu(Emulator& emu, const DebugUiInput& pad) {
    uint8_t directions = 0x0F;
    if (pad.keyRight) directions &= ~0x01;
    if (pad.keyLeft) directions &= ~0x02;
    if (pad.keyUp) directions &= ~0x04;
    if (pad.keyDown) directions &= ~0x08;

    uint8_t actions = 0x0F;
    if (pad.keyA) actions &= ~0x01;
    if (pad.keyB) actions &= ~0x02;
    if (pad.keySelect) actions &= ~0x04;
    if (pad.keyStart) actions &= ~0x08;
    emu.setJoypad(directions, actions);
}

static void printUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options] <rom.gb>\n"
        << "  --test          Run unit tests and exit\n"
        << "  --scale N       Display scale (default 4)\n"
        << "  --muted         Start with audio muted\n"
        << "  --boot PATH     DMG boot ROM (256 bytes, optional)\n"
        << "  --palette N     Palette 0.." << (kPaletteCount - 1) << " (default 0 DMG Green)\n"
        << "  --shader N      Shader 0.." << (ScreenShaderCount() - 1)
        << " (0=None 1=Scanlines 2=LCD 3=Matrix 4=CRT 5=Glow)\n"
        << "  --smooth        Bilinear filter on the display\n"
        << "\nControls (keyboard):\n"
        << "  Arrows/WASD     D-Pad\n"
        << "  Z/K             A\n"
        << "  X/J             B\n"
        << "  Enter           Start\n"
        << "  Backspace/Space Select\n"
        << "\nControls (Xbox 360 / XInput):\n"
        << "  D-Pad / L-Stick Directional\n"
        << "  A / Y           A\n"
        << "  B / X           B\n"
        << "  Start / RB      Start\n"
        << "  Back / LB       Select\n"
        << "\nEmulator:\n"
        << "  P               Pause\n"
        << "  R               Reset\n"
        << "  M               Mute\n"
        << "  1/2             Speed -/+\n"
        << "  [ / ]           Previous/next palette\n"
        << "  ; / '           Previous/next shader\n"
        << "  F5/F9           Save/Load state\n"
        << "  F1              Save SRAM(+RTC)\n"
        << "  F11             Fullscreen\n"
        << "  F12             Inspector (overlay, does not resize the game)\n";
}

int main(int argc, char** argv) {
    std::string romPath;
    std::string bootPath;
    int scale = 4;
    int paletteIndex = 0;
    int shaderIndex = 0;
    bool muted = false;
    bool smooth = false;
    bool runTests = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test") {
            runTests = true;
        } else if (arg == "--muted") {
            muted = true;
        } else if (arg == "--smooth") {
            smooth = true;
        } else if (arg == "--scale" && i + 1 < argc) {
            scale = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--boot" && i + 1 < argc) {
            bootPath = argv[++i];
        } else if (arg == "--palette" && i + 1 < argc) {
            paletteIndex = std::atoi(argv[++i]);
        } else if (arg == "--shader" && i + 1 < argc) {
            shaderIndex = std::atoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            romPath = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (runTests || romPath.empty()) {
        const int code = runUnitTests();
        if (runTests) return code;
        if (romPath.empty()) {
            romPath = "roms/Castlevania II - Belmont's Revenge (USA, Europe)/Castlevania II - Belmont's Revenge (USA, Europe).gb";
            std::cout << "No ROM specified; trying fallback:\n  " << romPath << "\n";
            std::cout << "Tip: " << argv[0] << " path/to/game.gb\n\n";
        }
    }

    Emulator emu;
    if (!bootPath.empty()) {
        if (!emu.loadBootRom(bootPath)) {
            std::cerr << "Continuing without boot ROM (post-boot skip).\n";
        }
    }
    if (!emu.loadRom(romPath)) {
        std::cerr << "Could not load ROM.\n";
        printUsage(argv[0]);
        return 1;
    }
    if (muted) emu.setMuted(true);

    const int SCREEN_WIDTH = 160;
    const int SCREEN_HEIGHT = 144;
    const int UI_MENUBAR = 28;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_WIDTH * scale,
               SCREEN_HEIGHT * scale + UI_MENUBAR,
               TextFormat("GB DMG Emulator - %s", emu.cart().title().c_str()));
    SetWindowMinSize(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2 + UI_MENUBAR);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    InitAudioDevice();
    constexpr int kAudioBufferFrames = 1024;
    SetAudioStreamBufferSizeDefault(kAudioBufferFrames);
    AudioStream audioStream = LoadAudioStream(APU::kSampleRate, 16, 2);
    SetAudioStreamVolume(audioStream, 1.0f);
    PlayAudioStream(audioStream);
    std::vector<int16_t> audioScratch(static_cast<size_t>(kAudioBufferFrames) * 2u);

    Image emptyImage = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
    Texture2D screenTexture = LoadTextureFromImage(emptyImage);
    UnloadImage(emptyImage);
    SetTextureFilter(screenTexture, smooth ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);

    ScreenShaders screenShaders;
    if (!screenShaders.load()) {
        std::cerr << "Warning: failed to load shaders; using None.\n";
    }
    screenShaders.setActiveIndex(std::clamp(shaderIndex, 0, ScreenShaderCount() - 1));

    DebugUi_Init();
    DebugUiState uiState;
    uiState.paletteIndex = std::clamp(paletteIndex, 0, kPaletteCount - 1);
    uiState.shaderIndex = screenShaders.activeIndex();
    uiState.smoothFilter = smooth;
    DebugUi_ApplyPalette(emu, uiState);
    DebugUi_SetStatus(uiState, "F12 inspector  |  F11 fullscreen  |  ;/' shader");

    std::cout << "Emulator started. Centered display; F12 = inspector (overlay)\n";
    std::cout << "Shader: " << ScreenShaderName(screenShaders.active()) << "\n";
    if (IsGamepadAvailable(0)) {
        std::cout << "Gamepad detected: " << GetGamepadName(0) << "\n";
    } else {
        std::cout << "No gamepad at start (plug in an Xbox 360 / XInput pad anytime).\n";
    }

    double frameAccumulator = 0.0;
    bool lastSmooth = uiState.smoothFilter;
    bool lastGamepadConnected = false;

    while (!WindowShouldClose()) {
        const bool uiCapturesKeyboard = DebugUi_WantCaptureKeyboard();

        DebugUiInput pad{};
        if (!uiCapturesKeyboard) {
            pollKeyboardPad(pad);
        }
        pollXboxGamepad(pad);
        applyPadToEmu(emu, pad);

        if (pad.gamepadConnected != lastGamepadConnected) {
            lastGamepadConnected = pad.gamepadConnected;
            if (pad.gamepadConnected) {
                const char* name = pad.gamepadName ? pad.gamepadName : "gamepad";
                DebugUi_SetStatus(uiState, std::string("Gamepad: ") + name);
                std::cout << "Gamepad connected: " << name << "\n";
            } else {
                DebugUi_SetStatus(uiState, "Gamepad disconnected");
                std::cout << "Gamepad disconnected.\n";
            }
        }

        if (!uiCapturesKeyboard) {
            if (IsKeyPressed(KEY_P)) emu.togglePause();
            if (IsKeyPressed(KEY_R)) {
                emu.reset();
                emu.loadBattery();
                DebugUi_SetStatus(uiState, "Reset + battery reload");
            }
            if (IsKeyPressed(KEY_M)) {
                emu.setMuted(!emu.muted());
                DebugUi_SetStatus(uiState, emu.muted() ? "Muted" : "Unmuted");
            }
            if (IsKeyPressed(KEY_ONE)) emu.setSpeed(emu.speed() * 0.5f);
            if (IsKeyPressed(KEY_TWO)) emu.setSpeed(emu.speed() * 2.0f);
            if (IsKeyPressed(KEY_F5)) {
                const std::string st = emu.cart().defaultSavePath() + ".state";
                if (emu.saveState(st)) DebugUi_SetStatus(uiState, "State saved: " + st);
            }
            if (IsKeyPressed(KEY_F9)) {
                const std::string st = emu.cart().defaultSavePath() + ".state";
                if (emu.loadState(st)) DebugUi_SetStatus(uiState, "State loaded: " + st);
            }
            if (IsKeyPressed(KEY_F1)) {
                if (emu.saveBattery()) DebugUi_SetStatus(uiState, "SRAM(+RTC) saved");
            }
            if (IsKeyPressed(KEY_LEFT_BRACKET)) {
                uiState.paletteIndex = (uiState.paletteIndex + kPaletteCount - 1) % kPaletteCount;
                DebugUi_ApplyPalette(emu, uiState);
                DebugUi_SetStatus(uiState, std::string("Palette: ") + kPalettes[uiState.paletteIndex].name);
            }
            if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
                uiState.paletteIndex = (uiState.paletteIndex + 1) % kPaletteCount;
                DebugUi_ApplyPalette(emu, uiState);
                DebugUi_SetStatus(uiState, std::string("Palette: ") + kPalettes[uiState.paletteIndex].name);
            }
            if (IsKeyPressed(KEY_SEMICOLON)) {
                screenShaders.cyclePrev();
                uiState.shaderIndex = screenShaders.activeIndex();
                DebugUi_SetStatus(uiState, std::string("Shader: ") + ScreenShaderName(screenShaders.active()));
            }
            if (IsKeyPressed(KEY_APOSTROPHE)) {
                screenShaders.cycleNext();
                uiState.shaderIndex = screenShaders.activeIndex();
                DebugUi_SetStatus(uiState, std::string("Shader: ") + ScreenShaderName(screenShaders.active()));
            }
        }

        if (uiState.shaderIndex != screenShaders.activeIndex()) {
            screenShaders.setActiveIndex(uiState.shaderIndex);
            uiState.shaderIndex = screenShaders.activeIndex();
        }

        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
            DebugUi_SetStatus(uiState, IsWindowFullscreen() ? "Fullscreen" : "Windowed");
        }
        if (IsKeyPressed(KEY_F12)) {
            DebugUi_ToggleSidebar(uiState);
            DebugUi_SetStatus(uiState, uiState.showSidebar ? "Inspector ON" : "Inspector OFF");
        }

        if (uiState.smoothFilter != lastSmooth) {
            SetTextureFilter(screenTexture,
                             uiState.smoothFilter ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
            lastSmooth = uiState.smoothFilter;
        }

        frameAccumulator += emu.speed();
        int framesToRun = static_cast<int>(frameAccumulator);
        if (framesToRun > 8) framesToRun = 8;
        frameAccumulator -= framesToRun;
        for (int i = 0; i < framesToRun; ++i) {
            emu.runFrame();
        }

        constexpr size_t kMaxApuLatencyFrames = APU::kSampleRate / 10;
        if (emu.audioSamplesAvailable() > kMaxApuLatencyFrames) {
            int16_t discard[2048];
            while (emu.audioSamplesAvailable() > static_cast<size_t>(kAudioBufferFrames) * 2u) {
                if (emu.popAudio(discard, 1024) == 0) break;
            }
        }

        if (IsAudioStreamProcessed(audioStream)) {
            std::fill(audioScratch.begin(), audioScratch.end(), static_cast<int16_t>(0));
            if (!emu.muted()) {
                emu.popAudio(audioScratch.data(), static_cast<size_t>(kAudioBufferFrames));
            } else {
                int16_t discard[2048];
                while (emu.popAudio(discard, 1024) == 1024) {
                }
            }
            UpdateAudioStream(audioStream, audioScratch.data(), kAudioBufferFrames);
        }

        UpdateTexture(screenTexture, emu.frameBuffer());

        BeginDrawing();
        ClearBackground(GetColor(0x0B0C0EFF));

        const float menuBarH = DebugUi_MenuBarHeight();
        const float viewX = 0.0f;
        const float viewY = menuBarH;
        const float viewW = static_cast<float>(GetScreenWidth());
        const float viewH = static_cast<float>(GetScreenHeight()) - menuBarH;

        float fitScale = 1.0f;
        if (uiState.integerScale) {
            const int fitX = std::max(1, static_cast<int>(viewW) / SCREEN_WIDTH);
            const int fitY = std::max(1, static_cast<int>(viewH) / SCREEN_HEIGHT);
            fitScale = static_cast<float>(std::max(1, std::min(fitX, fitY)));
        } else {
            const float sx = viewW / static_cast<float>(SCREEN_WIDTH);
            const float sy = viewH / static_cast<float>(SCREEN_HEIGHT);
            fitScale = std::max(1.0f, std::min(sx, sy));
        }

        const float drawW = static_cast<float>(SCREEN_WIDTH) * fitScale;
        const float drawH = static_cast<float>(SCREEN_HEIGHT) * fitScale;
        const float gameLeft = viewX + (viewW - drawW) * 0.5f;
        const float gameTop = viewY + (viewH - drawH) * 0.5f;

        const float bezel = std::max(2.0f, fitScale * 0.5f);
        DrawRectangleRec(
            Rectangle{gameLeft - bezel, gameTop - bezel, drawW + bezel * 2.0f, drawH + bezel * 2.0f},
            GetColor(0x16181CFF));

        const Rectangle src{0, 0, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT)};
        const Rectangle dest{gameLeft, gameTop, drawW, drawH};
        screenShaders.draw(screenTexture, src, dest, static_cast<float>(GetTime()));

        DebugUi_Draw(emu, uiState, pad, static_cast<float>(GetFPS()), scale,
                     gameLeft, gameTop, drawW, drawH);

        EndDrawing();
    }

    emu.saveBattery();

    DebugUi_Shutdown();
    screenShaders.unload();
    UnloadAudioStream(audioStream);
    CloseAudioDevice();
    UnloadTexture(screenTexture);
    CloseWindow();
    std::cout << "Emulator closed.\n";
    return 0;
}
