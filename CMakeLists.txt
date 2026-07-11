cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(gb_emulator VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

set(GB_SOURCES
  src/main.cpp
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

target_link_libraries(gb_emulator PRIVATE raylib)

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
