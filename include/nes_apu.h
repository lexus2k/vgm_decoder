#pragma once

#include <stdint.h>

#define APU_MAX_REG   (0x30)
#define APU_MAX_MEMORY_BLOCKS (0x10)

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
    const uint8_t *data;
    uint16_t addr;
    uint16_t size;

} NesMemoryBlock;


class NesApu
{
public:
    NesApu();

    ~NesApu();

    void write(uint16_t reg, uint8_t val);

    uint8_t read(uint16_t reg, uint8_t val);

    uint32_t getSample();

    /** Sets volume, default volume is 64 */
    void setVolume(uint16_t volume);

    void reset();

    void setDataBlock( const uint8_t *data, uint32_t len );

private:
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

    void updateRectChannel(int i);
    void updateTriangleChannel(ChannelInfo &info);
    void updateNoiseChannel(int i);
    void updateDmcChannel(ChannelInfo &info);
    void updateFrameCounter();
};
