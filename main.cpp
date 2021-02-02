/*
MIT License

Copyright (c) 2020-2021 Aleksei Dynda

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

#include "vgm_file.h"
#include "formats/wav_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef AUDIO_PLAYER
#define AUDIO_PLAYER 0
#endif

#if AUDIO_PLAYER
#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#endif

int readFile(const char *name, uint8_t **buffer )
{
    FILE *fileptr;
    int filelen;

    fileptr = fopen(name, "rb");
    if ( fileptr == nullptr )
    {
        return -1;
    }
    fseek(fileptr, 0, SEEK_END);
    filelen = ftell(fileptr);
    rewind(fileptr);
    *buffer = (uint8_t *)malloc(filelen);
    fread(*buffer, filelen, 1, fileptr);
    fclose(fileptr);
    return filelen;
}

int writeFile(const char *name, VgmFile *vgm, int trackIndex)
{
    bool warningDisplayed = false;
    uint8_t buffer[1024];
    FILE *fileptr;

    if ( trackIndex >= vgm->getTrackCount() )
    {
        fprintf( stderr, "Source sound file has only %d tracks\n", vgm->getTrackCount() );
        return -1;
    }

    fileptr = fopen(name, "wb");
    if ( fileptr == nullptr )
    {
        fprintf( stderr, "Failed to open file %s \n", name );
        return -1;
    }
    WaveHeader header =
    {
        0x46464952, 0, 0x45564157,
        0x20746d66, 16, 1, 2, 44100, 44100 * 2 * (16 / 8), 2 * (16 / 8), 16,
        0x61746164, 0
    };
    fwrite( &header, sizeof(header), 1, fileptr );
    vgm->setMaxDuration( 90000 );
    vgm->setFading( true );
    vgm->setSampleFrequency( 44100 );
    vgm->setTrack( trackIndex );
    vgm->setVolume( 100 );
    for(;;)
    {
        int size = vgm->decodePcm(buffer, sizeof(buffer));
        if ( size < 0 )
        {
            break;
        }
        for (int i = 0; i<sizeof(buffer); i+=2)
        {
            // Convert unsigned PCM16 to signed PCM16
            if ( *reinterpret_cast<uint16_t *>(buffer+i) == 65535 && !warningDisplayed )
            {
                warningDisplayed = true;
                fprintf( stderr, "Warning. Melody is too loud, possible peak cuts\n" );
            }
            *reinterpret_cast<int16_t *>(buffer+i) = *reinterpret_cast<uint16_t *>(buffer+i) - 32768;
        }
        fwrite( buffer, size, 1, fileptr );
        if ( size < 1024 )
        {
            break;
        }
    }
    int totalSize = ftell( fileptr );
    header.subchunk2Size = totalSize - sizeof(header);
    header.chunkSize = totalSize - 8;
    fseek(fileptr, 0, SEEK_SET);
    fwrite( &header, sizeof(header), 1, fileptr );
    fclose(fileptr);
    return 0;
}

#if AUDIO_PLAYER

static bool s_stopped = false;

void getAudioCallback(void *udata, uint8_t *stream, int len)
{
    VgmFile *vgm = (VgmFile *)udata;
    memset(stream, 0, len);
    int size = vgm->decodePcm(stream, len);
    s_stopped = size == 0;
}

int playTrack(VgmFile *vgm, int trackIndex)
{
    SDL_Init(SDL_INIT_AUDIO);
    SDL_AudioSpec spec{};
    spec.freq = 44100;
    spec.channels = 2;
    spec.format = AUDIO_U16SYS;
    spec.samples = 1024;
    spec.callback = getAudioCallback;
    spec.userdata = vgm;

    vgm->setMaxDuration( 90000 );
    vgm->setFading( true );
    vgm->setSampleFrequency( 44100 );
    vgm->setTrack( trackIndex );
    vgm->setVolume( 100 );
    s_stopped = false;

    if ( SDL_OpenAudio(&spec, NULL) < 0 )
    {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return -1;
    }
    SDL_PauseAudio(0);
    while ( !s_stopped )
    {
        SDL_Delay(100);
    }
    SDL_CloseAudio();
    SDL_Quit();
    return 0;
}
#endif

int main(int argc, char *argv[])
{
    int trackIndex = 0;
    if (argc < 3)
    {
        fprintf(stderr, "Converts NSF or VGM files to wav data\n");
        fprintf(stderr, "Usage: vgm2pcm input output [track_index]\n");
        #if AUDIO_PLAYER
        fprintf(stderr, "Usage: vgm2pcm input play [track_index]\n");
        #endif
        return -1;
    }
    if (argc > 3)
    {
        trackIndex = strtoul(argv[3], nullptr, 10);
    }
    uint8_t *data;
    int size = readFile( argv[1], &data );
    if ( size < 0 )
    {
        fprintf( stderr, "Failed to open file %s \n", argv[1] );
        return -1;
    }
    VgmFile file;
    if (!file.open(data, size))
    {
        fprintf( stderr, "Failed to parse vgm data %s \n", argv[1] );
        return -1;
    }
    #if AUDIO_PLAYER
    if ( !strcmp( argv[2], "play" ) )
    {
         playTrack( &file, trackIndex );
    }
    else
    #endif
    if ( writeFile( argv[2], &file, trackIndex ) < 0 )
    {
        return -1;
    }
    fprintf(stderr, "DONE\n");
    return 0;
}
