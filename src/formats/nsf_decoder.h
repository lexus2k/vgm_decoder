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
#include "music_decoder.h"
#include "chips/nes_cpu.h"

typedef struct NsfHeader NsfHeader;

class NsfMusicDecoder: public BaseMusicDecoder
{
public:
    NsfMusicDecoder();
    ~NsfMusicDecoder();

    /** Allows to open NSF data blocks */
    bool open(const uint8_t *data, int size) override;

    static NsfMusicDecoder *tryOpen(const uint8_t *data, int size);

    /** Closes NSF data */
    void close();

    uint32_t getSample() override;

    /** Sets sampling frequency. ,Must be called before decodePcm */
//    void setSampleFrequency( uint32_t frequency ) virtual;

    /** Sets volume, default level is 64 */
    void setVolume(uint16_t volume) override;

    /** Returns number of tracks in opened file */
    int getTrackCount() override;

    /** Sets track to play */
    bool setTrack(int track) override;

    /**
     * Decodes data block and returns number of samples to read from decoder.
     * If it returns -1, then error occured, 0 means - nothing left.
     */
    int decodeBlock() override;

private:
    NesCpu m_nesChip{};
    uint32_t m_waitSamples;

    const uint8_t * m_rawData = nullptr;
    int m_size = 0;
    int m_headerSize = 0;
    const uint8_t * m_dataPtr = nullptr;

    const NsfHeader *m_nsfHeader = nullptr;
};



