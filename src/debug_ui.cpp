#include "debug_ui.hpp"
#include "palette.hpp"

#include "imgui.h"
#include "rlImGui.h"

#include <cstdio>
#include <string>

// Forward (definida após o namespace anônimo)
void DebugUi_ApplyPalette(Emulator& emu, DebugUiState& state);

namespace {

constexpr float kSidebarWidth = 380.0f;
constexpr float kSidebarPad = 6.0f;

const char* mbcTypeName(Cartridge::MbcType t) {
    switch (t) {
    case Cartridge::MbcType::None: return "ROM Only";
    case Cartridge::MbcType::MBC1: return "MBC1";
    case Cartridge::MbcType::MBC2: return "MBC2";
    case Cartridge::MbcType::MBC3: return "MBC3";
    case Cartridge::MbcType::MBC5: return "MBC5";
    default: return "Unknown";
    }
}

const char* ppuModeName(PPU::Mode m) {
    switch (m) {
    case PPU::ModeHBlank: return "HBlank (0)";
    case PPU::ModeVBlank: return "VBlank (1)";
    case PPU::ModeOAMSearch: return "OAM Search (2)";
    case PPU::ModePixelTransfer: return "Pixel Transfer (3)";
    default: return "?";
    }
}

void flagChip(const char* label, bool on) {
    ImGui::TextColored(on ? ImVec4(0.35f, 0.95f, 0.45f, 1.f)
                          : ImVec4(0.45f, 0.45f, 0.45f, 1.f),
                       "%s", label);
}

void sectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::SeparatorText(label);
}

// -------------------- Abas --------------------

void tabCpu(Emulator& emu) {
    const auto& r = emu.cpu().getRegs();
    const bool z = emu.cpu().getFlag(CPU::FlagZ);
    const bool n = emu.cpu().getFlag(CPU::FlagN);
    const bool h = emu.cpu().getFlag(CPU::FlagH);
    const bool c = emu.cpu().getFlag(CPU::FlagC);

    if (ImGui::BeginTable("cpu_regs", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("PC  %04X", r.pc);
        ImGui::TableNextColumn();
        ImGui::Text("SP  %04X", r.sp);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("AF  %04X", r.af());
        ImGui::TableNextColumn();
        ImGui::Text("BC  %04X", r.bc());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("DE  %04X", r.de());
        ImGui::TableNextColumn();
        ImGui::Text("HL  %04X", r.hl());
        ImGui::EndTable();
    }

    sectionHeader("Bytes");
    ImGui::Text("A %02X  F %02X  B %02X  C %02X", r.a, r.f, r.b, r.c);
    ImGui::Text("D %02X  E %02X  H %02X  L %02X", r.d, r.e, r.h, r.l);

    sectionHeader("Flags / Estado");
    flagChip("Z", z); ImGui::SameLine();
    flagChip("N", n); ImGui::SameLine();
    flagChip("H", h); ImGui::SameLine();
    flagChip("C", c);
    ImGui::Text("IME  %s     HALT  %s",
                emu.cpu().getIme() ? "ON" : "OFF",
                emu.cpu().isHalted() ? "yes" : "no");

    sectionHeader("Peek");
    const uint8_t op = emu.mmu().readByte(r.pc);
    ImGui::Text("Opcode @PC   %02X", op);
    ImGui::Text("[HL] %02X     [SP] %02X%02X",
                emu.mmu().readByte(r.hl()),
                emu.mmu().readByte(static_cast<uint16_t>(r.sp + 1)),
                emu.mmu().readByte(r.sp));
}

void tabPpu(Emulator& emu) {
    const auto& mmu = emu.mmu();
    const uint8_t lcdc = mmu.readByte(0xFF40);
    const uint8_t stat = mmu.readByte(0xFF41);
    const uint8_t scy = mmu.readByte(0xFF42);
    const uint8_t scx = mmu.readByte(0xFF43);
    const uint8_t ly = mmu.readByte(0xFF44);
    const uint8_t lyc = mmu.readByte(0xFF45);
    const uint8_t bgp = mmu.readByte(0xFF47);
    const uint8_t obp0 = mmu.readByte(0xFF48);
    const uint8_t obp1 = mmu.readByte(0xFF49);
    const uint8_t wy = mmu.readByte(0xFF4A);
    const uint8_t wx = mmu.readByte(0xFF4B);

    ImGui::Text("Mode   %s", ppuModeName(emu.ppu().mode()));
    ImGui::Text("LY     %3u   (int %u)    LYC  %3u", ly, emu.ppu().ly(), lyc);
    ImGui::Text("LCDC   %02X              STAT %02X", lcdc, stat);

    sectionHeader("Scroll / Window");
    ImGui::Text("SCX/SCY   %u / %u", scx, scy);
    ImGui::Text("WX/WY     %u / %u", wx, wy);
    ImGui::Text("BGP %02X   OBP0 %02X   OBP1 %02X", bgp, obp0, obp1);

    sectionHeader("LCDC bits");
    ImGui::Text("LCD        %s", (lcdc & 0x80) ? "ON" : "OFF");
    ImGui::Text("Window     %s", (lcdc & 0x20) ? "ON" : "OFF");
    ImGui::Text("Sprites    %s  (%s)",
                (lcdc & 0x02) ? "ON" : "OFF",
                (lcdc & 0x04) ? "8x16" : "8x8");
    ImGui::Text("BG map     %s", (lcdc & 0x08) ? "9C00" : "9800");
    ImGui::Text("Tile data  %s", (lcdc & 0x10) ? "8000 unsigned" : "8800 signed");
    ImGui::Text("Win map    %s", (lcdc & 0x40) ? "9C00" : "9800");
    ImGui::Text("DMA        %s", mmu.dmaActive() ? "ACTIVE" : "idle");
}

void tabTimer(Emulator& emu) {
    auto& mmu = emu.mmu();
    const uint16_t divFull = mmu.divCounter();
    const uint8_t div = mmu.readByte(0xFF04);
    const uint8_t tima = mmu.readByte(0xFF05);
    const uint8_t tma = mmu.readByte(0xFF06);
    const uint8_t tac = mmu.readByte(0xFF07);
    const uint8_t ie = mmu.readByte(0xFFFF);
    const uint8_t iff = mmu.readByte(0xFF0F);

    const char* tacHz = "4096 Hz";
    switch (tac & 0x03) {
    case 0: tacHz = "4096 Hz"; break;
    case 1: tacHz = "262144 Hz"; break;
    case 2: tacHz = "65536 Hz"; break;
    case 3: tacHz = "16384 Hz"; break;
    }

    sectionHeader("Timer");
    ImGui::Text("DIV   FF04  %02X   (int %04X)", div, divFull);
    ImGui::Text("TIMA  FF05  %02X", tima);
    ImGui::Text("TMA   FF06  %02X", tma);
    ImGui::Text("TAC   FF07  %02X  %s  %s",
                tac, (tac & 0x04) ? "enabled" : "disabled", tacHz);

    sectionHeader("Interrupts");
    ImGui::Text("IE FFFF  %02X     IF FF0F  %02X", ie, iff);

    if (ImGui::BeginTable("irqs", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("IRQ");
        ImGui::TableSetupColumn("IE", ImGuiTableColumnFlags_WidthFixed, 36.0f);
        ImGui::TableSetupColumn("IF", ImGuiTableColumnFlags_WidthFixed, 36.0f);
        ImGui::TableHeadersRow();

        auto row = [](const char* name, uint8_t mask, uint8_t iev, uint8_t ifv) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name);
            ImGui::TableNextColumn();
            ImGui::Text("%s", (iev & mask) ? "1" : "0");
            ImGui::TableNextColumn();
            const bool pend = (ifv & mask) != 0;
            if (pend) ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "1");
            else ImGui::Text("0");
        };
        row("VBlank", 0x01, ie, iff);
        row("LCD STAT", 0x02, ie, iff);
        row("Timer", 0x04, ie, iff);
        row("Serial", 0x08, ie, iff);
        row("Joypad", 0x10, ie, iff);
        ImGui::EndTable();
    }
}

void tabCart(Emulator& emu, DebugUiState& state) {
    const auto& cart = emu.cart();
    ImGui::Text("Title    %s", cart.title().empty() ? "(none)" : cart.title().c_str());
    ImGui::Text("MBC      %s", mbcTypeName(cart.type()));
    ImGui::Text("Battery  %s", cart.hasBattery() ? "yes" : "no");
    ImGui::Text("RTC      %s", cart.hasRtc() ? "yes" : "no");
    ImGui::Text("Boot ROM %s", emu.bootRomEnabled() ? "active path" : "skip (post-boot)");
    if (!cart.romPath().empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("ROM: %s", cart.romPath().c_str());
    }
    ImGui::TextWrapped("Save: %s", cart.defaultSavePath().c_str());

    sectionHeader("Actions");
    if (ImGui::Button("Save SRAM  F1", ImVec2(-1, 0))) {
        if (emu.saveBattery()) DebugUi_SetStatus(state, "SRAM saved");
    }
    if (ImGui::Button("Save State  F5", ImVec2(-1, 0))) {
        const std::string st = cart.defaultSavePath() + ".state";
        if (emu.saveState(st)) DebugUi_SetStatus(state, "State saved");
    }
    if (ImGui::Button("Load State  F9", ImVec2(-1, 0))) {
        const std::string st = cart.defaultSavePath() + ".state";
        if (emu.loadState(st)) DebugUi_SetStatus(state, "State loaded");
    }
}

void tabJoypad(const DebugUiInput& input, Emulator& emu) {
    const uint8_t joy = emu.mmu().readByte(0xFF00);
    ImGui::Text("P1 (FF00)  %02X", joy);

    sectionHeader("D-Pad");
    auto chip = [](const char* label, bool on) {
        if (on) ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.f), "[%s]", label);
        else ImGui::TextDisabled("[%s]", label);
        ImGui::SameLine();
    };
    chip("Up", input.keyUp);
    chip("Down", input.keyDown);
    chip("Left", input.keyLeft);
    chip("Right", input.keyRight);
    ImGui::NewLine();

    sectionHeader("Buttons");
    chip("A", input.keyA);
    chip("B", input.keyB);
    chip("Select", input.keySelect);
    chip("Start", input.keyStart);
    ImGui::NewLine();

    ImGui::Spacing();
    ImGui::TextDisabled("WASD/Arrows  Z/K=A  X/J=B");
    ImGui::TextDisabled("Enter=Start  Backspace/Space=Select");
}

void tabApu(Emulator& emu) {
    const auto& mmu = emu.mmu();
    const uint8_t nr52 = mmu.readByte(0xFF26);
    const uint8_t nr51 = mmu.readByte(0xFF25);
    const uint8_t nr50 = mmu.readByte(0xFF24);

    ImGui::Text("Host mute   %s", emu.muted() ? "YES" : "no");
    ImGui::Text("APU master  %s", emu.apu().enabled() ? "enabled" : "disabled");
    ImGui::Text("NR52 %02X     power %s", nr52, (nr52 & 0x80) ? "ON" : "OFF");

    sectionHeader("Channels");
    auto ch = [](const char* name, bool on) {
        ImGui::Text("%s  ", name);
        ImGui::SameLine();
        if (on) ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.f), "on");
        else ImGui::TextDisabled("off");
    };
    ch("CH1 square", (nr52 & 0x01) != 0);
    ch("CH2 square", (nr52 & 0x02) != 0);
    ch("CH3 wave  ", (nr52 & 0x04) != 0);
    ch("CH4 noise ", (nr52 & 0x08) != 0);

    sectionHeader("Mixer");
    ImGui::Text("NR51 pan   %02X", nr51);
    ImGui::Text("NR50 vol   %02X   L=%u  R=%u", nr50, (nr50 >> 4) & 0x7, nr50 & 0x7);
    ImGui::Text("Rate       %d Hz", APU::kSampleRate);

    sectionHeader("Registers");
    ImGui::Text("CH1  %02X %02X %02X %02X %02X",
                mmu.readByte(0xFF10), mmu.readByte(0xFF11), mmu.readByte(0xFF12),
                mmu.readByte(0xFF13), mmu.readByte(0xFF14));
    ImGui::Text("CH2     %02X %02X %02X %02X",
                mmu.readByte(0xFF16), mmu.readByte(0xFF17),
                mmu.readByte(0xFF18), mmu.readByte(0xFF19));
    ImGui::Text("CH3  %02X %02X %02X %02X %02X",
                mmu.readByte(0xFF1A), mmu.readByte(0xFF1B), mmu.readByte(0xFF1C),
                mmu.readByte(0xFF1D), mmu.readByte(0xFF1E));
    ImGui::Text("CH4     %02X %02X %02X %02X",
                mmu.readByte(0xFF20), mmu.readByte(0xFF21),
                mmu.readByte(0xFF22), mmu.readByte(0xFF23));
}

void tabMemory(Emulator& emu, DebugUiState& state) {
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputScalar("Addr", ImGuiDataType_S32, &state.memAddress,
                       nullptr, nullptr, "%04X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderInt("##bytes", &state.memBytes, 16, 256, "%d bytes");
    state.memAddress &= 0xFFFF;
    if (state.memBytes < 16) state.memBytes = 16;

    if (ImGui::Button("WRAM")) state.memAddress = 0xC000;
    ImGui::SameLine();
    if (ImGui::Button("HRAM")) state.memAddress = 0xFF80;
    ImGui::SameLine();
    if (ImGui::Button("IO")) state.memAddress = 0xFF00;
    ImGui::SameLine();
    if (ImGui::Button("VRAM")) state.memAddress = 0x8000;
    ImGui::SameLine();
    if (ImGui::Button("OAM")) state.memAddress = 0xFE00;

    ImGui::Separator();
    ImGui::BeginChild("memdump", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushFont(ImGui::GetFont()); // mono-ish if available; default ok
    const uint16_t base = static_cast<uint16_t>(state.memAddress);
    const int count = state.memBytes;
    for (int row = 0; row < count; row += 16) {
        char line[128];
        int pos = std::snprintf(line, sizeof(line), "%04X:",
                                static_cast<unsigned>((base + row) & 0xFFFF));
        for (int col = 0; col < 16 && (row + col) < count; ++col) {
            const uint16_t addr = static_cast<uint16_t>(base + row + col);
            const uint8_t b = emu.mmu().readByte(addr);
            pos += std::snprintf(line + pos, sizeof(line) - static_cast<size_t>(pos),
                                 " %02X", b);
        }
        pos += std::snprintf(line + pos, sizeof(line) - static_cast<size_t>(pos), "  ");
        for (int col = 0; col < 16 && (row + col) < count; ++col) {
            const uint16_t addr = static_cast<uint16_t>(base + row + col);
            const uint8_t b = emu.mmu().readByte(addr);
            const char ch = (b >= 32 && b < 127) ? static_cast<char>(b) : '.';
            pos += std::snprintf(line + pos, sizeof(line) - static_cast<size_t>(pos), "%c", ch);
        }
        ImGui::TextUnformatted(line);
    }
    ImGui::PopFont();
    ImGui::EndChild();
}

void tabEmulation(Emulator& emu, DebugUiState& state, float hostFps, int scale,
                  float gameW, float gameH) {
    ImGui::Text("FPS      %.0f", hostFps);
    ImGui::Text("Scale    %dx  (%.0fx%.0f px)", scale, gameW, gameH);
    ImGui::Text("Speed    %.2fx", emu.speed());

    if (emu.paused()) {
        ImGui::TextColored(ImVec4(1.f, 0.65f, 0.2f, 1.f), "PAUSED");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.f, 1.f), "Running");
    }
    if (emu.muted()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "  MUTE");
    }

    sectionHeader("Controls");
    if (ImGui::Button(emu.paused() ? "Resume  P" : "Pause  P", ImVec2(-1, 0))) {
        emu.togglePause();
    }
    if (ImGui::Button("Reset  R", ImVec2(-1, 0))) {
        emu.reset();
        emu.loadBattery();
        DebugUi_SetStatus(state, "Reset + battery reload");
    }
    if (ImGui::Button(emu.muted() ? "Unmute  M" : "Mute  M", ImVec2(-1, 0))) {
        emu.setMuted(!emu.muted());
    }

    float speed = emu.speed();
    if (ImGui::SliderFloat("Speed##slider", &speed, 0.25f, 8.0f, "%.2fx")) {
        emu.setSpeed(speed);
    }
    if (ImGui::Button("0.5x")) emu.setSpeed(0.5f);
    ImGui::SameLine();
    if (ImGui::Button("1x")) emu.setSpeed(1.0f);
    ImGui::SameLine();
    if (ImGui::Button("2x")) emu.setSpeed(2.0f);
    ImGui::SameLine();
    if (ImGui::Button("4x")) emu.setSpeed(4.0f);

    if (state.statusTimer > 0.0f && !state.status.empty()) {
        sectionHeader("Status");
        ImGui::TextColored(ImVec4(0.6f, 1.f, 0.6f, 1.f), "%s", state.status.c_str());
    }

    sectionHeader("Display");
    if (ImGui::BeginCombo("Palette", kPalettes[state.paletteIndex].name)) {
        for (int i = 0; i < kPaletteCount; ++i) {
            const bool sel = (i == state.paletteIndex);
            if (ImGui::Selectable(kPalettes[i].name, sel)) {
                state.paletteIndex = i;
                DebugUi_ApplyPalette(emu, state);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    // Preview de cores
    for (int i = 0; i < 4; ++i) {
        const uint32_t c = kPalettes[state.paletteIndex].colors[i];
        ImGui::ColorButton(("##p" + std::to_string(i)).c_str(),
                           ImVec4(((c >> 0) & 0xFF) / 255.f,
                                  ((c >> 8) & 0xFF) / 255.f,
                                  ((c >> 16) & 0xFF) / 255.f, 1.f),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(28, 18));
        if (i < 3) ImGui::SameLine();
    }
    ImGui::Checkbox("Smooth filter", &state.smoothFilter);
    ImGui::Checkbox("Integer scale", &state.integerScale);

    sectionHeader("Hotkeys");
    ImGui::BulletText("P pause   R reset   M mute");
    ImGui::BulletText("1 / 2  speed   [ ] palette");
    ImGui::BulletText("F5/F9 state  F1 SRAM");
    ImGui::BulletText("F11 fullscreen  F12 sidebar");
}

} // namespace

void DebugUi_ApplyPalette(Emulator& emu, DebugUiState& state) {
    if (state.paletteIndex < 0) state.paletteIndex = 0;
    if (state.paletteIndex >= kPaletteCount) state.paletteIndex = kPaletteCount - 1;
    emu.ppu().setPalette(kPalettes[state.paletteIndex].colors);
}

void DebugUi_Init() {
    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Sidebar densa e legível
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowPadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
}

void DebugUi_Shutdown() {
    rlImGuiShutdown();
}

void DebugUi_SetStatus(DebugUiState& state, const std::string& msg) {
    state.status = msg;
    state.statusTimer = 3.0f;
}

void DebugUi_ToggleSidebar(DebugUiState& state) {
    state.showSidebar = !state.showSidebar;
}

float DebugUi_SidebarWidth(const DebugUiState& state) {
    return state.showSidebar ? (kSidebarWidth + kSidebarPad * 2.0f) : 0.0f;
}

bool DebugUi_WantCaptureKeyboard() {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool DebugUi_WantCaptureMouse() {
    return ImGui::GetIO().WantCaptureMouse;
}

void DebugUi_Draw(Emulator& emu, DebugUiState& state, const DebugUiInput& input,
                  float hostFps, int scale, float gameLeft, float gameTop,
                  float gameW, float gameH) {
    (void)gameLeft;
    (void)gameTop;

    rlImGuiBegin();

    const float dt = GetFrameTime();
    if (state.statusTimer > 0.0f) {
        state.statusTimer -= dt;
        if (state.statusTimer < 0.0f) state.statusTimer = 0.0f;
    }

    const float menuH = ImGui::GetFrameHeight();
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());

    // ---- Menu bar compacta ----
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Debug sidebar", "F12", &state.showSidebar);
            ImGui::MenuItem("Smooth filter", nullptr, &state.smoothFilter);
            ImGui::MenuItem("Integer scale", nullptr, &state.integerScale);
            if (ImGui::BeginMenu("Palette")) {
                for (int i = 0; i < kPaletteCount; ++i) {
                    if (ImGui::MenuItem(kPalettes[i].name, nullptr, state.paletteIndex == i)) {
                        state.paletteIndex = i;
                        DebugUi_ApplyPalette(emu, state);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulator")) {
            if (ImGui::MenuItem(emu.paused() ? "Resume" : "Pause", "P")) emu.togglePause();
            if (ImGui::MenuItem("Reset", "R")) {
                emu.reset();
                emu.loadBattery();
                DebugUi_SetStatus(state, "Reset + battery reload");
            }
            if (ImGui::MenuItem(emu.muted() ? "Unmute" : "Mute", "M")) {
                emu.setMuted(!emu.muted());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save SRAM", "F1")) {
                if (emu.saveBattery()) DebugUi_SetStatus(state, "SRAM saved");
            }
            if (ImGui::MenuItem("Save State", "F5")) {
                const std::string st = emu.cart().defaultSavePath() + ".state";
                if (emu.saveState(st)) DebugUi_SetStatus(state, "State saved");
            }
            if (ImGui::MenuItem("Load State", "F9")) {
                const std::string st = emu.cart().defaultSavePath() + ".state";
                if (emu.loadState(st)) DebugUi_SetStatus(state, "State loaded");
            }
            ImGui::EndMenu();
        }

        // Status à direita na menu bar
        const char* title = emu.cart().title().empty() ? "GB DMG" : emu.cart().title().c_str();
        char right[128];
        std::snprintf(right, sizeof(right), "%s  |  %.0f FPS  |  %s%s",
                      title, hostFps,
                      emu.paused() ? "PAUSED" : "RUN",
                      emu.muted() ? "  MUTE" : "");
        const float rightW = ImGui::CalcTextSize(right).x;
        ImGui::SetCursorPosX(screenW - rightW - 12.0f);
        ImGui::TextUnformatted(right);
        ImGui::EndMainMenuBar();
    }

    if (!state.showSidebar) {
        rlImGuiEnd();
        return;
    }

    // ---- Sidebar fixa à direita (não sobrepõe a tela GB) ----
    const float sideW = kSidebarWidth;
    const float x = screenW - sideW - kSidebarPad;
    const float y = menuH + kSidebarPad;
    const float h = screenH - menuH - kSidebarPad * 2.0f;

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sideW, h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("Debug", &state.showSidebar, flags)) {
        if (ImGui::BeginTabBar("##debug_tabs", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable)) {
            if (ImGui::BeginTabItem("CPU")) {
                tabCpu(emu);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("PPU")) {
                tabPpu(emu);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Timer")) {
                tabTimer(emu);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("APU")) {
                tabApu(emu);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Mem")) {
                tabMemory(emu, state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Cart")) {
                tabCart(emu, state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Pad")) {
                tabJoypad(input, emu);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Emu")) {
                tabEmulation(emu, state, hostFps, scale, gameW, gameH);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    rlImGuiEnd();
}
