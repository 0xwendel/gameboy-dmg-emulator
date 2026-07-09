#pragma once

#include "mmu.hpp"
#include <cstdint>

class CPU {
public:
    // Estrutura dos registradores da CPU do Game Boy (SM83)
    struct Registers {
        uint8_t a;
        uint8_t f;
        uint8_t b;
        uint8_t c;
        uint8_t d;
        uint8_t e;
        uint8_t h;
        uint8_t l;
        uint16_t sp;
        uint16_t pc;

        // Getters para os pares de registradores de 16 bits
        uint16_t af() const { return (a << 8) | f; }
        uint16_t bc() const { return (b << 8) | c; }
        uint16_t de() const { return (d << 8) | e; }
        uint16_t hl() const { return (h << 8) | l; }

        // Setters para os pares de registradores de 16 bits
        // Importante: Os 4 bits menos significativos de F são sempre 0
        void set_af(uint16_t val) { a = val >> 8; f = val & 0xF0; }
        void set_bc(uint16_t val) { b = val >> 8; c = val & 0xFF; }
        void set_de(uint16_t val) { d = val >> 8; e = val & 0xFF; }
        void set_hl(uint16_t val) { h = val >> 8; l = val & 0xFF; }
    };

    // Definição dos bits de flags no registrador F
    enum Flags : uint8_t {
        FlagZ = 1 << 7, // Zero Flag
        FlagN = 1 << 6, // Subtract Flag
        FlagH = 1 << 5, // Half-Carry Flag
        FlagC = 1 << 4  // Carry Flag
    };

    CPU();
    ~CPU() = default;

    // Executa uma única instrução (Fetch -> Decode -> Execute)
    // Retorna a quantidade de ciclos de máquina (M-cycles) gastos
    uint8_t step(MMU& mmu);

    // Helpers para controle individual de flags
    void setFlag(uint8_t flag, bool value);
    bool getFlag(uint8_t flag) const;

    // Acesso aos registradores (para testes e depuração)
    const Registers& getRegs() const { return m_regs; }
    Registers& getRegs() { return m_regs; }

private:
    Registers m_regs;
    bool m_ime; // Interrupt Master Enable flag

    // Métodos utilitários de barramento para fetch de instruções
    uint8_t fetchByte(MMU& mmu);
    uint16_t fetchWord(MMU& mmu);

    // Helpers para manipulação da pilha
    void pushWord(MMU& mmu, uint16_t value);
    uint16_t popWord(MMU& mmu);

    // Helpers para decodificação genérica de registradores de 8 bits (0-7)
    uint8_t getRegister(uint8_t index, MMU& mmu) const;
    void setRegister(uint8_t index, uint8_t value, MMU& mmu);

    // Auxiliar para executar operações lógicas/aritméticas da ALU de 8 bits
    void executeALU(uint8_t op, uint8_t val);

    // Avalia as condições de branch (NZ, Z, NC, C) codificadas no opcode
    bool checkCondition(uint8_t cond) const;

    // Decodifica e executa uma instrução padrão (unprefixed)
    uint8_t executeOpcode(uint8_t opcode, MMU& mmu);

    // Decodifica e executa uma instrução prefixada (0xCB)
    uint8_t executeCBOpcode(uint8_t cbOpcode, MMU& mmu);
};
