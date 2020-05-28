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

#include "nsf_decoder.h"
#include "formats/nsf_format.h"
#include "chips/nsf_cartridge.h"

#define NSF_DECODER_DEBUG 1

#if NSF_DECODER_DEBUG && !defined(VGM_DECODER_LOGGER)
#define VGM_DECODER_LOGGER NSF_DECODER_DEBUG
#endif
#include "../vgm_logger.h"

/** Vgm file are always based on 44.1kHz rate */
#define VGM_SAMPLE_RATE 44100

NsfMusicDecoder::NsfMusicDecoder(): BaseMusicDecoder()
{
}

NsfMusicDecoder::~NsfMusicDecoder()
{
    close();
}

NsfMusicDecoder *NsfMusicDecoder::tryOpen(const uint8_t *data, int size)
{
    NsfMusicDecoder *decoder = new NsfMusicDecoder();
    if ( !decoder->open( data, size ) )
    {
        delete decoder;
        decoder = nullptr;
    }
    return decoder;
}

bool NsfMusicDecoder::open(const uint8_t * data, int size)
{
    close();
    if ( size < sizeof(NsfHeader) )
    {
        return false;
    }
    m_rawData = data;
    m_size = size;
    m_nsfHeader = reinterpret_cast<const NsfHeader *>(m_rawData);
    m_dataPtr = m_rawData;
    if ( m_nsfHeader->ident != 0x4D53454E )
    {
        LOGE("%08X\n", m_nsfHeader->ident );
        m_nsfHeader = nullptr;
        return false;
    }
    NsfCartridge *cartridge = new NsfCartridge();
    cartridge->setDataBlock( m_nsfHeader->loadAddress, m_dataPtr + 0x80, m_size - 0x80 );
    m_nesChip.insertCartridge( cartridge );
    if ( !setTrack( 0 ) )
    {
        return false;
    }
    LOG( "Init complete \n" );
    LOG( "Nsf NTSC rate: %d us\n", m_nsfHeader->ntscPlaySpeed );
    m_waitSamples = 0;
    return true;
}

void NsfMusicDecoder::close()
{
    m_nesChip.insertCartridge( nullptr );
    m_nsfHeader = nullptr;
    m_rawData = nullptr;
}

void NsfMusicDecoder::setVolume( uint16_t volume )
{
    m_nesChip.getApu()->setVolume( volume );
}

int NsfMusicDecoder::getTrackCount()
{
    // read nsf track count
    if ( m_nsfHeader ) return m_nsfHeader->songIndex;
    return 0;
}

bool NsfMusicDecoder::setTrack(int track)
{
    // For vgm files this is not supported
    if ( !m_nsfHeader ) return false;

    m_nesChip.reset();
    bool useBanks = false;
    for (int i=0; i<8; i++)
        if ( m_nsfHeader->bankSwitch[i] )
            useBanks = true;
    if ( useBanks )
    {
        for (int i=0; i<8; i++)
            m_nesChip.write( 0x5FF8 + i, m_nsfHeader->bankSwitch[i] );
    }
    // Reset NES CPU memory and state
    NesCpuState &cpu = m_nesChip.cpuState();
    for (uint16_t i = 0; i < 0x07FF; i++) m_nesChip.write(i, 0);
    for (uint16_t i = 0x4000; i < 0x4013; i++) m_nesChip.write(i, 0);
    m_nesChip.write(0x4015, 0x00);
    m_nesChip.write(0x4015, 0x0F);
    m_nesChip.write(0x4017, 0x40);
    // if the tune is bank switched, load the bank values from $070-$077 into $5FF8-$5FFF.
    cpu.x = 0; // ntsc
    cpu.a = track < m_nsfHeader->songIndex ? track: 0;
    cpu.sp = 0xEF;
    if ( m_nesChip.callSubroutine( m_nsfHeader->initAddress ) < 0 )
    {
        LOGE( "Failed to call init subroutine for NSF file\n" );
        return false;
    }
//    m_samplesPlayed = 0;
    return true;
}

uint32_t NsfMusicDecoder::getSample()
{
    return m_nesChip.getApu()->getSample();
}

int NsfMusicDecoder::decodeBlock()
{
    int result = m_nesChip.callSubroutine( m_nsfHeader->playAddress, 20000 );
    if ( result < 0 )
    {
        LOGE( "Failed to call play subroutine due to CPU error, stopping\n" );
        return -1;
    }
    if ( result == 0 )
    {
        LOGE( "Failed to call play subroutine, it looks infinite loop, stopping\n" );
        return 0;
    }
    m_waitSamples = (VGM_SAMPLE_RATE * static_cast<uint64_t>( m_nsfHeader->ntscPlaySpeed )) / 1000000;
    return m_waitSamples;
}
