#pragma once

#include "mmu.hpp"
#include <cstdint>
#include <vector>

// Timer DMG baseado em falling-edge de bits do contador DIV de 16 bits.
class Timer {
public:
    Timer();
    ~Timer() = default;

    void reset();
    void tick(uint8_t mCycles, MMU& mmu);
    // Um T-cycle (compartilhado com a APU para o DIV).
    void tickTCycle(MMU& mmu);

    void serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const uint8_t*& ptr, const uint8_t* end);

private:
    // Delay de reload do TIMA após overflow (em T-cycles). -1 = inativo.
    int m_timaReloadDelay = -1;
    uint8_t m_timaReloadValue = 0;

    static int tacBitIndex(uint8_t tac);
};
