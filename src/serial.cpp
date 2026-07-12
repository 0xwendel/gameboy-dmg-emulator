#include "serial.hpp"
#include "mmu.hpp"

void Serial::reset() {
    m_sb = 0x00;
    m_sc = 0x7E;
    m_transferring = false;
    m_bitsLeft = 0;
    m_cycleAcc = 0;
}

void Serial::writeSB(uint8_t value) {
    m_sb = value;
}

void Serial::writeSC(uint8_t value) {
    m_sc = static_cast<uint8_t>(0x7E | (value & 0x81));
    if ((value & 0x80) != 0) {
        m_transferring = true;
        m_bitsLeft = 8;
        m_cycleAcc = 0;
    }
}

uint8_t Serial::readSC() const {
    return static_cast<uint8_t>(m_sc | 0x7E);
}

void Serial::tick(uint32_t tCycles, MMU& mmu) {
    if (!m_transferring) return;

    if ((m_sc & 0x01) == 0) {
        m_sb = 0xFF;
        m_transferring = false;
        m_bitsLeft = 0;
        m_sc &= static_cast<uint8_t>(~0x80);
        mmu.io()[0x0F] |= 0x08;
        return;
    }

    m_cycleAcc += static_cast<int>(tCycles);
    while (m_transferring && m_cycleAcc >= kCyclesPerBit) {
        m_cycleAcc -= kCyclesPerBit;
        m_sb = static_cast<uint8_t>((m_sb << 1) | 0x01);
        --m_bitsLeft;
        if (m_bitsLeft <= 0) {
            m_transferring = false;
            m_sc &= static_cast<uint8_t>(~0x80);
            mmu.io()[0x0F] |= 0x08;
        }
    }
}

void Serial::serialize(std::vector<uint8_t>& out) const {
    out.push_back(m_sb);
    out.push_back(m_sc);
    out.push_back(m_transferring ? 1 : 0);
    out.push_back(static_cast<uint8_t>(m_bitsLeft & 0xFF));
    out.push_back(static_cast<uint8_t>((m_bitsLeft >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(m_cycleAcc & 0xFF));
    out.push_back(static_cast<uint8_t>((m_cycleAcc >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((m_cycleAcc >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((m_cycleAcc >> 24) & 0xFF));
}

bool Serial::deserialize(const uint8_t*& ptr, const uint8_t* end) {
    if (end - ptr < 9) return false;
    m_sb = ptr[0];
    m_sc = ptr[1];
    m_transferring = ptr[2] != 0;
    m_bitsLeft = static_cast<int>(ptr[3] | (ptr[4] << 8));
    m_cycleAcc = static_cast<int>(ptr[5] | (ptr[6] << 8) | (ptr[7] << 16) | (ptr[8] << 24));
    ptr += 9;
    return true;
}
