#include "vgm_file.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct
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
    vgm->setVolume( 128 );
    vgm->setSampleFrequency( 44100 );
    vgm->setTrack( trackIndex );
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

int main(int argc, char *argv[])
{
    int trackIndex = 0;
    if (argc < 3)
    {
        fprintf(stderr, "Converts NSF or VGM files to wav data\n");
        fprintf(stderr, "Usage: vgm2pcm input output [track_index]\n");
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
    if ( writeFile( argv[2], &file, trackIndex ) < 0 )
    {
        return -1;
    }
    fprintf(stderr, "DONE\n");
    return 0;
}
