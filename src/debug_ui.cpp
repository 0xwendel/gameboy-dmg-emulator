#include "debug_ui.hpp"
#include "palette.hpp"
#include "shaders.hpp"

#include "imgui.h"
#include "rlImGui.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

void DebugUi_ApplyPalette(Emulator& emu, DebugUiState& state);

namespace {

constexpr float kInspectorWidth = 380.0f;
constexpr float kInspectorPad = 10.0f;
constexpr float kMenuBarApprox = 22.0f;

const ImVec4 kAccent     = ImVec4(0.25f, 0.78f, 0.62f, 1.f);
const ImVec4 kAccentDim  = ImVec4(0.18f, 0.45f, 0.38f, 1.f);
const ImVec4 kWarn       = ImVec4(1.00f, 0.72f, 0.28f, 1.f);
const ImVec4 kDanger     = ImVec4(0.95f, 0.35f, 0.40f, 1.f);
const ImVec4 kInfo       = ImVec4(0.40f, 0.72f, 1.00f, 1.f);
const ImVec4 kMuted      = ImVec4(0.55f, 0.58f, 0.65f, 1.f);
const ImVec4 kCardBg     = ImVec4(0.12f, 0.13f, 0.16f, 1.f);
const ImVec4 kCardBorder = ImVec4(0.22f, 0.24f, 0.30f, 1.f);

const char* kPanelNames[] = {
    "Home", "CPU", "Video", "Audio", "Memory", "Cart", "Input", "Display"
};
constexpr int kPanelCount = 8;

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
    case PPU::ModeHBlank: return "HBlank";
    case PPU::ModeVBlank: return "VBlank";
    case PPU::ModeOAMSearch: return "OAM";
    case PPU::ModePixelTransfer: return "Transfer";
    default: return "?";
    }
}

void applyModernTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 10.0f;
    s.ChildRounding = 8.0f;
    s.FrameRounding = 6.0f;
    s.PopupRounding = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.GrabRounding = 6.0f;
    s.TabRounding = 6.0f;
    s.WindowPadding = ImVec2(14.0f, 12.0f);
    s.FramePadding = ImVec2(10.0f, 6.0f);
    s.ItemSpacing = ImVec2(10.0f, 8.0f);
    s.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    s.IndentSpacing = 16.0f;
    s.ScrollbarSize = 12.0f;
    s.WindowBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 1.0f;
    s.TabBorderSize = 0.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.09f, 0.10f, 0.12f, 0.98f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.11f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.11f, 0.14f, 0.98f);
    c[ImGuiCol_Border]               = ImVec4(0.22f, 0.24f, 0.30f, 0.60f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.22f, 0.30f, 0.28f, 1.00f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.09f, 0.11f, 0.98f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.09f, 0.10f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.40f, 0.48f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = kAccentDim;
    c[ImGuiCol_CheckMark]            = kAccent;
    c[ImGuiCol_SliderGrab]           = kAccent;
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.35f, 0.90f, 0.72f, 1.f);
    c[ImGuiCol_Button]               = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.40f, 0.36f, 1.00f);
    c[ImGuiCol_ButtonActive]         = kAccentDim;
    c[ImGuiCol_Header]               = ImVec4(0.16f, 0.28f, 0.26f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.38f, 0.34f, 1.00f);
    c[ImGuiCol_HeaderActive]         = kAccentDim;
    c[ImGuiCol_Separator]            = ImVec4(0.22f, 0.24f, 0.30f, 0.50f);
    c[ImGuiCol_Tab]                  = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.22f, 0.40f, 0.36f, 1.00f);
    c[ImGuiCol_TabSelected]          = ImVec4(0.16f, 0.32f, 0.28f, 1.00f);
    c[ImGuiCol_TabSelectedOverline]  = kAccent;
    c[ImGuiCol_TabDimmed]            = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.14f, 0.22f, 0.20f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled]         = kMuted;
}

void Badge(const char* label, ImVec4 bg, ImVec4 fg = ImVec4(1, 1, 1, 1)) {
    ImGui::PushStyleColor(ImGuiCol_Button, bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg);
    ImGui::PushStyleColor(ImGuiCol_Text, fg);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 3));
    ImGui::SmallButton(label);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void PillFlag(const char* label, bool on) {
    if (on) {
        Badge(label, ImVec4(0.15f, 0.45f, 0.32f, 1.f), kAccent);
    } else {
        Badge(label, ImVec4(0.16f, 0.17f, 0.20f, 1.f), kMuted);
    }
}

bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(-1, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.36f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.55f, 0.46f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentDim);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return pressed;
}

void BeginCard(const char* id) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
}

void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void CardTitle(const char* title) {
    ImGui::TextColored(kAccent, "%s", title);
    ImGui::Spacing();
}

void KvRow(const char* key, const char* fmt, ...) {
    ImGui::TextColored(kMuted, "%s", key);
    ImGui::SameLine(110.0f);
    char buf[128];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ImGui::TextUnformatted(buf);
}

void HexReg(const char* name, unsigned value, int width = 4) {
    ImGui::BeginGroup();
    ImGui::TextColored(kMuted, "%s", name);
    if (width >= 4) ImGui::Text("%04X", value & 0xFFFF);
    else ImGui::Text("%02X", value & 0xFF);
    ImGui::EndGroup();
}

void panelHome(Emulator& emu, DebugUiState& state, float hostFps, float gameW, float gameH) {
    const char* title = emu.cart().title().empty() ? "No cart" : emu.cart().title().c_str();

    BeginCard("home_status");
    ImGui::Text("%s", title);
    ImGui::SameLine();
    if (emu.paused()) Badge("PAUSED", ImVec4(0.45f, 0.30f, 0.10f, 1.f), kWarn);
    else Badge("RUNNING", ImVec4(0.12f, 0.38f, 0.28f, 1.f), kAccent);
    if (emu.muted()) {
        ImGui::SameLine();
        Badge("MUTE", ImVec4(0.40f, 0.15f, 0.18f, 1.f), kDanger);
    }
    ImGui::Spacing();
    ImGui::TextColored(kMuted, "%.0f FPS  ·  %.2fx  ·  %.0f×%.0f",
                       hostFps, emu.speed(), gameW, gameH);
    EndCard();

    BeginCard("home_transport");
    CardTitle("Controls");
    if (PrimaryButton(emu.paused() ? "Resume   P" : "Pause   P")) emu.togglePause();
    if (ImGui::Button("Reset   R", ImVec2(-1, 0))) {
        emu.reset();
        emu.loadBattery();
        DebugUi_SetStatus(state, "Reset");
    }
    if (ImGui::Button(emu.muted() ? "Unmute   M" : "Mute   M", ImVec2(-1, 0))) {
        emu.setMuted(!emu.muted());
    }
    ImGui::Spacing();
    float speed = emu.speed();
    if (ImGui::SliderFloat("Speed", &speed, 0.25f, 4.0f, "%.2fx")) emu.setSpeed(speed);
    if (ImGui::Button("0.5x")) emu.setSpeed(0.5f);
    ImGui::SameLine();
    if (ImGui::Button("1x")) emu.setSpeed(1.0f);
    ImGui::SameLine();
    if (ImGui::Button("2x")) emu.setSpeed(2.0f);
    EndCard();

    BeginCard("home_quick");
    CardTitle("Quick save");
    if (ImGui::Button("Save SRAM  F1", ImVec2(-1, 0))) {
        if (emu.saveBattery()) DebugUi_SetStatus(state, "SRAM saved");
    }
    if (ImGui::Button("Save State  F5", ImVec2(-1, 0))) {
        const std::string st = emu.cart().defaultSavePath() + ".state";
        if (emu.saveState(st)) DebugUi_SetStatus(state, "State saved");
    }
    if (ImGui::Button("Load State  F9", ImVec2(-1, 0))) {
        const std::string st = emu.cart().defaultSavePath() + ".state";
        if (emu.loadState(st)) DebugUi_SetStatus(state, "State loaded");
    }
    EndCard();

    BeginCard("home_snapshot");
    CardTitle("Live");
    const auto& r = emu.cpu().getRegs();
    KvRow("PC", "%04X", r.pc);
    KvRow("SP", "%04X", r.sp);
    KvRow("LY", "%u  %s", emu.ppu().ly(), ppuModeName(emu.ppu().mode()));
    const uint8_t nr52 = emu.mmu().readByte(0xFF26);
    KvRow("APU", "CH %c%c%c%c",
          (nr52 & 1) ? '1' : '-',
          (nr52 & 2) ? '2' : '-',
          (nr52 & 4) ? '3' : '-',
          (nr52 & 8) ? '4' : '-');
    KvRow("MBC", "%s%s", mbcTypeName(emu.cart().type()),
          emu.cart().hasRtc() ? " +RTC" : "");
    EndCard();

    if (state.statusTimer > 0.0f && !state.status.empty()) {
        BeginCard("home_toast");
        ImGui::TextColored(kAccent, "%s", state.status.c_str());
        EndCard();
    }
}

void panelCpu(Emulator& emu) {
    const auto& r = emu.cpu().getRegs();

    BeginCard("cpu_regs");
    CardTitle("Registers");
    if (ImGui::BeginTable("regs", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); HexReg("PC", r.pc);
        ImGui::TableNextColumn(); HexReg("SP", r.sp);
        ImGui::TableNextColumn(); HexReg("AF", r.af());
        ImGui::TableNextColumn(); HexReg("BC", r.bc());
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); HexReg("DE", r.de());
        ImGui::TableNextColumn(); HexReg("HL", r.hl());
        ImGui::TableNextColumn(); HexReg("A", r.a, 2);
        ImGui::TableNextColumn(); HexReg("F", r.f, 2);
        ImGui::EndTable();
    }
    EndCard();

    BeginCard("cpu_bytes");
    CardTitle("Bytes");
    ImGui::Text("A %02X   B %02X   C %02X   D %02X", r.a, r.b, r.c, r.d);
    ImGui::Text("E %02X   H %02X   L %02X   F %02X", r.e, r.h, r.l, r.f);
    EndCard();

    BeginCard("cpu_flags");
    CardTitle("Flags");
    PillFlag("Z", emu.cpu().getFlag(CPU::FlagZ)); ImGui::SameLine();
    PillFlag("N", emu.cpu().getFlag(CPU::FlagN)); ImGui::SameLine();
    PillFlag("H", emu.cpu().getFlag(CPU::FlagH)); ImGui::SameLine();
    PillFlag("C", emu.cpu().getFlag(CPU::FlagC));
    ImGui::Spacing();
    PillFlag("IME", emu.cpu().getIme()); ImGui::SameLine();
    PillFlag("HALT", emu.cpu().isHalted());
    EndCard();

    BeginCard("cpu_peek");
    CardTitle("Peek");
    KvRow("Opcode", "%02X @ %04X", emu.mmu().readByte(r.pc), r.pc);
    KvRow("[HL]", "%02X", emu.mmu().readByte(r.hl()));
    KvRow("[SP]", "%02X%02X",
          emu.mmu().readByte(static_cast<uint16_t>(r.sp + 1)),
          emu.mmu().readByte(r.sp));
    EndCard();
}

void panelVideo(Emulator& emu) {
    const auto& mmu = emu.mmu();
    const uint8_t lcdc = mmu.readByte(0xFF40);
    const uint8_t stat = mmu.readByte(0xFF41);

    BeginCard("vid_mode");
    CardTitle("PPU");
    KvRow("Mode", "%s", ppuModeName(emu.ppu().mode()));
    KvRow("LY / LYC", "%u / %u", mmu.readByte(0xFF44), mmu.readByte(0xFF45));
    KvRow("LCDC", "%02X", lcdc);
    KvRow("STAT", "%02X", stat);
    KvRow("DMA", "%s", mmu.dmaActive() ? "ACTIVE" : "idle");
    EndCard();

    BeginCard("vid_scroll");
    CardTitle("Scroll / Window");
    KvRow("SCX / SCY", "%u / %u", mmu.readByte(0xFF43), mmu.readByte(0xFF42));
    KvRow("WX / WY", "%u / %u", mmu.readByte(0xFF4B), mmu.readByte(0xFF4A));
    KvRow("BGP", "%02X", mmu.readByte(0xFF47));
    KvRow("OBP0/1", "%02X / %02X", mmu.readByte(0xFF48), mmu.readByte(0xFF49));
    EndCard();

    BeginCard("vid_lcdc");
    CardTitle("LCDC");
    PillFlag("LCD", (lcdc & 0x80) != 0); ImGui::SameLine();
    PillFlag("WIN", (lcdc & 0x20) != 0); ImGui::SameLine();
    PillFlag("OBJ", (lcdc & 0x02) != 0); ImGui::SameLine();
    PillFlag("BG", (lcdc & 0x01) != 0);
    ImGui::Spacing();
    KvRow("Sprites", "%s", (lcdc & 0x04) ? "8x16" : "8x8");
    KvRow("BG map", "%s", (lcdc & 0x08) ? "9C00" : "9800");
    KvRow("Tiles", "%s", (lcdc & 0x10) ? "8000" : "8800");
    KvRow("Win map", "%s", (lcdc & 0x40) ? "9C00" : "9800");
    EndCard();
}

void panelAudio(Emulator& emu) {
    const auto& mmu = emu.mmu();
    const uint8_t nr52 = mmu.readByte(0xFF26);
    const uint8_t nr51 = mmu.readByte(0xFF25);
    const uint8_t nr50 = mmu.readByte(0xFF24);

    BeginCard("apu_status");
    CardTitle("APU");
    PillFlag("POWER", (nr52 & 0x80) != 0); ImGui::SameLine();
    PillFlag("MUTE", emu.muted());
    ImGui::Spacing();
    KvRow("NR52", "%02X", nr52);
    KvRow("Rate", "%d Hz", APU::kSampleRate);
    EndCard();

    BeginCard("apu_ch");
    CardTitle("Channels");
    PillFlag("CH1", (nr52 & 0x01) != 0); ImGui::SameLine();
    PillFlag("CH2", (nr52 & 0x02) != 0); ImGui::SameLine();
    PillFlag("CH3", (nr52 & 0x04) != 0); ImGui::SameLine();
    PillFlag("CH4", (nr52 & 0x08) != 0);
    EndCard();

    BeginCard("apu_mix");
    CardTitle("Mixer");
    KvRow("NR51 pan", "%02X", nr51);
    KvRow("NR50", "%02X  L=%u R=%u", nr50, (nr50 >> 4) & 7, nr50 & 7);
    EndCard();

    BeginCard("apu_regs");
    CardTitle("Registers");
    ImGui::TextDisabled("CH1");
    ImGui::Text("%02X %02X %02X %02X %02X",
                mmu.readByte(0xFF10), mmu.readByte(0xFF11), mmu.readByte(0xFF12),
                mmu.readByte(0xFF13), mmu.readByte(0xFF14));
    ImGui::TextDisabled("CH2");
    ImGui::Text("%02X %02X %02X %02X",
                mmu.readByte(0xFF16), mmu.readByte(0xFF17),
                mmu.readByte(0xFF18), mmu.readByte(0xFF19));
    ImGui::TextDisabled("CH3");
    ImGui::Text("%02X %02X %02X %02X %02X",
                mmu.readByte(0xFF1A), mmu.readByte(0xFF1B), mmu.readByte(0xFF1C),
                mmu.readByte(0xFF1D), mmu.readByte(0xFF1E));
    ImGui::TextDisabled("CH4");
    ImGui::Text("%02X %02X %02X %02X",
                mmu.readByte(0xFF20), mmu.readByte(0xFF21),
                mmu.readByte(0xFF22), mmu.readByte(0xFF23));
    EndCard();
}

void panelMemory(Emulator& emu, DebugUiState& state) {
    BeginCard("mem_nav");
    CardTitle("Memory");
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputScalar("Addr", ImGuiDataType_S32, &state.memAddress,
                       nullptr, nullptr, "%04X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderInt("##n", &state.memBytes, 16, 256, "%d B");
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
    EndCard();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("memdump", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    const uint16_t base = static_cast<uint16_t>(state.memAddress);
    for (int row = 0; row < state.memBytes; row += 16) {
        char line[128];
        int pos = std::snprintf(line, sizeof(line), "%04X  ",
                                static_cast<unsigned>((base + row) & 0xFFFF));
        for (int col = 0; col < 16 && (row + col) < state.memBytes; ++col) {
            const uint8_t b = emu.mmu().readByte(static_cast<uint16_t>(base + row + col));
            pos += std::snprintf(line + pos, sizeof(line) - static_cast<size_t>(pos), "%02X ", b);
        }
        pos += std::snprintf(line + pos, sizeof(line) - static_cast<size_t>(pos), " ");
        for (int col = 0; col < 16 && (row + col) < state.memBytes; ++col) {
            const uint8_t b = emu.mmu().readByte(static_cast<uint16_t>(base + row + col));
            const char ch = (b >= 32 && b < 127) ? static_cast<char>(b) : '.';
            pos += std::snprintf(line + pos, sizeof(line) - static_cast<size_t>(pos), "%c", ch);
        }
        ImGui::TextUnformatted(line);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void panelCart(Emulator& emu, DebugUiState& state) {
    const auto& cart = emu.cart();
    BeginCard("cart_info");
    CardTitle("Cartridge");
    KvRow("Title", "%s", cart.title().empty() ? "(none)" : cart.title().c_str());
    KvRow("MBC", "%s", mbcTypeName(cart.type()));
    KvRow("Battery", "%s", cart.hasBattery() ? "yes" : "no");
    KvRow("RTC", "%s", cart.hasRtc() ? "yes" : "no");
    KvRow("Boot", "%s", emu.bootRomEnabled() ? "enabled" : "skip");
    if (!cart.romPath().empty()) {
        ImGui::Spacing();
        ImGui::TextColored(kMuted, "ROM");
        ImGui::TextWrapped("%s", cart.romPath().c_str());
    }
    ImGui::TextColored(kMuted, "Save");
    ImGui::TextWrapped("%s", cart.defaultSavePath().c_str());
    EndCard();

    BeginCard("cart_actions");
    CardTitle("Actions");
    if (PrimaryButton("Save SRAM  F1")) {
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
    EndCard();

    BeginCard("sys_timer");
    CardTitle("Timer / IRQs");
    auto& mmu = emu.mmu();
    KvRow("DIV", "%02X  (%04X)", mmu.readByte(0xFF04), mmu.divCounter());
    KvRow("TIMA/TMA", "%02X / %02X", mmu.readByte(0xFF05), mmu.readByte(0xFF06));
    KvRow("TAC", "%02X", mmu.readByte(0xFF07));
    KvRow("IE / IF", "%02X / %02X", mmu.readByte(0xFFFF), mmu.readByte(0xFF0F));
    EndCard();
}

void panelInput(const DebugUiInput& input, Emulator& emu) {
    BeginCard("pad_state");
    CardTitle("Joypad");
    KvRow("P1", "%02X", emu.mmu().readByte(0xFF00));
    ImGui::Spacing();
    ImGui::TextColored(kMuted, "D-Pad");
    PillFlag("Up", input.keyUp); ImGui::SameLine();
    PillFlag("Down", input.keyDown); ImGui::SameLine();
    PillFlag("Left", input.keyLeft); ImGui::SameLine();
    PillFlag("Right", input.keyRight);
    ImGui::Spacing();
    ImGui::TextColored(kMuted, "Buttons");
    PillFlag("A", input.keyA); ImGui::SameLine();
    PillFlag("B", input.keyB); ImGui::SameLine();
    PillFlag("Select", input.keySelect); ImGui::SameLine();
    PillFlag("Start", input.keyStart);
    EndCard();

    BeginCard("pad_device");
    CardTitle("Gamepad");
    if (input.gamepadConnected) {
        Badge("CONNECTED", ImVec4(0.12f, 0.38f, 0.28f, 1.f), kAccent);
        if (input.gamepadName) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", input.gamepadName);
        }
        ImGui::TextColored(kMuted, "A/Y = A · B/X = B · Start/Back");
    } else {
        Badge("NO PAD", ImVec4(0.20f, 0.20f, 0.24f, 1.f), kMuted);
        ImGui::Spacing();
        ImGui::TextColored(kMuted, "Connect an Xbox 360 / XInput controller");
    }
    EndCard();

    BeginCard("pad_keys");
    CardTitle("Keyboard");
    ImGui::TextColored(kMuted, "WASD / Arrows · Z/K = A · X/J = B");
    ImGui::TextColored(kMuted, "Enter = Start · Space = Select");
    EndCard();
}

void panelDisplay(Emulator& emu, DebugUiState& state, int scale, float gameW, float gameH) {
    BeginCard("disp_info");
    CardTitle("Viewport");
    KvRow("Scale", "%dx  (%.0f×%.0f)", scale, gameW, gameH);
    KvRow("Integer", "%s", state.integerScale ? "on" : "off");
    KvRow("Filter", "%s", state.smoothFilter ? "bilinear" : "point");
    EndCard();

    BeginCard("disp_palette");
    CardTitle("Palette");
    if (ImGui::BeginCombo("##pal", kPalettes[state.paletteIndex].name)) {
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
    for (int i = 0; i < 4; ++i) {
        const uint32_t c = kPalettes[state.paletteIndex].colors[i];
        ImGui::ColorButton(("##sw" + std::to_string(i)).c_str(),
                           ImVec4(((c) & 0xFF) / 255.f,
                                  ((c >> 8) & 0xFF) / 255.f,
                                  ((c >> 16) & 0xFF) / 255.f, 1.f),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(36, 22));
        if (i < 3) ImGui::SameLine();
    }
    EndCard();

    BeginCard("disp_shader");
    CardTitle("Shader");
    if (ImGui::BeginCombo("##sh", ScreenShaderName(static_cast<ScreenShaderId>(state.shaderIndex)))) {
        for (int i = 0; i < ScreenShaderCount(); ++i) {
            const bool sel = (i == state.shaderIndex);
            if (ImGui::Selectable(ScreenShaderName(static_cast<ScreenShaderId>(i)), sel)) {
                state.shaderIndex = i;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Checkbox("Smooth filter", &state.smoothFilter);
    ImGui::Checkbox("Integer scale", &state.integerScale);
    EndCard();

    BeginCard("disp_keys");
    CardTitle("Shortcuts");
    ImGui::BulletText("P pause · R reset · M mute");
    ImGui::BulletText("[ ] palette · ; ' shader");
    ImGui::BulletText("F5/F9 state · F1 SRAM");
    ImGui::BulletText("F11 fullscreen · F12 sidebar");
    EndCard();
}

void drawAboutModal(DebugUiState& state) {
    if (!state.showAbout) return;

    ImGui::OpenPopup("About##about_modal");
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("About##about_modal", &state.showAbout,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(kAccent, "GB DMG Emulator");
        ImGui::Text("Version 0.4.1");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "A Game Boy (DMG) emulator written in C++20, "
            "with raylib, Dear ImGui, and embedded GLSL shaders.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Author");
        ImGui::SameLine(100.0f);
        ImGui::Text("0xwendel");
        ImGui::Text("License");
        ImGui::SameLine(100.0f);
        ImGui::Text("GPL-3.0");
        ImGui::Text("Repo");
        ImGui::SameLine(100.0f);
        ImGui::TextWrapped("github.com/0xwendel/gameboy-dmg-emulator");
        ImGui::Spacing();
        ImGui::TextColored(kMuted, "No commercial ROMs included.");
        ImGui::Spacing();
        if (PrimaryButton("Close", ImVec2(120, 0))) {
            state.showAbout = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (state.showAbout && !ImGui::IsPopupOpen("About##about_modal")) {
        ImGui::OpenPopup("About##about_modal");
    }
}

void drawMenuBar(Emulator& emu, DebugUiState& state, float hostFps, float screenW) {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Inspector", "F12", &state.showSidebar);
        ImGui::MenuItem("Smooth filter", nullptr, &state.smoothFilter);
        ImGui::MenuItem("Integer scale", nullptr, &state.integerScale);
        ImGui::Separator();
        if (ImGui::BeginMenu("Palette")) {
            for (int i = 0; i < kPaletteCount; ++i) {
                if (ImGui::MenuItem(kPalettes[i].name, nullptr, state.paletteIndex == i)) {
                    state.paletteIndex = i;
                    DebugUi_ApplyPalette(emu, state);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Shader")) {
            for (int i = 0; i < ScreenShaderCount(); ++i) {
                if (ImGui::MenuItem(ScreenShaderName(static_cast<ScreenShaderId>(i)),
                                    nullptr, state.shaderIndex == i)) {
                    state.shaderIndex = i;
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
            DebugUi_SetStatus(state, "Reset");
        }
        if (ImGui::MenuItem(emu.muted() ? "Unmute" : "Mute", "M")) emu.setMuted(!emu.muted());
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
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {
            state.showAbout = true;
        }
        ImGui::EndMenu();
    }

    const char* title = emu.cart().title().empty() ? "GB DMG" : emu.cart().title().c_str();
    char fpsBuf[32];
    std::snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", hostFps);

    const float pad = 10.0f;
    float rightX = screenW - pad;
    auto placeBadge = [&](const char* text, ImVec4 bg, ImVec4 fg) {
        const float w = ImGui::CalcTextSize(text).x + 22.0f;
        rightX -= w + 6.0f;
        ImGui::SameLine(rightX);
        Badge(text, bg, fg);
    };

    if (emu.muted()) placeBadge("MUTE", ImVec4(0.40f, 0.15f, 0.18f, 1.f), kDanger);
    placeBadge(emu.paused() ? "PAUSED" : "RUN",
               emu.paused() ? ImVec4(0.45f, 0.30f, 0.10f, 1.f) : ImVec4(0.12f, 0.38f, 0.28f, 1.f),
               emu.paused() ? kWarn : kAccent);
    placeBadge(fpsBuf, ImVec4(0.16f, 0.18f, 0.22f, 1.f), kInfo);

    const float titleW = ImGui::CalcTextSize(title).x;
    rightX -= titleW + 16.0f;
    ImGui::SameLine(rightX);
    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.92f, 1.f), "%s", title);

    ImGui::EndMainMenuBar();
}

} // namespace

void DebugUi_ApplyPalette(Emulator& emu, DebugUiState& state) {
    if (state.paletteIndex < 0) state.paletteIndex = 0;
    if (state.paletteIndex >= kPaletteCount) state.paletteIndex = kPaletteCount - 1;
    emu.ppu().setPalette(kPalettes[state.paletteIndex].colors);
}

void DebugUi_Init() {
    rlImGuiSetup(true);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    applyModernTheme();
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

float DebugUi_SidebarWidth(const DebugUiState& /*state*/) {
    return 0.0f;
}

float DebugUi_MenuBarHeight() {
    return kMenuBarApprox;
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
    if (state.panel < 0 || state.panel >= kPanelCount) state.panel = 0;

    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    const float menuH = ImGui::GetFrameHeight();

    drawMenuBar(emu, state, hostFps, screenW);
    drawAboutModal(state);

    if (!state.showSidebar) {
        rlImGuiEnd();
        return;
    }

    const float sideW = kInspectorWidth;
    const float maxH = screenH - menuH - kInspectorPad * 2.0f;
    const float prefH = std::min(maxH, 720.0f);

    ImGui::SetNextWindowPos(
        ImVec2(screenW - sideW - kInspectorPad, menuH + kInspectorPad),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(sideW, prefH), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 280.0f),
                                        ImVec2(sideW + 80.0f, maxH));
    ImGui::SetNextWindowBgAlpha(0.94f);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("Inspector", &state.showSidebar, flags)) {

        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));
        const float btnW = (ImGui::GetContentRegionAvail().x - 6.0f * 3.0f) / 4.0f;
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 4; ++col) {
                const int i = row * 4 + col;
                if (col > 0) ImGui::SameLine();
                const bool selected = (state.panel == i);
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.36f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.50f, 0.42f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                }
                if (ImGui::Button(kPanelNames[i], ImVec2(btnW, 30.0f))) {
                    state.panel = i;
                }
                if (selected) ImGui::PopStyleColor(3);
            }
        }
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("##panel_body", ImVec2(0, 0), ImGuiChildFlags_None);
        switch (state.panel) {
        case 0: panelHome(emu, state, hostFps, gameW, gameH); break;
        case 1: panelCpu(emu); break;
        case 2: panelVideo(emu); break;
        case 3: panelAudio(emu); break;
        case 4: panelMemory(emu, state); break;
        case 5: panelCart(emu, state); break;
        case 6: panelInput(input, emu); break;
        case 7: panelDisplay(emu, state, scale, gameW, gameH); break;
        default: break;
        }
        ImGui::EndChild();
    }
    ImGui::End();

    rlImGuiEnd();
}
