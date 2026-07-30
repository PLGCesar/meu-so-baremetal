#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

void sound_play(uint32_t freq);
void sound_stop(void);
void sound_click(void);
void sound_startup(void);

#endif
