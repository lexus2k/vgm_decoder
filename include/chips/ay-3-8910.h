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

#include <stdint.h>
#include <stdlib.h>

enum
{
    CHIP_TYPE_AY8910 = 0x00,
    CHIP_TYPE_AY8912 = 0x01,
    CHIP_TYPE_AY8913 = 0x02,
    CHIP_TYPE_AY8930 = 0x03,
    CHIP_TYPE_AY8914 = 0x04,
    CHIP_TYPE_YM2149 = 0x10,
    CHIP_TYPE_YM3439 = 0x11,
    CHIP_TYPE_YMZ284 = 0x12,
    CHIP_TYPE_YMZ294 = 0x13,
    CHIP_TYPE_YM2203 = 0x20,
    CHIP_TYPE_YM2608 = 0x21,
    CHIP_TYPE_YM2610 = 0x22,
    CHIP_TYPE_YM2610B = 0x23,
};

class AY38910
{
public:
    AY38910() = default;

    AY38910(uint8_t chipType, uint8_t flags);

    /** Sets chip configuration */
    void setType( uint8_t chipType, uint8_t flags );

    /** Resets ay-3-8910 state */
    void reset();

    /**
     * Return value of given PSG register.
     */
    uint16_t read(uint8_t reg);

    /**
     * Set value of given PSG register.
     */
    void write(uint8_t reg, uint16_t value);

    /**
     * Initialize PSG by setting up a source data line (audio output stream).
     * Call this before starting thread.
     */
    void init();

    /** Returns left (16 bits) and right (16 bits) channels if next sound sample */
    uint32_t getSample();

    /** Set chip clock external frequency */
    void setFrequency( uint32_t frequency );

    /**
     * Although AY-3-8910/YM2149 emulator allows to run at lower sample frequencies,
     * it is recommended to leave 44100 Hz as base emulator frequency for good quality.
     * Setting low sample frequencies like 11025 Hz greatly reduces quality of high
     * frequency tones. It is recommended to do downsampling with the output of AY-3-8910
     * emulator.
     */
    void setSampleFrequency( uint32_t sampleFrequency );

    /** Returns currently set sample frequency */
    uint32_t getSampleFrequency() const { return m_sampleFrequency; }

    /** Changes volume level. Default level is 100! */
    void setVolume(uint16_t volume);

    /** TODO: */
    //void setStereoMode(uint8_t mode);

private:
    /** Chip Type. */
    uint8_t m_chipType = 0;

    /** Chip flags */
    uint8_t m_flags = 0;

    /** Random generator */
    uint32_t m_rng = 1;

    /** AY-3-8910 Chip frequency in HZ */
    uint32_t m_frequency = 3579545;

    /** Audio hardware sample frequency */
    uint32_t m_sampleFrequency = 44100;

    /** How many tone ticks in one audio sample */
    uint32_t m_toneFrequencyScale = 0;

    uint32_t m_envFrequencyScale = 0;

    /** period value for sound channels: A, B, C. */
    uint32_t m_period[3]{};

    /** Period value for noise. */
    uint32_t m_periodNoise = 0;

    /** Mixer register. */
    uint8_t m_mixer = 0x00;

    /** Volume value for sound channel. */
    uint8_t m_amplitude[3]{};

    uint8_t m_ampR[3]{};

    /** Period value for envelope. */
    uint32_t m_periodE = 0;

    /** Envelope register is used only for storing passed value */
    uint8_t m_envelopeReg = 0;

    bool m_holding = true;

    bool m_hold = false;

    bool m_attack = false;

    bool m_continue = false;

    bool m_alternate = false;

    bool m_noiseRecalc = false;

    uint8_t m_envStepMask = 0x0F;

    /** Indicates whether a particular channel is in envelope or period mode. */
    bool m_useEnvelope[3]{};

    /** Channel A/B/C frequency counter */
    uint32_t m_counter[3]{};

    /** Turn channel A/B/C on/off */
    bool m_channelOutput[3]{};

    /** Noise frequency counter */
    uint32_t m_counterNoise = 0;

    /** Turn noise on/off */
    bool m_noiseHigh = false;

    /** Envelope frequency counter */
    uint32_t m_counterEnv = 0;

    /** Envelope tick counter */
    uint8_t m_envVolume = 0;

    /** volume level table for the chip */
    uint16_t m_levelTable[32];

    /** user volume level */
    uint16_t m_userVolume = 100;

    /** Recalculates volume tables */
    void calcVolumeTables();
};


