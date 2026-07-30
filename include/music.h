#ifndef MUSIC_H
#define MUSIC_H

#include <stdint.h>

#define NOTE_C4  261
#define NOTE_D4  293
#define NOTE_E4  329
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_REST 0

typedef struct {
    uint32_t freq;
    uint32_t duration;
} note_t;

void music_init(void);
void music_update(void);
void music_toggle_play(void);
void music_next_track(void);
int music_is_playing(void);
const char* music_get_track_name(void);

#endif
