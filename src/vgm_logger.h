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

/*
    VGM_DECODER_LOGGER accepts the following values
    0 - disable logging
    1 - enable only error logging
    2 - enable error and info logging
    3 - enable error, info and memory logging
*/

#ifndef VGM_DECODER_LOGGER
#define VGM_DECODER_LOGGER 0
#endif

#if VGM_DECODER_LOGGER
#include <stdio.h>

#define LOGE(...) fprintf( stderr, __VA_ARGS__ )
#if VGM_DECODER_LOGGER > 1
#define LOG(...) fprintf( stderr, __VA_ARGS__ )
#define LOGI(...) LOG(__VA_ARGS__)
#else
#define LOG(...)
#define LOGI(...)
#endif
#if VGM_DECODER_LOGGER > 2
#define LOGM(...) fprintf( stderr, __VA_ARGS__ )
#else
#define LOGM(...)
#endif

#else
#define LOGE(...)
#define LOG(...)
#define LOGI(...)
#define LOGM(...)
#endif
