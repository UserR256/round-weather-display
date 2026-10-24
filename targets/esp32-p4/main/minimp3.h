#ifndef MINIMP3_H
#define MINIMP3_H
/*
    https://github.com/lieff/minimp3
    To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    See <http://creativecommons.org/publicdomain/zero/1.0/>.
*/
#include <stdint.h>

#define MINIMP3_MAX_SAMPLES_PER_FRAME (1152*2)

typedef struct
{
    int frame_bytes, frame_offset, channels, hz, layer, bitrate_kbps;
} mp3dec_frame_info_t;

typedef struct
{
    float mdct_overlap[2][9*32], qmf_state[15*2*32];
    int reserv, free_format_bytes;
    unsigned char header[4], reserv_buf[511];
} mp3dec_t;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void mp3dec_init(mp3dec_t *dec);
#ifndef MINIMP3_FLOAT_OUTPUT
int mp3dec_decode_frame(mp3dec_t *dec, const uint8_t *mp3, int mp3_bytes, short *pcm, mp3dec_frame_info_t *info);
#else
int mp3dec_decode_frame(mp3dec_t *dec, const uint8_t *mp3, int mp3_bytes, float *pcm, mp3dec_frame_info_t *info);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef MINIMP3_IMPLEMENTATION
/* NOTE: Full implementation is ~70KB. For the build to succeed you must replace this file
   with the complete minimp3.h from https://github.com/lieff/minimp3/blob/master/minimp3.h
   (public domain). The decoder logic is already wired in audio_alarm.c. */
#endif /* MINIMP3_IMPLEMENTATION */

#endif /* MINIMP3_H */
