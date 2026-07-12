# GB DMG Emulator

**English** · **[Português (BR)](README.pt-BR.md)**

A **Game Boy (DMG)** emulator written in C++20, with a **raylib** frontend, **Dear ImGui** debug inspector, and embedded GLSL screen shaders.

Built for real gameplay: CPU, PPU, timers, joypad, MBC cartridges, stereo APU, save states, MBC3 RTC, Xbox 360 pad support, and a centered native-aspect display with post-processing.

| | |
|---|---|
| **Version** | 0.4.0 |
| **Language** | C++20 |
| **Build** | CMake ≥ 3.20 (Ninja, Make, or MSVC — generator depends on the environment) |
| **Platform** | Windows (primary; dependencies are cross-platform) |
| **Repo** | [github.com/0xwendel/gameboy-dmg-emulator](https://github.com/0xwendel/gameboy-dmg-emulator) |

---

## Table of contents

- [Features](#features)
- [Requirements](#requirements)
- [Build](#build)
- [Run](#run)
- [Controls](#controls)
- [Inspector UI](#inspector-ui)
- [Shaders and palettes](#shaders-and-palettes)
- [Architecture](#architecture)
- [Tests](#tests)
- [Saves and states](#saves-and-states)
- [Boot ROM](#boot-rom)
- [Limitations](#limitations)
- [Legal](#legal)
- [License](#license)

---

## Features

### DMG core

| Subsystem | Details |
|-----------|---------|
| **CPU** | LR35902, flags, EI delay, HALT bug, interrupts |
| **MMU** | Memory map, VRAM/OAM locks, timed OAM DMA |
| **PPU** | BG, Window, 8×8/8×16 sprites, STAT, LYC, priority |
| **Timer** | DIV falling-edge, TIMA/TMA/TAC, reload delay |
| **APU** | 4 channels (2× square, wave, noise), DIV frame sequencer, high-pass, 44.1 kHz stereo |
| **Serial** | SB/SC, internal clock, open cable (`0xFF` + IRQ) |
| **Joypad** | Active-low bus, multiplexing, edge IRQ |

### Cartridges

- ROM-only, **MBC1**, **MBC2**, **MBC3** (+ **RTC**), **MBC5**
- Battery-backed SRAM (`.sav` file)
- MBC3 RTC with latch and a 48-byte trailer on the save file (e.g. clock-using titles)

### Frontend

- **Centered** display (letterboxing), native 160×144 aspect ratio
- Integer or smooth scaling
- Floating **Inspector** overlay (does not resize the game viewport)
- DMG palettes (Green, Greyscale, Pocket, Brown, Blue, Inverted)
- Shaders: None, Scanlines, LCD Grid, LCD Matrix, CRT, Soft Glow
- Fullscreen, mute, speed control, save/load state
- **Xbox 360 / XInput** (D-Pad, stick, A/B/X/Y, Start/Back, shoulders)
- Optional boot ROM (`dmg_boot.bin`, 256 bytes)

### Dependencies (FetchContent)

Fetched automatically on configure:

- [raylib](https://github.com/raysan5/raylib) **5.5**
- [Dear ImGui](https://github.com/ocornut/imgui) **v1.92.7**
- [rlImGui](https://github.com/raylib-extras/rlImGui) tag **Raylib_5_5**

---

## Requirements

### Software

- **CMake** ≥ 3.20  
- **C++20** compiler (MSVC, MinGW/GCC, Clang)  
- **Git** (for FetchContent)  
- OpenGL 3.3+ (desktop)  
- Network on the **first** configure (dependency download)

### Windows (MSYS2 UCRT64 example)

```bash
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja git
```

---

## Build

```bash
git clone https://github.com/0xwendel/gameboy-dmg-emulator.git
cd gameboy-dmg-emulator

cmake -S . -B build
cmake --build build --config Release
```

Typical binary path:

```text
build/gb-dmg.exe    # Windows
build/gb-dmg        # Unix
```

If a `roms/` folder exists in the project root, CMake copies it next to the binary after build (dev convenience). **Do not commit commercial ROMs** (see [Legal](#legal)).

---

## Run

```bash
# Game
./build/gb-dmg.exe "path/to/game.gb"

# Common options
./build/gb-dmg.exe --scale 4 --shader 4 --palette 0 game.gb
./build/gb-dmg.exe --muted --smooth game.gb
./build/gb-dmg.exe --boot dmg_boot.bin game.gb

# Unit tests
./build/gb-dmg.exe --test

# Help
./build/gb-dmg.exe --help
```

### CLI

| Flag | Description |
|------|-------------|
| `--test` | Run unit tests and exit |
| `--scale N` | Initial scale (default `4`) |
| `--muted` | Start muted |
| `--boot PATH` | DMG boot ROM (256 bytes) |
| `--palette N` | Palette index (`0`…`5`) |
| `--shader N` | Shader index (`0`…`5`) |
| `--smooth` | Bilinear texture filter |
| `-h` / `--help` | Usage and controls |

With no ROM argument, the binary may try a dev fallback under `roms/…` (if present). With `--test`, the unit suite runs and exits.

---

## Controls

### Keyboard — game

| Key | Action |
|-----|--------|
| Arrows / WASD | D-Pad |
| Z / K | A |
| X / J | B |
| Enter | Start |
| Backspace / Space | Select |

### Keyboard — emulator

| Key | Action |
|-----|--------|
| P | Pause / resume |
| R | Reset (+ battery reload) |
| M | Mute |
| 1 / 2 | Speed − / + |
| [ / ] | Previous / next palette |
| ; / ' | Previous / next shader |
| F1 | Save SRAM (+ RTC if present) |
| F5 / F9 | Save / load state |
| F11 | Fullscreen |
| F12 | Inspector (overlay) |

### Xbox 360 / XInput

| Control | Game Boy |
|---------|----------|
| D-Pad / left stick | D-Pad |
| A / Y | A |
| B / X | B |
| Start / RB | Start |
| Back / LB | Select |

The first connected gamepad is used. Hot-plug at runtime is supported.

---

## Inspector UI

The game display is **centered** in the window (letterboxed), always 160×144 aspect.

The **Inspector** (F12) is a **floating** panel — it does not steal width or resize the emulated screen. It starts **closed** by default.

### Panels

| Panel | Contents |
|-------|----------|
| **Home** | Status, pause/reset/mute, speed, saves, live snapshot |
| **CPU** | Registers, flags, peek @PC / [HL] / [SP] |
| **Video** | PPU mode, LY, LCDC, scroll, window |
| **Audio** | Channels, NR50–52, registers |
| **Memory** | Hex + ASCII dump (WRAM/HRAM/IO/VRAM/OAM) |
| **Cart** | Title, MBC, battery, RTC, saves; timer/IRQs |
| **Input** | Joypad + gamepad |
| **Display** | Palette, shader, integer scale, smooth filter |

Menu bar: **View** (inspector, filter, palette, shader) and **Emulator** (pause, reset, mute, saves).

---

## Shaders and palettes

### Shaders (`--shader N` or `;` / `'`)

| N | Name | Effect |
|---|------|--------|
| 0 | None | No post-process |
| 1 | Scanlines | CRT horizontal lines |
| 2 | LCD Grid | Grid between logical pixels |
| 3 | LCD Matrix | R/G/B subpixel stripes |
| 4 | CRT | Barrel, scanlines, vignette |
| 5 | Soft Glow | Soft blur / glow |

### Palettes (`--palette N` or `[` / `]`)

| N | Name |
|---|------|
| 0 | DMG Green |
| 1 | Greyscale |
| 2 | Pocket |
| 3 | Brown |
| 4 | Blue |
| 5 | Inverted |

---

## Architecture

```text
src/
  main.cpp          CLI, host loop, audio, input, display layout
  emulator.cpp      Frame/instruction orchestration + saves
  cpu.cpp           LR35902
  mmu.cpp           Bus, DMA, boot map, serial hook
  ppu.cpp           160×144 render → RGBA framebuffer
  timer.cpp         DIV / TIMA
  apu.cpp           APU + sample ring buffer
  serial.cpp        Link cable (SB/SC)
  cartridge.cpp     MBC + SRAM + RTC
  debug_ui.cpp      ImGui inspector
  shaders.cpp       GLSL post-process

include/            Public module headers
```

Host frame (simplified):

1. Poll keyboard + gamepad → joypad  
2. `runFrame()` until VBlank (CPU + peripherals in T-cycles)  
3. APU samples → raylib audio stream  
4. Framebuffer → texture → shader → centered screen  
5. ImGui inspector (if open)

---

## Tests

```bash
./build/gb-dmg.exe --test
```

Current coverage (headless / strong `REQUIRE` checks):

- MBC1 banking + RAM enable  
- Basic timers / IRQs  
- Joypad  
- EI delay  
- APU (signal energy, power, length)  
- Serial (transfer + IF)  
- MBC3 RTC (latch / read)

---

## Saves and states

| Type | File | Contents |
|------|------|----------|
| Battery | `<rom>.sav` | SRAM (+ 48-byte RTC if MBC3+timer) |
| State | `<rom>.sav.state` | CPU, MMU, PPU, Timer, APU, serial… |

Hotkeys: **F1** (SRAM), **F5** (save state), **F9** (load state).  
On exit, battery SRAM is saved automatically when the cart has a battery.

---

## Boot ROM

Optional. Without `--boot`, the emulator starts in a **post-boot** state (PC = `0x0100`, typical I/O).

With a legitimate DMG boot dump (256 bytes):

```bash
./build/gb-dmg.exe --boot dmg_boot.bin game.gb
```

The boot ROM maps `0x0000–0x00FF` until a write to `FF50`.  
We **do not redistribute** Nintendo’s boot ROM.

---

## Limitations

Work in progress. Honest expectations:

- APU/PPU are **good for playing**, not lab-grade cycle-perfect  
- Serial is **open-cable only** (no multiplayer peer)  
- No full CGB / SGB  
- No netplay / rewind / cheats (yet)  
- Accuracy varies by commercial title  

Issues and PRs are welcome.

---

## Legal

- This software **does not include** game ROMs or the official boot ROM.  
- Use dumps only from cartridges **you own**, or free/homebrew software.  
- Commercial ROMs must **not** be committed to git (`roms/` is gitignored).

---

## License

This project is licensed under the **[GNU General Public License v3.0](LICENSE)** (GPL-3.0).

You may redistribute and/or modify it under the terms of the GPL-3.0 as published by the Free Software Foundation.

Third-party dependencies keep their original licenses (typically zlib/MIT):

- [raylib](https://github.com/raysan5/raylib)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [rlImGui](https://github.com/raylib-extras/rlImGui)

See the [LICENSE](LICENSE) file for the full text.

---

## Credits

- Hardware docs: [Pan Docs](https://gbdev.io/pandocs/), gbdev community  
- [raylib](https://www.raylib.com/) · [Dear ImGui](https://github.com/ocornut/imgui) · [rlImGui](https://github.com/raylib-extras/rlImGui)

---

**0xwendel** · Game Boy DMG Emulator
