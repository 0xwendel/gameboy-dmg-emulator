#pragma once

#include "mmu.hpp"
#include <cstdint>

class Timer {
public:
    Timer();
    ~Timer() = default;

    // Avança o estado dos timers com base nos M-cycles executados pela CPU
    void tick(uint8_t mCycles, MMU& mmu);

private:
    uint16_t m_divCounter;      // Contador interno de 16 bits para DIV (incrementado a cada dot)
    uint32_t m_timaAccumulator; // Acumulador de M-cycles para controle de TIMA
};
