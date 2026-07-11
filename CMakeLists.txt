cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(gb_emulator VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --- raylib 5.5 (compatível com rlImGui Raylib_5_5) ---
FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# --- Dear ImGui 1.92.x (API de texturas exigida pelo rlImGui atual) ---
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.7
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
if(MSVC)
  target_compile_options(imgui PRIVATE /W0)
else()
  target_compile_options(imgui PRIVATE -w)
endif()

# --- rlImGui (backend raylib para ImGui) ---
FetchContent_Declare(
  rlimgui
  GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
  GIT_TAG Raylib_5_5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(rlimgui)

add_library(rlimgui STATIC
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui PUBLIC ${rlimgui_SOURCE_DIR})
# Evita dependência de Font Awesome embutida no header
target_compile_definitions(rlimgui PUBLIC NO_FONT_AWESOME)
target_link_libraries(rlimgui PUBLIC imgui raylib)
if(MSVC)
  target_compile_options(rlimgui PRIVATE /W0)
else()
  target_compile_options(rlimgui PRIVATE -w)
endif()

# --- Emulador ---
set(GB_SOURCES
  src/main.cpp
  src/debug_ui.cpp
  src/mmu.cpp
  src/cpu.cpp
  src/ppu.cpp
  src/timer.cpp
  src/cartridge.cpp
  src/apu.cpp
  src/emulator.cpp
)

add_executable(gb_emulator ${GB_SOURCES})

target_include_directories(gb_emulator PRIVATE include)

target_link_libraries(gb_emulator PRIVATE raylib imgui rlimgui)

if(MSVC)
  target_compile_options(gb_emulator PRIVATE /W4)
else()
  target_compile_options(gb_emulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Cópia opcional de ROMs de desenvolvimento se existirem
if(EXISTS "${CMAKE_SOURCE_DIR}/roms")
  add_custom_command(TARGET gb_emulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/roms"
      "$<TARGET_FILE_DIR:gb_emulator>/roms"
    COMMENT "Copying roms/ next to binary"
  )
endif()
