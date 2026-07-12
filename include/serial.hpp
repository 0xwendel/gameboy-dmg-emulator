#pragma once

#include <cstdint>
#include <vector>

class MMU;

class Serial {
public:
    Serial() = default;

    void reset();
    void writeSB(uint8_t value);
    void writeSC(uint8_t value);
    uint8_t readSB() const { return m_sb; }
    uint8_t readSC() const;

    void tick(uint32_t tCycles, MMU& mmu);

    void serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const uint8_t*& ptr, const uint8_t* end);

private:
    uint8_t m_sb = 0x00;
    uint8_t m_sc = 0x7E;
    bool m_transferring = false;
    int m_bitsLeft = 0;
    int m_cycleAcc = 0;

    static constexpr int kCyclesPerBit = 512;
};
