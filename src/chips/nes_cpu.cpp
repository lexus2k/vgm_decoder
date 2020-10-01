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

#include "chips/nes_apu.h"
#include "chips/nes_cpu.h"

#include <malloc.h>
#include <stdio.h>

#define NES_CPU_DEBUG 1
//#define DEBUG_NES_CPU

#if NES_CPU_DEBUG && !defined(VGM_DECODER_LOGGER)
#define VGM_DECODER_LOGGER NES_CPU_DEBUG
#endif
#include "../vgm_logger.h"

#define NES_CPU_FREQUENCY (1789773)

NesCpu::NesCpu()
    : m_apu( this )
{
    reset();
}

NesCpu::~NesCpu()
{
    if ( m_ram )
    {
        free( m_ram );
        m_ram = nullptr;
    }
    if ( m_cartridge )
    {
        delete m_cartridge;
        m_cartridge = nullptr;
    }
}


void NesCpu::insertCartridge( NesCartridge * cartridge )
{
    if ( m_cartridge )
    {
        delete m_cartridge;
        m_cartridge = nullptr;
    }
    m_cartridge = cartridge;
}

NesCartridge *NesCpu::getCartridge()
{
    return m_cartridge;
}

void NesCpu::reset()
{
    m_apu.reset();
    if ( m_cartridge ) m_cartridge->reset();
}

void NesCpu::power()
{
    m_apu.power();
    if ( m_cartridge ) m_cartridge->power();
}

uint8_t NesCpu::read(uint16_t address)
{
    if ( address < 0x2000 )
    {
        if ( m_ram == nullptr ) m_ram = static_cast<uint8_t *>(malloc(2048));
        LOGM("[%04X] ==> %02X\n", address, m_ram[address & 0x07FF]);
        return m_ram[address & 0x07FF];
    }
    if ( address >= 0x4000 && address < 0x4020 )
    {
        return m_apu.read( address );
    }
    if ( address >= 0x4020 && m_cartridge )
    {
        return m_cartridge->read( address );
    }
    LOGE("Memory data fetch error 0x%04X\n", address);
    return 0xFF;
}

uint8_t NesCpu::readInternal(uint16_t address)
{
    return read( address );
}

bool NesCpu::write(uint16_t address, uint8_t data)
{
    if ( address < 0x2000 )
    {
        if ( m_ram == nullptr ) m_ram = static_cast<uint8_t *>(malloc(2048));
        m_ram[address & 0x07FF] = data;
        LOGM("[%04X] <== %02X\n", address, data);
        return true;
    }
    if ( address >= 0x4000 && address < 0x4020 )
    {
        m_apu.write( address, data );
        return true;
    }
    if ( address >= 0x4020 && m_cartridge )
    {
        return m_cartridge->write( address, data );
    }
    LOGE("Memory data write error (ROM) 0x%04X\n", address);
    return false;
}

NesCpuState &NesCpu::cpuState()
{
    return m_cpu;
}

void NesCpu::IMD()
{
    m_cpu.absAddr = m_cpu.pc;
    m_cpu.pc++;
}

void NesCpu::ZP()
{
    m_cpu.absAddr = fetch();
}

void NesCpu::ZPX()
{
    m_cpu.absAddr = (fetch() + m_cpu.x) & 0x00FF;
}

void NesCpu::ZPY()
{
    m_cpu.absAddr = (fetch() + m_cpu.y) & 0x00FF;
}

void NesCpu::REL()
{
    m_cpu.relAddr = fetch();
    if ( m_cpu.relAddr & 0x80)
        m_cpu.relAddr |= 0xFF00;
}

void NesCpu::ABS()
{
    m_cpu.absAddr = fetch();
    m_cpu.absAddr |= static_cast<uint16_t>(fetch()) << 8;
}

void NesCpu::ABX()
{
    m_cpu.absAddr = static_cast<uint16_t>(fetch());
    m_cpu.absAddr |= (static_cast<uint16_t>(fetch()) << 8);
    m_cpu.absAddr += m_cpu.x;
}

void NesCpu::ABY()
{
    m_cpu.absAddr = fetch();
    m_cpu.absAddr |= (static_cast<uint16_t>(fetch()) << 8);
    m_cpu.absAddr += m_cpu.y;
}

void NesCpu::IND()
{
    m_cpu.absAddr = fetch();
    m_cpu.absAddr |= (static_cast<uint16_t>(fetch()) << 8);
    // no page boundary hardware bug
    m_cpu.absAddr = (read(m_cpu.absAddr)) | (read( m_cpu.absAddr + 1 ) << 8);
}

void NesCpu::IDX()
{
    m_cpu.absAddr = (fetch() + m_cpu.x) & 0x00FF;
    m_cpu.absAddr = (read(m_cpu.absAddr)) | (read( (m_cpu.absAddr + 1) & 0xFF ) << 8);
}

void NesCpu::IDY()
{
    m_cpu.absAddr = fetch();
    m_cpu.absAddr = static_cast<uint16_t>(read( m_cpu.absAddr )) |
                    (static_cast<uint16_t>(read( (m_cpu.absAddr + 1) & 0xFF )) << 8);
    m_cpu.absAddr += m_cpu.y;
}

enum
{
    C_FLAG = 0x01,
    Z_FLAG = 0x02,
    I_D_FLAG = 0x04,
    D_FLAG = 0x08,
    B_FLAG = 0x10,
    U_FLAG = 0x20, // Unused
    V_FLAG = 0x40,
    N_FLAG = 0x80,
};

void NesCpu::modifyFlags(uint8_t baseData)
{
    m_cpu.flags &= ~(Z_FLAG | N_FLAG);
    m_cpu.flags |= baseData ? 0: Z_FLAG;
    m_cpu.flags |= (baseData & 0x80) ? N_FLAG : 0;
}

void NesCpu::printCpuState(const Instruction & instruction, uint16_t pc)
{
    LOGI("SP:%02X A:%02X X:%02X Y:%02X F:%02X [%04X] (0x%02X) %s\n",
         m_cpu.sp, m_cpu.a, m_cpu.x, m_cpu.y, m_cpu.flags, pc, readInternal( pc ),
         getOpCode( instruction, readInternal(pc + 1) | (static_cast<uint16_t>(readInternal(pc + 2)) << 8)).c_str() );
}

#define GEN_OPCODE(x) if ( instruction.opcode == &NesCpu::x ) opcode = # x
#define GEN_ADDRMODE(x, y) if ( instruction.addrmode == &NesCpu::x ) opcode += y

static std::string hexToString( uint16_t hex )
{
    char str[6]{};
    sprintf( str, "%04X", hex );
    return str;
}

static std::string hexToString( uint8_t hex )
{
    char str[4]{};
    sprintf( str, "%02X", hex );
    return str;
}

using c = NesCpu;
const NesCpu::Instruction NesCpu::commands[256] =
{
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 0X */{ &c::BRK, &c::UND }, { &c::ORA, &c::IDX }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::ORA, &c::ZP  }, { &c::ASL, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 0X */{ &c::UND, &c::UND }, { &c::ORA, &c::IMD }, { &c::ASL, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::ORA, &c::ABS }, { &c::ASL, &c::ABS }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 1X */{ &c::BPL, &c::REL }, { &c::ORA, &c::IDY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::ORA, &c::ZPX }, { &c::ASL, &c::ZPX }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 1X */{ &c::CLC, &c::UND }, { &c::ORA, &c::ABY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::ORA, &c::ABX }, { &c::ASL, &c::ABX }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 2X */{ &c::JSR, &c::ABS }, { &c::AND, &c::IDX }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::BIT, &c::ZP  }, { &c::AND, &c::ZP  }, { &c::ROL, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 2X */{ &c::UND, &c::UND }, { &c::AND, &c::IMD }, { &c::ROL, &c::UND }, { &c::UND, &c::UND }, { &c::BIT, &c::ABS }, { &c::AND, &c::ABS }, { &c::ROL, &c::ABS }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 3X */{ &c::BMI, &c::REL }, { &c::AND, &c::IDY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::AND, &c::ZPX }, { &c::ROL, &c::ZPX }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 3X */{ &c::SEC, &c::UND }, { &c::AND, &c::ABY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::AND, &c::ABX }, { &c::ROL, &c::ABX }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 4X */{ &c::UND, &c::UND }, { &c::EOR, &c::IDX }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::EOR, &c::ZP  }, { &c::LSR, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 4X */{ &c::PHA, &c::UND }, { &c::EOR, &c::IMD }, { &c::LSR, &c::UND }, { &c::UND, &c::UND }, { &c::JMP, &c::ABS }, { &c::EOR, &c::ABS }, { &c::LSR, &c::ABS }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 5X */{ &c::UND, &c::UND }, { &c::EOR, &c::IDY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::EOR, &c::ZPX }, { &c::LSR, &c::ZPX }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 5X */{ &c::UND, &c::UND }, { &c::EOR, &c::ABY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::EOR, &c::ABX }, { &c::LSR, &c::ABX }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 6X */{ &c::RTS, &c::UND }, { &c::ADC, &c::IDX }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::ADC, &c::ZP  }, { &c::ROR, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 6X */{ &c::PLA, &c::UND }, { &c::ADC, &c::IMD }, { &c::ROR, &c::UND }, { &c::UND, &c::UND }, { &c::JMP, &c::IND }, { &c::ADC, &c::ABS }, { &c::ROR, &c::ABS }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 7X */{ &c::UND, &c::UND }, { &c::ADC, &c::IDY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::ADC, &c::ZPX }, { &c::ROR, &c::ZPX }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 7X */{ &c::UND, &c::UND }, { &c::ADC, &c::ABY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::ADC, &c::ABX }, { &c::ROR, &c::ABX }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 8X */{ &c::UND, &c::UND }, { &c::STA, &c::IDX }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::STY, &c::ZP  }, { &c::STA, &c::ZP  }, { &c::STX, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 8X */{ &c::DEY, &c::UND }, { &c::UND, &c::UND }, { &c::TXA, &c::UND }, { &c::UND, &c::UND }, { &c::STY, &c::ABS }, { &c::STA, &c::ABS }, { &c::STX, &c::ABS }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* 9X */{ &c::BCC, &c::REL }, { &c::STA, &c::IDY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::STY, &c::ZPX }, { &c::STA, &c::ZPX }, { &c::STX, &c::ZPY }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 9X */{ &c::TYA, &c::UND }, { &c::STA, &c::ABY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::STA, &c::ABX }, { &c::UND, &c::UND }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* AX */{ &c::LDY, &c::IMD }, { &c::LDA, &c::IDX }, { &c::LDX, &c::IMD }, { &c::UND, &c::UND }, { &c::LDY, &c::ZP  }, { &c::LDA, &c::ZP  }, { &c::LDX, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* AX */{ &c::TAY, &c::UND }, { &c::LDA, &c::IMD }, { &c::TAX, &c::UND }, { &c::UND, &c::UND }, { &c::LDY, &c::ABS }, { &c::LDA, &c::ABS }, { &c::LDX, &c::ABS }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* BX */{ &c::BCS, &c::REL }, { &c::LDA, &c::IDY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::LDY, &c::ZPX }, { &c::LDA, &c::ZPX }, { &c::LDX, &c::ZPY }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* BX */{ &c::UND, &c::UND }, { &c::LDA, &c::ABY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::LDY, &c::ABX }, { &c::LDA, &c::ABX }, { &c::LDX, &c::ABY }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* CX */{ &c::CPY, &c::IMD }, { &c::CMP, &c::IDX }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::CPY, &c::ZP  }, { &c::CMP, &c::ZP  }, { &c::DEC, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* CX */{ &c::INY, &c::UND }, { &c::CMP, &c::IMD }, { &c::DEX, &c::UND }, { &c::UND, &c::UND }, { &c::CPY, &c::ABS }, { &c::CMP, &c::ABS }, { &c::DEC, &c::ABS }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* DX */{ &c::BNE, &c::REL }, { &c::CMP, &c::IDY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::CMP, &c::ZPX }, { &c::DEC, &c::ZPX }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* DX */{ &c::UND, &c::UND }, { &c::CMP, &c::ABY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::CMP, &c::ABX }, { &c::DEC, &c::ABX }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* EX */{ &c::CPX, &c::IMD }, { &c::SBC, &c::IDX }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::CPX, &c::ZP  }, { &c::SBC, &c::ZP  }, { &c::INC, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* EX */{ &c::INX, &c::UND }, { &c::SBC, &c::IMD }, { &c::NOP, &c::UND }, { &c::UND, &c::UND }, { &c::CPX, &c::ABS }, { &c::SBC, &c::ABS }, { &c::INC, &c::ABS }, { &c::UND, &c::UND },
/*      X0                    X1                    X2                    X3                    X4                    X5                    X6                    X7                 */
/* FX */{ &c::BEQ, &c::REL }, { &c::SBC, &c::IDY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::SBC, &c::ZPX }, { &c::INC, &c::ZPX }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* FX */{ &c::UND, &c::UND }, { &c::SBC, &c::ABY }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::SBC, &c::ABX }, { &c::INC, &c::ABX }, { &c::UND, &c::UND },
};

std::string NesCpu::getOpCode(const Instruction & instruction, uint16_t data)
{
    std::string opcode = "???";
    GEN_OPCODE(ADC);
    GEN_OPCODE(SBC);
    GEN_OPCODE(CLC);
    GEN_OPCODE(BPL);
    GEN_OPCODE(BEQ);
    GEN_OPCODE(BNE);
    GEN_OPCODE(BMI);
    GEN_OPCODE(BCC);
    GEN_OPCODE(BCS);
    GEN_OPCODE(CMP);
    GEN_OPCODE(CPX);
    GEN_OPCODE(CPY);
    GEN_OPCODE(JSR);
    GEN_OPCODE(ASL);
    GEN_OPCODE(INY);
    GEN_OPCODE(LDA);
    GEN_OPCODE(LDX);
    GEN_OPCODE(LDY);
    GEN_OPCODE(STA);
    GEN_OPCODE(STX);
    GEN_OPCODE(STY);
    GEN_OPCODE(TAX);
    GEN_OPCODE(TAY);
    GEN_OPCODE(TXA);
    GEN_OPCODE(TYA);
    GEN_OPCODE(JMP);
    GEN_OPCODE(RTS);
    GEN_OPCODE(DEC);
    GEN_OPCODE(INC);
    GEN_OPCODE(DEX);
    GEN_OPCODE(DEY);
    GEN_OPCODE(INX);
    GEN_OPCODE(AND);
    GEN_OPCODE(ORA);
    GEN_OPCODE(EOR);
    GEN_OPCODE(NOP);
    GEN_OPCODE(LSR);
    GEN_OPCODE(ROR);
    GEN_OPCODE(ROL);
    GEN_OPCODE(PHA);
    GEN_OPCODE(PLA);
    GEN_OPCODE(SEC);
    GEN_OPCODE(BIT);

    GEN_ADDRMODE(UND, "");
    GEN_ADDRMODE(IMD, " #" + hexToString( static_cast<uint8_t>( data ) ) );
    GEN_ADDRMODE(ZP,  " $" + hexToString( static_cast<uint8_t>( data ) ) );
    GEN_ADDRMODE(ZPX, " $" + hexToString( static_cast<uint8_t>( data ) ) + std::string(", X"));
    GEN_ADDRMODE(ZPY, " $" + hexToString( static_cast<uint8_t>( data ) ) + std::string(", Y"));
    GEN_ADDRMODE(ABS, " $" + hexToString( data ));
    GEN_ADDRMODE(ABX, " $" + hexToString( static_cast<uint16_t>( data ) ) + std::string(", X"));
    GEN_ADDRMODE(ABY, " $" + hexToString( static_cast<uint16_t>( data ) ) + std::string(", Y"));
    GEN_ADDRMODE(REL, " $" + hexToString( static_cast<uint8_t>( data ) ));
    GEN_ADDRMODE(IND, " ($" + hexToString( data ) + std::string(")"));
    GEN_ADDRMODE(IDX, " ($" + hexToString( static_cast<uint8_t>(data) ) + std::string(", X)"));
    GEN_ADDRMODE(IDY, " ($" + hexToString( static_cast<uint8_t>(data) ) + std::string("), Y"));
    return opcode;
}

bool NesCpu::executeInstruction()
{
#ifdef DEBUG_NES_CPU
    uint16_t commandPc = m_cpu.pc;
#endif
    uint8_t opcode = fetch();
    if ( commands[ opcode ].opcode == &c::UND )
    {
        LOGE("Unknown instruction (0x%02X) detected at [0x%04X]\n", opcode, m_cpu.pc - 1);
        return false;
    }
    m_cpu.implied = false;
    (this->*commands[ opcode ].addrmode)();
#ifdef DEBUG_NES_CPU
    printCpuState( commands[ opcode ], commandPc );
#endif
    (this->*commands[ opcode ].opcode)();
    return true;
}

int NesCpu::callSubroutine(uint16_t addr, int maxInstructions)
{
    m_stopSp = m_cpu.sp;
    m_cpu.absAddr = addr;
    JSR();
    return continueSubroutine( maxInstructions );
}

int NesCpu::continueSubroutine(int maxInstructions)
{
    while ( m_stopSp != m_cpu.sp && executeInstruction( ) && maxInstructions )
    {
        if ( maxInstructions > 0 ) maxInstructions--;
    }
    // Exit if we returned from subroutine call
    if ( m_stopSp == m_cpu.sp )
    {
        return 1;
    }
    // If instruction limit is reached, inform that there are more commands to execute
    if ( maxInstructions == 0 )
    {
        return 0;
    }
    // Error occured, set pc to problem instruction
    m_cpu.pc--;
    return -1;
}

void NesCpu::BPL()
{
    if ( !(m_cpu.flags & N_FLAG) ) m_cpu.pc += m_cpu.relAddr;
}

void NesCpu::ADC()
{
    uint8_t data = read( m_cpu.absAddr );
    uint16_t temp = (uint16_t)m_cpu.a + (uint16_t)data + (uint16_t)(( m_cpu.flags & C_FLAG ) ? 1: 0);
    if ( temp > 255 ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp );
    if ((~((uint16_t)m_cpu.a ^ (uint16_t)data) & ((uint16_t)m_cpu.a ^ (uint16_t)data)) & 0x0080)
        m_cpu.flags |= V_FLAG;
    else
        m_cpu.flags &= ~V_FLAG;
    m_cpu.a = temp & 0xFF;
}

void NesCpu::SBC()
{
    uint16_t data = static_cast<uint16_t>(read( m_cpu.absAddr )) ^ 0x00FF;
    uint16_t temp = (uint16_t)m_cpu.a + (uint16_t)data + (uint16_t)(( m_cpu.flags & C_FLAG ) ? 1: 0);
    if ( temp > 255 ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp );
    if ((~((uint16_t)m_cpu.a ^ (uint16_t)data) & ((uint16_t)m_cpu.a ^ (uint16_t)data)) & 0x0080)
        m_cpu.flags |= V_FLAG;
    else
        m_cpu.flags &= ~V_FLAG;
    m_cpu.a = temp & 0xFF;
}

void NesCpu::TAX()
{
    m_cpu.x = m_cpu.a;
    modifyFlags( m_cpu.x );
}

void NesCpu::TAY()
{
    m_cpu.y = m_cpu.a;
    modifyFlags( m_cpu.y );
}

void NesCpu::TXA()
{
    m_cpu.a = m_cpu.x;
    modifyFlags( m_cpu.a );
}

void NesCpu::TYA()
{
    m_cpu.a = m_cpu.y;
    modifyFlags( m_cpu.a );
}

void NesCpu::INY()
{
    m_cpu.y++;
    modifyFlags( m_cpu.y );
}

void NesCpu::LDA()
{
    m_cpu.a = read( m_cpu.absAddr );
    modifyFlags( m_cpu.a );
}

void NesCpu::ASL()
{
    uint8_t data = m_cpu.implied ? m_cpu.a : read( m_cpu.absAddr );
    m_cpu.flags &= ~(C_FLAG | Z_FLAG | N_FLAG);
    m_cpu.flags |= (data & 0x80) ? C_FLAG : 0;
    m_cpu.flags |= (data & 0x40) ? N_FLAG : 0;
    m_cpu.flags |= (data & 0x7F) ? 0 : Z_FLAG;
    data <<= 1;
    if ( m_cpu.implied ) m_cpu.a = data; else write( m_cpu.absAddr, data );
}

void NesCpu::LSR()
{
    uint8_t data = m_cpu.implied ? m_cpu.a : read( m_cpu.absAddr );
    m_cpu.flags &= ~(C_FLAG | Z_FLAG | N_FLAG);
    m_cpu.flags |= (data & 0x01) ? C_FLAG : 0;
    m_cpu.flags |= (data == 1) ? Z_FLAG : 0;
    data >>= 1;
    if ( m_cpu.implied ) m_cpu.a = data; else write( m_cpu.absAddr, data );
}

void NesCpu::ROL()
{
    uint8_t data = m_cpu.implied ? m_cpu.a : read( m_cpu.absAddr );
    uint8_t cflag = (m_cpu.flags & C_FLAG) ? 0x01 : 0x00;
    m_cpu.flags &= ~(C_FLAG | Z_FLAG | N_FLAG);
    m_cpu.flags |= (data & 0x80) ? C_FLAG : 0;
    data <<= 1;
    data |= cflag;
    modifyFlags( data );
    if ( m_cpu.implied ) m_cpu.a = data; else write( m_cpu.absAddr, data );
}

void NesCpu::ROR()
{
    uint8_t data = m_cpu.implied ? m_cpu.a : read( m_cpu.absAddr );
    uint8_t cflag = (m_cpu.flags & C_FLAG) ? 0x80 : 0x00;
    m_cpu.flags &= ~(C_FLAG | Z_FLAG | N_FLAG);
    m_cpu.flags |= (data & 0x01) ? C_FLAG : 0;
    data >>= 1;
    data |= cflag;
    modifyFlags( data );
    if ( m_cpu.implied ) m_cpu.a = data; else write( m_cpu.absAddr, data );
}

void NesCpu::CLC()
{
    m_cpu.flags &= ~C_FLAG;
}

void NesCpu::PHA()
{
    write( m_cpu.sp-- + 0x100, m_cpu.a );
}

void NesCpu::PLA()
{
    m_cpu.a = read( ++m_cpu.sp + 0x100 );
}

void NesCpu::JMP()
{
    m_cpu.pc = m_cpu.absAddr;
}

void NesCpu::JSR()
{
    uint16_t addr = m_cpu.pc - 1;
    write( m_cpu.sp-- + 0x100, addr >> 8 );
    write( m_cpu.sp-- + 0x100, addr & 0x00FF );
    m_cpu.pc = m_cpu.absAddr;
}

void NesCpu::RTS()
{
    uint16_t addr = read( ++m_cpu.sp + 0x100 );
    addr |= static_cast<uint16_t>(read( ++m_cpu.sp + 0x100 )) << 8;
    addr++;
    m_cpu.pc = addr;
}

void NesCpu::STA()
{
    write( m_cpu.absAddr, m_cpu.a );
}

void NesCpu::STX()
{
    write( m_cpu.absAddr, m_cpu.x );
}

void NesCpu::STY()
{
    write( m_cpu.absAddr, m_cpu.y );
}

void NesCpu::LDY()
{
    m_cpu.y = read( m_cpu.absAddr );
    modifyFlags( m_cpu.y );
}

void NesCpu::LDX()
{
    m_cpu.x = read( m_cpu.absAddr );
    modifyFlags( m_cpu.x );
}

void NesCpu::BEQ()
{
    if ( m_cpu.flags & Z_FLAG )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesCpu::BNE()
{
    if ( !(m_cpu.flags & Z_FLAG) )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesCpu::BMI()
{
    if ( m_cpu.flags & N_FLAG )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesCpu::BCC()
{
    if ( !(m_cpu.flags & C_FLAG) )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesCpu::BCS()
{
    if ( m_cpu.flags & C_FLAG )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesCpu::CMP()
{
    uint8_t data = read( m_cpu.absAddr );
    uint16_t temp = (uint16_t)m_cpu.a - (uint16_t)data;
    if ( m_cpu.a >= data ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp & 0xFF );
}

void NesCpu::CPX()
{
    uint8_t data = read( m_cpu.absAddr );
    uint16_t temp = (uint16_t)m_cpu.x - (uint16_t)data;
    if ( m_cpu.x >= data ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp & 0xFF );
}

void NesCpu::CPY()
{
    uint8_t data = read( m_cpu.absAddr );
    uint16_t temp = (uint16_t)m_cpu.y - (uint16_t)data;
    if ( m_cpu.y >= data ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp & 0xFF );
}

void NesCpu::DEC()
{
    uint8_t data = read( m_cpu.absAddr ) - 1;
    write( m_cpu.absAddr, data );
    modifyFlags( data );
}

void NesCpu::INC()
{
    uint8_t data = read( m_cpu.absAddr ) + 1;
    write( m_cpu.absAddr, data );
    modifyFlags( data );
}

void NesCpu::BIT()
{
    uint8_t data = read( m_cpu.absAddr );
    uint8_t result = m_cpu.a & data;
    if ( result ) m_cpu.flags &= ~Z_FLAG; else m_cpu.flags |= Z_FLAG;
    if ( data & 0x40 ) m_cpu.flags |= V_FLAG; else m_cpu.flags &= ~V_FLAG;
    if ( data & 0x80 ) m_cpu.flags |= N_FLAG; else m_cpu.flags &= ~N_FLAG;
}

void NesCpu::DEX()
{
    m_cpu.x--;
    modifyFlags( m_cpu.x );
}

void NesCpu::DEY()
{
    m_cpu.y--;
    modifyFlags( m_cpu.y );
}

void NesCpu::INX()
{
    m_cpu.x++;
    modifyFlags( m_cpu.x );
}

void NesCpu::AND()
{
    m_cpu.a &= read( m_cpu.absAddr );
    modifyFlags( m_cpu.a );
}

void NesCpu::ORA()
{
    m_cpu.a |= read( m_cpu.absAddr );
    modifyFlags( m_cpu.a );
}

void NesCpu::EOR()
{
    m_cpu.a ^= read( m_cpu.absAddr );
    modifyFlags( m_cpu.a );
}

void NesCpu::NOP()
{
}

void NesCpu::SEC()
{
    m_cpu.flags |= C_FLAG;
}

void NesCpu::BRK()
{
    m_cpu.absAddr = read( 0xFFFE );
    m_cpu.absAddr |= static_cast<uint16_t>( read( 0xFFFF) ) << 8;
    JSR();
    write( m_cpu.sp-- + 0x100, m_cpu.flags );
    m_cpu.flags |= B_FLAG;
}
