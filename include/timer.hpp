#pragma once

#include "mmu.hpp"
#include <cstdint>
#include <vector>

class Timer {
public:
    Timer();
    ~Timer() = default;

    void reset();
    void tick(uint8_t mCycles, MMU& mmu);
    void tickTCycle(MMU& mmu);

    void serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const uint8_t*& ptr, const uint8_t* end);

private:
    int m_timaReloadDelay = -1;
    uint8_t m_timaReloadValue = 0;

    static int tacBitIndex(uint8_t tac);
};
