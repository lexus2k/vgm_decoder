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

typedef struct NsfHeader
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

