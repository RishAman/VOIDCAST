#ifndef VOIDCAST_PLATFORM_H
#define VOIDCAST_PLATFORM_H

#include "render.h"

typedef struct {
    Controls controls;
    int enter, pause, restart, new_maze, map, quit;
} Input;

int platform_init(void);
void platform_shutdown(void);
void platform_size(int *width, int *height);
Input platform_input(void);
int platform_present(const Screen *screen);
double platform_time(void);
void platform_sleep(unsigned milliseconds);
int platform_interrupted(void);

#endif
