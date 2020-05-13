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

#include <stdint.h>

#include "ay-3-8910.h"
#include "nes_apu.h"

typedef struct
{
    uint32_t ident; // "Vgm "
    uint32_t eofOffset;
    uint32_t version;
    uint32_t sn76489Clock;
    uint32_t ym2413Clock;
    uint32_t gd3Offset;
    uint32_t totalSamples;
    uint32_t loopOffset;
    uint32_t loopSamples;
    uint32_t rate;
    uint16_t sn76489Feedback;
    uint16_t sn76489ShiftReg;
    uint32_t ym2612Clock;
    uint32_t ym2151Clock;
    uint32_t vgmDataOffset;
    uint32_t segaPcmClock;
    uint32_t spcmInterface;
    uint32_t rf5c68Clock;
    uint32_t ym2203Clock;
    uint32_t ym2608Clock;
    uint32_t ym2610bClock;
    uint32_t ym3812Clock;
    uint32_t ym3526Clock;
    uint32_t y8950Clock;
    uint32_t ymf262Clock;
    uint32_t ymf278bClock;
    uint32_t ymf271Clock;
    uint32_t ymz280bClock;
    uint32_t rf5c164Clock;
    uint32_t pwmClock;
    uint32_t ay8910Clock;
    uint8_t  ay8910Type;
    uint8_t  ay8910Flags;
    uint8_t  ym2203ay8910Flags;
    uint8_t  ym2608ay8910Flags;
    uint8_t  volumeModifier;
    uint8_t  reserved1;
    uint8_t  loopBase;
    uint8_t  loopModifier;
    uint32_t gbDmgClock;
    uint32_t nesApuClock;
    uint32_t multiPcmClock;
    uint32_t upd7759Clock;
    uint32_t okim6258Clock;
    uint8_t okim6258Flags;
    uint8_t k054539Flags;
    uint8_t c140Flags;
    uint8_t reserved2;
    uint32_t oki6295Clock;
    uint32_t k051649Clock;
    uint32_t k054539Clock;
    uint32_t huc6280Clock;
    uint32_t c140Clock;
    uint32_t k053260Clock;
    uint32_t pokeyClock;
    uint32_t qsoundClock;
    uint32_t scspClock;
    uint32_t extraHeaderOffset;
    uint32_t wswanClock;
    uint32_t vsuClock;
    uint32_t saa1090Clock;
    uint32_t es5503Clock;
    uint32_t es5506Clock;
    uint8_t es5503Channels;
    uint8_t es5506Channels;
    uint8_t c352ClockDivider;
    uint8_t reserved3;
    uint32_t x1010Clock;
    uint32_t c352Clock;
    uint32_t ga20Clock;
    uint8_t reserved4[7 * 4];
} VgmHeader;

typedef struct
{
    uint32_t ident; // NESM
    uint8_t byte1A;
    uint8_t version;
    uint8_t songIndex; // 1, 2, ...
    uint16_t loadAddress;
    uint16_t initAddress;
    uint16_t playAddress;
    uint8_t name[32];
    uint8_t artist[32];
    uint8_t copyright[32];
    uint16_t ntscPlaySpeed; // 1/1000000 second ticks
    uint8_t bankSwitch[8];
    uint16_t palPlaySpeed; // 1/1000000 second ticks
    uint8_t palNtscBits;
    uint8_t extraSoundChip;
    uint8_t nsf2Reserved;
    uint8_t dataLength[3];
} NsfHeader;

class VgmFile
{
public:
    VgmFile();
    ~VgmFile();

    /** Allows to open NSF and VGM data blocks */
    bool open(const uint8_t *data, int size);

    /** Closes either VGM or NSF data */
    void close();

    /**
     * Decodes next block and fill pcm buffer.
     * If there is not more data to play returns size less than maxSize.
     * outBuffer is filled up with 16-bit unsigned PCM for 2 channels (stereo).
     */
    int decodePcm(uint8_t *outBuffer, int maxSize);

    /** Sets sampling frequency. ,Must be called before decodePcm */
    void setSampleFrequency( uint32_t frequency );

    /** Sets volume, default level is 64 */
    void setVolume(uint16_t volume);

    /** Returns number of tracks in opened file */
    int getTrackCount();

    /** Sets track to play */
    bool setTrack(int track);

    /**
     * Sets maximum decoding duration in milliseconds.
     * Useful for looped music
     */
    void setMaxDuration( uint32_t milliseconds );

    AY38910 *getMsxChip() { return m_msxChip; }
    NesApu *getNesChip() { return m_nesChip; }

private:
    const uint8_t * m_rawData = nullptr;
    int m_size = 0;
    int m_headerSize = 0;
    const uint8_t * m_dataPtr = nullptr;

    const VgmHeader *m_header = nullptr;
    const NsfHeader *m_nsfHeader = nullptr;

    /** Duration in samples */
    uint32_t m_duration = 0;

    uint32_t m_rate;
    uint32_t m_vgmDataOffset;
    uint32_t m_loopOffset;
    uint32_t m_samplesPlayed;
    uint32_t m_waitSamples;
    uint8_t m_loops;
    uint32_t m_readCounter;
    uint32_t m_writeCounter;
    uint32_t m_readScaler;
    uint32_t m_writeScaler;

    uint32_t m_sampleSum;
    bool m_sampleSumValid = false;

    uint8_t m_state = 0;

    AY38910 *m_msxChip = nullptr;
    NesApu *m_nesChip = nullptr;

    void interpolateSample();
    void deleteChips();

    bool openVgm(const uint8_t * vgmdata, int size);
    bool openNsf(const uint8_t * nsfdata, int size);

    int decodeNfsPcm(uint8_t *outBuffer, int maxSize);
    int decodeVgmPcm(uint8_t *outBuffer, int maxSize);

    bool nextCommand();
};
