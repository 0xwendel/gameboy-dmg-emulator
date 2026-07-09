#include "cpu.hpp"
#include <iostream>
#include <iomanip>

CPU::CPU() : m_ime(false), m_halted(false) {
    // Inicialização dos registradores após o término da Boot ROM do DMG (Game Boy Clássico)
    // conforme especificado na Pan Docs.
    m_regs.a = 0x01;
    m_regs.f = 0xB0; // Z=1, N=0, H=1, C=1 (1011 0000)
    m_regs.b = 0x00;
    m_regs.c = 0x13;
    m_regs.d = 0x00;
    m_regs.e = 0xD8;
    m_regs.h = 0x01;
    m_regs.l = 0x4D;
    m_regs.sp = 0xFFFE;
    m_regs.pc = 0x0100; // Início do fluxo de execução do cartucho
}

void CPU::setFlag(uint8_t flag, bool value) {
    if (value) {
        m_regs.f |= flag;
    } else {
        m_regs.f &= ~flag;
    }
    m_regs.f &= 0xF0; // Garante que os 4 bits inferiores são sempre zero
}

bool CPU::getFlag(uint8_t flag) const {
    return (m_regs.f & flag) != 0;
}

uint8_t CPU::fetchByte(MMU& mmu) {
    uint8_t byte = mmu.readByte(m_regs.pc);
    m_regs.pc++;
    return byte;
}

uint16_t CPU::fetchWord(MMU& mmu) {
    uint8_t lo = fetchByte(mmu);
    uint8_t hi = fetchByte(mmu);
    return (hi << 8) | lo;
}

uint8_t CPU::step(MMU& mmu) {
    uint8_t ie = mmu.readByte(0xFFFF);
    uint8_t ifReg = mmu.readByte(0xFF0F);
    uint8_t pendingInterrupts = ie & ifReg;

    if (m_halted) {
        if (pendingInterrupts != 0) {
            m_halted = false; // CPU acorda do HALT
        } else {
            // Continua parado consome 1 M-cycle
            return 1;
        }
    }

    // Se IME estiver ativo e houver interrupções habilitadas solicitadas
    if (m_ime && pendingInterrupts != 0) {
        handleInterrupts(mmu);
        return 5; // O processamento do desvio da interrupção consome 5 M-cycles
    }

    uint8_t opcode = fetchByte(mmu);
    return executeOpcode(opcode, mmu);
}

void CPU::pushWord(MMU& mmu, uint16_t value) {
    m_regs.sp--;
    mmu.writeByte(m_regs.sp, value >> 8);
    m_regs.sp--;
    mmu.writeByte(m_regs.sp, value & 0xFF);
}

uint16_t CPU::popWord(MMU& mmu) {
    uint8_t lo = mmu.readByte(m_regs.sp);
    m_regs.sp++;
    uint8_t hi = mmu.readByte(m_regs.sp);
    m_regs.sp++;
    return (hi << 8) | lo;
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
        case 0: return !getFlag(FlagZ); // NZ
        case 1: return getFlag(FlagZ);  // Z
        case 2: return !getFlag(FlagC); // NC
        case 3: return getFlag(FlagC);  // C
        default: return false;
    }
}

void CPU::executeALU(uint8_t op, uint8_t val) {
    uint8_t a = m_regs.a;
    switch (op) {
        case 0: { // ADD
            uint16_t result = a + val;
            setFlag(FlagZ, (result & 0xFF) == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, ((a & 0x0F) + (val & 0x0F)) > 0x0F);
            setFlag(FlagC, result > 0xFF);
            m_regs.a = result & 0xFF;
            break;
        }
        case 1: { // ADC
            uint8_t carry = getFlag(FlagC) ? 1 : 0;
            uint16_t result = a + val + carry;
            setFlag(FlagZ, (result & 0xFF) == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, ((a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
            setFlag(FlagC, result > 0xFF);
            m_regs.a = result & 0xFF;
            break;
        }
        case 2: { // SUB
            int16_t result = a - val;
            setFlag(FlagZ, (result & 0xFF) == 0);
            setFlag(FlagN, true);
            setFlag(FlagH, (a & 0x0F) < (val & 0x0F));
            setFlag(FlagC, a < val);
            m_regs.a = result & 0xFF;
            break;
        }
        case 3: { // SBC
            uint8_t carry = getFlag(FlagC) ? 1 : 0;
            int16_t result = a - val - carry;
            setFlag(FlagZ, (result & 0xFF) == 0);
            setFlag(FlagN, true);
            setFlag(FlagH, (static_cast<int>(a & 0x0F) - static_cast<int>(val & 0x0F) - carry) < 0);
            setFlag(FlagC, (static_cast<int>(a) - static_cast<int>(val) - carry) < 0);
            m_regs.a = result & 0xFF;
            break;
        }
        case 4: { // AND
            m_regs.a = a & val;
            setFlag(FlagZ, m_regs.a == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, true);
            setFlag(FlagC, false);
            break;
        }
        case 5: { // XOR
            m_regs.a = a ^ val;
            setFlag(FlagZ, m_regs.a == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, false);
            break;
        }
        case 6: { // OR
            m_regs.a = a | val;
            setFlag(FlagZ, m_regs.a == 0);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, false);
            break;
        }
        case 7: { // CP
            setFlag(FlagZ, a == val);
            setFlag(FlagN, true);
            setFlag(FlagH, (a & 0x0F) < (val & 0x0F));
            setFlag(FlagC, a < val);
            break;
        }
    }
}

uint8_t CPU::executeOpcode(uint8_t opcode, MMU& mmu) {
    // 1. LD r, r' (0x40 - 0x7F, excluindo HALT 0x76)
    if (opcode >= 0x40 && opcode <= 0x7F) {
        if (opcode == 0x76) {
            m_halted = true;
            return 1;
        }
        uint8_t dest = (opcode >> 3) & 0x07;
        uint8_t src = opcode & 0x07;
        uint8_t val = getRegister(src, mmu);
        setRegister(dest, val, mmu);
        return (dest == 6 || src == 6) ? 2 : 1;
    }

    // 2. LD r, n (0x06, 0x0E, 0x16, 0x1E, 0x26, 0x2E, 0x36, 0x3E)
    if ((opcode & 0xC7) == 0x06) {
        uint8_t dest = (opcode >> 3) & 0x07;
        uint8_t val = fetchByte(mmu);
        setRegister(dest, val, mmu);
        return (dest == 6) ? 3 : 2;
    }

    // 3. LD rr, nn (0x01, 0x11, 0x21, 0x31)
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

    // 4. PUSH rr (0xC5, 0xD5, 0xE5, 0xF5)
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

    // 5. POP rr (0xC1, 0xD1, 0xE1, 0xF1)
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

    // --- Instruções de Controle de Fluxo (Saltos, Chamadas, Retornos) ---
    // JP cc, nn (0xC2, 0xCA, 0xD2, 0xDA)
    if ((opcode & 0xE7) == 0xC2) {
        uint8_t cond = (opcode >> 3) & 0x03;
        uint16_t addr = fetchWord(mmu);
        if (checkCondition(cond)) {
            m_regs.pc = addr;
            return 4;
        }
        return 3;
    }

    // JP nn (0xC3)
    if (opcode == 0xC3) {
        m_regs.pc = fetchWord(mmu);
        return 3;
    }

    // JP HL (0xE9)
    if (opcode == 0xE9) {
        m_regs.pc = m_regs.hl();
        return 1;
    }

    // JR cc, e (0x20, 0x28, 0x30, 0x38)
    if ((opcode & 0xE7) == 0x20) {
        uint8_t cond = (opcode >> 3) & 0x03;
        int8_t offset = static_cast<int8_t>(fetchByte(mmu));
        if (checkCondition(cond)) {
            m_regs.pc += offset;
            return 3;
        }
        return 2;
    }

    // JR e (0x18)
    if (opcode == 0x18) {
        int8_t offset = static_cast<int8_t>(fetchByte(mmu));
        m_regs.pc += offset;
        return 3;
    }

    // CALL cc, nn (0xC4, 0xCC, 0xD4, 0xDC)
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

    // CALL nn (0xCD)
    if (opcode == 0xCD) {
        uint16_t addr = fetchWord(mmu);
        pushWord(mmu, m_regs.pc);
        m_regs.pc = addr;
        return 6;
    }

    // RET cc (0xC0, 0xC8, 0xD0, 0xD8)
    if ((opcode & 0xE7) == 0xC0) {
        uint8_t cond = (opcode >> 3) & 0x03;
        if (checkCondition(cond)) {
            m_regs.pc = popWord(mmu);
            return 5;
        }
        return 2;
    }

    // RET (0xC9)
    if (opcode == 0xC9) {
        m_regs.pc = popWord(mmu);
        return 4;
    }

    // RETI (0xD9)
    if (opcode == 0xD9) {
        m_regs.pc = popWord(mmu);
        m_ime = true;
        return 4;
    }

    // RST f (0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7, 0xFF)
    if ((opcode & 0xC7) == 0xC7) {
        uint8_t target = opcode & 0x38;
        pushWord(mmu, m_regs.pc);
        m_regs.pc = target;
        return 4;
    }

    // 6. Operações Aritméticas/Lógicas da ALU de 8 bits (0x80 - 0xBF)
    if (opcode >= 0x80 && opcode <= 0xBF) {
        uint8_t op = (opcode >> 3) & 0x07;
        uint8_t index = opcode & 0x07;
        uint8_t val = getRegister(index, mmu);
        executeALU(op, val);
        return (index == 6) ? 2 : 1;
    }

    // 7. Operações Aritméticas/Lógicas da ALU de 8 bits Imediatas
    if ((opcode & 0xC7) == 0xC6) {
        uint8_t op = (opcode >> 3) & 0x07;
        uint8_t val = fetchByte(mmu);
        executeALU(op, val);
        return 2;
    }

    // 8. INC r (0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C)
    if ((opcode & 0xC7) == 0x04) {
        uint8_t index = (opcode >> 3) & 0x07;
        uint8_t old_val = getRegister(index, mmu);
        uint8_t new_val = old_val + 1;
        setRegister(index, new_val, mmu);
        setFlag(FlagZ, new_val == 0);
        setFlag(FlagN, false);
        setFlag(FlagH, (old_val & 0x0F) == 0x0F);
        return (index == 6) ? 3 : 1;
    }

    // 9. DEC r (0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D)
    if ((opcode & 0xC7) == 0x05) {
        uint8_t index = (opcode >> 3) & 0x07;
        uint8_t old_val = getRegister(index, mmu);
        uint8_t new_val = old_val - 1;
        setRegister(index, new_val, mmu);
        setFlag(FlagZ, new_val == 0);
        setFlag(FlagN, true);
        setFlag(FlagH, (old_val & 0x0F) == 0x00);
        return (index == 6) ? 3 : 1;
    }

    // 10. ADD HL, rr (0x09, 0x19, 0x29, 0x39)
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
        m_regs.set_hl(result & 0xFFFF);
        return 2;
    }

    // 11. INC rr (0x03, 0x13, 0x23, 0x33)
    if ((opcode & 0xCF) == 0x03) {
        uint8_t pair = (opcode >> 4) & 0x03;
        switch (pair) {
            case 0: m_regs.set_bc(m_regs.bc() + 1); break;
            case 1: m_regs.set_de(m_regs.de() + 1); break;
            case 2: m_regs.set_hl(m_regs.hl() + 1); break;
            case 3: m_regs.sp++; break;
        }
        return 2;
    }

    // 12. DEC rr (0x0B, 0x1B, 0x2B, 0x3B)
    if ((opcode & 0xCF) == 0x0B) {
        uint8_t pair = (opcode >> 4) & 0x03;
        switch (pair) {
            case 0: m_regs.set_bc(m_regs.bc() - 1); break;
            case 1: m_regs.set_de(m_regs.de() - 1); break;
            case 2: m_regs.set_hl(m_regs.hl() - 1); break;
            case 3: m_regs.sp--; break;
        }
        return 2;
    }

    switch (opcode) {
        case 0x00: // NOP
            return 1;

        // --- Instruções Especiais de Carga de 8 bits ---
        case 0x02: // LD [BC], A
            mmu.writeByte(m_regs.bc(), m_regs.a);
            return 2;
        case 0x0A: // LD A, [BC]
            m_regs.a = mmu.readByte(m_regs.bc());
            return 2;

        case 0x12: // LD [DE], A
            mmu.writeByte(m_regs.de(), m_regs.a);
            return 2;
        case 0x1A: // LD A, [DE]
            m_regs.a = mmu.readByte(m_regs.de());
            return 2;

        case 0x22: // LDI [HL], A (LD [HL+], A)
            mmu.writeByte(m_regs.hl(), m_regs.a);
            m_regs.set_hl(m_regs.hl() + 1);
            return 2;
        case 0x2A: // LDI A, [HL] (LD A, [HL+])
            m_regs.a = mmu.readByte(m_regs.hl());
            m_regs.set_hl(m_regs.hl() + 1);
            return 2;

        case 0x32: // LDD [HL], A (LD [HL-], A)
            mmu.writeByte(m_regs.hl(), m_regs.a);
            m_regs.set_hl(m_regs.hl() - 1);
            return 2;
        case 0x3A: // LDD A, [HL] (LD A, [HL-])
            m_regs.a = mmu.readByte(m_regs.hl());
            m_regs.set_hl(m_regs.hl() - 1);
            return 2;

        case 0xE0: { // LDH [n], A
            uint8_t n = fetchByte(mmu);
            mmu.writeByte(0xFF00 + n, m_regs.a);
            return 3;
        }
        case 0xF0: { // LDH A, [n]
            uint8_t n = fetchByte(mmu);
            m_regs.a = mmu.readByte(0xFF00 + n);
            return 3;
        }

        case 0xE2: // LD [C], A
            mmu.writeByte(0xFF00 + m_regs.c, m_regs.a);
            return 2;
        case 0xF2: // LD A, [C]
            m_regs.a = mmu.readByte(0xFF00 + m_regs.c);
            return 2;

        case 0xEA: { // LD [nn], A
            uint16_t nn = fetchWord(mmu);
            mmu.writeByte(nn, m_regs.a);
            return 4;
        }
        case 0xFA: { // LD A, [nn]
            uint16_t nn = fetchWord(mmu);
            m_regs.a = mmu.readByte(nn);
            return 4;
        }

        // --- Instruções Especiais de Carga de 16 bits ---
        case 0x08: { // LD [nn], SP
            uint16_t nn = fetchWord(mmu);
            mmu.writeByte(nn, m_regs.sp & 0xFF);
            mmu.writeByte(nn + 1, m_regs.sp >> 8);
            return 5;
        }

        case 0xF9: // LD SP, HL
            m_regs.sp = m_regs.hl();
            return 2;

        case 0xF8: { // LD HL, SP+e
            int8_t e = static_cast<int8_t>(fetchByte(mmu));
            uint16_t sp = m_regs.sp;
            uint32_t result = sp + e;
            uint8_t u_e = static_cast<uint8_t>(e);

            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, ((sp & 0x0F) + (u_e & 0x0F)) > 0x0F);
            setFlag(FlagC, ((sp & 0xFF) + (u_e & 0xFF)) > 0xFF);

            m_regs.set_hl(result & 0xFFFF);
            return 3;
        }

        // --- Outros Opcodes Especiais da ALU ---
        case 0x27: { // DAA (Decimal Adjust Accumulator)
            uint8_t a = m_regs.a;
            uint8_t correction = 0;
            bool carry = false;
            
            if (getFlag(FlagN)) { // Subtração
                if (getFlag(FlagC)) {
                    correction |= 0x60;
                    carry = true;
                }
                if (getFlag(FlagH)) {
                    correction |= 0x06;
                }
                a -= correction;
            } else { // Adição
                if (getFlag(FlagC) || a > 0x99) {
                    correction |= 0x60;
                    carry = true;
                }
                if (getFlag(FlagH) || (a & 0x0F) > 0x09) {
                    correction |= 0x06;
                }
                a += correction;
            }
            
            m_regs.a = a;
            setFlag(FlagZ, a == 0);
            setFlag(FlagH, false);
            setFlag(FlagC, carry);
            return 1;
        }

        case 0x2F: // CPL (Complement A)
            m_regs.a = ~m_regs.a;
            setFlag(FlagN, true);
            setFlag(FlagH, true);
            return 1;

        case 0x37: // SCF (Set Carry Flag)
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, true);
            return 1;

        case 0x3F: // CCF (Complement Carry Flag)
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, !getFlag(FlagC));
            return 1;

        case 0xE8: { // ADD SP, e
            int8_t e = static_cast<int8_t>(fetchByte(mmu));
            uint16_t sp = m_regs.sp;
            uint32_t result = sp + e;
            uint8_t u_e = static_cast<uint8_t>(e);

            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, ((sp & 0x0F) + (u_e & 0x0F)) > 0x0F);
            setFlag(FlagC, ((sp & 0xFF) + (u_e & 0xFF)) > 0xFF);

            m_regs.sp = result & 0xFFFF;
            return 4;
        }

        // --- Instruções Rápidas de Rotação (Apenas Acumulador A) ---
        case 0x07: { // RLCA
            uint8_t c = (m_regs.a >> 7) & 1;
            m_regs.a = (m_regs.a << 1) | c;
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, c != 0);
            return 1;
        }
        case 0x0F: { // RRCA
            uint8_t c = m_regs.a & 1;
            m_regs.a = (m_regs.a >> 1) | (c << 7);
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, c != 0);
            return 1;
        }
        case 0x17: { // RLA
            uint8_t old_c = getFlag(FlagC) ? 1 : 0;
            uint8_t new_c = (m_regs.a >> 7) & 1;
            m_regs.a = (m_regs.a << 1) | old_c;
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, new_c != 0);
            return 1;
        }
        case 0x1F: { // RRA
            uint8_t old_c = getFlag(FlagC) ? 1 : 0;
            uint8_t new_c = m_regs.a & 1;
            m_regs.a = (m_regs.a >> 1) | (old_c << 7);
            setFlag(FlagZ, false);
            setFlag(FlagN, false);
            setFlag(FlagH, false);
            setFlag(FlagC, new_c != 0);
            return 1;
        }

        // --- Instruções de Controle do Processador ---
        case 0x10: // STOP
            fetchByte(mmu); // Consome o byte imediato extra (0x00)
            return 1;
        case 0x76: // HALT
            return 1;
        case 0xF3: // DI
            m_ime = false;
            return 1;
        case 0xFB: // EI
            m_ime = true;
            return 1;

        case 0xCB: { // Prefixo para instruções de manipulação de bits
            uint8_t cbOpcode = fetchByte(mmu);
            return executeCBOpcode(cbOpcode, mmu);
        }
            
        default:
            std::cerr << "AVISO: Opcode nao implementado: 0x" 
                      << std::hex << std::setw(2) << std::setfill('0') << (int)opcode 
                      << " em PC: 0x" << std::setw(4) << (m_regs.pc - 1) << std::endl;
            return 1;
    }
}

uint8_t CPU::executeCBOpcode(uint8_t cbOpcode, MMU& mmu) {
    uint8_t mode = (cbOpcode >> 6) & 0x03;       // bits 6-7
    uint8_t bit_or_op = (cbOpcode >> 3) & 0x07;  // bits 3-5
    uint8_t reg_index = cbOpcode & 0x07;         // bits 0-2
    
    uint8_t val = getRegister(reg_index, mmu);
    uint8_t cycles = (reg_index == 6) ? 4 : 2; // Memória [HL] leva 4 M-cycles, registradores levam 2 M-cycles

    if (mode == 0) { // Rotate / Shift / Swap
        switch (bit_or_op) {
            case 0: { // RLC
                uint8_t c = (val >> 7) & 1;
                uint8_t res = (val << 1) | c;
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
            case 1: { // RRC
                uint8_t c = val & 1;
                uint8_t res = (val >> 1) | (c << 7);
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
            case 2: { // RL
                uint8_t old_c = getFlag(FlagC) ? 1 : 0;
                uint8_t new_c = (val >> 7) & 1;
                uint8_t res = (val << 1) | old_c;
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, new_c != 0);
                break;
            }
            case 3: { // RR
                uint8_t old_c = getFlag(FlagC) ? 1 : 0;
                uint8_t new_c = val & 1;
                uint8_t res = (val >> 1) | (old_c << 7);
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, new_c != 0);
                break;
            }
            case 4: { // SLA
                uint8_t c = (val >> 7) & 1;
                uint8_t res = val << 1;
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
            case 5: { // SRA
                uint8_t c = val & 1;
                uint8_t res = (val >> 1) | (val & 0x80);
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
            case 6: { // SWAP
                uint8_t res = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, false);
                break;
            }
            case 7: { // SRL
                uint8_t c = val & 1;
                uint8_t res = val >> 1;
                setRegister(reg_index, res, mmu);
                setFlag(FlagZ, res == 0);
                setFlag(FlagN, false);
                setFlag(FlagH, false);
                setFlag(FlagC, c != 0);
                break;
            }
        }
    } else if (mode == 1) { // BIT
        uint8_t bit = bit_or_op;
        bool is_set = (val & (1 << bit)) != 0;
        setFlag(FlagZ, !is_set);
        setFlag(FlagN, false);
        setFlag(FlagH, true);
        if (reg_index == 6) {
            cycles = 3; // BIT b, [HL] leva 3 M-cycles
        }
    } else if (mode == 2) { // RES
        uint8_t bit = bit_or_op;
        uint8_t res = val & ~(1 << bit);
        setRegister(reg_index, res, mmu);
    } else if (mode == 3) { // SET
        uint8_t bit = bit_or_op;
        uint8_t res = val | (1 << bit);
        setRegister(reg_index, res, mmu);
    }
    
    return cycles;
}

void CPU::handleInterrupts(MMU& mmu) {
    uint8_t ie = mmu.readByte(0xFFFF);
    uint8_t ifReg = mmu.readByte(0xFF0F);
    uint8_t pending = ie & ifReg;

    if (pending == 0) return;

    // Prioridade física: V-Blank (Bit 0) > LCD STAT (Bit 1) > Timer (Bit 2) > Serial (Bit 3) > Joypad (Bit 4)
    if (pending & 0x01) {
        serviceInterrupt(0, 0x0040, mmu);
    } else if (pending & 0x02) {
        serviceInterrupt(1, 0x0048, mmu);
    } else if (pending & 0x04) {
        serviceInterrupt(2, 0x0050, mmu);
    } else if (pending & 0x08) {
        serviceInterrupt(3, 0x0058, mmu);
    } else if (pending & 0x10) {
        serviceInterrupt(4, 0x0060, mmu);
    }
}

void CPU::serviceInterrupt(uint8_t interruptBit, uint16_t vectorAddress, MMU& mmu) {
    m_ime = false; // Desliga chave geral de interrupções

    // Limpa a flag de solicitação correspondente em IF (0xFF0F)
    uint8_t ifReg = mmu.readByte(0xFF0F);
    mmu.writeByte(0xFF0F, ifReg & ~(1 << interruptBit));

    // Salva o PC de retorno na pilha
    pushWord(mmu, m_regs.pc);

    // Desvia fluxo para o endereço do vetor
    m_regs.pc = vectorAddress;
}
