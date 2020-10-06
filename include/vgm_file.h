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

    /**
     * Enables fade effect
     *
     * @param enable true to enable fade effect at the end
     */
    void setFading(bool enable);

private:
    BaseMusicDecoder * m_decoder = nullptr;

    /** Duration in samples */
    uint32_t m_duration = 0;

    uint32_t m_samplesPlayed;
    uint32_t m_waitSamples;

    uint32_t m_readCounter;
    uint32_t m_writeCounter;
    uint32_t m_readScaler;
    uint32_t m_writeScaler;

    uint32_t m_sampleSum;
    bool m_sampleSumValid = false;
    bool m_fadeEffect = false;
    uint16_t m_shifter = 0;
    uint16_t m_volume = 64;

    void interpolateSample();
    void deleteDecoder();
};
