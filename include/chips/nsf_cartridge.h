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

#include "nes_cartridge.h"

#define APU_MAX_MEMORY_BLOCKS (4)

class NsfCartridge: public NesCartridge
{
public:
    NsfCartridge();
    virtual ~NsfCartridge();

    uint8_t read(uint16_t address) override;

    bool write(uint16_t address, uint8_t data) override;

    void reset() override;

    void power() override;

    /**
     * Registers new data memory blockю
     * @param data pointer to VGM data block (first 2 bytes is length).
     * @param len length in bytes of data passed to function.
     */
    void setDataBlock( const uint8_t *data, uint32_t len );

    /**
     * Registers new data memory blockю
     * @param addr physical memory address for emulated CPU
     * @param data pointer to memory data.
     * @param len length of memory data.
     */
    void setDataBlock( uint32_t addr, const uint8_t *data, uint32_t len );

private:
    NesMemoryBlock m_mem[APU_MAX_MEMORY_BLOCKS]{};
    /** Battery backed RAM */
    uint8_t *m_bbRam = nullptr;
    uint8_t m_bank[8]{};
    bool m_bankingEnabled = false;
    uint16_t m_mapper031BaseAddress = 0xFFFF;

    uint32_t mapper031(uint16_t address);

    bool allocBbRam();
};

