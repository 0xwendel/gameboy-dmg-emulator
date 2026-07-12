# GB DMG Emulator

**[English](README.md)** · **Português (BR)**

Emulador de **Game Boy (DMG)** em C++20, com frontend **raylib**, inspector **Dear ImGui** e shaders GLSL embutidos.

Feito para jogar de verdade: CPU, PPU, timers, joypad, cartuchos MBC, APU estéreo, save states, RTC MBC3, controle Xbox 360 e display centrado com pós-processamento.

| | |
|---|---|
| **Versão** | 0.4.0 |
| **Linguagem** | C++20 |
| **Build** | CMake ≥ 3.20 (Ninja, Make ou MSVC — o gerador depende do ambiente) |
| **Plataforma** | Windows (principal; dependências multiplataforma) |
| **Repo** | [github.com/0xwendel/gameboy-dmg-emulator](https://github.com/0xwendel/gameboy-dmg-emulator) |

---

## Índice

- [Funcionalidades](#funcionalidades)
- [Requisitos](#requisitos)
- [Compilar](#compilar)
- [Executar](#executar)
- [Controles](#controles)
- [Inspector](#inspector)
- [Shaders e paletas](#shaders-e-paletas)
- [Arquitetura](#arquitetura)
- [Testes](#testes)
- [Saves e estados](#saves-e-estados)
- [Boot ROM](#boot-rom)
- [Limitações](#limitações)
- [Legal](#legal)
- [Licença](#licença)

---

## Funcionalidades

### Núcleo DMG

| Subsistema | Detalhes |
|------------|----------|
| **CPU** | LR35902, flags, EI delay, HALT bug, interrupções |
| **MMU** | Mapa de memória, locks de VRAM/OAM, OAM DMA timed |
| **PPU** | BG, Window, sprites 8×8/8×16, STAT, LYC, prioridade |
| **Timer** | DIV falling-edge, TIMA/TMA/TAC, reload delay |
| **APU** | 4 canais (2× square, wave, noise), frame sequencer no DIV, high-pass, 44.1 kHz estéreo |
| **Serial** | SB/SC, clock interno, cabo aberto (`0xFF` + IRQ) |
| **Joypad** | Active-low, multiplex, edge IRQ |

### Cartuchos

- ROM-only, **MBC1**, **MBC2**, **MBC3** (+ **RTC**), **MBC5**
- SRAM com bateria (arquivo `.sav`)
- RTC MBC3 com latch e trailer de 48 bytes no save

### Frontend

- Display **centrado** (letterbox), aspecto nativo 160×144
- Escala inteira ou fluida
- **Inspector** flutuante (não redimensiona a tela do jogo)
- Paletas DMG (Green, Greyscale, Pocket, Brown, Blue, Inverted)
- Shaders: None, Scanlines, LCD Grid, LCD Matrix, CRT, Soft Glow
- Fullscreen, mute, velocidade, save/load state
- **Xbox 360 / XInput** (D-Pad, stick, A/B/X/Y, Start/Back, ombros)
- Boot ROM opcional (`dmg_boot.bin`, 256 bytes)

### Dependências (FetchContent)

Baixadas no configure:

- [raylib](https://github.com/raysan5/raylib) **5.5**
- [Dear ImGui](https://github.com/ocornut/imgui) **v1.92.7**
- [rlImGui](https://github.com/raylib-extras/rlImGui) tag **Raylib_5_5**

---

## Requisitos

### Software

- **CMake** ≥ 3.20  
- Compilador **C++20** (MSVC, MinGW/GCC, Clang)  
- **Git** (FetchContent)  
- OpenGL 3.3+  
- Rede na **primeira** configuração  

### Windows (MSYS2 UCRT64)

```bash
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja git
```

---

## Compilar

```bash
git clone https://github.com/0xwendel/gameboy-dmg-emulator.git
cd gameboy-dmg-emulator

cmake -S . -B build
cmake --build build --config Release
```

Binário típico:

```text
build/gb_emulator.exe    # Windows
build/gb_emulator        # Unix
```

Se existir `roms/` na raiz, o CMake copia para o lado do executável (dev). **Não commite ROMs comerciais** (veja [Legal](#legal)).

---

## Executar

```bash
# Jogo
./build/gb_emulator.exe "caminho/para/jogo.gb"

# Opções comuns
./build/gb_emulator.exe --scale 4 --shader 4 --palette 0 jogo.gb
./build/gb_emulator.exe --muted --smooth jogo.gb
./build/gb_emulator.exe --boot dmg_boot.bin jogo.gb

# Testes
./build/gb_emulator.exe --test

# Ajuda
./build/gb_emulator.exe --help
```

### CLI

| Flag | Descrição |
|------|-----------|
| `--test` | Roda testes e sai |
| `--scale N` | Escala inicial (padrão `4`) |
| `--muted` | Inicia sem áudio |
| `--boot PATH` | Boot ROM DMG (256 bytes) |
| `--palette N` | Paleta (`0`…`5`) |
| `--shader N` | Shader (`0`…`5`) |
| `--smooth` | Filtro bilinear |
| `-h` / `--help` | Uso e controles |

Sem ROM, pode tentar fallback em `roms/…` (se existir). Com `--test`, roda a suite e encerra.

---

## Controles

### Teclado — jogo

| Tecla | Ação |
|-------|------|
| Setas / WASD | D-Pad |
| Z / K | A |
| X / J | B |
| Enter | Start |
| Backspace / Space | Select |

### Teclado — emulador

| Tecla | Ação |
|-------|------|
| P | Pause / resume |
| R | Reset (+ reload da bateria) |
| M | Mute |
| 1 / 2 | Velocidade − / + |
| [ / ] | Paleta anterior / próxima |
| ; / ' | Shader anterior / próximo |
| F1 | Salvar SRAM (+ RTC se houver) |
| F5 / F9 | Save / load state |
| F11 | Fullscreen |
| F12 | Inspector (overlay) |

### Xbox 360 / XInput

| Controle | Game Boy |
|----------|----------|
| D-Pad / stick esquerdo | D-Pad |
| A / Y | A |
| B / X | B |
| Start / RB | Start |
| Back / LB | Select |

Usa o primeiro gamepad. Hot-plug em runtime é suportado.

---

## Inspector

O display fica **centrado** na janela (letterbox), sempre 160×144.

O **Inspector** (F12) é um painel **flutuante** — não rouba largura nem redimensiona o jogo. Começa **fechado**.

### Painéis

| Painel | Conteúdo |
|--------|----------|
| **Home** | Status, pause/reset/mute, speed, saves, snapshot |
| **CPU** | Registradores, flags, peek @PC / [HL] / [SP] |
| **Video** | Modo PPU, LY, LCDC, scroll, window |
| **Audio** | Canais, NR50–52, registradores |
| **Memory** | Dump hex + ASCII (WRAM/HRAM/IO/VRAM/OAM) |
| **Cart** | Título, MBC, battery, RTC, saves; timer/IRQs |
| **Input** | Joypad + gamepad |
| **Display** | Paleta, shader, integer scale, smooth |

Menubar: **View** e **Emulator**; **Help → About**.

---

## Shaders e paletas

### Shaders (`--shader N` ou `;` / `'`)

| N | Nome | Efeito |
|---|------|--------|
| 0 | None | Sem pós-processamento |
| 1 | Scanlines | Linhas CRT |
| 2 | LCD Grid | Grade entre pixels |
| 3 | LCD Matrix | Subpixels R/G/B |
| 4 | CRT | Curvatura, scanlines, vinheta |
| 5 | Soft Glow | Blur/glow |

### Paletas (`--palette N` ou `[` / `]`)

| N | Nome |
|---|------|
| 0 | DMG Green |
| 1 | Greyscale |
| 2 | Pocket |
| 3 | Brown |
| 4 | Blue |
| 5 | Inverted |

---

## Arquitetura

```text
src/
  main.cpp          CLI, loop host, áudio, input, layout
  emulator.cpp      Frame/instrução + saves
  cpu.cpp           LR35902
  mmu.cpp           Barramento, DMA, boot, serial
  ppu.cpp           Render 160×144 → framebuffer RGBA
  timer.cpp         DIV / TIMA
  apu.cpp           APU + ring buffer
  serial.cpp        Link cable (SB/SC)
  cartridge.cpp     MBC + SRAM + RTC
  debug_ui.cpp      Inspector ImGui
  shaders.cpp       Pós-processamento GLSL

include/            Headers públicos
```

Frame de host (resumo):

1. Poll teclado + gamepad → joypad  
2. `runFrame()` até VBlank  
3. Samples APU → stream raylib  
4. Framebuffer → textura → shader → tela centrada  
5. Inspector ImGui (se aberto)

---

## Testes

```bash
./build/gb_emulator.exe --test
```

Cobertura atual:

- MBC1 banking + RAM  
- Timers / IRQs básicos  
- Joypad  
- EI delay  
- APU (energia, power, length)  
- Serial (transfer + IF)  
- MBC3 RTC (latch / read)

---

## Saves e estados

| Tipo | Arquivo | Conteúdo |
|------|---------|----------|
| Bateria | `<rom>.sav` | SRAM (+ 48 bytes RTC se MBC3+timer) |
| State | `<rom>.sav.state` | CPU, MMU, PPU, Timer, APU, serial… |

Atalhos: **F1** (SRAM), **F5** (save state), **F9** (load state).  
Ao sair, a SRAM é salva se o cartucho tiver bateria.

---

## Boot ROM

Opcional. Sem `--boot`, inicia em estado **pós-boot** (PC = `0x0100`).

Com dump legítimo de boot DMG (256 bytes):

```bash
./build/gb_emulator.exe --boot dmg_boot.bin jogo.gb
```

Mapeia `0x0000–0x00FF` até escrita em `FF50`.  
**Não redistribuímos** a boot ROM da Nintendo.

---

## Limitações

Projeto em evolução:

- APU/PPU boas para **jogar**, não cycle-perfect de laboratório  
- Serial **sem peer** (cabo aberto)  
- Sem CGB / SGB completo  
- Sem netplay / rewind / cheats (ainda)  
- Accuracy varia por título  

Issues e PRs são bem-vindos.

---

## Legal

- Este software **não inclui** ROMs de jogos nem a boot ROM oficial.  
- Use dumps de cartuchos **que você possui** ou software livre/homebrew.  
- ROMs comerciais **não** devem ir para o git (`roms/` está no `.gitignore`).

---

## Licença

Este projeto está sob a **[GNU General Public License v3.0](LICENSE)** (GPL-3.0).

Dependências de terceiros mantêm as licenças originais (em geral zlib/MIT):

- [raylib](https://github.com/raysan5/raylib)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [rlImGui](https://github.com/raylib-extras/rlImGui)

Veja o arquivo [LICENSE](LICENSE) para o texto completo.

---

## Créditos

- Docs de hardware: [Pan Docs](https://gbdev.io/pandocs/), comunidade gbdev  
- [raylib](https://www.raylib.com/) · [Dear ImGui](https://github.com/ocornut/imgui) · [rlImGui](https://github.com/raylib-extras/rlImGui)

---

**0xwendel** · Game Boy DMG Emulator
