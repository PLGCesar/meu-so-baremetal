#include "../include/music.h"
#include "../include/sound.h"
#include <stddef.h>
#include <stdint.h>

static note_t track_tetris[] = {
    {NOTE_E5, 4}, {NOTE_B4, 2}, {NOTE_C5, 2}, {NOTE_D5, 4}, {NOTE_C5, 2}, {NOTE_B4, 2},
    {NOTE_A4, 4}, {NOTE_A4, 2}, {NOTE_C5, 2}, {NOTE_E5, 4}, {NOTE_D5, 2}, {NOTE_C5, 2},
    {NOTE_B4, 6}, {NOTE_C5, 2}, {NOTE_D5, 4}, {NOTE_E5, 4}, {NOTE_C5, 4}, {NOTE_A4, 6},
    {NOTE_REST, 4}
};

static note_t track_mario[] = {
    {NOTE_E5, 2}, {NOTE_E5, 2}, {NOTE_REST, 2}, {NOTE_E5, 2}, {NOTE_REST, 2}, {NOTE_C5, 2}, {NOTE_E5, 4},
    {NOTE_G5, 6}, {NOTE_REST, 4}, {NOTE_G4, 6}, {NOTE_REST, 4}
};

static int is_playing = 0;
static int current_track = 0;
static size_t note_index = 0;
static uint32_t note_timer = 0;

void music_init(void) {
    is_playing = 0;
    note_index = 0;
    note_timer = 0;
}

void music_toggle_play(void) {
    is_playing = !is_playing;
    if (!is_playing) {
        sound_stop();
    }
}

void music_next_track(void) {
    current_track = (current_track + 1) % 2;
    note_index = 0;
    note_timer = 0;
}

int music_is_playing(void) { return is_playing; }

const char* music_get_track_name(void) {
    if (current_track == 0) return "TETRIS THEME 8-BIT";
    return "SUPER MARIO CHIPTUNE";
}

void music_update(void) {
    if (!is_playing) return;

    note_t* current_score = (current_track == 0) ? track_tetris : track_mario;
    size_t score_length = (current_track == 0) ? (sizeof(track_tetris)/sizeof(note_t)) : (sizeof(track_mario)/sizeof(note_t));

    if (note_timer == 0) {
        note_t n = current_score[note_index];
        if (n.freq == NOTE_REST) {
            sound_stop();
        } else {
            sound_play(n.freq);
        }
        note_timer = n.duration * 3;
    } else {
        note_timer--;
        if (note_timer == 0) {
            note_index = (note_index + 1) % score_length;
        }
    }
}
