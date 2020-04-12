#include "vgm_file.h"

#define VGM_FILE_DEBUG 1

#if VGM_FILE_DEBUG
#include <stdio.h>
#define LOG(...) fprintf( stderr, __VA_ARGS__ )
#else
#define LOG(...)
#endif

/** Vgm file are always based on 44.1kHz rate */
#define VGM_SAMPLE_RATE 44100

VgmFile::VgmFile()
    : m_writeScaler( VGM_SAMPLE_RATE )
    , m_readScaler( VGM_SAMPLE_RATE )
{
}

VgmFile::~VgmFile()
{
    if ( m_chip )
    {
        delete m_chip;
        m_chip = nullptr;
    }
}

bool VgmFile::open(const uint8_t * vgmdata, int size)
{
    if ( size < sizeof(VgmHeader) )
    {
        return false;
    }
    m_rawData = vgmdata;
    m_size = size;
    m_header = reinterpret_cast<const VgmHeader *>(m_rawData);
    m_dataPtr = m_rawData;
    if ( m_header->ident != 0x206D6756 )
    {
        return false;
    }
    if ( m_header->eofOffset != size - 4 )
    {
        return false;
    }
    LOG( "Version: %X.%X \n", m_header->version >> 8, m_header->version & 0xFF );
    m_rate = m_header->rate ?: 50;
    m_headerSize = 64;
    if ( m_header->version >= 0x00000161 )
    {
        m_headerSize = 128;
    }
    m_vgmDataOffset = 0x40;
    if ( m_header->version >= 0x00000150 && m_header->vgmDataOffset )
    {
        m_vgmDataOffset = m_header->vgmDataOffset + 0x34;
    }

    m_dataPtr += m_vgmDataOffset;
    m_samplesPlayed = 0;
    m_waitSamples = 0;
    if ( m_header->loopOffset )
    {
        m_loopOffset = 0x1C + m_header->loopOffset;
        m_loops = 2;
    }
    else
    {
        m_loopOffset = 0;
        m_loops = 1;
    }

    m_writeCounter = 0;
    m_sampleSum = 0;

    LOG( "Rate: %d\n", m_rate );
    LOG( "ay8910 frequency: %dHz\n", m_header->ay8910Clock );
    LOG( "chip type: 0x%02X\n", m_header->ay8910Type );
    LOG( "chip flags: 0x%02X\n", m_header->ay8910Flags );
    LOG( "total samples: %d\n", m_header->totalSamples );
    LOG( "vgm data offset: 0x%08X\n", m_vgmDataOffset );
    LOG( "loop offset: 0x%08X\n", m_loopOffset );
    LOG( "loop samples: %d\n", m_header->loopSamples );
    LOG( "loop modifier: %d\n", m_header->loopModifier );
    LOG( "loop base: %d\n", m_header->loopBase ); // NumLoops = NumLoopsModified - LoopBase
    m_chip = new AY38910( m_header->ay8910Type, m_header->ay8910Flags );
    m_chip->setFrequency( m_header->ay8910Clock );
    return true;
}


bool VgmFile::nextCommand()
{
    uint8_t cmd = *m_dataPtr;
    LOG( "[0x%08X] command: 0x%02X", static_cast<uint32_t>(m_dataPtr - m_rawData), cmd);
    switch ( cmd )
    {
        case 0x31: /* dd    : Set AY8910 stereo mask
               Bit 0-1: Channel A mask (00=off, 01=left, 10=right, 11=center)
               Bit 2-3: Channel B mask (00=off, 01=left, 10=right, 11=center)
               Bit 4-5: Channel C mask (00=off, 01=left, 10=right, 11=center)
               Bit 6: Chip type, 0=AY8910, 1=YM2203 SSG part
               Bit 7: Chip number, 0 or 1 */
            m_dataPtr += 2;
            break;
        case 0x4F: // dd    : Game Gear PSG stereo, write dd to port 0x06
            m_dataPtr += 2;
            break;
        case 0x50: // dd    : PSG (SN76489/SN76496) write value dd
            m_dataPtr += 2;
            break;
        case 0x51: // aa dd : YM2413, write value dd to register aa
        case 0x52: // aa dd : YM2612 port 0, write value dd to register aa
        case 0x53: // aa dd : YM2612 port 1, write value dd to register aa
        case 0x54: // aa dd : YM2151, write value dd to register aa
        case 0x55: // aa dd : YM2203, write value dd to register aa
        case 0x56: // aa dd : YM2608 port 0, write value dd to register aa
        case 0x57: // aa dd : YM2608 port 1, write value dd to register aa
        case 0x58: // aa dd : YM2610 port 0, write value dd to register aa
        case 0x59: // aa dd : YM2610 port 1, write value dd to register aa
        case 0x5A: // aa dd : YM3812, write value dd to register aa
        case 0x5B: // aa dd : YM3526, write value dd to register aa
        case 0x5C: // aa dd : Y8950, write value dd to register aa
        case 0x5D: // aa dd : YMZ280B, write value dd to register aa
        case 0x5E: // aa dd : YMF262 port 0, write value dd to register aa
        case 0x5F: // aa dd : YMF262 port 1, write value dd to register aa
            m_dataPtr += 3;
            break;
        case 0x61: // nn nn : Wait n samples, n can range from 0 to 65535 (approx 1.49
                   // seconds). Longer pauses than this are represented by multiple
                   // wait commands.
            m_waitSamples = ( m_dataPtr[1] | (m_dataPtr[2] << 8) ) + 1;
            LOG( " [wait %d samples]", m_waitSamples);
            m_dataPtr += 3;
            break;
        case 0x62: //       : wait 735 samples (60th of a second), a shortcut for 0x61 0xdf 0x02
            m_waitSamples = 735;
            LOG( " [wait 735 samples]");
            m_dataPtr += 1;
            break;
        case 0x63: //       : wait 882 samples (50th of a second), a shortcut for 0x61 0x72 0x03
            m_waitSamples = 882;
            m_dataPtr += 1;
            break;
        case 0x66: //       : end of sound data
            if ( m_loopOffset && m_loops != 1  )
            {
                m_dataPtr = m_rawData + m_loopOffset;
                if ( m_loops ) m_loops--;
            }
            else
            {
                LOG( " [stop]\n" );
                return false;
            }
        case 0x67: // ...   : data block: see below
            break;
        case 0x68: // ...   : PCM RAM write: see below
            break;
        case 0xA0: // aa dd : AY8910, write value dd to register aa
            LOG( " [write ay8910 reg [%d] = 0x%02X ]", m_dataPtr[1], m_dataPtr[2]);
            m_chip->write( m_dataPtr[1], m_dataPtr[2] );
            m_dataPtr += 3;
            break;
        case 0xB0: // aa dd : RF5C68, write value dd to register aa
        case 0xB1: // aa dd : RF5C164, write value dd to register aa
        case 0xB2: // ad dd : PWM, write value ddd to register a (d is MSB, dd is LSB)
        case 0xB3: // aa dd : GameBoy DMG, write value dd to register aa
                   // Note: Register 00 equals GameBoy address FF10.
        case 0xB4: // aa dd : NES APU, write value dd to register aa
                   // Note: Registers 00-1F equal NES address 4000-401F,
                   //       registers 20-3E equal NES address 4080-409E,
                   //       register 3F equals NES address 4023,
                   //       registers 40-7F equal NES address 4040-407F.
        case 0xB5: // aa dd : MultiPCM, write value dd to register aa
        case 0xB6: // aa dd : uPD7759, write value dd to register aa
        case 0xB7: // aa dd : OKIM6258, write value dd to register aa
        case 0xB8: // aa dd : OKIM6295, write value dd to register aa
        case 0xB9: // aa dd : HuC6280, write value dd to register aa
        case 0xBA: // aa dd : K053260, write value dd to register aa
        case 0xBB: // aa dd : Pokey, write value dd to register aa
        case 0xBC: // aa dd : WonderSwan, write value dd to register aa
        case 0xBD: // aa dd : SAA1099, write value dd to register aa
        case 0xBE: // aa dd : ES5506, write 8-bit value dd to register aa
        case 0xBF: // aa dd : GA20, write value dd to register aa
            m_dataPtr += 3;
            break;
        case 0x30: // dd    : Used for dual chip support: see below
        case 0x3F: // dd    : Used for dual chip support: see below
            m_dataPtr += 2;
            break;
        case 0xC0: // bbaa dd : Sega PCM, write value dd to memory offset aabb
        case 0xC1: // bbaa dd : RF5C68, write value dd to memory offset aabb
        case 0xC2: // bbaa dd : RF5C164, write value dd to memory offset aabb
        case 0xC3: // cc bbaa : MultiPCM, write set bank offset aabb to channel cc
        case 0xC4: // mmll rr : QSound, write value mmll to register rr
                   // (mm - data MSB, ll - data LSB)
        case 0xC5: // mmll dd : SCSP, write value dd to memory offset mmll
                   // (mm - offset MSB, ll - offset LSB)
        case 0xC6: // mmll dd : WonderSwan, write value dd to memory offset mmll
                   // (mm - offset MSB, ll - offset LSB)
        case 0xC7: // mmll dd : VSU, write value dd to register mmll
                   // (mm - MSB, ll - LSB)
        case 0xC8: // mmll dd : X1-010, write value dd to memory offset mmll
                   // (mm - offset MSB, ll - offset LSB)
        case 0xD0: // pp aa dd : YMF278B port pp, write value dd to register aa
        case 0xD1: // pp aa dd : YMF271 port pp, write value dd to register aa
        case 0xD2: // pp aa dd : SCC1 port pp, write value dd to register aa
        case 0xD3: // pp aa dd : K054539 write value dd to register ppaa
        case 0xD4: // pp aa dd : C140 write value dd to register ppaa
        case 0xD5: // pp aa dd : ES5503 write value dd to register ppaa
        case 0xD6: // aa ddee  : ES5506 write 16-bit value ddee to register aa
            m_dataPtr += 4;
            break;
        case 0xE0: // dddddddd : seek to offset dddddddd (Intel byte order) in PCM data bank
        case 0xE1: // aabb ddee: C352 write 16-bit value ddee to register aabb
            m_dataPtr += 5;
            break;
        default:
            if ( cmd >= 0x70 && cmd <= 0x7F )
            {
                LOG( " [wait %d samples]", (cmd & 0x0F) + 1);
                m_waitSamples = (cmd & 0x0F) + 1;
                //       : wait n+1 samples, n can range from 0 to 15.
                m_dataPtr += 1;
                break;
            }
            else if ( cmd >= 0x80 && cmd <= 0x8F )
            {
                //       : YM2612 port 0 address 2A write from the data bank, then wait 
                //       n samples; n can range from 0 to 15. Note that the wait is n,
                //       NOT n+1. (Note: Written to first chip instance only.)
                m_dataPtr += 1;
                break;
            }
            else if ( cmd >= 0x90 && cmd <= 0x95 )
            {
                //  : DAC Stream Control Write: see below
                break;
            }
            else if ( cmd >= 0x32 && cmd <= 0x3E )
            {
                // dd          : one operand, reserved for future use
                m_dataPtr += 2;
                break;
            }
            else if ( cmd >= 0x40 && cmd <= 0x4E )
            {
                // dd dd       : two operands, reserved for future use Note: was one operand only til v1.60
                m_dataPtr += 3;
                break;
            }
            else if ( cmd >= 0xA1 && cmd <= 0xAF)
            {
                // aa dd : Used for dual chip support: see below
                m_dataPtr += 3;
                break;
            }
            else if ( ( cmd >= 0xC9 && cmd <= 0xCF ) || ( cmd >= 0xD7 && cmd <= 0xDF ) )
            {
                // dd dd dd    : three operands, reserved for future use
                m_dataPtr += 4;
                break;
            }
            else if ( cmd >= 0xE2 && cmd <= 0xFF )
            {
                // dd dd dd dd : four operands, reserved for future use
                m_dataPtr += 5;
                break;
            }
            LOG( "Unknown command (0x%02X) is detected at position 0x%08X \n",
                     *m_dataPtr, static_cast<uint32_t>(m_dataPtr - m_rawData) );
            return false;
    }
    LOG("\n");
    return true;
}

void VgmFile::setVolume( uint8_t volume )
{
    if ( m_chip ) m_chip->setVolume( volume );
}

void VgmFile::interpolateSample()
{
    uint32_t nextSample = m_chip->getSound();
    if ( m_sampleSum == 0xFFFFFFFF ) // If no sample previously reached the mixer assign new sample
    {
        m_sampleSum = nextSample;
    }
    else if ( nextSample >= 8192 && nextSample > m_sampleSum )
    {
        m_sampleSum = nextSample;
    }
    else if ( nextSample < 8192 && nextSample < m_sampleSum )
    {
        m_sampleSum = nextSample;
    }
}

int VgmFile::decodePcm(uint8_t *outBuffer, int maxSize)
{
    int decoded = 0;
    while ( decoded + 4 <= maxSize )
    {
        while ( !m_waitSamples )
        {
            if (!nextCommand())
            {
                return decoded;
            }
        }
        while ( m_waitSamples && (decoded + 4 <= maxSize) )
        {
            interpolateSample();

            m_writeCounter += m_writeScaler;
            m_waitSamples--;

            if ( m_writeCounter >= VGM_SAMPLE_RATE )
            {
                *(reinterpret_cast<uint32_t *>(outBuffer)) = (m_sampleSum << 16) | m_sampleSum;
                outBuffer += 4;
                decoded += 4;
                m_writeCounter -= VGM_SAMPLE_RATE;
                m_sampleSum = 0xFFFFFFFF;
            }
        }
    }
    return decoded;
}

void VgmFile::setSampleFrequency( uint32_t frequency )
{
    m_writeScaler = frequency;
    if ( m_chip->getSampleFrequency() != VGM_SAMPLE_RATE )
    {
        LOG( "Error: chip must run at 44100 Hz sample frequency!!!\n" );
    }
}
