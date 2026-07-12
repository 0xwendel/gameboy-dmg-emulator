#include "cpu.hpp"

#include <iostream>
#include <iomanip>

CPU::CPU() {
    reset();
}

void CPU::reset(bool useBootRom) {
    m_ime = false;
    m_imeEnablePending = false;
    m_halted = false;
    m_haltBug = false;

    if (useBootRom) {
        m_regs = {};
        m_regs.pc = 0x0000;
        m_regs.sp = 0x0000;
        return;
    }

    m_regs.a = 0x01;
    m_regs.f = 0xB0;
    m_regs.b = 0x00;
    m_regs.c = 0x13;
    m_regs.d = 0x00;
    m_regs.e = 0xD8;
    m_regs.h = 0x01;
    m_regs.l = 0x4D;
    m_regs.sp = 0xFFFE;
    m_regs.pc = 0x0100;
}

void CPU::setFlag(uint8_t flag, bool value) {
    if (value) m_regs.f |= flag;
    else m_regs.f &= static_cast<uint8_t>(~flag);
    m_regs.f &= 0xF0;
}

bool CPU::getFlag(uint8_t flag) const {
    return (m_regs.f & flag) != 0;
}

uint8_t CPU::fetchByte(MMU& mmu) {
    uint8_t byte = mmu.readByte(m_regs.pc);
    if (m_haltBug) {
        m_haltBug = false;
    } else {
        m_regs.pc++;
    }
    return byte;
}

uint16_t CPU::fetchWord(MMU& mmu) {
    uint8_t lo = fetchByte(mmu);
    uint8_t hi = fetchByte(mmu);
    return static_cast<uint16_t>((hi << 8) | lo);
}

uint8_t CPU::step(MMU& mmu) {
    const uint8_t ie = mmu.readByte(0xFFFF);
    const uint8_t ifReg = mmu.readByte(0xFF0F);
    const uint8_t pendingInterrupts = static_cast<uint8_t>(ie & ifReg);

    const bool enableImeAfter = m_imeEnablePending;
    m_imeEnablePending = false;

    if (m_halted) {
        if (pendingInterrupts != 0) {
            m_halted = false;
            if (m_ime) {
                handleInterrupts(mmu);
                if (enableImeAfter) m_ime = true;
                return 5;
            }
        } else {
            if (enableImeAfter) m_ime = true;
            return 1;
        }
    }

    if (m_ime && pendingInterrupts != 0) {
        handleInterrupts(mmu);
        if (enableImeAfter) m_ime = true;
        return 5;
    }

    const uint8_t opcode = fetchByte(mmu);
    const uint8_t cycles = executeOpcode(opcode, mmu);

    if (enableImeAfter) {
        m_ime = true;
    }

    return cycles;
}

void CPU::pushWord(MMU& mmu, uint16_t value) {
    m_regs.sp--;
    mmu.writeByte(m_regs.sp, static_cast<uint8_t>(value >> 8));
    m_regs.sp--;
    mmu.writeByte(m_regs.sp, static_cast<uint8_t>(value & 0xFF));
}

uint16_t CPU::popWord(MMU& mmu) {
    uint8_t lo = mmu.readByte(m_regs.sp);
    m_regs.sp++;
    uint8_t hi = mmu.readByte(m_regs.sp);
    m_regs.sp++;
    return static_cast<uint16_t>((hi << 8) | lo);
}

uint8_t CPU::getRegister(uint8_t index, MMU& mmu) const {
    switch (index) {
        case 0: return m_regs.b;
        case 1: return m_regs.c;
        case 2: return m_regs.d;
        case 3: return m_regs.e;
        case 4: return m_regs.h;
        case 5: return m_regs.l;
        case 6: return mmu.readByte(m_regs.hl());
        case 7: return m_regs.a;
        default: return 0;
    }
}

void CPU::setRegister(uint8_t index, uint8_t value, MMU& mmu) {
    switch (index) {
        case 0: m_regs.b = value; break;
        case 1: m_regs.c = value; break;
        case 2: m_regs.d = value; break;
        case 3: m_regs.e = value; break;
        case 4: m_regs.h = value; break;
        case 5: m_regs.l = value; break;
        case 6: mmu.writeByte(m_regs.hl(), value); break;
        case 7: m_regs.a = value; break;
    }
}

bool CPU::checkCondition(uint8_t cond) const {
    switch (cond) {
        case 0: return !getFlag(FlagZ);
        case 1: return getFlag(FlagZ);
        case 2: return !getFlag(FlagC);
        case 3: return getFlag(FlagC);
        default: return false;
    }
}

void CPU::executeALU(uint8_t op, uint8_t val) {
    uint8_t a = m_regs.a;
    switch (op) {
        case 0: {
            uint16_t result = a + val;
            setFlag(FlagZ, (result & 0xFF) == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, ((a & 0x0F) + (val & 0x0F)) > 0x0F);
            setFlag(FlagC, result > 0xFF);
            m_regs.a = static_cast<uint8_t>(result & 0xFF);
            break;
        }
        case 1: {
            uint8_t carry = getFlag(FlagC) ? 1 : 0;
            uint16_t result = a + val + carry;
            setFlag(FlagZ, (result & 0xFF) == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, ((a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
            setFlag(FlagC, result > 0xFF);
            m_regs.a = static_cast<uint8_t>(result & 0xFF);
            break;
        }
        case 2: {
            int16_t result = a - val;
            setFlag(FlagZ, (result & 0xFF) == 0);
            setFlag(FlagN, true);
            setFlag(FlagH, (a & 0x0F) < (val & 0x0F));
            setFlag(FlagC, a < val);
            m_regs.a = static_cast<uint8_t>(result & 0xFF);
            break;
        }
        case 3: {
            uint8_t carry = getFlag(FlagC) ? 1 : 0;
            int16_t result = a - val - carry;
            setFlag(FlagZ, (result & 0xFF) == 0);
            setFlag(FlagN, true);
            setFlag(FlagH, (static_cast<int>(a & 0x0F) - static_cast<int>(val & 0x0F) - carry) < 0);
            setFlag(FlagC, (static_cast<int>(a) - static_cast<int>(val) - carry) < 0);
            m_regs.a = static_cast<uint8_t>(result & 0xFF);
            break;
        }
        case 4:
            m_regs.a = static_cast<uint8_t>(a & val);
            setFlag(FlagZ, m_regs.a == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, true);
            setFlag(FlagC, false);
            break;
        case 5:
            m_regs.a = static_cast<uint8_t>(a ^ val);
            setFlag(FlagZ, m_regs.a == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, false);
            break;
        case 6:
            m_regs.a = static_cast<uint8_t>(a | val);
            setFlag(FlagZ, m_regs.a == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, false);
            break;
        case 7:
            setFlag(FlagZ, a == val);
            setFlag(FlagN, true);
            setFlag(FlagH, (a & 0x0F) < (val & 0x0F));
            setFlag(FlagC, a < val);
            break;
    }
}

uint8_t CPU::executeOpcode(uint8_t opcode, MMU& mmu) {
    if (opcode >= 0x40 && opcode <= 0x7F) {
        if (opcode == 0x76) {
            // HALT
            const uint8_t ie = mmu.readByte(0xFFFF);
            const uint8_t ifReg = mmu.readByte(0xFF0F);
            if (!m_ime && (ie & ifReg) != 0) {
                m_haltBug = true;
            } else {
                m_halted = true;
            }
            return 1;
        }
        uint8_t dest = (opcode >> 3) & 0x07;
        uint8_t src = opcode & 0x07;
        uint8_t val = getRegister(src, mmu);
        setRegister(dest, val, mmu);
        return (dest == 6 || src == 6) ? 2 : 1;
    }

    if ((opcode & 0xC7) == 0x06) {
        uint8_t dest = (opcode >> 3) & 0x07;
        uint8_t val = fetchByte(mmu);
        setRegister(dest, val, mmu);
        return (dest == 6) ? 3 : 2;
    }

    if ((opcode & 0xCF) == 0x01) {
        uint8_t pair = (opcode >> 4) & 0x03;
        uint16_t nn = fetchWord(mmu);
        switch (pair) {
            case 0: m_regs.set_bc(nn); break;
            case 1: m_regs.set_de(nn); break;
            case 2: m_regs.set_hl(nn); break;
            case 3: m_regs.sp = nn; break;
        }
        return 3;
    }

    if ((opcode & 0xCF) == 0xC5) {
        uint8_t pair = (opcode >> 4) & 0x03;
        switch (pair) {
            case 0: pushWord(mmu, m_regs.bc()); break;
            case 1: pushWord(mmu, m_regs.de()); break;
            case 2: pushWord(mmu, m_regs.hl()); break;
            case 3: pushWord(mmu, m_regs.af()); break;
        }
        return 4;
    }

    if ((opcode & 0xCF) == 0xC1) {
        uint8_t pair = (opcode >> 4) & 0x03;
        uint16_t val = popWord(mmu);
        switch (pair) {
            case 0: m_regs.set_bc(val); break;
            case 1: m_regs.set_de(val); break;
            case 2: m_regs.set_hl(val); break;
            case 3: m_regs.set_af(val); break;
        }
        return 3;
    }

    if ((opcode & 0xE7) == 0xC2) {
        uint8_t cond = (opcode >> 3) & 0x03;
        uint16_t addr = fetchWord(mmu);
        if (checkCondition(cond)) {
            m_regs.pc = addr;
            return 4;
        }
        return 3;
    }

    if (opcode == 0xC3) {
        m_regs.pc = fetchWord(mmu);
        return 3;
    }

    if (opcode == 0xE9) {
        m_regs.pc = m_regs.hl();
        return 1;
    }

    if ((opcode & 0xE7) == 0x20) {
        uint8_t cond = (opcode >> 3) & 0x03;
        int8_t offset = static_cast<int8_t>(fetchByte(mmu));
        if (checkCondition(cond)) {
            m_regs.pc = static_cast<uint16_t>(m_regs.pc + offset);
            return 3;
        }
        return 2;
    }

    if (opcode == 0x18) {
        int8_t offset = static_cast<int8_t>(fetchByte(mmu));
        m_regs.pc = static_cast<uint16_t>(m_regs.pc + offset);
        return 3;
    }

    if ((opcode & 0xE7) == 0xC4) {
        uint8_t cond = (opcode >> 3) & 0x03;
        uint16_t addr = fetchWord(mmu);
        if (checkCondition(cond)) {
            pushWord(mmu, m_regs.pc);
            m_regs.pc = addr;
            return 6;
        }
        return 3;
    }

    if (opcode == 0xCD) {
        uint16_t addr = fetchWord(mmu);
        pushWord(mmu, m_regs.pc);
        m_regs.pc = addr;
        return 6;
    }

    if ((opcode & 0xE7) == 0xC0) {
        uint8_t cond = (opcode >> 3) & 0x03;
        if (checkCondition(cond)) {
            m_regs.pc = popWord(mmu);
            return 5;
        }
        return 2;
    }

    if (opcode == 0xC9) {
        m_regs.pc = popWord(mmu);
        return 4;
    }

    if (opcode == 0xD9) {
        m_regs.pc = popWord(mmu);
        m_ime = true;
        return 4;
    }

    if ((opcode & 0xC7) == 0xC7) {
        uint8_t target = opcode & 0x38;
        pushWord(mmu, m_regs.pc);
        m_regs.pc = target;
        return 4;
    }

    if (opcode >= 0x80 && opcode <= 0xBF) {
        uint8_t op = (opcode >> 3) & 0x07;
        uint8_t index = opcode & 0x07;
        uint8_t val = getRegister(index, mmu);
        executeALU(op, val);
        return (index == 6) ? 2 : 1;
    }

    if ((opcode & 0xC7) == 0xC6) {
        uint8_t op = (opcode >> 3) & 0x07;
        uint8_t val = fetchByte(mmu);
        executeALU(op, val);
        return 2;
    }

    if ((opcode & 0xC7) == 0x04) {
        uint8_t index = (opcode >> 3) & 0x07;
        uint8_t old_val = getRegister(index, mmu);
        uint8_t new_val = static_cast<uint8_t>(old_val + 1);
        setRegister(index, new_val, mmu);
        setFlag(FlagZ, new_val == 0);
        setFlag(FlagN, false);
        setFlag(FlagH, (old_val & 0x0F) == 0x0F);
        return (index == 6) ? 3 : 1;
    }

    if ((opcode & 0xC7) == 0x05) {
        uint8_t index = (opcode >> 3) & 0x07;
        uint8_t old_val = getRegister(index, mmu);
        uint8_t new_val = static_cast<uint8_t>(old_val - 1);
        setRegister(index, new_val, mmu);
        setFlag(FlagZ, new_val == 0);
        setFlag(FlagN, true);
        setFlag(FlagH, (old_val & 0x0F) == 0x00);
        return (index == 6) ? 3 : 1;
    }

    if ((opcode & 0xCF) == 0x09) {
        uint8_t pair = (opcode >> 4) & 0x03;
        uint16_t hl = m_regs.hl();
        uint16_t val = 0;
        switch (pair) {
            case 0: val = m_regs.bc(); break;
            case 1: val = m_regs.de(); break;
            case 2: val = m_regs.hl(); break;
            case 3: val = m_regs.sp; break;
        }
        uint32_t result = hl + val;
        setFlag(FlagN, false);
        setFlag(FlagH, ((hl & 0x0FFF) + (val & 0x0FFF)) > 0x0FFF);
        setFlag(FlagC, result > 0xFFFF);
        m_regs.set_hl(static_cast<uint16_t>(result & 0xFFFF));
        return 2;
    }

    if ((opcode & 0xCF) == 0x03) {
        uint8_t pair = (opcode >> 4) & 0x03;
        switch (pair) {
            case 0: m_regs.set_bc(static_cast<uint16_t>(m_regs.bc() + 1)); break;
            case 1: m_regs.set_de(static_cast<uint16_t>(m_regs.de() + 1)); break;
            case 2: m_regs.set_hl(static_cast<uint16_t>(m_regs.hl() + 1)); break;
            case 3: m_regs.sp++; break;
        }
        return 2;
    }

    if ((opcode & 0xCF) == 0x0B) {
        uint8_t pair = (opcode >> 4) & 0x03;
        switch (pair) {
            case 0: m_regs.set_bc(static_cast<uint16_t>(m_regs.bc() - 1)); break;
            case 1: m_regs.set_de(static_cast<uint16_t>(m_regs.de() - 1)); break;
            case 2: m_regs.set_hl(static_cast<uint16_t>(m_regs.hl() - 1)); break;
            case 3: m_regs.sp--; break;
        }
        return 2;
    }

    switch (opcode) {
        case 0x00: return 1;

        case 0x02:
            mmu.writeByte(m_regs.bc(), m_regs.a);
            return 2;
        case 0x0A:
            m_regs.a = mmu.readByte(m_regs.bc());
            return 2;
        case 0x12:
            mmu.writeByte(m_regs.de(), m_regs.a);
            return 2;
        case 0x1A:
            m_regs.a = mmu.readByte(m_regs.de());
            return 2;
        case 0x22:
            mmu.writeByte(m_regs.hl(), m_regs.a);
            m_regs.set_hl(static_cast<uint16_t>(m_regs.hl() + 1));
            return 2;
        case 0x2A:
            m_regs.a = mmu.readByte(m_regs.hl());
            m_regs.set_hl(static_cast<uint16_t>(m_regs.hl() + 1));
            return 2;
        case 0x32:
            mmu.writeByte(m_regs.hl(), m_regs.a);
            m_regs.set_hl(static_cast<uint16_t>(m_regs.hl() - 1));
            return 2;
        case 0x3A:
            m_regs.a = mmu.readByte(m_regs.hl());
            m_regs.set_hl(static_cast<uint16_t>(m_regs.hl() - 1));
            return 2;

        case 0xE0: {
            uint8_t n = fetchByte(mmu);
            mmu.writeByte(static_cast<uint16_t>(0xFF00 + n), m_regs.a);
            return 3;
        }
        case 0xF0: {
            uint8_t n = fetchByte(mmu);
            m_regs.a = mmu.readByte(static_cast<uint16_t>(0xFF00 + n));
            return 3;
        }
        case 0xE2:
            mmu.writeByte(static_cast<uint16_t>(0xFF00 + m_regs.c), m_regs.a);
            return 2;
        case 0xF2:
            m_regs.a = mmu.readByte(static_cast<uint16_t>(0xFF00 + m_regs.c));
            return 2;
        case 0xEA: {
            uint16_t nn = fetchWord(mmu);
            mmu.writeByte(nn, m_regs.a);
            return 4;
        }
        case 0xFA: {
            uint16_t nn = fetchWord(mmu);
            m_regs.a = mmu.readByte(nn);
            return 4;
        }
        case 0x08: {
            uint16_t nn = fetchWord(mmu);
            mmu.writeByte(nn, static_cast<uint8_t>(m_regs.sp & 0xFF));
            mmu.writeByte(static_cast<uint16_t>(nn + 1), static_cast<uint8_t>(m_regs.sp >> 8));
            return 5;
        }
        case 0xF9:
            m_regs.sp = m_regs.hl();
            return 2;
        case 0xF8: {
            int8_t e = static_cast<int8_t>(fetchByte(mmu));
            uint16_t sp = m_regs.sp;
            uint8_t u_e = static_cast<uint8_t>(e);
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, ((sp & 0x0F) + (u_e & 0x0F)) > 0x0F);
            setFlag(FlagC, ((sp & 0xFF) + u_e) > 0xFF);
            m_regs.set_hl(static_cast<uint16_t>(sp + e));
            return 3;
        }
        case 0x27: {
            uint8_t a = m_regs.a;
            uint8_t correction = 0;
            bool carry = false;
            if (getFlag(FlagN)) {
                if (getFlag(FlagC)) { correction |= 0x60; carry = true; }
                if (getFlag(FlagH)) correction |= 0x06;
                a = static_cast<uint8_t>(a - correction);
            } else {
                if (getFlag(FlagC) || a > 0x99) { correction |= 0x60; carry = true; }
                if (getFlag(FlagH) || (a & 0x0F) > 0x09) correction |= 0x06;
                a = static_cast<uint8_t>(a + correction);
            }
            m_regs.a = a;
            setFlag(FlagZ, a == 0);
            setFlag(FlagH, false);
            setFlag(FlagC, carry);
            return 1;
        }
        case 0x2F:
            m_regs.a = static_cast<uint8_t>(~m_regs.a);
            setFlag(FlagN, true);
            setFlag(FlagH, true);
            return 1;
        case 0x37:
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, true);
            return 1;
        case 0x3F:
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, !getFlag(FlagC));
            return 1;
        case 0xE8: {
            int8_t e = static_cast<int8_t>(fetchByte(mmu));
            uint16_t sp = m_regs.sp;
            uint8_t u_e = static_cast<uint8_t>(e);
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, ((sp & 0x0F) + (u_e & 0x0F)) > 0x0F);
            setFlag(FlagC, ((sp & 0xFF) + u_e) > 0xFF);
            m_regs.sp = static_cast<uint16_t>(sp + e);
            return 4;
        }
        case 0x07: {
            uint8_t c = (m_regs.a >> 7) & 1;
            m_regs.a = static_cast<uint8_t>((m_regs.a << 1) | c);
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, c != 0);
            return 1;
        }
        case 0x0F: {
            uint8_t c = m_regs.a & 1;
            m_regs.a = static_cast<uint8_t>((m_regs.a >> 1) | (c << 7));
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, c != 0);
            return 1;
        }
        case 0x17: {
            uint8_t old_c = getFlag(FlagC) ? 1 : 0;
            uint8_t new_c = (m_regs.a >> 7) & 1;
            m_regs.a = static_cast<uint8_t>((m_regs.a << 1) | old_c);
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, new_c != 0);
            return 1;
        }
        case 0x1F: {
            uint8_t old_c = getFlag(FlagC) ? 1 : 0;
            uint8_t new_c = m_regs.a & 1;
            m_regs.a = static_cast<uint8_t>((m_regs.a >> 1) | (old_c << 7));
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, new_c != 0);
            return 1;
        }
        case 0x10:
            fetchByte(mmu);
            return 1;
        case 0xF3:
            m_ime = false;
            m_imeEnablePending = false;
            return 1;
        case 0xFB:
            // IME enables after the next instruction
            m_imeEnablePending = true;
            return 1;
        case 0xCB:
            return executeCBOpcode(fetchByte(mmu), mmu);

        default:
            std::cerr << "WARNING: Unimplemented opcode: 0x"
                      << std::hex << std::setw(2) << std::setfill('0') << (int)opcode
                      << " at PC: 0x" << std::setw(4) << (m_regs.pc - 1) << std::dec << std::endl;
            return 1;
    }
}

uint8_t CPU::executeCBOpcode(uint8_t cbOpcode, MMU& mmu) {
    uint8_t mode = (cbOpcode >> 6) & 0x03;
    uint8_t bit_or_op = (cbOpcode >> 3) & 0x07;
    uint8_t reg_index = cbOpcode & 0x07;

    uint8_t val = getRegister(reg_index, mmu);
    uint8_t cycles = (reg_index == 6) ? 4 : 2;

    if (mode == 0) {
        switch (bit_or_op) {
            case 0: {
                uint8_t c = (val >> 7) & 1;
                uint8_t res = static_cast<uint8_t>((val << 1) | c);
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
            case 1: {
                uint8_t c = val & 1;
                uint8_t res = static_cast<uint8_t>((val >> 1) | (c << 7));
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
            case 2: {
                uint8_t old_c = getFlag(FlagC) ? 1 : 0;
                uint8_t new_c = (val >> 7) & 1;
                uint8_t res = static_cast<uint8_t>((val << 1) | old_c);
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, new_c != 0);
                break;
            }
            case 3: {
                uint8_t old_c = getFlag(FlagC) ? 1 : 0;
                uint8_t new_c = val & 1;
                uint8_t res = static_cast<uint8_t>((val >> 1) | (old_c << 7));
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, new_c != 0);
                break;
            }
            case 4: {
                uint8_t c = (val >> 7) & 1;
                uint8_t res = static_cast<uint8_t>(val << 1);
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
            case 5: {
                uint8_t c = val & 1;
                uint8_t res = static_cast<uint8_t>((val >> 1) | (val & 0x80));
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
            case 6: {
                uint8_t res = static_cast<uint8_t>(((val & 0x0F) << 4) | ((val & 0xF0) >> 4));
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, false);
                break;
            }
            case 7: {
                uint8_t c = val & 1;
                uint8_t res = static_cast<uint8_t>(val >> 1);
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
        }
    } else if (mode == 1) {
        bool is_set = (val & (1 << bit_or_op)) != 0;
        setFlag(FlagZ, !is_set);
        setFlag(FlagN, false);
        setFlag(FlagH, true);
        if (reg_index == 6) cycles = 3;
    } else if (mode == 2) {
        setRegister(reg_index, static_cast<uint8_t>(val & ~(1 << bit_or_op)), mmu);
    } else if (mode == 3) {
        setRegister(reg_index, static_cast<uint8_t>(val | (1 << bit_or_op)), mmu);
    }

    return cycles;
}

void CPU::handleInterrupts(MMU& mmu) {
    uint8_t ie = mmu.readByte(0xFFFF);
    uint8_t ifReg = mmu.readByte(0xFF0F);
    uint8_t pending = static_cast<uint8_t>(ie & ifReg);
    if (pending == 0) return;

    if (pending & 0x01) serviceInterrupt(0, 0x0040, mmu);
    else if (pending & 0x02) serviceInterrupt(1, 0x0048, mmu);
    else if (pending & 0x04) serviceInterrupt(2, 0x0050, mmu);
    else if (pending & 0x08) serviceInterrupt(3, 0x0058, mmu);
    else if (pending & 0x10) serviceInterrupt(4, 0x0060, mmu);
}

void CPU::serviceInterrupt(uint8_t interruptBit, uint16_t vectorAddress, MMU& mmu) {
    m_ime = false;
    m_halted = false;
    uint8_t ifReg = mmu.readByte(0xFF0F);
    mmu.writeByte(0xFF0F, static_cast<uint8_t>(ifReg & ~(1 << interruptBit)));
    pushWord(mmu, m_regs.pc);
    m_regs.pc = vectorAddress;
}

void CPU::serialize(std::vector<uint8_t>& out) const {
    out.push_back(m_regs.a);
    out.push_back(m_regs.f);
    out.push_back(m_regs.b);
    out.push_back(m_regs.c);
    out.push_back(m_regs.d);
    out.push_back(m_regs.e);
    out.push_back(m_regs.h);
    out.push_back(m_regs.l);
    out.push_back(static_cast<uint8_t>(m_regs.sp & 0xFF));
    out.push_back(static_cast<uint8_t>(m_regs.sp >> 8));
    out.push_back(static_cast<uint8_t>(m_regs.pc & 0xFF));
    out.push_back(static_cast<uint8_t>(m_regs.pc >> 8));
    out.push_back(m_ime ? 1 : 0);
    out.push_back(m_imeEnablePending ? 1 : 0);
    out.push_back(m_halted ? 1 : 0);
    out.push_back(m_haltBug ? 1 : 0);
}

bool CPU::deserialize(const uint8_t*& ptr, const uint8_t* end) {
    if (end - ptr < 16) return false;
    m_regs.a = ptr[0];
    m_regs.f = ptr[1] & 0xF0;
    m_regs.b = ptr[2];
    m_regs.c = ptr[3];
    m_regs.d = ptr[4];
    m_regs.e = ptr[5];
    m_regs.h = ptr[6];
    m_regs.l = ptr[7];
    m_regs.sp = static_cast<uint16_t>(ptr[8] | (ptr[9] << 8));
    m_regs.pc = static_cast<uint16_t>(ptr[10] | (ptr[11] << 8));
    m_ime = ptr[12] != 0;
    m_imeEnablePending = ptr[13] != 0;
    m_halted = ptr[14] != 0;
    m_haltBug = ptr[15] != 0;
    ptr += 16;
    return true;
}
