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
