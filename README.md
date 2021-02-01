# VGM/NSF decoder (*This is ARDUINO branch*)

The library can be used to convert VGM / NSF data to PCM on the fly.

It recognizes:
 * vgm files (AY-3-8910, YM2149 (MSX2) and NES APU (Nes console))
 * nsf files (Nintendo Sound Format - NES APU)

## How to install to Arduino IDE

1. Open https://github.com/lexus2k/vgm_decoder project page and switch from master to Arduino.
2. "Download ZIP" file (the Green Code button on the right).
3. Start your Arduino IDE and select Add ZIP library.

## Examples

```.cpp
#include "vgm_file.h"

VgmFile vgm;

void setup()
{
    vgm.open(data, len);   // Here data is pointer to Vgm/Nes file in memory
    vgm.setTrack( 0 );     // Valid for NSF files
    vgm.setVolume( 100 );  // Volume control
}

static uint16_t buffer[8192];

void loop()
{
    int len = vgm.decodePcm((uint8_t *)buffer, sizeof(buffer));
    // Do what you want with decoded PCM 16-bit stereo data
    if ( len == 0 )
    {
        // That's the end of track
    }
}

```

## Known issues

## Links

Wonderful resources for NSF files: https://www.zophar.net/music, for VGM files: https://vgmrips.net

## License

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
