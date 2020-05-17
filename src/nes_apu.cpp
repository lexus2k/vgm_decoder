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

#include "nes_apu.h"

#include <malloc.h>
#include <stdio.h>

#define NES_APU_DEBUG 1
//#define DEBUG_NES_CPU

#if NES_APU_DEBUG && !defined(VGM_DECODER_LOGGER)
#define VGM_DECODER_LOGGER NES_APU_DEBUG
#endif
#include "vgm_logger.h"

#define NES_CPU_FREQUENCY (1789773)
#define SAMPLING_RATE  (44100)
#define CONST_SHIFT_BITS (4)

static constexpr uint32_t counterScaler = ((NES_CPU_FREQUENCY<<CONST_SHIFT_BITS) / SAMPLING_RATE);
static constexpr uint32_t frameCounterPeriod = ((NES_CPU_FREQUENCY<<CONST_SHIFT_BITS) / 240 );

/*
static constexpr uint8_t lengthLut[] =
{
    5, 127, 10, 1, 20, 2, 40, 3,
    80, 4, 30, 5, 7, 6, 13, 7,
    6, 8, 12, 9, 24, 10, 48, 11,
    96, 12, 36, 13, 8, 14, 16, 15,
};
*/

static constexpr uint8_t lengthLut[] =
{
    10, 254, 20, 2, 40, 4, 80, 6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30,
};

static constexpr uint16_t noiseLut[] =
{
    0x002, 0x004, 0x008, 0x010,
    0x020, 0x030, 0x040, 0x050,
    0x065, 0x07F, 0x0BE, 0x0FE,
    0x17D, 0x1FC, 0x3F9, 0x7F2,
};

// NTSC
static constexpr uint16_t dmcLut[] =
{
    0x1AC, 0x17C, 0x154, 0x140,
    0x11E, 0x0FE, 0x0E2, 0x0D6,
    0x0BE, 0x0A0, 0x08E, 0x080,
    0x06A, 0x054, 0x048, 0x036,
};

static constexpr uint16_t nesApuLevelTable[16] =
{
0,	1092,	2184,	3276,	4369,	5461,	6553,	7645,	8738,	9830,	10922,	12014,	13107,	14199,	15291,	16384,
};

enum
{
    APU_RECT_VOL1  = 0x00,  // 4000
    APU_SWEEP1     = 0x01,  // 4001
    APU_RECT_FREQ1 = 0x02,  // 4002
    APU_RECT_LEN1  = 0x03,  // 4003

    APU_RECT_VOL2  = 0x04,  // 4004
    APU_SWEEP2     = 0x05,  // 4005
    APU_RECT_FREQ2 = 0x06,  // 4006
    APU_RECT_LEN2  = 0x07,  // 4007

    APU_TRIANGLE   = 0x08,  // 4008
    // NOT USED 0x09
    APU_TRI_FREQ   = 0x0A,  // 400A
    APU_TRI_LEN    = 0x0B,  // 400B

    APU_NOISE_VOL  = 0x0C,  // 400C
    // NOT USED 0x0D
    APU_NOISE_FREQ = 0x0E,  // 400E
    APU_NOISE_LEN  = 0x0F,  // 400F

    APU_DMC_DMA_FREQ = 0x10,// 4010
    APU_DMC_DELTA_COUNTER = 0x11, // 4011
    APU_DMC_ADDR   = 0x12,  // 4012
    APU_DMC_LEN    = 0x13,  // 4013
    APU_STATUS     = 0x15,  // 4014

    APU_LOW_TIMER  = 0x17,  // 4017
};

#define VALUE_VOL_MASK  (0x0F)
#define FIXED_VOL_MASK  (0x10)
#define DISABLE_LEN_MASK (0x20)
#define ENABLE_LOOP_MASK (0x20) // Yes the same as DISABLE_LEN
#define DUTY_CYCLE_MASK (0xC0)

#define SWEEP_ENABLE_MASK (0x80)
#define SWEEP_SHIFT_MASK (0x07)
#define SWEEP_DIR_MASK (0x08)
#define SWEEP_RATE_MASK (0x70)

#define PAL_MODE_MASK (0x80)

#define CHAN1_ENABLE_MASK (0x01)
#define CHAN2_ENABLE_MASK (0x02)
#define TRI_ENABLE_MASK (0x04)

#define NOISE_FREQ_MASK (0x0F)
#define NOISE_MODE_MASK (0x80)

#define DMC_ENABLE_MASK (0x10)
#define DMC_RATE_MASK (0x0F)
#define DMC_LOOP_MASK (0x40)
#define DMC_IRQ_ENABLE_MASK (0x80)

static constexpr uint32_t noise_freq_table[] =
{
    0x002, 0x004, 0x008, 0x010, 0x020, 0x030, 0x040, 0x050, 0x065, 0x07F, 0x0BE, 0x0FE, 0x17D, 0x1FC, 0x3F9, 0x7F2
};

                   // aa dd : NES APU, write value dd to register aa
                   // Note: Registers 00-1F equal NES address 4000-401F,
                   //       registers 20-3E equal NES address 4080-409E,
                   //       register 3F equals NES address 4023,
                   //       registers 40-7F equal NES address 4040-407F.

inline uint8_t getRegIndex(uint16_t reg)
{
    if ( reg < 0x4000 ) return reg;
    if ( reg <= 0x407F )
    {
        if ( reg == 4023 ) return 0x3F;
        return reg - 0x4000;
    }
    return reg - 0x4060;
}

inline uint16_t getRegAddress(uint8_t reg)
{
    if ( reg < 128 ) reg += 0x4000;
    return reg;
}

NesApu::NesApu()
{
    m_volume = 64;
    reset();
}

NesApu::~NesApu()
{
    if ( m_ram )
    {
        free( m_ram );
        m_ram = nullptr;
    }
}

void NesApu::reset()
{
    m_shiftNoise = 0x0001;
    m_lastFrameCounter = 0;
    m_mapper031BaseAddress = 0xFFFF;
    for (int i=0; i<APU_MAX_MEMORY_BLOCKS; i++)
    {
        m_mem[i].data = nullptr;
    }
    setVolume( m_volume );
    for (int i=0; i<8; i++)
        m_bank[i] = i;
}

void NesApu::write(uint16_t reg, uint8_t val)
{
    uint16_t originReg = reg;
    uint8_t oldVal = val;
    LOGI( "Write 0x%02X to [%04X] reg\n", val, getRegAddress( reg ) );
    reg = getRegIndex(reg);
    if ( reg < APU_MAX_REG )
    {
        oldVal = m_regs[reg];
        m_regs[reg] = val;
    }
    int chanIndex = (reg - APU_RECT_VOL1) / 4;
    switch (reg)
    {
        case APU_RECT_VOL1:
        case APU_RECT_VOL2:
        case APU_NOISE_VOL:
//            m_chan[chanIndex].divider = val & VALUE_VOL_MASK;
            break;
        case APU_TRIANGLE:
            break;
        case APU_SWEEP1:
        case APU_SWEEP2:
            break;
        case APU_RECT_FREQ1:
        case APU_RECT_FREQ2:
        case APU_TRI_FREQ:
            m_chan[chanIndex].period = (m_chan[chanIndex].period & (0xFF00 << (CONST_SHIFT_BITS + 4))) |
                                       (val << (CONST_SHIFT_BITS + 4));
            if ( m_chan[chanIndex].counter > m_chan[chanIndex].period ) m_chan[chanIndex].counter = m_chan[chanIndex].period;
            break;
        case APU_NOISE_FREQ:
        {
            // reset noise generator when switching modes
            if ( ( oldVal & NOISE_MODE_MASK ) != ( val & NOISE_MODE_MASK ) )
            {
                m_shiftNoise = 0x0001;
            }
            m_chan[chanIndex].period = (noiseLut[val & NOISE_FREQ_MASK]) << (CONST_SHIFT_BITS + 4);
            if ( m_chan[chanIndex].counter > m_chan[chanIndex].period ) m_chan[chanIndex].counter = m_chan[chanIndex].period;
            break;
        }

        case APU_RECT_LEN1:
        case APU_RECT_LEN2:
            // We must reset duty cycle sequencer only for channels 1, 2 according to datasheet
            m_chan[chanIndex].sequencer = 0;
            // Reset counter also to prevent unexpected clicks
            m_chan[chanIndex].counter = 0;
            m_chan[chanIndex].updateEnvelope = true;
            // Unused on noise channel
            m_chan[chanIndex].period = (m_chan[chanIndex].period & (0x000000FF << (CONST_SHIFT_BITS + 4))) |
                                       (static_cast<uint32_t>(val & 0x07) << (8 + CONST_SHIFT_BITS + 4));
            m_chan[chanIndex].lenCounter = lengthLut[val >> 3];
            m_chan[chanIndex].counter = 0;
            break;

        case APU_TRI_LEN:
            // Do not reset triangle sequencer to prevent clicks. This is required per datasheet
            // Unused on noise channel
            m_chan[2].period = (m_chan[2].period & (0x000000FF << (CONST_SHIFT_BITS + 4))) |
                                (static_cast<uint32_t>(val & 0x07) << (8 + CONST_SHIFT_BITS + 4));
            m_chan[2].lenCounter = lengthLut[val >> 3];
            m_chan[2].linearReloadFlag = true;
            m_chan[2].counter = 0;
            break;

        case APU_NOISE_LEN:
            m_chan[3].updateEnvelope = true;
            m_chan[3].lenCounter = lengthLut[val >> 3];
            m_chan[3].counter = 0;
            break;

        case APU_DMC_DMA_FREQ:
            m_chan[4].period = dmcLut[ val & DMC_RATE_MASK ] << CONST_SHIFT_BITS;
            if (m_chan[4].counter >= m_chan[4].period) m_chan[4].counter = m_chan[4].period;
            break;
        case APU_DMC_DELTA_COUNTER:
            m_chan[4].volume = val & 0x7F;
            break;
        case APU_DMC_ADDR:
            break;
        case APU_DMC_LEN:
            break;
        case APU_STATUS:
            for(int i=0; i<4; i++)
            {
                if (!(val & (1<<i)))
                {
                    m_chan[i].counter = 0;
                    m_chan[i].lenCounter = 0;
                }
            }
            if ((m_regs[APU_STATUS] & 0x10) && !m_chan[4].dmcActive )
            {
                m_chan[4].dmcActive = true;
                m_chan[4].dmcAddr = m_regs[APU_DMC_ADDR] * 0x40 + 0xC000;
                m_chan[4].dmcLen = m_regs[APU_DMC_LEN] * 16 + 1;
                m_chan[4].dmcIrqFlag = false;
            }
            else if (!(m_regs[APU_STATUS] & 0x10))
            {
                m_chan[4].dmcActive = false;
            }

            break;
        case APU_LOW_TIMER:
            m_lastFrameCounter = 0;
            m_apuFrames = 0;
            break;
        default:
            // Bank switching registers
            if ( originReg >= 0x5000 && originReg <= 0x5FFF )
            {
                m_bank[originReg & 0x07] = val;
                LOGI( "BANK %d [%04X] = %02X (%d) 0x%08X\n", originReg & 0x07, originReg,
                      val, 0x8000 + val * 4096, 0x8000 + val * 4096 );
            }
            // Check for sweep support on channels TRI and NOISE
            else if ( reg != 0x09 && reg != 0x0D )
            {
                LOGE( "Unknown reg 0x%02X [0x%04X]\n", reg, originReg );
            }
            break;
    }
}

void NesApu::setVolume(uint16_t volume)
{
    for(int i=0; i<16; i++)
    {
        uint32_t vol = static_cast<uint32_t>(nesApuLevelTable[i]) * volume / 64;
        if ( vol > 65535 ) vol = 65535;
        m_volTable[i] = vol;
    }
}

uint8_t NesApu::read(uint16_t reg)
{
    return 0;
}

void NesApu::setDataBlock( const uint8_t *data, uint32_t len )
{
    if ( len < 2 )
    {
        LOGE("Invalid data - too short\n");
        return;
    }
    uint16_t address = data[0] + (data[1] << 8);
    setDataBlock( address, data + 2, len - 2 );
}

void NesApu::setDataBlock( uint32_t addr, const uint8_t *data, uint32_t len )
{
    if ( len < 2 )
    {
        LOGE("Invalid data - too short\n");
        return;
    }
    int blockNumber = -1;
    for(int i=0; i<APU_MAX_MEMORY_BLOCKS; i++)
    {
        if ( m_mem[i].data == nullptr )
        {
            blockNumber = i;
            break;
        }
    }
    if ( blockNumber < 0 )
    {
        LOGE("Out of memory blocks\n");
        return;
    }
    if ( addr < m_mapper031BaseAddress )
        m_mapper031BaseAddress = addr & 0xF000;
    m_mem[ blockNumber ].data = data;
    m_mem[ blockNumber ].size = len;
    m_mem[ blockNumber ].addr = addr;
    LOGI("New data block [0x%04X] (len=%d)\n", addr, len);
}

uint32_t NesApu::getSample()
{
    updateFrameCounter();

    updateRectChannel(0);
    updateRectChannel(1);
    updateTriangleChannel(m_chan[2]);
    updateNoiseChannel(m_chan[3]);
    updateDmcChannel(m_chan[4]);

    uint32_t sample = 0;
    sample += (m_chan[0].output * 0x0B0) >> 8; // chan 1
    sample += (m_chan[1].output * 0x0B0) >> 8; // chan 2
    sample += (m_chan[2].output * 0x050) >> 8; // tri
    sample += (m_chan[3].output * 0x050) >> 8; // noise
    sample += (m_chan[4].output * 0x180) >> 8; // dmc

    if ( sample > 65535 ) sample = 65535;
    return sample | (sample << 16);
}

//--------------
//Square Channel
//--------------
//
//                   +---------+    +---------+
//                   |  Sweep  |--->|Timer / 2|
//                   +---------+    +---------+
//                        |              |
//                        |              v
//                        |         +---------+    +---------+
//                        |         |Sequencer|    | Length  |
//                        |         +---------+    +---------+
//                        |              |              |
//                        v              v              v
//    +---------+        |\             |\             |\          +---------+
//    |Envelope |------->| >----------->| >----------->| >-------->|   DAC   |
//    +---------+        |/             |/             |/          +---------+

void NesApu::updateRectChannel(int i)
{
    ChannelInfo &chan = m_chan[i];
    static constexpr uint8_t sequencerTable[] =
    {
        0b01000000,
        0b01100000,
        0b01111000,
        0b10011111,
    };

    if (!(m_regs[APU_STATUS] & (1<<i)))
    {
        chan.volume = 0;
        chan.output = m_volTable[ chan.volume ];
        return;
    }

    uint8_t volumeReg = m_regs[APU_RECT_VOL1 + i*4];

    // Decay counters are always enabled
    // So, run the envelope anyway
    if ( m_quaterSignal )
    {
        if ( chan.updateEnvelope )
        {
            chan.decayCounter = 0x0F;
            chan.divider = volumeReg & VALUE_VOL_MASK;
            chan.updateEnvelope = false;
        }
        else if (chan.divider)
        {
            chan.divider--;
        }
        else
        {
            chan.divider = volumeReg & VALUE_VOL_MASK;
            if (chan.decayCounter)
            {
                chan.decayCounter--;
            }
            else if (volumeReg & ENABLE_LOOP_MASK)
            {
                chan.decayCounter = 0x0F;
            }
        }
    }
    if ( volumeReg & FIXED_VOL_MASK ) // fixed volume
    {
        chan.volume = volumeReg & VALUE_VOL_MASK;
    }
    else
    {
        chan.volume = chan.decayCounter;
    }
    // Countdown len counter if enabled
    if ( !(volumeReg & DISABLE_LEN_MASK) )
    {
        if (chan.lenCounter && m_halfSignal) // once per frame
        {
            chan.lenCounter--;
        }
    }
    // Sequencer
    // Sweep works only if lengthCounter is non-zero
    if (!chan.lenCounter)
    {
        chan.volume = 0;
        chan.output = m_volTable[ chan.volume ];
        return;
    }
    // Sweep works
    uint8_t sweepReg = m_regs[APU_SWEEP1 + i*4];
    if ( (sweepReg & SWEEP_ENABLE_MASK) && (sweepReg & SWEEP_RATE_MASK) &&
          chan.period >= (8 << (CONST_SHIFT_BITS + 4)) &&
          chan.period <= (0x7FF << (CONST_SHIFT_BITS + 4)) )
    {
        if ( m_halfSignal )
        {
            if ( chan.sweepCounter == (sweepReg & SWEEP_RATE_MASK) )
            {
                chan.sweepCounter = 0;
                auto delta = chan.period >> (sweepReg & SWEEP_SHIFT_MASK);
                if ( sweepReg & SWEEP_DIR_MASK )
                {
                    delta = ~delta;
                    if ( i == 1 ) delta += 1 << (CONST_SHIFT_BITS + 4);
                }
                delta &= (0xFFFF << (CONST_SHIFT_BITS + 4));
            }
            else
            {
                chan.sweepCounter += 0x10;
            }
        }
    }

    if ( chan.period < (8 << (CONST_SHIFT_BITS + 4)) ||
         chan.period > (0x7FF << (CONST_SHIFT_BITS + 4)) )
    {
        chan.volume = 0;
        chan.output = m_volTable[ chan.volume ];
        return;
    }

    chan.counter += counterScaler << 3;
    while ( chan.counter >= chan.period + (1 <<  (CONST_SHIFT_BITS + 4)) )
    {
        chan.sequencer++;
        chan.sequencer &= 0x07;
        chan.counter -= (chan.period + (1 <<  (CONST_SHIFT_BITS + 4)));
    }
    if ( !(sequencerTable[ (volumeReg &  DUTY_CYCLE_MASK) >> 6 ] & (1<<chan.sequencer)) )
    {
        chan.volume = 0;
    }
    chan.output = m_volTable[ chan.volume ];
}


//----------------
//Triangle Channel
//----------------
//
//                   +---------+    +---------+
//                   |LinearCtr|    | Length  |
//                   +---------+    +---------+
//                        |              |
//                        v              v
//    +---------+        |\             |\         +---------+    +---------+
//    |  Timer  |------->| >----------->| >------->|Sequencer|--->|   DAC   |
//    +---------+        |/             |/         +---------+    +---------+

void NesApu::updateTriangleChannel(ChannelInfo &chan)
{
    static constexpr uint8_t triangleTable[] =
    {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
        15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
    };
    bool disabled = !(m_regs[APU_STATUS] & TRI_ENABLE_MASK);

    if (disabled)
    {
        chan.output = m_volTable[ chan.volume ];
        return;
    }
    // Linear counter control
    uint8_t triangleReg = m_regs[APU_TRIANGLE];
    if ( m_quaterSignal )
    {
        if ( chan.linearReloadFlag )
            chan.linearCounter = triangleReg & 0x7F;
        else if ( chan.linearCounter)
            chan.linearCounter--;
        if (!(triangleReg & 0x80))
        {
            chan.linearReloadFlag = false;
        }
    }
    // Length counter control
    if (m_halfSignal && !(triangleReg & 0x80) && chan.lenCounter) // once per frame
    {
        chan.lenCounter--;
    }

    if ((!chan.lenCounter || !chan.linearCounter))
    {
        chan.output = m_volTable[ chan.volume ];
        return;
    }

    chan.counter += counterScaler << 4;
    while ( chan.counter >= ( chan.period + (1 <<  (CONST_SHIFT_BITS + 4))) )
    {
        chan.sequencer++;
        chan.sequencer &= 0x1F;
        chan.counter -= ( chan.period + (1 <<  (CONST_SHIFT_BITS + 4)) );
        chan.volume = triangleTable[ chan.sequencer ];
    }
    chan.output = m_volTable[ chan.volume ];
}

//-------------
//Noise Channel
//-------------
//
//    +---------+    +---------+    +---------+
//    |  Timer  |--->| Random  |    | Length  |
//    +---------+    +---------+    +---------+
//                        |              |
//                        v              v
//    +---------+        |\             |\         +---------+
//    |Envelope |------->| >----------->| >------->|   DAC   |
//    +---------+        |/             |/         +---------+

void NesApu::updateNoiseChannel(ChannelInfo &chan)
{
    if (!(m_regs[APU_STATUS] & (1<<3)))
    {
        chan.volume = 0;
        chan.output = m_volTable[ chan.volume ];
        return;
    }

    uint8_t volumeReg = m_regs[APU_NOISE_VOL];

    // Decay counters are always enabled
    // So, run the envelope anyway
    if ( m_quaterSignal )
    {
        if ( chan.updateEnvelope )
        {
            chan.decayCounter = 0x0F;
            chan.divider = volumeReg & VALUE_VOL_MASK;
            chan.updateEnvelope = false;
        }
        else if (chan.divider)
        {
            chan.divider--;
        }
        else
        {
            chan.divider = volumeReg & VALUE_VOL_MASK;
            if (chan.decayCounter)
            {
                chan.decayCounter--;
            }
            else if (volumeReg & ENABLE_LOOP_MASK)
            {
                chan.decayCounter = 0x0F;
            }
        }
    }
    if ( volumeReg & FIXED_VOL_MASK ) // fixed volume
    {
        chan.volume = volumeReg & VALUE_VOL_MASK;
    }
    else
    {
        chan.volume = chan.decayCounter;
    }

    // Countdown len counter if enabled
    if ( !(volumeReg & DISABLE_LEN_MASK) )
    {
        if (chan.lenCounter && m_halfSignal) // once per frame
        {
            chan.lenCounter--;
        }
    }

    if (!chan.lenCounter)
    {
        chan.volume = 0;
        chan.output = m_volTable[ chan.volume ];
        return;
    }

    chan.counter += counterScaler << 3;
    while ( chan.counter >= chan.period + (1 <<  (CONST_SHIFT_BITS + 4)) )
    {
        uint8_t temp;
        if ( m_regs[APU_NOISE_FREQ] & NOISE_MODE_MASK )
        {
            // 93-bits
            temp = ((m_shiftNoise >> 6)^m_shiftNoise) & 1;
        }
        else
        {
            // 32768-bits
            temp = ((m_shiftNoise >> 1)^m_shiftNoise) & 1;
        }
        m_shiftNoise >>= 1;
        m_shiftNoise |= (temp << 14);
        chan.counter -= (chan.period + (1 <<  (CONST_SHIFT_BITS + 4)));
    }
    if ( m_shiftNoise & 0x01 )
    {
        chan.volume = 0;
    }
    chan.output = m_volTable[ chan.volume ];
}

//------------------------------
//Delta Modulation Channel (DMC)
//------------------------------
//
//    +----------+    +---------+
//    |DMA Reader|    |  Timer  |
//    +----------+    +---------+
//         |               |
//         |               v
//    +----------+    +---------+     +---------+     +---------+
//    |  Buffer  |----| Output  |---->| Counter |---->|   DAC   |
//    +----------+    +---------+     +---------+     +---------+

void NesApu::updateDmcChannel(ChannelInfo &info)
{
    if ( info.dmcActive && !info.sequencer )
    {
        if ( info.dmcLen == 0 )
        {
            if ( m_regs[APU_DMC_DMA_FREQ] & DMC_LOOP_MASK )
            {
                info.dmcAddr = m_regs[APU_DMC_ADDR] * 0x40 + 0xC000;
                info.dmcLen = m_regs[APU_DMC_LEN] * 16 + 1;
            }
            else
            {
                info.dmcIrqFlag = !!(m_regs[APU_DMC_DMA_FREQ] & DMC_IRQ_ENABLE_MASK);
                info.dmcActive = false;
                info.output = (static_cast<uint32_t>(m_volTable[15]) * info.volume) >> 7;
                return;
            }
        }
        info.dmcBuffer = readMem( info.dmcAddr );
        info.sequencer = 8;
        info.dmcAddr++;
        info.dmcLen--;
        if ( info.dmcAddr == 0x0000 ) info.dmcAddr = 0x8000;
    }

    if ( info.sequencer )
    {
        info.counter += counterScaler;
        while ( info.counter >= info.period )
        {
            if ( info.dmcBuffer & 1 )
            {
                if ( info.volume <= 125 ) info.volume += 2;
            }
            else
            {
                if ( info.volume >= 2 ) info.volume -= 2;
            }
            info.sequencer--;
            info.dmcBuffer >>= 1;
            info.counter -= info.period;
        }
    }
    info.output = (static_cast<uint32_t>(m_volTable[15]) * info.volume) >> 7;
    return;
}

void NesApu::updateFrameCounter()
{
    const uint8_t upperThreshold = (m_regs[APU_LOW_TIMER] & PAL_MODE_MASK) ? 5: 4;
    m_quaterSignal = false;
    m_halfSignal = false;
    m_fullSignal = false;
    m_lastFrameCounter += counterScaler;
    if ( m_lastFrameCounter >= frameCounterPeriod )
    {
        m_lastFrameCounter -= frameCounterPeriod;
        m_apuFrames++;
        if ( m_apuFrames != 4 || upperThreshold != 5 )
        {
            m_quaterSignal = true;
        }
        m_halfSignal = (m_apuFrames == 2) || (m_apuFrames >= upperThreshold);
        if ( m_apuFrames >= upperThreshold )
        {
            m_fullSignal = true;
            m_apuFrames = 0;
        }
    }
}

uint32_t NesApu::mapper031(uint16_t address)
{
    if ( address < 0x8000 )
    {
        return address;
    }
    if ( address >= 0xFFFA )
    {
        return address;
    }
    return m_mapper031BaseAddress + ((static_cast<uint32_t>(m_bank[(address >> 12) & 0x07]) << 12) |
                        (address & 0x0FFF));
}

uint8_t NesApu::readMem(uint16_t address)
{
    uint32_t mappedAddr = mapper031( address );
    if ( address < 0x2000 )
    {
        if ( m_ram == nullptr ) m_ram = static_cast<uint8_t *>(malloc(2048));
        LOGM("[%04X] ==> %02X\n", address, m_ram[address & 0x07FF]);
        return m_ram[address & 0x07FF];
    }
    else if ( address >= 0x2000 && address < 0x6000 )
    {
        return read( address );
    }
    // Cartridge
    else if ( address >= 0x6000 && address < 0x8000 )
    {
        LOGE("Memory data fetch error (Battery backed RAM) 0x%04X\n", address);
        return 0;
    }
    for (int i=0; i<APU_MAX_MEMORY_BLOCKS; i++)
    {
        if ( m_mem[i].data == nullptr )
        {
            LOGE("Memory data fetch error 0x%04X (mapped to 0x%08X)\n", address, mappedAddr);
            return 0;
        }
        if ( mappedAddr >= m_mem[i].addr &&
             mappedAddr < m_mem[i].addr + m_mem[i].size )
        {
            uint32_t addr = mappedAddr - m_mem[i].addr;
            LOGM("[%04X] ==> %02X\n", address, m_mem[i].data[ addr ]);
            return  m_mem[i].data[ addr ];
        }
    }
    return 0xFF;
}

uint8_t NesApu::readMemInternal(uint16_t address)
{
    uint32_t mappedAddr = mapper031( address );
    if ( address < 0x0800 )
    {
        if ( m_ram == nullptr ) m_ram = static_cast<uint8_t *>(malloc(2048));
        return m_ram[address];
    }
    else if ( address < 0x6000 )
    {
        return read( address );
    }
    // Cartridge
    else if ( address >= 0x6000 && address < 0x8000 )
    {
        LOGE("Memory data fetch error 0x%04X\n", address);
        return 0;
    }
    for (int i=0; i<APU_MAX_MEMORY_BLOCKS; i++)
    {
        if ( m_mem[i].data == nullptr )
        {
            LOGE("Memory data fetch error 0x%04X (mapped to 0x%08X)\n", address, mappedAddr);
            return 0;
        }
        if ( mappedAddr >= m_mem[i].addr &&
             mappedAddr < m_mem[i].addr + m_mem[i].size )
        {
            uint16_t addr = mappedAddr - m_mem[i].addr;
            return  m_mem[i].data[ addr ];
        }
    }
    return 0xFF;
}

bool NesApu::writeMem(uint16_t address, uint8_t data)
{
    if ( address < 0x2000 )
    {
        if ( m_ram == nullptr ) m_ram = static_cast<uint8_t *>(malloc(2048));
        m_ram[address & 0x07FF] = data;
        LOGM("[%04X] <== %02X\n", address, data);
        return true;
    }
    else if ( address < 0x6000 )
    {
        write( address, data );
        return true;
    }
    else if ( address < 0x8000 )
    {
        LOGE( "Memory data write error (Battery backed RAM) 0x%04X\n", address );
        return false;
    }
    LOGE("Memory data write error (ROM) 0x%04X\n", address);
    return false;
}

NesCpuState &NesApu::cpuState()
{
    return m_cpu;
}

void NesApu::IMD()
{
    m_cpu.absAddr = m_cpu.pc;
    m_cpu.pc++;
}

void NesApu::ZP()
{
    m_cpu.absAddr = fetch();
}

void NesApu::ZPX()
{
    m_cpu.absAddr = (fetch() + m_cpu.x) & 0x00FF;
}

void NesApu::ZPY()
{
    m_cpu.absAddr = (fetch() + m_cpu.y) & 0x00FF;
}

void NesApu::REL()
{
    m_cpu.relAddr = fetch();
    if ( m_cpu.relAddr & 0x80)
        m_cpu.relAddr |= 0xFF00;
}

void NesApu::ABS()
{
    m_cpu.absAddr = fetch();
    m_cpu.absAddr |= static_cast<uint16_t>(fetch()) << 8;
}

void NesApu::ABX()
{
    m_cpu.absAddr = static_cast<uint16_t>(fetch());
    m_cpu.absAddr |= (static_cast<uint16_t>(fetch()) << 8);
    m_cpu.absAddr += m_cpu.x;
}

void NesApu::ABY()
{
    m_cpu.absAddr = fetch();
    m_cpu.absAddr |= (static_cast<uint16_t>(fetch()) << 8);
    m_cpu.absAddr += m_cpu.y;
}

void NesApu::IND()
{
    m_cpu.absAddr = fetch();
    m_cpu.absAddr |= (static_cast<uint16_t>(fetch()) << 8);
    // no page boundary hardware bug
    m_cpu.absAddr = (readMem(m_cpu.absAddr)) | (readMem( m_cpu.absAddr + 1 ) << 8);
}

void NesApu::IDX()
{
    m_cpu.absAddr = (fetch() + m_cpu.x) & 0x00FF;
    m_cpu.absAddr = (readMem(m_cpu.absAddr)) | (readMem( (m_cpu.absAddr + 1) & 0xFF ) << 8);
}

void NesApu::IDY()
{
    m_cpu.absAddr = fetch();
    m_cpu.absAddr = static_cast<uint16_t>(readMem( m_cpu.absAddr )) |
                    (static_cast<uint16_t>(readMem( (m_cpu.absAddr + 1) & 0xFF )) << 8);
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

void NesApu::modifyFlags(uint8_t baseData)
{
    m_cpu.flags &= ~(Z_FLAG | N_FLAG);
    m_cpu.flags |= baseData ? 0: Z_FLAG;
    m_cpu.flags |= (baseData & 0x80) ? N_FLAG : 0;
}

void NesApu::printCpuState(const Instruction & instruction, uint16_t pc)
{
    LOGI("SP:%02X A:%02X X:%02X Y:%02X F:%02X [%04X] (0x%02X) %s\n",
         m_cpu.sp, m_cpu.a, m_cpu.x, m_cpu.y, m_cpu.flags, pc, readMemInternal( pc ),
         getOpCode( instruction, readMemInternal(pc + 1) | (static_cast<uint16_t>(readMemInternal(pc + 2)) << 8)).c_str() );
}

#define GEN_OPCODE(x) if ( instruction.opcode == &NesApu::x ) opcode = # x
#define GEN_ADDRMODE(x, y) if ( instruction.addrmode == &NesApu::x ) opcode += y

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

using c = NesApu;
const NesApu::Instruction NesApu::commands[256] =
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
/* 2X */{ &c::JSR, &c::ABS }, { &c::AND, &c::IDX }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::AND, &c::ZP  }, { &c::ROL, &c::ZP  }, { &c::UND, &c::UND },
/*      X8                    X9                    XA                    XB                    XC                    XD                    XE                    XF                 */
/* 2X */{ &c::UND, &c::UND }, { &c::AND, &c::IMD }, { &c::ROL, &c::UND }, { &c::UND, &c::UND }, { &c::UND, &c::UND }, { &c::AND, &c::ABS }, { &c::ROL, &c::ABS }, { &c::UND, &c::UND },
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

std::string NesApu::getOpCode(const Instruction & instruction, uint16_t data)
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

bool NesApu::executeInstruction()
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

int NesApu::callSubroutine(uint16_t addr, int maxInstructions)
{
    m_stopSp = m_cpu.sp;
    m_cpu.absAddr = addr;
    JSR();
    return continueSubroutine( maxInstructions );
}

int NesApu::continueSubroutine(int maxInstructions)
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

void NesApu::BPL()
{
    if ( !(m_cpu.flags & N_FLAG) ) m_cpu.pc += m_cpu.relAddr;
}

void NesApu::ADC()
{
    uint8_t data = readMem( m_cpu.absAddr );
    uint16_t temp = (uint16_t)m_cpu.a + (uint16_t)data + (uint16_t)(( m_cpu.flags & C_FLAG ) ? 1: 0);
    if ( temp > 255 ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp );
    if ((~((uint16_t)m_cpu.a ^ (uint16_t)data) & ((uint16_t)m_cpu.a ^ (uint16_t)data)) & 0x0080)
        m_cpu.flags |= V_FLAG;
    else
        m_cpu.flags &= ~V_FLAG;
    m_cpu.a = temp & 0xFF;
}

void NesApu::SBC()
{
    uint16_t data = static_cast<uint16_t>(readMem( m_cpu.absAddr )) ^ 0x00FF;
    uint16_t temp = (uint16_t)m_cpu.a + (uint16_t)data + (uint16_t)(( m_cpu.flags & C_FLAG ) ? 1: 0);
    if ( temp > 255 ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp );
    if ((~((uint16_t)m_cpu.a ^ (uint16_t)data) & ((uint16_t)m_cpu.a ^ (uint16_t)data)) & 0x0080)
        m_cpu.flags |= V_FLAG;
    else
        m_cpu.flags &= ~V_FLAG;
    m_cpu.a = temp & 0xFF;
}

void NesApu::TAX()
{
    m_cpu.x = m_cpu.a;
    modifyFlags( m_cpu.x );
}

void NesApu::TAY()
{
    m_cpu.y = m_cpu.a;
    modifyFlags( m_cpu.y );
}

void NesApu::TXA()
{
    m_cpu.a = m_cpu.x;
    modifyFlags( m_cpu.a );
}

void NesApu::TYA()
{
    m_cpu.a = m_cpu.y;
    modifyFlags( m_cpu.a );
}

void NesApu::INY()
{
    m_cpu.y++;
    modifyFlags( m_cpu.y );
}

void NesApu::LDA()
{
    m_cpu.a = readMem( m_cpu.absAddr );
    modifyFlags( m_cpu.a );
}

void NesApu::ASL()
{
    uint8_t data = m_cpu.implied ? m_cpu.a : readMem( m_cpu.absAddr );
    m_cpu.flags &= ~(C_FLAG | Z_FLAG | N_FLAG);
    m_cpu.flags |= (data & 0x80) ? C_FLAG : 0;
    m_cpu.flags |= (data & 0x40) ? N_FLAG : 0;
    m_cpu.flags |= (data & 0x7F) ? 0 : Z_FLAG;
    data <<= 1;
    if ( m_cpu.implied ) m_cpu.a = data; else writeMem( m_cpu.absAddr, data );
}

void NesApu::LSR()
{
    uint8_t data = m_cpu.implied ? m_cpu.a : readMem( m_cpu.absAddr );
    m_cpu.flags &= ~(C_FLAG | Z_FLAG | N_FLAG);
    m_cpu.flags |= (data & 0x01) ? C_FLAG : 0;
    m_cpu.flags |= (data == 1) ? Z_FLAG : 0;
    data >>= 1;
    if ( m_cpu.implied ) m_cpu.a = data; else writeMem( m_cpu.absAddr, data );
}

void NesApu::ROL()
{
    uint8_t data = m_cpu.implied ? m_cpu.a : readMem( m_cpu.absAddr );
    uint8_t cflag = (m_cpu.flags & C_FLAG) ? 0x01 : 0x00;
    m_cpu.flags &= ~(C_FLAG | Z_FLAG | N_FLAG);
    m_cpu.flags |= (data & 0x80) ? C_FLAG : 0;
    data <<= 1;
    data |= cflag;
    modifyFlags( data );
    if ( m_cpu.implied ) m_cpu.a = data; else writeMem( m_cpu.absAddr, data );
}

void NesApu::ROR()
{
    uint8_t data = m_cpu.implied ? m_cpu.a : readMem( m_cpu.absAddr );
    uint8_t cflag = (m_cpu.flags & C_FLAG) ? 0x80 : 0x00;
    m_cpu.flags &= ~(C_FLAG | Z_FLAG | N_FLAG);
    m_cpu.flags |= (data & 0x01) ? C_FLAG : 0;
    data >>= 1;
    data |= cflag;
    modifyFlags( data );
    if ( m_cpu.implied ) m_cpu.a = data; else writeMem( m_cpu.absAddr, data );
}

void NesApu::CLC()
{
    m_cpu.flags &= ~C_FLAG;
}

void NesApu::PHA()
{
    writeMem( m_cpu.sp-- + 0x100, m_cpu.a );
}

void NesApu::PLA()
{
    m_cpu.a = readMem( ++m_cpu.sp + 0x100 );
}

void NesApu::JMP()
{
    m_cpu.pc = m_cpu.absAddr;
}

void NesApu::JSR()
{
    uint16_t addr = m_cpu.pc - 1;
    writeMem( m_cpu.sp-- + 0x100, addr >> 8 );
    writeMem( m_cpu.sp-- + 0x100, addr & 0x00FF );
    m_cpu.pc = m_cpu.absAddr;
}

void NesApu::RTS()
{
    uint16_t addr = readMem( ++m_cpu.sp + 0x100 );
    addr |= static_cast<uint16_t>(readMem( ++m_cpu.sp + 0x100 )) << 8;
    addr++;
    m_cpu.pc = addr;
}

void NesApu::STA()
{
    writeMem( m_cpu.absAddr, m_cpu.a );
}

void NesApu::STX()
{
    writeMem( m_cpu.absAddr, m_cpu.x );
}

void NesApu::STY()
{
    writeMem( m_cpu.absAddr, m_cpu.y );
}

void NesApu::LDY()
{
    m_cpu.y = readMem( m_cpu.absAddr );
    modifyFlags( m_cpu.y );
}

void NesApu::LDX()
{
    m_cpu.x = readMem( m_cpu.absAddr );
    modifyFlags( m_cpu.x );
}

void NesApu::BEQ()
{
    if ( m_cpu.flags & Z_FLAG )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesApu::BNE()
{
    if ( !(m_cpu.flags & Z_FLAG) )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesApu::BMI()
{
    if ( m_cpu.flags & N_FLAG )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesApu::BCC()
{
    if ( !(m_cpu.flags & C_FLAG) )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesApu::BCS()
{
    if ( m_cpu.flags & C_FLAG )
    {
        m_cpu.pc += m_cpu.relAddr;
    }
}

void NesApu::CMP()
{
    uint8_t data = readMem( m_cpu.absAddr );
    uint16_t temp = (uint16_t)m_cpu.a - (uint16_t)data;
    if ( m_cpu.a >= data ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp & 0xFF );
}

void NesApu::CPX()
{
    uint8_t data = readMem( m_cpu.absAddr );
    uint16_t temp = (uint16_t)m_cpu.x - (uint16_t)data;
    if ( m_cpu.x >= data ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp & 0xFF );
}

void NesApu::CPY()
{
    uint8_t data = readMem( m_cpu.absAddr );
    uint16_t temp = (uint16_t)m_cpu.y - (uint16_t)data;
    if ( m_cpu.y >= data ) m_cpu.flags |= C_FLAG; else m_cpu.flags &= ~C_FLAG;
    modifyFlags( temp & 0xFF );
}

void NesApu::DEC()
{
    uint8_t data = readMem( m_cpu.absAddr ) - 1;
    writeMem( m_cpu.absAddr, data );
    modifyFlags( data );
}

void NesApu::INC()
{
    uint8_t data = readMem( m_cpu.absAddr ) + 1;
    writeMem( m_cpu.absAddr, data );
    modifyFlags( data );
}

void NesApu::DEX()
{
    m_cpu.x--;
    modifyFlags( m_cpu.x );
}

void NesApu::DEY()
{
    m_cpu.y--;
    modifyFlags( m_cpu.y );
}

void NesApu::INX()
{
    m_cpu.x++;
    modifyFlags( m_cpu.x );
}

void NesApu::AND()
{
    m_cpu.a &= readMem( m_cpu.absAddr );
    modifyFlags( m_cpu.a );
}

void NesApu::ORA()
{
    m_cpu.a |= readMem( m_cpu.absAddr );
    modifyFlags( m_cpu.a );
}

void NesApu::EOR()
{
    m_cpu.a ^= readMem( m_cpu.absAddr );
    modifyFlags( m_cpu.a );
}

void NesApu::NOP()
{
}

void NesApu::SEC()
{
    m_cpu.flags |= C_FLAG;
}

void NesApu::BRK()
{
    m_cpu.absAddr = readMem( 0xFFFE );
    m_cpu.absAddr |= static_cast<uint16_t>( readMem( 0xFFFF) ) << 8;
    JSR();
    writeMem( m_cpu.sp-- + 0x100, m_cpu.flags );
    m_cpu.flags |= B_FLAG;
}
