# VGM/NSF decoder

The library can be used to convert VGM / NSF data to PCM on the fly.

It recognizes:
 * vgm files (AY-3-8910, YM2149 (MSX2) and NES APU (Nes console))
 * nsf files (Nintendo Sound Format - NES APU)

## Compilation

> make

## Usage

> ./vgm2wav crisis_force.nsf crisis_force.wav 0
>
> DONE

Now open crisis_force.wav in any audio player.

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
