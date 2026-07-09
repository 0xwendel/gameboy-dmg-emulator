#include "mmu.hpp"
#include "cpu.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cassert>

void printCpuState(const CPU& cpu, const std::string& label) {
    const auto& regs = cpu.getRegs();
    std::cout << "--- " << label << " ---\n"
              << "PC: 0x" << std::hex << regs.pc 
              << " | SP: 0x" << regs.sp 
              << " | AF: 0x" << regs.af() 
              << " | BC: 0x" << regs.bc() 
              << " | DE: 0x" << regs.de() 
              << " | HL: 0x" << regs.hl() 
              << " | IME: " << (cpu.getIme() ? "ON" : "OFF")
              << " | Flags (ZNHC): " 
              << (cpu.getFlag(CPU::FlagZ) ? 'Z' : '-')
              << (cpu.getFlag(CPU::FlagN) ? 'N' : '-')
              << (cpu.getFlag(CPU::FlagH) ? 'H' : '-')
              << (cpu.getFlag(CPU::FlagC) ? 'C' : '-')
              << std::endl;
}

int main() {
    std::cout << "Inicializando Emulador de Game Boy (Fase 2.4: Teste de Bits e Controle)..." << std::endl;
    
    MMU mmu;
    CPU cpu;
    
    // Opcodes a serem testados colocados a partir de 0x0100:
    std::vector<uint8_t> testROM(0x200, 0x00);
    
    // 1. Teste de RLA (Rotação do Acumulador pelo Carry)
    testROM[0x0100] = 0x3E; // LD A, 0x80
    testROM[0x0101] = 0x80;
    testROM[0x0102] = 0x37; // SCF (Carry=1)
    testROM[0x0103] = 0x17; // RLA (A = 0x80 rotacionado por Carry=1 -> A = 0x01, Carry=1, Z=0)
    
    // 2. Teste de SWAP A (Opcode 0xCB 0x37)
    testROM[0x0104] = 0x3E; // LD A, 0x3C
    testROM[0x0105] = 0x3C;
    testROM[0x0106] = 0xCB; // Prefixo CB
    testROM[0x0107] = 0x37; // SWAP A (A = 0xC3)
    
    // 3. Teste de BIT 3, A (Opcode 0xCB 0x5F)
    testROM[0x0108] = 0xCB; // Prefixo CB
    testROM[0x0109] = 0x5F; // BIT 3, A (A=0xC3 -> bit 3 é 0, logo Z=1, N=0, H=1)
    
    // 4. Teste de SET 3, A (Opcode 0xCB 0xDF)
    testROM[0x010a] = 0xCB; // Prefixo CB
    testROM[0x010b] = 0xDF; // SET 3, A (A = 0xC3 | 0x08 = 0xCB)
    
    // 5. Teste de RES 3, A (Opcode 0xCB 0x9F)
    testROM[0x010c] = 0xCB; // Prefixo CB
    testROM[0x010d] = 0x9F; // RES 3, A (A = 0xCB & ~0x08 = 0xC3)
    
    // 6. Teste de SRL B (Deslocamento Lógico de B para Direita) (Opcode 0xCB 0x38)
    testROM[0x010e] = 0x06; // LD B, 0x81
    testROM[0x010f] = 0x81;
    testROM[0x0110] = 0xCB; // Prefixo CB
    testROM[0x0111] = 0x38; // SRL B (B = 0x40, Carry=1, Z=0)
    
    // 7. Teste de Controle: DI / EI
    testROM[0x0112] = 0xF3; // DI (IME = OFF)
    testROM[0x0113] = 0xFB; // EI (IME = ON)
    
    if (!mmu.loadROM(testROM)) {
        std::cerr << "Falha ao carregar ROM de teste!" << std::endl;
        return 1;
    }
    
    uint8_t cycles = 0;

    // --- Executa 1. RLA ---
    cpu.step(mmu); // LD A, 0x80
    cpu.step(mmu); // SCF
    cycles = cpu.step(mmu); // RLA
    printCpuState(cpu, "Apos RLA (Standard Rotate)");
    assert(cpu.getRegs().a == 0x01);
    assert(cpu.getFlag(CPU::FlagZ) == false); // Z deve ser zerada na rotação padrão
    assert(cpu.getFlag(CPU::FlagC) == true);  // Bit 7 original (1) foi para o carry
    assert(cycles == 1);                      // RLCA/RRCA/RLA/RRA duram 1 M-cycle

    // --- Executa 2. SWAP A ---
    cpu.step(mmu); // LD A, 0x3C
    cycles = cpu.step(mmu); // SWAP A
    printCpuState(cpu, "Apos SWAP A");
    assert(cpu.getRegs().a == 0xC3);
    assert(cpu.getFlag(CPU::FlagZ) == false);
    assert(cpu.getFlag(CPU::FlagC) == false);
    assert(cycles == 2); // Rotates/Shifts/Swaps de registrador em CB duram 2 M-cycles

    // --- Executa 3. BIT 3, A ---
    cycles = cpu.step(mmu); // BIT 3, A
    printCpuState(cpu, "Apos BIT 3, A");
    assert(cpu.getFlag(CPU::FlagZ) == true); // Bit 3 de 0xC3 (1100 0011) é 0, então Z=1
    assert(cpu.getFlag(CPU::FlagN) == false);
    assert(cpu.getFlag(CPU::FlagH) == true);
    assert(cycles == 2);

    // --- Executa 4. SET 3, A ---
    cycles = cpu.step(mmu); // SET 3, A
    printCpuState(cpu, "Apos SET 3, A");
    assert(cpu.getRegs().a == 0xCB); // 0xC3 | 0x08 = 0xCB
    assert(cycles == 2);

    // --- Executa 5. RES 3, A ---
    cycles = cpu.step(mmu); // RES 3, A
    printCpuState(cpu, "Apos RES 3, A");
    assert(cpu.getRegs().a == 0xC3); // 0xCB & ~0x08 = 0xC3
    assert(cycles == 2);

    // --- Executa 6. SRL B ---
    cpu.step(mmu); // LD B, 0x81
    cycles = cpu.step(mmu); // SRL B
    printCpuState(cpu, "Apos SRL B");
    assert(cpu.getRegs().b == 0x40);
    assert(cpu.getFlag(CPU::FlagZ) == false);
    assert(cpu.getFlag(CPU::FlagC) == true); // Bit 0 original (1) foi para o Carry
    assert(cycles == 2);

    // --- Executa 7. DI / EI ---
    assert(cpu.getIme() == false); // Começa desligado
    cycles = cpu.step(mmu); // DI
    printCpuState(cpu, "Apos DI");
    assert(cpu.getIme() == false);
    assert(cycles == 1);

    cycles = cpu.step(mmu); // EI
    printCpuState(cpu, "Apos EI");
    assert(cpu.getIme() == true);
    assert(cycles == 1);

    std::cout << "\nTodos os testes de bits e controle de estado passaram com sucesso!" << std::endl;
    return 0;
}
