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

#include "chips/nsf_cartridge.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#define CLR_VALUE 0x00
#define BBRAM_SIZE 0x2000

#define NSF_CARTRIDGE_DEBUG 1

#if NSF_CARTRIDGE_DEBUG && !defined(VGM_DECODER_LOGGER)
#define VGM_DECODER_LOGGER NSF_CARTRIDGE_DEBUG
#endif
#include "../vgm_logger.h"

NsfCartridge::NsfCartridge(): NesCartridge()
{
    m_bankingEnabled = false;
    m_mapper031BaseAddress = 0xFFFF;
    for (int i=0; i<8; i++)
        m_bank[i] = i;
    reset();
}

NsfCartridge::~NsfCartridge()
{
    for (int i=0; i<APU_MAX_MEMORY_BLOCKS; i++)
    {
        m_mem[i].data = nullptr;
    }
    if ( m_bbRam != nullptr )
    {
        free( m_bbRam );
        m_bbRam = nullptr;
    }
}

void NsfCartridge::reset()
{
}

void NsfCartridge::power()
{
}

bool NsfCartridge::allocBbRam()
{
    if ( m_bbRam == nullptr )
    {
        m_bbRam = static_cast<uint8_t *>(malloc(BBRAM_SIZE));
        if ( m_bbRam == nullptr )
        {
            return false;
        }
        memset( m_bbRam, CLR_VALUE, BBRAM_SIZE );
    }
    return true;
}

bool NsfCartridge::write(uint16_t address, uint8_t data)
{
    uint32_t mappedAddr = mapper031( address );
    if ( address < 0x5000 )
    {
        LOGE( "Not cartridge space: 0x%04X (mapped to 0x%08X)\n", address, mappedAddr);
        return false;
    }
    if ( address <= 0x5FFF )
    {
        m_bankingEnabled = true;
        m_bank[ address & 0x07] = data;
        LOGI( "BANK %d [%04X] = %02X (%d) 0x%08X\n", address & 0x07, address,
               data, 0x8000 + data * 4096, 0x8000 + data * 4096 );
        return true;
    }
    if ( address < 0x8000 )
    {
        if ( !allocBbRam() )
        {
            LOGE("Failed to allocate battery backed RAM 0x%04X\n", address);
            return false;
        }
        m_bbRam[ address - 0x6000 ] = data;
        LOGM("Battery backed RAM [%04X] <== %02X\n", address, data);
        return true;
    }
    LOGE("Memory data write error (ROM) 0x%04X\n", address);
    return false;

}

uint8_t NsfCartridge::read(uint16_t address)
{
    uint32_t mappedAddr = mapper031( address );
    if ( address < 0x5000 )
    {
        LOGE( "Not cartridge space: 0x%04X (mapped to 0x%08X)\n", address, mappedAddr);
        return CLR_VALUE;
    }
    if ( address < 0x6000 )
    {
        return m_bank[ address & 0x07 ];
    }
    // Cartridge
    if ( address >= 0x6000 && address < 0x8000 )
    {
        if ( !allocBbRam() )
        {
            LOGE("Failed to allocate battery backed RAM 0x%04X\n", address);
            return CLR_VALUE;
        }
        LOGM("Battery backed RAM [%04X] ==> %02X\n", address, m_bbRam[ address - 0x6000 ]);
        return m_bbRam[ address - 0x6000 ];
    }
    for (int i=0; i<APU_MAX_MEMORY_BLOCKS; i++)
    {
        if ( m_mem[i].data == nullptr )
        {
            LOGE("Memory data fetch error 0x%04X (mapped to 0x%08X)\n", address, mappedAddr);
            break;
        }
        if ( mappedAddr >= m_mem[i].addr &&
             mappedAddr < m_mem[i].addr + m_mem[i].size )
        {
            uint32_t addr = mappedAddr - m_mem[i].addr;
            LOGM("[%04X] ==> %02X\n", address, m_mem[i].data[ addr ]);
            return  m_mem[i].data[ addr ];
        }
    }
    return CLR_VALUE;
}

void NsfCartridge::setDataBlock( const uint8_t *data, uint32_t len )
{
    if ( len < 2 )
    {
        LOGE("Invalid data - too short\n");
        return;
    }
    uint16_t address = data[0] + (data[1] << 8);
    setDataBlock( address, data + 2, len - 2 );
}

void NsfCartridge::setDataBlock( uint32_t addr, const uint8_t *data, uint32_t len )
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

uint32_t NsfCartridge::mapper031(uint16_t address)
{
    if ( !m_bankingEnabled )
    {
         return address;
    }
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

