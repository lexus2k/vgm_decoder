/*
MIT License

Copyright (c) 2020-2021 Aleksei Dynda

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

#include <stdint.h>
#include <string>

#define APU_MAX_REG   (0x20)

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

class NesCpu;

class NesApu
{
public:
    NesApu(NesCpu * cpu);

    ~NesApu();

    /** Writes data to specified registetr */
    void write(uint16_t reg, uint8_t val);

    /** Reads data from sepcified register */
    uint8_t read(uint16_t reg);

    /**
     * Returns next sample at 44100 frequency rate. Each call to getSample()
     * simulates ~ 40.5 nes cpu ticks (1789773 Hz / 44100 Hz).
     */
    uint32_t getSample();

    /** Sets volume, default volume is 100 */
    void setVolume(uint16_t volume);

    /** Resets nes apu state */
    void reset();

    /** Power nes apu */
    void power();

private:
    NesCpu *m_cpu = nullptr;

    uint8_t m_regs[APU_MAX_REG]{};
    uint32_t m_rectVolTable[16]{};
    uint32_t m_triVolTable[16]{};
    uint32_t m_noiseVolTable[16]{};
    uint32_t m_dmcVolTable[16]{};

    uint32_t m_lastFrameCounter = 0;
    uint8_t m_apuFrames = 0;
    uint16_t m_shiftNoise;
    bool m_quaterSignal = false;
    bool m_halfSignal = false;
    bool m_fullSignal = false;
    uint16_t m_volume = 100;
    ChannelInfo m_chan[5]{};

    // APU Processing
    void updateRectChannel(int i);
    void updateTriangleChannel(ChannelInfo &info);
    void updateNoiseChannel(ChannelInfo &chan);
    void updateDmcChannel(ChannelInfo &info);
    void updateFrameCounter();
};
