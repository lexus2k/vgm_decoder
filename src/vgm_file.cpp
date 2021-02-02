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

#include "vgm_file.h"
#include "formats/vgm_decoder.h"
#include "formats/nsf_decoder.h"

#define VGM_FILE_DEBUG 1

#if VGM_FILE_DEBUG && !defined(VGM_DECODER_LOGGER)
#define VGM_DECODER_LOGGER VGM_FILE_DEBUG
#endif
#include "vgm_logger.h"

/** Vgm file are always based on 44.1kHz rate */
#define VGM_SAMPLE_RATE 44100

VgmFile::VgmFile()
    : m_readScaler( VGM_SAMPLE_RATE )
    , m_writeScaler( VGM_SAMPLE_RATE )
{
    setMaxDuration( 3 * 60 * 1000 );
}

VgmFile::~VgmFile()
{
    deleteDecoder();
}

void VgmFile::deleteDecoder()
{
    if ( m_decoder )
    {
        delete m_decoder;
        m_decoder = nullptr;
    }
}

bool VgmFile::open(const uint8_t * data, int size)
{
    close();
    m_samplesPlayed = 0;
    m_waitSamples = 0;
    m_decoder = VgmMusicDecoder::tryOpen( data, size );
    if ( !m_decoder )
    {
        m_decoder = NsfMusicDecoder::tryOpen( data, size );
    }
    if ( m_decoder )
    {
        if ( m_volume != 100 ) m_decoder->setVolume( m_volume );
        return true;
    }
    return false;
}

void VgmFile::close()
{
    deleteDecoder();
}

void VgmFile::setVolume( uint16_t volume )
{
    m_volume = volume;
    if ( m_decoder ) m_decoder->setVolume( m_volume );
}

int VgmFile::getTrackCount()
{
    if ( m_decoder ) return m_decoder->getTrackCount();
    return 0;
}

bool VgmFile::setTrack(int track)
{
    if ( m_decoder ) return m_decoder->setTrack( track );
    return false;
}

void VgmFile::setMaxDuration( uint32_t milliseconds )
{
    m_duration = static_cast<uint64_t>(milliseconds) * VGM_SAMPLE_RATE / 1000;
}

typedef struct
{
    uint16_t left, right;
} StereoChannels;

void VgmFile::interpolateSample()
{
    uint32_t nextSample = m_decoder->getSample();
    StereoChannels &source = reinterpret_cast<StereoChannels&>(nextSample);
//    StereoChannels &dest = reinterpret_cast<StereoChannels&>(m_sampleSum);
    if ( m_shifter )
    {
        source.left = static_cast<uint32_t>(source.left) * m_shifter / 1024;
        source.right = static_cast<uint32_t>(source.right) * m_shifter / 1024;
    }

    if ( !m_sampleSumValid ) // If no sample previously reached the mixer assign new sample
    {
        m_sampleSum = nextSample;
        m_sampleSumValid = true;
    }
/*    if ( (source.left >= 8192 && source.left > dest.left) ||
         (source.left < 8192 && source.left < dest.left) )
    {
        dest.left = source.left;
    }
    if ( (source.right >= 8192 && source.right > dest.right) ||
         (source.right < 8192 && source.right < dest.right) )
    {
        dest.right = source.right;
    } */
}

int VgmFile::decodePcm(uint8_t *outBuffer, int maxSize)
{
    int decoded = 0;
    if ( !m_decoder )
    {
        return 0;
    }
    while ( decoded + 4 <= maxSize )
    {
        if ( !m_waitSamples )
        {
            m_shifter = 0;
            if ( m_duration )
            {
                if ( m_samplesPlayed >= m_duration )
                {
                    LOGI("m_samplesPlayed: %d\n", m_samplesPlayed);
                    break;
                }
                if ( m_fadeEffect && (m_duration - m_samplesPlayed < VGM_SAMPLE_RATE * 2) )
                {
                    m_shifter = (m_duration - m_samplesPlayed) >> 7;
                }
            }
            int result = m_decoder->decodeBlock();
            if ( result < 0 )
            {
                LOGE( "Failed to play melody, stopping\n" );
                break;
            }
            if ( result == 0 )
            {
                LOGI( "No more samples to play, stopping\n" );
                break;
            }
            m_waitSamples = result;
            LOGI( "Next block %d samples [%d.%03d - %d.%03d]\n", m_waitSamples,
                   m_samplesPlayed / VGM_SAMPLE_RATE, 1000 * (m_samplesPlayed % VGM_SAMPLE_RATE) / VGM_SAMPLE_RATE,
                   (m_samplesPlayed + m_waitSamples) / VGM_SAMPLE_RATE,
                   1000 * ((m_samplesPlayed + m_waitSamples) % VGM_SAMPLE_RATE) / VGM_SAMPLE_RATE );
        }
        while ( m_waitSamples && (decoded + 4 <= maxSize) )
        {
            interpolateSample();

            m_writeCounter += m_writeScaler;
            m_samplesPlayed++;
            m_waitSamples--;

            if ( m_writeCounter >= VGM_SAMPLE_RATE )
            {
                *(reinterpret_cast<uint32_t *>(outBuffer)) = m_sampleSum;
                outBuffer += 4;
                decoded += 4;
                m_writeCounter -= VGM_SAMPLE_RATE;
                m_sampleSumValid = false;
            }
        }
    }
    return decoded;
}

void VgmFile::setSampleFrequency( uint32_t frequency )
{
    m_writeScaler = frequency;
}

void VgmFile::setFading(bool enable)
{
    m_fadeEffect = enable;
}
