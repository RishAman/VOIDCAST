#ifndef VOIDCAST_RENDER_H
#define VOIDCAST_RENDER_H

#include "game.h"

#define SCREEN_MAX_W 200
#define SCREEN_MAX_H 70
#define COLOR_COUNT 16

enum {
    C_BG, C_BLACK, C_DIM, C_WALL_DARK, C_WALL, C_WALL_LIGHT, C_CYAN,
    C_WHITE, C_PURPLE, C_PINK, C_RED, C_YELLOW, C_GREEN, C_FLOOR, C_PANEL, C_MUTED
};

typedef struct { unsigned char ch, fg, bg; } Cell;
typedef struct {
    int width, height;
    Cell cells[SCREEN_MAX_W * SCREEN_MAX_H];
    double depth[SCREEN_MAX_W];
} Screen;

extern const unsigned int render_colors[COLOR_COUNT];
void render_game(Screen *s, const Game *g, int width, int height, double fps);
int render_save_bmp(const Screen *s, const char *path);

#endif
