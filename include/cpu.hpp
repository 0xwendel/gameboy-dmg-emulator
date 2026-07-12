#pragma once

#include "mmu.hpp"
#include <cstdint>
#include <vector>

class CPU {
public:
    struct Registers {
        uint8_t a = 0;
        uint8_t f = 0;
        uint8_t b = 0;
        uint8_t c = 0;
        uint8_t d = 0;
        uint8_t e = 0;
        uint8_t h = 0;
        uint8_t l = 0;
        uint16_t sp = 0;
        uint16_t pc = 0;

        uint16_t af() const { return static_cast<uint16_t>((a << 8) | f); }
        uint16_t bc() const { return static_cast<uint16_t>((b << 8) | c); }
        uint16_t de() const { return static_cast<uint16_t>((d << 8) | e); }
        uint16_t hl() const { return static_cast<uint16_t>((h << 8) | l); }

        void set_af(uint16_t val) { a = static_cast<uint8_t>(val >> 8); f = static_cast<uint8_t>(val & 0xF0); }
        void set_bc(uint16_t val) { b = static_cast<uint8_t>(val >> 8); c = static_cast<uint8_t>(val & 0xFF); }
        void set_de(uint16_t val) { d = static_cast<uint8_t>(val >> 8); e = static_cast<uint8_t>(val & 0xFF); }
        void set_hl(uint16_t val) { h = static_cast<uint8_t>(val >> 8); l = static_cast<uint8_t>(val & 0xFF); }
    };

    enum Flags : uint8_t {
        FlagZ = 1 << 7,
        FlagN = 1 << 6,
        FlagH = 1 << 5,
        FlagC = 1 << 4
    };

    CPU();
    ~CPU() = default;

    void reset(bool useBootRom = false);
    uint8_t step(MMU& mmu);

    void setFlag(uint8_t flag, bool value);
    bool getFlag(uint8_t flag) const;

    const Registers& getRegs() const { return m_regs; }
    Registers& getRegs() { return m_regs; }
    bool getIme() const { return m_ime; }
    bool isHalted() const { return m_halted; }

    void serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const uint8_t*& ptr, const uint8_t* end);

private:
    Registers m_regs;
    bool m_ime = false;
    bool m_imeEnablePending = false;
    bool m_halted = false;
    bool m_haltBug = false;

    void handleInterrupts(MMU& mmu);
    void serviceInterrupt(uint8_t interruptBit, uint16_t vectorAddress, MMU& mmu);

    uint8_t fetchByte(MMU& mmu);
    uint16_t fetchWord(MMU& mmu);

    void pushWord(MMU& mmu, uint16_t value);
    uint16_t popWord(MMU& mmu);

    uint8_t getRegister(uint8_t index, MMU& mmu) const;
    void setRegister(uint8_t index, uint8_t value, MMU& mmu);

    void executeALU(uint8_t op, uint8_t val);
    bool checkCondition(uint8_t cond) const;

    uint8_t executeOpcode(uint8_t opcode, MMU& mmu);
    uint8_t executeCBOpcode(uint8_t cbOpcode, MMU& mmu);
};
