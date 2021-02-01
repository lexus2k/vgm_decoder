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

typedef struct WaveHeader
{
    uint32_t chunkId; // 0x52494646
    uint32_t chunkSize; //  36 + SubChunk2Size, or more precisely: 4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
    uint32_t format; // 0x57415645
    uint32_t subchunk1Id; // 0x666d7420
    uint32_t subchunk1Size; // 16
    uint16_t audioFormat; // 1 - PCM
    uint16_t numChannels; // 2
    uint32_t sampleRate; // 44100
    uint32_t byteRate; // == SampleRate * NumChannels * BitsPerSample/8
    uint16_t blockAlign; // == NumChannels * BitsPerSample/8
    uint16_t bitsPerSample; // 16
    uint32_t subchunk2Id; // 0x64617461
    uint32_t subchunk2Size; // in bytes
} WaveHeader;



