#ifndef MINIMP3_H
#define MINIMP3_H
/*
    https://github.com/lieff/minimp3
    To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
*/
#include <stdint.h>

#define MINIMP3_MAX_SAMPLES_PER_FRAME (1152*2)

typedef struct {
    int frame_bytes, frame_offset, channels, hz, layer, bitrate_kbps;
} mp3dec_frame_info_t;

typedef struct mp3dec_t mp3dec_t;

#ifdef __cplusplus
extern "C" {
#endif

void mp3dec_init(mp3dec_t *dec);
int mp3dec_decode_frame(mp3dec_t *dec, const uint8_t *mp3, int mp3_bytes, short *pcm, mp3dec_frame_info_t *info);

#ifdef __cplusplus
}
#endif

#ifdef MINIMP3_IMPLEMENTATION
/* Full implementation omitted in this stub for size; the real minimp3.h is large.
   For the actual project we will use a proper include. */
#endif

#endif /* MINIMP3_H */
