#include "timer.hpp"

Timer::Timer() {
    reset();
}

void Timer::reset() {
    m_timaReloadDelay = -1;
    m_timaReloadValue = 0;
}

int Timer::tacBitIndex(uint8_t tac) {
    // Internal 16-bit counter bits that generate TIMA falling edges:
    // 00: bit 9 (4096 Hz), 01: bit 3 (262144 Hz), 10: bit 5 (65536 Hz), 11: bit 7 (16384 Hz)
    switch (tac & 0x03) {
        case 0: return 9;
        case 1: return 3;
        case 2: return 5;
        default: return 7;
    }
}

void Timer::tickTCycle(MMU& mmu) {
    // TIMA reload delay (4 T-cycles after overflow)
    if (m_timaReloadDelay >= 0) {
        m_timaReloadDelay--;
        if (m_timaReloadDelay == 0) {
            mmu.io()[0x05] = m_timaReloadValue;
            mmu.io()[0x0F] |= 0x04; // Timer interrupt
            m_timaReloadDelay = -1;
        } else if (m_timaReloadDelay > 0) {
            // During the delay, TIMA stays at 0
            mmu.io()[0x05] = 0;
        }
    }

    uint16_t& div = mmu.divCounter();
    const uint16_t oldDiv = div;
    div = static_cast<uint16_t>(div + 1);
    mmu.setDivHigh();

    const uint8_t tac = mmu.io()[0x07];
    if ((tac & 0x04) == 0) {
        return;
    }

    const int bit = tacBitIndex(tac);
    const bool oldBit = (oldDiv & (1u << bit)) != 0;
    const bool newBit = (div & (1u << bit)) != 0;

    // Falling edge: 1 -> 0
    if (oldBit && !newBit) {
        uint8_t tima = mmu.io()[0x05];
        if (tima == 0xFF) {
            // Overflow: TIMA = 0 for 4 T-cycles, then TMA + IRQ
            mmu.io()[0x05] = 0;
            m_timaReloadValue = mmu.io()[0x06];
            m_timaReloadDelay = 4;
        } else {
            mmu.io()[0x05] = static_cast<uint8_t>(tima + 1);
        }
    }
}

void Timer::tick(uint8_t mCycles, MMU& mmu) {
    const uint32_t tCycles = static_cast<uint32_t>(mCycles) * 4u;
    for (uint32_t i = 0; i < tCycles; ++i) {
        tickTCycle(mmu);
    }
}

void Timer::serialize(std::vector<uint8_t>& out) const {
    out.push_back(static_cast<uint8_t>(m_timaReloadDelay & 0xFF));
    out.push_back(static_cast<uint8_t>((m_timaReloadDelay >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((m_timaReloadDelay >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((m_timaReloadDelay >> 24) & 0xFF));
    out.push_back(m_timaReloadValue);
}

bool Timer::deserialize(const uint8_t*& ptr, const uint8_t* end) {
    if (end - ptr < 5) return false;
    m_timaReloadDelay = static_cast<int>(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24));
    m_timaReloadValue = ptr[4];
    ptr += 5;
    return true;
}
