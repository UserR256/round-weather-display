#ifndef MINIMP3_H
#define MINIMP3_H
/*
    https://github.com/lieff/minimp3
    Public domain.
*/
#include <stdint.h>

#define MINIMP3_MAX_SAMPLES_PER_FRAME (1152*2)

typedef struct {
    int frame_bytes, frame_offset, channels, hz, layer, bitrate_kbps;
} mp3dec_frame_info_t;

typedef struct mp3dec_t mp3dec_t;

void mp3dec_init(mp3dec_t *dec);
int mp3dec_decode_frame(mp3dec_t *dec, const uint8_t *mp3, int mp3_bytes, short *pcm, mp3dec_frame_info_t *info);

#ifdef MINIMP3_IMPLEMENTATION
/* Implementation is large; for this project we will use a pre-integrated version.
   Full minimp3.h should be downloaded from https://github.com/lieff/minimp3
   and placed here for compilation. */
#endif

#endif
