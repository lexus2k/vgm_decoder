#include "vgm_file.h"

#include <stdio.h>
#include <stdlib.h>


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

void writeFile(const char *name, VgmFile *vgm)
{
    uint8_t buffer[1024];
    FILE *fileptr;

    fileptr = fopen(name, "wb");
    if ( fileptr == nullptr )
    {
        return;
    }
    vgm->setVolume( 128 );
    vgm->setSampleFrequency( 44100 );
    for(;;)
    {
        int size = vgm->decodePcm(buffer, sizeof(buffer));
        if ( size < 0 ) break;
        for (int i = 0; i<sizeof(buffer); i+=2)
        {
             *reinterpret_cast<int16_t *>(buffer+i) = *reinterpret_cast<uint16_t *>(buffer+i) - 32768;
        }
        fwrite( buffer, size, 1, fileptr );
        if ( size < 1024 ) break;
    }
    fclose(fileptr);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: vgm2pcm input output\n");
        return -1;
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
    writeFile( argv[2], &file );
    fprintf(stderr, "DONE \n");
    return 0;
}
