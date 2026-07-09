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
              << " | Flags (ZNHC): " 
              << (cpu.getFlag(CPU::FlagZ) ? 'Z' : '-')
              << (cpu.getFlag(CPU::FlagN) ? 'N' : '-')
              << (cpu.getFlag(CPU::FlagH) ? 'H' : '-')
              << (cpu.getFlag(CPU::FlagC) ? 'C' : '-')
              << std::endl;
}

int main() {
    std::cout << "Inicializando Emulador de Game Boy (Fase 2.3: Teste de Control Flow)..." << std::endl;
    
    MMU mmu;
    CPU cpu;
    
    // Opcodes a serem testados colocados a partir de 0x0100:
    std::vector<uint8_t> testROM(0x200, 0x00);
    
    // 0x0100: JP 0x0105 (0xC3 0x05 0x01)
    testROM[0x0100] = 0xC3;
    testROM[0x0101] = 0x05;
    testROM[0x0102] = 0x01;
    
    // 0x0105: XOR A (0xAF -> ativa Zero Z=1)
    testROM[0x0105] = 0xAF;
    
    // 0x0106: JR Z, 2 (0x28 0x02 -> Salto tomado porque Z=1. PC novo = 0x0108 + 2 = 0x010A)
    testROM[0x0106] = 0x28;
    testROM[0x0107] = 0x02;
    
    // 0x010a: XOR A (0xAF -> mantém Zero Z=1)
    testROM[0x010a] = 0xAF;
    
    // 0x010b: JR NZ, 5 (0x20 0x05 -> Salto NAO tomado porque Z=1. PC avança normalmente para 0x010D)
    testROM[0x010b] = 0x20;
    testROM[0x010c] = 0x05;
    
    // 0x010d: CALL 0x0115 (0xCD 0x15 0x01 -> PC vai para 0x0115, empilha 0x0110 no stack)
    testROM[0x010d] = 0xCD;
    testROM[0x010e] = 0x15;
    testROM[0x010f] = 0x01;
    
    // 0x0110: RST 0x18 (0xDF -> PC vai para 0x0018, empilha 0x0111 no stack)
    testROM[0x0110] = 0xDF;
    
    // Subrotina em 0x0115: RET (0xC9 -> desempilha 0x0110 e volta para lá)
    testROM[0x0115] = 0xC9;
    
    // Rotina de Restart RST 0x18 em 0x0018 do barramento da MMU (a MMU suporta carregar toda a ROM)
    testROM[0x0018] = 0xC9; // RET (desempilha 0x0111 e volta para lá)

    if (!mmu.loadROM(testROM)) {
        std::cerr << "Falha ao carregar ROM de teste!" << std::endl;
        return 1;
    }
    
    // Desabilita a Boot ROM na MMU para expor a ROM na faixa 0x0000 - 0x00FF
    mmu.writeByte(0xFF50, 1);
    
    uint8_t cycles = 0;
    uint16_t spBefore = cpu.getRegs().sp;

    // 1. JP 0x0105
    cycles = cpu.step(mmu);
    printCpuState(cpu, "Apos JP 0x0105");
    assert(cpu.getRegs().pc == 0x0105);
    assert(cycles == 3);

    // 2. XOR A
    cpu.step(mmu);
    assert(cpu.getFlag(CPU::FlagZ) == true);

    // 3. JR Z, 2 (Tomado!)
    cycles = cpu.step(mmu);
    printCpuState(cpu, "Apos JR Z, 2 (Tomado)");
    assert(cpu.getRegs().pc == 0x010a);
    assert(cycles == 3); // Tomado custa 3 M-cycles

    // 4. XOR A
    cpu.step(mmu);

    // 5. JR NZ, 5 (Nao tomado!)
    cycles = cpu.step(mmu);
    printCpuState(cpu, "Apos JR NZ, 5 (Nao Tomado)");
    assert(cpu.getRegs().pc == 0x010d);
    assert(cycles == 2); // Nao tomado custa 2 M-cycles (verificação de penalidade de desvio)

    // 6. CALL 0x0115
    cycles = cpu.step(mmu);
    printCpuState(cpu, "Apos CALL 0x0115");
    assert(cpu.getRegs().pc == 0x0115);
    assert(cpu.getRegs().sp == spBefore - 2);
    // Verifica se salvou o PC de retorno correto (0x0110) na pilha
    assert(mmu.readByte(cpu.getRegs().sp) == 0x10);
    assert(mmu.readByte(cpu.getRegs().sp + 1) == 0x01);
    assert(cycles == 6); // CALL custa 6 M-cycles

    // 7. RET (dentro da subrotina)
    cycles = cpu.step(mmu);
    printCpuState(cpu, "Apos RET");
    assert(cpu.getRegs().pc == 0x0110);
    assert(cpu.getRegs().sp == spBefore);
    assert(cycles == 4); // RET incondicional custa 4 M-cycles

    // 8. RST 0x18 (em 0x0110)
    cycles = cpu.step(mmu);
    printCpuState(cpu, "Apos RST 0x18");
    assert(cpu.getRegs().pc == 0x0018);
    assert(cpu.getRegs().sp == spBefore - 2);
    // Verifica se empilhou retorno correto (0x0111)
    assert(mmu.readByte(cpu.getRegs().sp) == 0x11);
    assert(mmu.readByte(cpu.getRegs().sp + 1) == 0x01);
    assert(cycles == 4); // RST custa 4 M-cycles

    // 9. RET (em 0x0018)
    cycles = cpu.step(mmu);
    printCpuState(cpu, "Apos RET (do RST)");
    assert(cpu.getRegs().pc == 0x0111);
    assert(cpu.getRegs().sp == spBefore);
    assert(cycles == 4);

    std::cout << "\nTodos os testes de fluxo de controle (Jumps/Calls/RST) passaram com sucesso!" << std::endl;
    return 0;
}
