/*
MIT License

Copyright (c) 2020 Aleksei Dynda

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "nes_cartridge.h"
#include "nes_apu.h"

#include <stdint.h>
#include <string>

typedef struct
{
    uint16_t pc;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
    uint8_t sp;

    uint16_t absAddr;
    uint16_t relAddr;
    bool implied;
} NesCpuState;

class NesCpu
{
public:
    NesCpu();

    ~NesCpu();

    /** Resets nes apu state */
    void reset();

    /** Power nes apu */
    void power();

    /** executes single instruction. returns false if cpu hardware error is detected */
    bool executeInstruction();

    /**
     * Calls subroutine limiting the maximum number of instructions.
     * Returns negative number if cpu error detected
     * Returns positive number if subroutine call is completed
     * Returns zero if limit of instructions is reached, but subroutine is not completed (use continueSubroutine).
     */
    int callSubroutine(uint16_t addr, int maxInstructions = -1);

    /**
     * Continues subroutine from the last place limiting the maximum number of instructions.
     * Returns negative number if cpu error detected
     * Returns positive number if subroutine call is completed
     * Returns zero if limit of instructions is reached, but subroutine is not completed (use continueSubroutine).
     */
    int continueSubroutine( int maxInstructions = -1 );

    /**
     * Reads memory byte at specified address.
     * Real accessed address depends on iNES mapper used.
     */
    uint8_t read(uint16_t address);

    /**
     * Wrties memory byte to specified address.
     * Real accessed address depends on iNES mapper used.
     */
    bool write(uint16_t address, uint8_t data);

    /**
     * Associates Nes CPU with cartridge.
     * cartridge object will be deleted by NesApu.
     */
    void insertCartridge( NesCartridge *cartridge );

    /**
     * Returns pointer to currently associated cartridge
     */
    NesCartridge *getCartridge();

    /**
     * Returns pointer to Nes APU unit
     */
    NesApu *getApu() { return &m_apu; }

    NesCpuState &cpuState();

private:
    struct Instruction
    {
        Instruction(void (NesCpu::* &&a)(void), void (NesCpu::* &&b)(void)): opcode(a), addrmode(b) {}
        void (NesCpu::*opcode)  (void) = nullptr;
        void (NesCpu::*addrmode)(void) = nullptr;
    };

    NesApu m_apu;
    NesCpuState m_cpu{};
    uint8_t m_stopSp;
    // Nes cpu RAM
    uint8_t *m_ram = nullptr;
    NesCartridge *m_cartridge = nullptr;

    static const NesCpu::Instruction commands[256];

    // APU Processing
    void updateRectChannel(int i);
    void updateTriangleChannel(ChannelInfo &info);
    void updateNoiseChannel(ChannelInfo &chan);
    void updateDmcChannel(ChannelInfo &info);
    void updateFrameCounter();

    // RAM/ROM Access
    void IMD();
    void ZP();
    void ZPX();
    void ZPY();
    void REL();
    void ABS();
    void ABX();
    void ABY();
    void IND();
    void IDX();
    void IDY();
    void UND() { m_cpu.implied = true; };
    uint8_t fetch() { return read( m_cpu.pc++ ); }
    uint8_t readInternal(uint16_t address);

    // CPU Core
    // opcodes
    void BPL();
    void ADC();
    void SBC();
    void TAX();
    void TAY();
    void TXA();
    void TYA();
    void INY();
    void LDA();
    void ASL();
    void CLC();
    void BIT();
    void JMP();
    void JSR();
    void STA();
    void STX();
    void STY();
    void LDX();
    void LDY();
    void BEQ();
    void BNE();
    void BMI();
    void BCC();
    void BCS();
    void CMP();
    void CPY();
    void CPX();
    void RTS();
    void DEC();
    void INC();
    void DEX();
    void DEY();
    void INX();
    void AND();
    void ORA();
    void EOR();
    void NOP();
    void LSR();
    void ROR();
    void ROL();
    void PHA();
    void PLA();
    void SEC();
    void BRK();

    std::string getOpCode(const Instruction & instruction, uint16_t data);
    void modifyFlags(uint8_t data);
    void printCpuState( const Instruction & instruction, uint16_t pc );

};
