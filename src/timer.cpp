#include "timer.hpp"

Timer::Timer() : m_divCounter(0), m_timaAccumulator(0) {}

void Timer::tick(uint8_t mCycles, MMU& mmu) {
    // 1. Atualiza o registrador divisor DIV (roda sempre a 16384Hz)
    // 1 M-cycle equivale a 4 dots (ticks do clock principal)
    mmu.tickDIV(mCycles * 4);

    // 2. Atualiza o contador de timer TIMA se ativado em TAC
    uint8_t tac = mmu.readByte(0xFF07);
    bool timerEnabled = (tac & 0x04) != 0;

    if (timerEnabled) {
        // Mapeamento de frequência selecionada em TAC bits 0-1 para M-cycles por tick:
        // 00: 4096 Hz -> 256 M-cycles
        // 01: 262144 Hz -> 4 M-cycles
        // 10: 65536 Hz -> 16 M-cycles
        // 11: 16384 Hz -> 64 M-cycles
        const uint16_t cyclesPerTick[4] = {256, 4, 16, 64};
        uint16_t limit = cyclesPerTick[tac & 0x03];

        m_timaAccumulator += mCycles;

        while (m_timaAccumulator >= limit) {
            m_timaAccumulator -= limit;

            uint8_t tima = mmu.readByte(0xFF05);

            if (tima == 0xFF) {
                // TIMA estourou: recarrega com TMA e solicita interrupção de Timer (Bit 2 de IF)
                uint8_t tma = mmu.readByte(0xFF06);
                mmu.writeByte(0xFF05, tma);

                uint8_t ifReg = mmu.readByte(0xFF0F);
                mmu.writeByte(0xFF0F, ifReg | 0x04);
            } else {
                mmu.writeByte(0xFF05, tima + 1);
            }
        }
    } else {
        // Se o timer for desativado, descarta ciclos acumulados parciais
        m_timaAccumulator = 0;
    }
}
