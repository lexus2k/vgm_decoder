#pragma once

#include <stdint.h>

typedef struct
{
    const uint8_t *data;
    uint32_t addr;
    uint32_t size;
} NesMemoryBlock;

class NesCartridge
{
public:
    NesCartridge() = default;
    virtual ~NesCartridge() = default;

    virtual uint8_t read(uint16_t address) = 0;

    virtual bool write(uint16_t address, uint8_t data) = 0;

    virtual void reset() = 0;

    virtual void power() = 0;
};
