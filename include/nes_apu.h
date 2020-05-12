#pragma once

#include <stdint.h>
#include <string>

#define APU_MAX_REG   (0x30)
#define APU_MAX_MEMORY_BLOCKS (4)

typedef struct
{
    uint16_t lenCounter;
    uint16_t linearCounter;
    bool linearReloadFlag;

    uint32_t period;
    uint32_t counter;

    uint8_t decayCounter;
    uint8_t divider;
    bool updateEnvelope;
    uint8_t envVolume;

    uint8_t sequencer;

    uint8_t volume;
    uint32_t output;

    uint8_t sweepCounter;

    bool dmcActive;
    uint32_t dmcAddr;
    uint32_t dmcLen;
    uint8_t dmcBuffer;
    bool    dmcIrqFlag;
} ChannelInfo;

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

typedef struct
{
    const uint8_t *data;
    uint32_t addr;
    uint32_t size;
} NesMemoryBlock;


class NesApu
{
public:
    NesApu();

    ~NesApu();

    void write(uint16_t reg, uint8_t val);

    uint8_t read(uint16_t reg);

    uint32_t getSample();

    /** Sets volume, default volume is 64 */
    void setVolume(uint16_t volume);

    void reset();

    void setDataBlock( const uint8_t *data, uint32_t len );
    void setDataBlock( uint32_t addr, const uint8_t *data, uint32_t len );

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

    uint8_t getData(uint16_t address);
    bool setData(uint16_t address, uint8_t data);

    NesCpuState &cpuState();

private:

    struct Instruction
    {
        void (NesApu::*opcode)  (void) = nullptr;
        void (NesApu::*addrmode)(void) = nullptr;
    };


    uint8_t m_regs[APU_MAX_REG]{};
    NesMemoryBlock m_mem[APU_MAX_MEMORY_BLOCKS]{};
    uint64_t m_volTable[16]{};

    uint32_t m_lastFrameCounter = 0;
    uint8_t m_apuFrames = 0;
    uint16_t m_shiftNoise;
    bool m_quaterSignal = false;
    bool m_halfSignal = false;
    bool m_fullSignal = false;
    uint16_t m_volume = 64;
    ChannelInfo m_chan[5]{};

    NesCpuState m_cpu{};
    uint8_t m_stopSp;
    uint8_t *m_ram = nullptr;
    uint8_t m_bank[8];

    // APU Processing
    void updateRectChannel(int i);
    void updateTriangleChannel(ChannelInfo &info);
    void updateNoiseChannel(ChannelInfo &chan);
    void updateDmcChannel(ChannelInfo &info);
    void updateFrameCounter();

    // RAM/ROM Access
    uint8_t getDataInt(uint16_t address);
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

    // opcodes
    std::string getOpCode(const Instruction & instruction, uint16_t data);
    void UND() { m_cpu.implied = true; };
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

    // CPU Core
    void modifyFlags(uint8_t data);
    uint8_t fetch() { return getData( m_cpu.pc++ ); }
    void printCpuState( const Instruction & instruction, uint16_t pc );
};
